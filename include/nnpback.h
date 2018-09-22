#include <xen/io/xenbus.h>
#include <mini-os/types.h>
#include <xen/xen.h>
#ifndef NNPBACK_H
#define NNPBACK_H

struct backend_param
{
	float* param_ptr;
	int param_size;
};

void init_nnpback(void);

void shutdown_nnpback(void);

#endif
