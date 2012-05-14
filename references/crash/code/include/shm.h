#ifndef _LINUX_SHM_H_
#define _LINUX_SHM_H_
#include <page.h>
/* shm_mode upper byte flags */
#define	SHM_DEST	01000	/* segment will be destroyed on last detach */
#define SHM_LOCKED      02000   /* segment will not be swapped */
#define SHM_HUGETLB     04000   /* segment will use huge TLB pages */
#define SHM_NORESERVE   010000  /* don't check for reservations */

#define SHMMAX 0x2000000		 /* max shared seg size (bytes) */
#define SHMMIN 1			 /* min shared seg size (bytes) */
#define SHMMNI 4096			 /* max num of segs system wide */
#define SHMALL (SHMMAX/PAGE_SIZE*(SHMMNI/16)) /* max shm system wide (pages) */
#define SHMSEG SHMMNI			 /* max shared segs per process */
#endif
