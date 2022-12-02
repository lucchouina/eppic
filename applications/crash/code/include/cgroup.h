#ifndef _CGROUP_H
#define _CGROUP_H


#include <list.h>
#define for_each_root(root)						\
	list_for_each_entry((root), &cgroup_roots, root_list)

/* bits in struct cgroup_subsys_state flags field */
#define	        CSS_NO_REF	1
#define		CSS_ONLINE	2
#define		CSS_RELEASED	4
#define		CSS_VISIBLE	8
#define		CSS_DYING       16

#endif
