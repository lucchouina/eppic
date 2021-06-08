/*
    Directory entries
*/
#include <kernel.h>
string de_opt()   { return "d:f:"; }
string de_usage() { return "-d direntry [ -f file ]\n"; }
string de_help()  { return "provide information on a directory entry\n"; }

string d_path(struct dentry *dentry)
{
    string path="";

    if(dentry->d_parent != dentry) path=d_path(dentry->d_parent);
    else return "";
    path=path+"/"+getnstr(dentry->d_name.name, dentry->d_name.len);
    return path;
}

string d_filepath(struct file *f)
{
    struct mount *m=container_of(f->f_path.mnt, struct mount, mnt);
    struct dentry *d=f->f_path.dentry;
    string mntpt="", fpath=d_path(d);
    while(1) {
        mntpt=d_path(m->mnt_mountpoint)+mntpt;
        if(m == m->mnt_parent) break;
        m=m->mnt_parent;
        d=m->mnt_mountpoint;
    }
    if(mntpt+fpath == "") return getnstr(d->d_name.name, d->d_name.len);;
    return mntpt+fpath;
}

int de()
{
    struct mount *mnt;
    int mnts;
    mnts=getMounts(CURTASK);
    if(dflag) {
        struct dentry *d;
        d=(typeof(d))(atoi(darg));
        printf("dentry %p - path '%s'\n", d, d_path(d));
    }
    if(fflag) {
        struct file *f=(typeof(f))(atoi(farg));
        printf("file %p - path '%s'\n", f, d_filepath(f));
    }
    return 1;
}
