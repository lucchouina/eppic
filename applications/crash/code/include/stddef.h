#ifndef _LINUX_STDDEF_H
#define _LINUX_STDDEF_H


enum {
	false	= 0,
	true	= 1
};

/* extensive use of prefetch. Eliminate from parsing. */
#define prefetch(x)

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#endif
