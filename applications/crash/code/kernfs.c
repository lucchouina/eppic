/**
    Kern FS support
**/

struct kernfs_node *kernfs_common_ancestor(struct kernfs_node *a, struct kernfs_node *b)
{
	size_t da, db;
	struct kernfs_root *ra = kernfs_root(a), *rb = kernfs_root(b);

	if (ra != rb)
		return NULL;

	da = kernfs_depth(ra->kn, a);
	db = kernfs_depth(rb->kn, b);

	while (da > db) {
		a = a->parent;
		da--;
	}
	while (db > da) {
		b = b->parent;
		db--;
	}

	/* worst case b and a will be the same at root */
	while (b != a) {
		b = b->parent;
		a = a->parent;
	}

	return a;
}

struct kernfs_root *kernfs_root(struct kernfs_node *kn)
{
	if (kn->parent) kn = kn->parent;
	return kn->dir.root;
}

size_t kernfs_depth(struct kernfs_node *from, struct kernfs_node *to)
{
	size_t depth = 0;

	while (to->parent && to != from) {
		depth++;
		to = to->parent;
	}
	return depth;
}

string kernfs_path_from_node(struct kernfs_node *kn_to, struct kernfs_node *kn_from)
{
	struct kernfs_node *kn, *common;
	string parent_str = "/..", path="";
	size_t depth_from, depth_to, len = 0;
	int i, j;

	if (!kn_to) return "(null)";

	if (!kn_from) kn_from = kernfs_root(kn_to)->kn;

	if (kn_from == kn_to) return "/";

	common = kernfs_common_ancestor(kn_from, kn_to);

	depth_to = kernfs_depth(common, kn_to);
	depth_from = kernfs_depth(common, kn_from);

	for (i = 0; i < depth_from; i++) path += parent_str;

	for (i = depth_to - 1; i >= 0; i--) {
		for (kn = kn_to, j = 0; j < i; j++) kn = kn->parent;
		path += "/" + getstr(kn->name);
	}

	return path;
}
