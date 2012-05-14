#include <ipc/util.h>
#include <kernel.h>

struct msg_queue *msg_lock(struct ipc_namespace *ns, int id)
{
	struct kern_ipc_perm *ipcp = ipc_lock(&msg_ids(ns), id);

	return container_of(ipcp, struct msg_queue, q_perm);
}
