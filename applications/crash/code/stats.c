#include <stat.h>
int typeTbl=0;
string inode_type(struct inode *i)
{
    if(!typeTbl) {
        /* prime the table */
        typeTbl[S_IFLNK]="LNK";
        typeTbl[S_IFREG]="REG";
        typeTbl[S_IFDIR]="DIR";
        typeTbl[S_IFCHR]="CHR";
        typeTbl[S_IFBLK]="BLK";
        typeTbl[S_IFIFO]="FIFO";
        typeTbl[S_IFSOCK]="SOCK";
        typeTbl=1;
    }
    if(!(i->i_mode & S_IFMT)) return "UNKN";
    if(S_ISFIFO(i->i_mode)) {
        if(i->i_fop == &pipefifo_fops) return "PIPE";
    }
    return typeTbl[i->i_mode & S_IFMT];
}
