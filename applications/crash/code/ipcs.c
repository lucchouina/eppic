/*
    Command similar to unix ipcs(1) that will display information about
    shared memory, semaphores and message queues of the entire system
    of a a particular ID (if specified via option -i).
*/
string ipcs2_opt()   { return "mqsai:"; }
string ipcs2_usage() { return "[-mqsa] [-i id]\n"; }
string ipcs2_help()  { return "provide information on ipc facilities\n"; }

#include <ipc/util.h>
#include <shm.h>

static int id=-1; 

int shm_callback(int mode, struct kern_ipc_perm *ipc, struct ipc_namespace *ns)
{
    switch(mode) {
    
        case 0:
            printf("key        shmid      owner      perms      bytes      nattch     status \n");
            break;
        case 1:
            if(id==-1 || id==ipc->id) {
                struct shmid_kernel *shp = shm_lock(ns, ipc->id);
                printf("0x%08x %-10d %-10d %-10o %-10ld %-10ld %-6s %-6s\n",
                ipc->key,
                ipc->id,
                ipc->uid,
                ipc->mode & 0777,
                shp->shm_segsz,
                shp->shm_nattch,
                ipc->mode & SHM_DEST ? "dest" : " ",
                ipc->mode & SHM_LOCKED ? "locked" : " ");
            }
        break;
        case 2:
        break;
    }
    return 1;
}

int msg_callback(int mode, struct kern_ipc_perm *ipc, struct ipc_namespace *ns)
{
    switch(mode) {
    
        case 0:
            printf("key        msqid      owner      perms      used-bytes   messages\n");
            break;
        case 1:
            {
            struct msg_queue *msg = msg_lock(ns, ipc->id);
            printf("0x%08x %-10d %-10d %-10o %-12ld %-12ld\n",
                ipc->key,
                ipc->id,
                ipc->uid,
                ipc->mode & 0777,
                msg->q_cbytes,
                msg->q_qnum);
            }
        break;
        case 2:
        break;
    }
    return 1;
}

int sem_callback(int mode, struct kern_ipc_perm *ipc, struct ipc_namespace *ns)
{
    switch(mode) {
    
        case 0:
            printf("key        msqid      owner      perms      nsems\n");
            break;
        case 1:
            {
            struct sem_array *sem = sem_lock(ns, ipc->id);
            printf("0x%08x %-10d %-10d %-10o %-12ld\n",
                ipc->key,
                ipc->id,
                ipc->uid,
                ipc->mode & 0777,
                sem->sem_nsems);
            }
        break;
        case 2:
        break;
    }
    return 1;
}

int ipcs2()
{
    if(iflag) id=atoi(iarg);
    if(aflag || mflag) for_all_ipc_shm("shm_callback");
    if(aflag || sflag) for_all_ipc_sem("sem_callback");
    if(aflag || qflag) for_all_ipc_msg("msg_callback");
    if(!iflag && !aflag && (!(mflag || sflag || qflag))) printf("usage: ipcs2 %s", ipcs2_usage());
    return 1;
}

int for_all_ipc_ids(struct ipc_namespace *ns, struct ipc_ids *ids, string ipc_cb)
{
    struct kern_ipc_perm *ipc;
    int max_id = -1;
    int total, id;
    ipc_cb(0, 0, 0);
    if (ids->in_use == 0)
        return max_id;

    if (ids->in_use == IPCMNI)
        return IPCMNI - 1;

    /* Look for the last assigned id */
    total = 0;
    for (id = 0; id < IPCMNI && total < ids->in_use; id++) {
        ipc = idr_find(&ids->ipcs_idr, id);
        if (ipc != NULL) {
            max_id = id;
            total++;
            if(!ipc_cb(1, ipc, ns)) break;
        }
    }
    ipc_cb(2, 0, 0);
    return max_id;
}

void for_all_ipc_shm(string ipc_cb)
{
    for_all_ipc_ids(init_nsproxy.ipc_ns, &shm_ids(init_nsproxy.ipc_ns), ipc_cb);
}

void for_all_ipc_msg(string ipc_cb)
{
    for_all_ipc_ids(init_nsproxy.ipc_ns, &msg_ids(init_nsproxy.ipc_ns), ipc_cb);
}

void for_all_ipc_sem(string ipc_cb)
{
    for_all_ipc_ids(init_nsproxy.ipc_ns, &sem_ids(init_nsproxy.ipc_ns), ipc_cb);
}
