#ifndef __IPCS_H__
#define __IPCS_H__

#define IPCMNI 32768  /* <= MAX_INT limit for ipc arrays (including sysctl changes) */

#define IPCNS_MEMCHANGED    0x00000001   /* Notify lowmem size changed */
#define IPCNS_CREATED       0x00000002   /* Notify new ipc namespace created */
#define IPCNS_REMOVED       0x00000003   /* Notify ipc namespace removed */

#define IPCNS_CALLBACK_PRI 0

#endif
