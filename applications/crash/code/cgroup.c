#include <cgroup.h>
#include <sched.h>
#include <list.h>

string cgroup_opt()   { return "a"; }
string cgroup_usage() { return "[-a]\n"; }
string cgroup_help()  { 
    return "\
Display the cgroups for current task.\n\
With '-a', show all current last leaf of the cgroup hierarchy for all tasks\n"; 
}

static inline bool cgroup_is_dead(const struct cgroup *cgrp)
{
	return !(cgrp->self.flags & CSS_ONLINE);
}

bool cgroup_on_dfl(const struct cgroup *cgrp)
{
	return cgrp->root == &cgrp_dfl_root;
}

struct cgroup *cset_cgroup_from_root(struct css_set *cset, struct cgroup_root *root)
{
	struct cgroup *res_cgroup = NULL;

	if (cset == &init_css_set) {
		res_cgroup = &root->cgrp;
	} else if (root == &cgrp_dfl_root) {
		res_cgroup = cset->dfl_cgrp;
	} else {
		struct cgrp_cset_link *link;

		list_for_each_entry(link, &cset->cgrp_links, cgrp_link) {
			struct cgroup *cg = link->cgrp;
			if (cg->root == root) {
				res_cgroup = cg;
				break;
			}
		}
	}

	return res_cgroup;
}
string cgroup_path_ns_locked(struct cgroup *cgrp, struct cgroup_namespace *ns)
{
	struct cgroup *root = cset_cgroup_from_root(ns->root_cset, cgrp->root);
	return kernfs_path_from_node(cgrp->kn, root->kn);
}

void print_cgroup_leaf(struct task_struct *tsk)
{
string comm=getstr(&tsk->comm);
string scgroup="";
#if 1
struct cgrp_cset_link *link;

    list_for_each_entry(link, &tsk->cgroups->cgrp_links, cgrp_link) {
            string grpname= getstr(&link->cgrp->root->name);
            if(grpname == "systemd") {
                scgroup=getstr(link->cgrp->kn->name);
                break;
            }
    }
#else
string comm=getstr(&tsk->comm);
struct cgroup_root *root;
    for_each_root(root) {

        struct cgroup *cgrp;
        if (root->name[0] && getstr(&root->name[0]) == "systemd") {
        
            struct cgroup *cgrp;
            cgrp = cset_cgroup_from_root(tsk->cgroups, root);

	    if (cgroup_on_dfl(cgrp) || !(tsk->flags & PF_EXITING)) {
		    scgroup = getstr(cgrp->kn->name);
		    break;
	    }
        }
    }
#endif
    printf("%-6d %-30s %s\n", 
        tsk->pid,
        comm,
        scgroup
    );
}

int cgroup()
{
struct cgroup_root *root;
struct task_struct *tsk=CURTASK;

    if(aflag) {
    
        for_each_task("print_cgroup_leaf");
    }
    else for_each_root(root) {

        struct cgroup *cgrp;

        printf("%d:", root->hierarchy_id);
        if (root->name[0]) printf("name=%s", getstr(&root->name[0]));
        
        cgrp = cset_cgroup_from_root(tsk->cgroups, root);

	if (cgroup_on_dfl(cgrp) || !(tsk->flags & PF_EXITING)) {
                string path;
		path = cgroup_path_ns_locked(cgrp, tsk->nsproxy->cgroup_ns);
		printf("%s", path);
	} else {
		printf("/");
	}

	if (cgroup_on_dfl(cgrp) && cgroup_is_dead(cgrp))
		printf(" (deleted)\n");
	else
		printf("\n");
    }
    return 1;
}
