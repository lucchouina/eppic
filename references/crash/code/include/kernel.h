#ifndef _LINUX_KERNEL_H
#define _LINUX_KERNEL_H
#include <stddef.h>

#define container_of(ptr, type, member) ((type *)( (char *)ptr - offsetof(type,member) ))

#endif
