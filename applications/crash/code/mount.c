/*
    Event poll
*/
string emount_opt()   { return "m:"; }
string emount_usage() { return "-m task\n"; }
string emount_help()  { return "list mounts in the current context or context of option -m task\n"; }

#include <kernel.h>
#include <list.h>
enum enum_m {
    MNTPOINT,
    DEVNAME,
};

/* we cache the mount table using an associative array with the nsproxy as the key */
int mounts;

int getMounts(struct task_struct *t)
{
    struct nsproxy *ns=t->nsproxy;
    struct mount *root;
    struct mount *mnt;

    /* return the cached table */
    if(mounts[ns]) return mounts[ns];

    /* show that this entry is populated */
    mounts[ns]=1;
    root=ns->mnt_ns->root;
    mounts[ns][root]=1;
    list_for_each_entry(mnt, &root->mnt_list, mnt_list) {
        if(!mnt->mnt_mountpoint) break;
        mounts[ns][mnt]=1; /* create the table entry in the associative array */
    }
    return mounts[ns];
}

int emount()
{
    struct mount *m;
    int mnts=0;

    mnts=getMounts(CURTASK);
    printf(" %>MOUNT     %>SUPERBLK TYPE        DEVNAME      DIRNAME\n");
    printf("=%>======%>=============================================\n");
    for (m in mnts) {
        string mntpt="";
        struct mount *this=m;
        while(1) {
            mntpt=d_path(m->mnt_mountpoint)+mntpt;
            if(m==m->mnt_parent) break;
            m=m->mnt_parent;
        }
        m=this;
        if(mntpt=="") mntpt="/";
        printf("%p %p %-11s %-12s %s\n", m, m->mnt.mnt_sb, getstr(m->mnt.mnt_sb->s_type->name), getstr(m->mnt_devname), mntpt);
    }
    return 0;
}
