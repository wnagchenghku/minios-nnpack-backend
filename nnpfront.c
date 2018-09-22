#include <mini-os/os.h>
#include <mini-os/xenbus.h>
#include <mini-os/xmalloc.h>
#include <mini-os/events.h>
#include <mini-os/gntmap.h>
#include <mini-os/gnttab.h>
#include <xen/io/xenbus.h>
#include <mini-os/lib.h>
#include <fcntl.h>
#include <mini-os/posix/sys/mman.h>
#include <mini-os/nnpfront.h>

#include <mini-os/4C8732DB_frontend.h> // squeezenet1_0
#include <mini-os/2D24C20E_frontend.h> // resnet18
#include <mini-os/264993A3_frontend.h> // alexnet

#define NNPFRONT_PRINT_DEBUG
#ifdef NNPFRONT_PRINT_DEBUG
#define NNPFRONT_DEBUG(fmt,...) printk("Nnpfront:Debug("__FILE__":%d) " fmt, __LINE__, ##__VA_ARGS__)
#define NNPFRONT_DEBUG_MORE(fmt,...) printk(fmt, ##__VA_ARGS__)
#else
#define NNPFRONT_DEBUG(fmt,...)
#endif
#define NNPFRONT_ERR(fmt,...) printk("Nnpfront:Error " fmt, ##__VA_ARGS__)
#define NNPFRONT_LOG(fmt,...) printk("Nnpfront:Info " fmt, ##__VA_ARGS__)

struct nnpfront_dev {
   struct gntmap map;
};
typedef struct nnpfront_dev nnpfront_dev_t;

static nnpfront_dev_t gtpmdev;

static inline size_t divide_round_up(size_t dividend, size_t divisor) {
   if (dividend % divisor == 0) {
      return dividend / divisor;
   } else {
      return dividend / divisor + 1;
   }
}

domid_t self_id;
char *model = "squeezenet1_0";
int total_page;
float *page;
void init_nnpfront(void)
{
   char path[512];
   char* value, *err;
   unsigned long long ival;
   uint32_t bedomid;
   char *entry_value, *value_it;
   xenbus_event_queue events = NULL;
   int total_item, total_bytes, i, j = 0;
   char entry_path[64];
   grant_ref_t *grant_ref, *grant_ref_ref;

   int total_grant_ref_ref_page;
   grant_ref_t *grant_ref_ref_page;
   int v, bytesread;
   
   self_id = xenbus_get_self_id();

   printk("============= Init NNP Front ================\n");

   /* Get backend domid */
   if((err = xenbus_read(XBT_NIL, "/local/domain/backend", &value))) {
      NNPFRONT_ERR("Unable to read %s during nnpfront initialization! error = %s\n", path, err);
      free(err);
   }
   if(sscanf(value, "%llu", &ival) != 1) {
      NNPFRONT_ERR("%s has non-integer value (%s)\n", path, value);
      free(value);
   }
   free(value);
   bedomid = ival;

   snprintf(path, 512, "%u", self_id);

   if((err = xenbus_printf(XBT_NIL, "/local/domain/frontend", path, "%s", model))) {
      NNPFRONT_ERR("Unable to write to xenstore frontend id\n");
      free(err);
   }

   snprintf(path, 512, "/local/domain/backend/%d/state", self_id);
   /*Setup the watch to wait for the backend */
   if((err = xenbus_watch_path_token(XBT_NIL, path, path, &events))) {
      NNPFRONT_ERR("Could not set a watch on %s, error was %s\n", path, err);
      free(err);
   }

   NNPFRONT_LOG("Waiting for backend to publish references..\n");
   while(1) {
      int state = xenbus_read_integer(path);
      if(state == 1)
         break;
       xenbus_wait_for_watch(&events);
   }

   total_bytes = 0;
   if (strcmp(model, "squeezenet1_0") == 0) {
      total_item = sizeof(P4C8732DB_frontend) / sizeof(struct frontend_param);
      for (i = 0; i < total_item; ++i)
         total_bytes += P4C8732DB_frontend[i].param_size * sizeof(float);
   } else if (strcmp(model, "resnet18") == 0) {
      total_item = sizeof(P2D24C20E_frontend) / sizeof(struct frontend_param);
      for (i = 0; i < total_item; ++i)
         total_bytes += P2D24C20E_frontend[i].param_size * sizeof(float);
   } else if (strcmp(model, "alexnet") == 0) {
      total_item = sizeof(P264993A3_frontend) / sizeof(struct frontend_param);
      for (i = 0; i < total_item; ++i)
         total_bytes += P264993A3_frontend[i].param_size * sizeof(float);
   }

   total_page = divide_round_up(total_bytes, PAGE_SIZE);
   
   grant_ref = (grant_ref_t*)malloc(sizeof(grant_ref_t) * total_page);

   total_grant_ref_ref_page = divide_round_up(total_page * sizeof(grant_ref_t), PAGE_SIZE);
   grant_ref_ref = (grant_ref_t*)malloc(sizeof(grant_ref_t) * total_grant_ref_ref_page);

   snprintf(entry_path, 64, "/local/domain/backend/%d/grant-ref-ref", self_id);
   if((err = xenbus_read(XBT_NIL, entry_path, &entry_value))) {
      NNPFRONT_ERR("Unable to read %s during tpmfront initialization! error = %s\n", entry_path, err);
      free(err);
   }
   value_it = entry_value;
   while(sscanf(value_it, "%d%n", &v, &bytesread) > 0) {
      grant_ref_ref[j++] = v;
      value_it += bytesread;
   }

   if ((grant_ref_ref_page = (grant_ref_t*)gntmap_map_grant_refs_batch(&gtpmdev.map, total_grant_ref_ref_page, &bedomid, 0, grant_ref_ref, PROT_READ)) == NULL) {
      NNPFRONT_ERR("Failed to map grant reference %u\n", (unsigned int) bedomid);
   }

   for (i = 0; i < total_page; ++i)
      grant_ref[i] = grant_ref_ref_page[i];

   gntmap_munmap(&gtpmdev.map, (unsigned long)(void*)grant_ref_ref_page, total_grant_ref_ref_page);
   
   if ((page = gntmap_map_grant_refs_batch(&gtpmdev.map, total_page, &bedomid, 0, grant_ref, PROT_READ)) == NULL) {
      NNPFRONT_ERR("Failed to map grant reference %u\n", (unsigned int) bedomid);
   }

   free(grant_ref_ref);
   free(grant_ref);
   NNPFRONT_LOG("Initialization Completed successfully\n");
}

void shutdown_nnpfront(void)
{
   char *err;
   char path[512];
   gntmap_munmap(&gtpmdev.map, (unsigned long)(void*)page, total_page);

   snprintf(path, 512, "/local/domain/frontend/%u", self_id);
   if((err = xenbus_write(XBT_NIL, path, "close"))) {
      NNPFRONT_ERR("Unable to write to xenstore closing state\n");
      free(err);
   }
}

float *resolve_param_cb(void)
{
   static int isFirst = 1, param_it, param_read;
   if (isFirst) {
      isFirst = 0;
      return page;
   }
   
   if (strcmp(model, "squeezenet1_0") == 0)
      param_read += P4C8732DB_frontend[param_it++].param_size;
   else if (strcmp(model, "resnet18") == 0)
      param_read += P2D24C20E_frontend[param_it++].param_size;
   else if (strcmp(model, "alexnet") == 0)
      param_read += P264993A3_frontend[param_it++].param_size;

   return page + param_read;
}
