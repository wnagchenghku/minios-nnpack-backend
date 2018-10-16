#include <mini-os/os.h>
#include <mini-os/xenbus.h>
#include <mini-os/events.h>
#include <errno.h>
#include <mini-os/gnttab.h>
#include <xen/io/xenbus.h>
#include <xen/io/protocols.h>
#include <mini-os/xmalloc.h>
#include <time.h>
#include <mini-os/lib.h>
#include <fcntl.h>
#include <mini-os/mm.h>
#include <mini-os/posix/sys/mman.h>

#include <mini-os/nnpback.h>
#include <mini-os/utlist.h>

#include <mini-os/4C8732DB_backend.h> // squeezenet1_0
#include <mini-os/2D24C20E_backend.h> // resnet18
#include <mini-os/264993A3_backend.h> // alexnet
#include <mini-os/C37828B0_backend.h> // densenet121
#include <mini-os/6614F490_backend.h> // vgg11

#define NNPBACK_PRINT_DEBUG
#ifdef NNPBACK_PRINT_DEBUG
#define NNPBACK_DEBUG(fmt,...) printk("Nnpback:Debug("__FILE__":%d) " fmt, __LINE__, ##__VA_ARGS__)
#define NNPBACK_DEBUG_MORE(fmt,...) printk(fmt, ##__VA_ARGS__)
#else
#define NNPBACK_DEBUG(fmt,...)
#endif
#define NNPBACK_ERR(fmt,...) printk("Nnpback:Error " fmt, ##__VA_ARGS__)
#define NNPBACK_LOG(fmt,...) printk("Nnpback:Info " fmt, ##__VA_ARGS__)

typedef struct el {
    domid_t domid;
    grant_ref_t *grant_ref;
    grant_ref_t *grant_ref_ref;
    int total_page;
    int total_grant_ref_ref_page;
    struct el *next, *prev;
} el;

int namecmp(el *a, el *b) {
    return a->domid == b->domid ? 0 : -1;
}

struct nnpback_dev {

   struct gntmap map;

   xenbus_event_queue events;
};
typedef struct nnpback_dev nnpback_dev_t;

enum { EV_NONE, EV_NEWFE, EV_CLOSEFE } tpm_ev_enum;

/* Global objects */
static struct thread* eventthread = NULL;
static nnpback_dev_t gtpmdev = {
   .events = NULL,
};

/* parses the string that comes out of xenbus_watch_wait_return. */
static int parse_eventstr(const char* evstr, domid_t* domid, char* model)
{
   char* err;
   char* value;
   unsigned int udomid = 0;

  if (sscanf(evstr, "/local/domain/frontend/%u", &udomid) == 1) {
      *domid = udomid;
      if((err = xenbus_read(XBT_NIL, evstr, &value))) {
         free(err);
         return EV_NONE;
      }

      sscanf(value, "%s", model);
      free(value);
      if (strcmp(model, "close") == 0) {
         return EV_CLOSEFE;
      }
      return EV_NEWFE;
   }
   return EV_NONE;
}

static inline size_t divide_round_up(size_t dividend, size_t divisor) {
   if (dividend % divisor == 0) {
      return dividend / divisor;
   } else {
      return dividend / divisor + 1;
   }
}

int log2(int v)
{
   if (v == 1)
      return 0;

   return 1 + log2(v >> 1);
}

unsigned int round_up_power_of_two(unsigned int v) // compute the next highest power of 2 of 32-bit v
{
   v--;
   v |= v >> 1;
   v |= v >> 2;
   v |= v >> 4;
   v |= v >> 8;
   v |= v >> 16;
   v++;

   return v;
}

el *head = NULL; /* important- initialize to NULL! */
static float *squeezenet1_0_page = NULL;
static float *resnet18_page = NULL;
static float *alexnet_page = NULL;
static float *densenet121_page = NULL;
static float *vgg11_page = NULL;

void handle_backend_event(char* evstr) {
   domid_t domid;
   int event;
   char *err;
   int i, j, k = 0, total_item, total_bytes, total_page, total_grant_ref_ref_page;
   char model[16], frontend_path[32];
   char entry_path[64], entry_value[1024];
   char state_path[64], state_value[8];
   grant_ref_t *grant_ref, *grant_ref_ref;
   void *page;
   grant_ref_t *grant_ref_ref_page;

   struct timeval start, end;
   unsigned long e_usec;
   el *name, *elt, etmp;

   NNPBACK_DEBUG("Xenbus Event: %s\n", evstr);

   event = parse_eventstr(evstr, &domid, model);
   
   if (event == EV_NEWFE) {
      snprintf(frontend_path, 32, "/local/domain/backend/%d", domid);
      if((err = xenbus_write(XBT_NIL, frontend_path, "0"))) {
         NNPBACK_ERR("Unable to write frontend domain id, error was %s\n", err);
         free(err);
      }

      total_bytes = 0;
      if (strcmp("squeezenet1_0", model) == 0) {
         total_item = sizeof(P4C8732DB_backend) / sizeof(struct backend_param);
         for (i = 0; i < total_item; ++i)
            total_bytes += P4C8732DB_backend[i].param_size * sizeof(float);
      } else if (strcmp("resnet18", model) == 0) {
         total_item = sizeof(P2D24C20E_backend) / sizeof(struct backend_param);
         for (i = 0; i < total_item; ++i)
            total_bytes += P2D24C20E_backend[i].param_size * sizeof(float);
      } else if (strcmp("alexnet", model) == 0) {
         total_item = sizeof(P264993A3_backend) / sizeof(struct backend_param);
         for (i = 0; i < total_item; ++i)
            total_bytes += P264993A3_backend[i].param_size * sizeof(float);
      } else if (strcmp("densenet121", model) == 0) {
         total_item = sizeof(PC37828B0_backend) / sizeof(struct backend_param);
         for (i = 0; i < total_item; ++i)
            total_bytes += PC37828B0_backend[i].param_size * sizeof(float);
      } else if (strcmp("vgg11", model) == 0) {
         total_item = sizeof(P6614F490_backend) / sizeof(struct backend_param);
         for (i = 0; i < total_item; ++i)
            total_bytes += P6614F490_backend[i].param_size * sizeof(float);
      } 

      total_page = divide_round_up(total_bytes, PAGE_SIZE);

      if (strcmp("squeezenet1_0", model) == 0) {
         if (squeezenet1_0_page == NULL) {
            squeezenet1_0_page = (float*)alloc_pages(log2(round_up_power_of_two(total_page)));

            for (i = 0; i < total_item; ++i)
               for (j = 0; j < P4C8732DB_backend[i].param_size; ++j)
                  *(squeezenet1_0_page + k++) = *(P4C8732DB_backend[i].param_ptr + j);
         }
         page = (void*)squeezenet1_0_page;
      } else if (strcmp("resnet18", model) == 0) {
         if (resnet18_page == NULL) {
            resnet18_page = (float*)alloc_pages(log2(round_up_power_of_two(total_page)));

            for (i = 0; i < total_item; ++i)
               for (j = 0; j < P2D24C20E_backend[i].param_size; ++j)
                  *(resnet18_page + k++) = *(P2D24C20E_backend[i].param_ptr + j);
         }
         page = (void*)resnet18_page;
      } else if (strcmp("alexnet", model) == 0) {
         if (alexnet_page == NULL) {
            alexnet_page = (float*)alloc_pages(log2(round_up_power_of_two(total_page)));

            for (i = 0; i < total_item; ++i)
               for (j = 0; j < P264993A3_backend[i].param_size; ++j)
                  *(alexnet_page + k++) = *(P264993A3_backend[i].param_ptr + j);
         }
         page = (void*)alexnet_page;
      } else if (strcmp("densenet121", model) == 0) {
         if (densenet121_page == NULL) {
            densenet121_page = (float*)alloc_pages(log2(round_up_power_of_two(total_page)));

            for (i = 0; i < total_item; ++i)
               for (j = 0; j < PC37828B0_backend[i].param_size; ++j)
                  *(densenet121_page + k++) = *(PC37828B0_backend[i].param_ptr + j);
         }
         page = (void*)densenet121_page;
      } else if (strcmp("vgg11", model) == 0) {
         if (vgg11_page == NULL) {
            vgg11_page = (float*)alloc_pages(log2(round_up_power_of_two(total_page)));

            for (i = 0; i < total_item; ++i)
               for (j = 0; j < P6614F490_backend[i].param_size; ++j)
                  *(vgg11_page + k++) = *(P6614F490_backend[i].param_ptr + j);
         }
         page = (void*)vgg11_page;
      }

      grant_ref = (grant_ref_t*)malloc(sizeof(grant_ref_t) * total_page);

      gettimeofday(&start, 0);
      for (i = 0; i < total_page; ++i) {
         grant_ref[i] = gnttab_grant_access(domid, virt_to_mfn((uintptr_t)page + i * PAGE_SIZE), 0);
      }
      gettimeofday(&end, 0);
      
      total_grant_ref_ref_page = divide_round_up(total_page * sizeof(grant_ref_t), PAGE_SIZE);
      grant_ref_ref = (grant_ref_t*)malloc(sizeof(grant_ref_t) * total_grant_ref_ref_page);
      grant_ref_ref_page = (grant_ref_t*)alloc_pages(log2(round_up_power_of_two(total_grant_ref_ref_page)));
      
      assert(total_grant_ref_ref_page <= 128);

      for (i = 0; i < total_page; ++i)
         grant_ref_ref_page[i] = grant_ref[i];

      for (i = 0; i < total_grant_ref_ref_page; ++i)
         grant_ref_ref[i] = gnttab_grant_access(domid, virt_to_mfn((uintptr_t)(void*)grant_ref_ref_page + i * PAGE_SIZE), 0);

      snprintf(entry_value, 1024, "%s", "");
      for (i = 0; i < total_grant_ref_ref_page; ++i) {
            snprintf(entry_value + strlen(entry_value), 1024 - strlen(entry_value), "%lu ", (unsigned long)grant_ref_ref[i]);
      }

      snprintf(entry_path, 64, "%s/grant-ref-ref", frontend_path);
      if((err = xenbus_write(XBT_NIL, entry_path, entry_value))) {
         NNPBACK_ERR("Unable to write ring-ref, error was %s\n", err);
         free(err);
      }

      snprintf(state_path, 64, "%s/state", frontend_path);
      snprintf(state_value, 8, "%d", 1);
      if((err = xenbus_write(XBT_NIL, state_path, state_value))) {
          NNPBACK_ERR("Unable to write state path, error was %s\n", err);
          free(err);
      }

      name = (el *)malloc(sizeof *name);
      name->domid = domid;
      name->grant_ref = grant_ref;
      name->grant_ref_ref = grant_ref_ref;
      name->total_page = total_page;
      name->total_grant_ref_ref_page = total_grant_ref_ref_page;
      DL_APPEND(head, name);
      e_usec = ((end.tv_sec * 1000000) + end.tv_usec) - ((start.tv_sec * 1000000) + start.tv_usec);
      NNPBACK_LOG("Publishing grant references takes %lu microseconds\n", e_usec);
   } else if (event == EV_CLOSEFE) {
      etmp.domid = domid;
      DL_SEARCH(head, elt, &etmp, namecmp);
      for (i = 0; i < elt->total_page; ++i) {
         gnttab_end_access(elt->grant_ref[i]);
      }
      for (i = 0; i < elt->total_grant_ref_ref_page; ++i) {
         gnttab_end_access(elt->grant_ref_ref[i]);
      }
      free(elt->grant_ref);
      free(elt->grant_ref_ref);
      DL_DELETE(head, elt);
      free(elt);
   }
}

static void event_listener(void)
{
   const char* bepath = "/local/domain/frontend";
   char **path;
   char* err;

   /* Setup the backend device watch */
   if((err = xenbus_watch_path_token(XBT_NIL, bepath, bepath, &gtpmdev.events)) != NULL) {
      NNPBACK_ERR("xenbus_watch_path_token(%s) failed with error %s!\n", bepath, err);
      free(err);
      goto egress;
   }

   /* Wait and listen for changes in frontend connections */
   while(1) {
      path = xenbus_wait_for_watch_return(&gtpmdev.events);

      handle_backend_event(*path);
      free(path);
   }

   if((err = xenbus_unwatch_path_token(XBT_NIL, bepath, bepath)) != NULL) {
      free(err);
   }
egress:
   return;
}

void event_thread(void* p) {
   event_listener();
}

void init_nnpback(void)
{
   char* err;
   char value[16];

   printk("============= Init NNP BACK ================\n");

   gnttab_reset_model();

   snprintf(value, 16, "%d", xenbus_get_self_id());
   if ((err = xenbus_write(XBT_NIL, "/local/domain/backend", value)))
   {
      NNPBACK_ERR("Unable to write backend id: %s\n", err);
      free(err);
   }

   eventthread = create_thread("nnpback-listener", event_thread, NULL);

}
