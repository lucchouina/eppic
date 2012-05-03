#include <ipc/util.h>
#include <kernel.h>

struct shmid_kernel *shm_lock(struct ipc_namespace *ns, int id)
{
	struct kern_ipc_perm *ipcp = ipc_lock(&shm_ids(ns), id);

	return container_of(ipcp, struct shmid_kernel, shm_perm);
}
