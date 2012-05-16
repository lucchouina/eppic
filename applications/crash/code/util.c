#include <ipc/util.h>
struct kern_ipc_perm *ipc_lock(struct ipc_ids *ids, int id)
{
	struct kern_ipc_perm *out;
	int lid = ipcid_to_idx(id);

	out = idr_find(&ids->ipcs_idr, lid);

	return out;
}
