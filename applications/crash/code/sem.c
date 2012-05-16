#include <ipc/util.h>
#include <kernel.h>

struct sem_array *sem_lock(struct ipc_namespace *ns, int id)
{
	struct kern_ipc_perm *ipcp = ipc_lock(&sem_ids(ns), id);

	return container_of(ipcp, struct sem_array, sem_perm);
}
