#ifndef NNPFRONT_H
#define NNPFRONT_H

#include <mini-os/types.h>
#include <mini-os/os.h>
#include <mini-os/events.h>
#include <xen/xen.h>
#include <xen/io/xenbus.h>

struct frontend_param
{
	char* param_name;
	int param_size;
};

void init_nnpfront(void);

void shutdown_nnpfront(void);
float *resolve_param_cb(void);

#endif
