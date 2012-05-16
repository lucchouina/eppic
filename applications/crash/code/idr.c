#include <idr.h>

void *idr_find(struct idr *idp, int id)
{
	int n;
	struct idr_layer *p;

	p = idp->top;
	if (!p)
		return NULL;
	n = (p->layer+1) * IDR_BITS;

	/* Mask off upper bits we don't use for the search. */
	id &= MAX_ID_MASK;

	if (id >= (1 << n))
		return NULL;

	while (n > 0 && p) {
		n -= IDR_BITS;
		p = p->ary[(id >> n) & IDR_MASK];
	}
	return((struct kern_ipc_perm  *)p);
}
