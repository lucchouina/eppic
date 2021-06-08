#ifndef _RBTREE_H
#define _RBTREE_H
#include <kernel.h>
#define	rb_entry(ptr, type, member) container_of(ptr, type, member)
#define RB_EMPTY_NODE(node)  \
	((node)->__rb_parent_color == (unsigned long)(node))
#define RB_CLEAR_NODE(node)  \
	((node)->__rb_parent_color = (unsigned long)(node))
#define rb_parent(r) ((struct rb_node *)((r)->__rb_parent_color & ~3))
#endif
