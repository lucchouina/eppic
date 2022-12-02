#include <cgroup.h>
#include <sched.h>
#include <list.h>

string cgroup_opt()   { return ""; }
string cgroup_usage() { return "\n"; }
string cgroup_help()  { return "Display the cgroups for current task\n"; }

static inline bool cgroup_is_dead(const struct cgroup *cgrp)
{
	return !(cgrp->self.flags & CSS_ONLINE);
}

bool cgroup_on_dfl(const struct cgroup *cgrp)
{
	return cgrp->root == &cgrp_dfl_root;
}

struct cgroup *cset_cgroup_from_root(struct css_set *cset,
					    struct cgroup_root *root)
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

int cgroup()
{
struct cgroup_root *root;
struct task_struct *tsk=CURTASK;

    for_each_root(root) {
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
