/*
    Event poll
*/
string epoll_opt()   { return "f:c"; }
string epoll_usage() { return "-f struct_file* [-c]\n"; }
string epoll_help()  { return "provide information on a event poll file\nFollow connections with 'c' option\n"; }

#include <stddef.h>
#include <rbtree.h>

static inline int is_file_epoll(struct file *f)
{
    return f->f_op == &eventpoll_fops;
}

int epoll()
{
    if(fflag) {
        struct file *f=(struct file*)(atoi(farg));
        struct eventpoll *ep;

        if(!is_file_epoll(f)) printf("0x%p is not an epoll file\n", f);
        else {
	    struct rb_node *rbp;
            struct eventpoll *ep = (typeof(ep))(f->private_data);
            file_print_header();
            for (rbp = rb_first(&ep->rbr); rbp; rbp = rb_next(rbp)) {
                struct epitem *epi = rb_entry(rbp, struct epitem, rbn);
                file_print(epi->ffd.fd, epi->ffd.file, cflag);
	    }
            file_print_footer();
        }
    }
    else printf("usage: epoll %s", epoll_usage());
    return 1;
}
