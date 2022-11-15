#ifndef _IPC_UTIL_H
#define _IPC_UTIL_H
#include <ipcs.h>

#define SEQ_MULTIPLIER	(IPCMNI)


#define IPC_SEM_IDS	0
#define IPC_MSG_IDS	1
#define IPC_SHM_IDS	2

#define ipcid_to_idx(id) ((id) % SEQ_MULTIPLIER)

#define sem_ids(ns) ((ns)->ids[IPC_SEM_IDS])
#define msg_ids(ns) ((ns)->ids[IPC_MSG_IDS])
#define shm_ids(ns) ((ns)->ids[IPC_SHM_IDS])

static inline int ipc_buildid(int id, int seq)
{
	return SEQ_MULTIPLIER * seq + id;
}

static inline int ipc_checkid(struct kern_ipc_perm *ipcp, int uid)
{
	if (uid / SEQ_MULTIPLIER != ipcp->seq)
		return 1;
	return 0;
}

#endif
