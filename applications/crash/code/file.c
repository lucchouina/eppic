string efiles_opt()   { return "f:i:c"; }
string efiles_usage() { return "-f struct_file* [-i fd] [-c]\n"; }
string efiles_help()  { return "provide information on a file\nFollow connections with 'c' option\n"; }

void file_print_header()
{
    printf(" FD %>FILE     %>DENTRY   %>INODE       TYPE  PATH\n");
    printf("====%>=========%>=========%>==================\n");
}

void file_print_footer()
{
    printf("====%>=========%>=========%>==================\n");
}

int efiles()
{
    struct task_struct *t=CURTASK;
    struct fdtable *fdt=t->files->fdt;
    int i, mfd=-1, maxfds=fdt->max_fds;
    struct file **fd=fdt->fd;

    file_print_header();

    if(iflag) mfd=atoi(iarg);

    for(i=0;i<maxfds; i++) {
        if(fd[i]) 
            if(!iflag || mfd==i) 
                file_print(i, fd[i], 0);
    }
    
    file_print_footer();
    return 1;
}

void file_print(int fd, struct file *f, int con)
{
    struct dentry *de=f->f_path.dentry;
    struct inode *i=de->d_inode;
    string t=inode_type(i);
    printf("%3d %p %p %p %-5s %s\n", fd, f, de, i, t, d_filepath(f));
    if(con) {
        if(t == "PIPE") {
            waiters(&(i->i_pipe)->wait);
        }
    }
}
