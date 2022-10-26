#ifndef _LINUX_STDDEF_H
#define _LINUX_STDDEF_H

/* extensive use of prefetch. Eliminate from parsing. */

#define prefetch(x) (1)

#define offsetof(type, member) ((size_t) &((type *)0)->member)

#endif
