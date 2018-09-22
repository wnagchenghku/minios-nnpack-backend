#ifdef HAVE_LIBC

#include <stdlib.h>
#include <malloc.h>

int posix_memalign(void **memptr, size_t alignment, size_t size)
{
        *memptr = memalign(alignment, size);
        if (*memptr == NULL)
        	return -1;
        
        return 0;
}

#endif