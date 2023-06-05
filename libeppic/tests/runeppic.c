#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eppic_api.h"

#define ARRAY_SIZE(arr)	(sizeof(arr) / sizeof(arr[0]))

static const char PROGRAM_NAME[] = "runeppic";

/************************************************************
 * EPPIC API
 */

struct def_enumconst {
	struct def_enumconst *next;
	char *name;
	unsigned long value;
};

struct def_member {
	struct def_member *next;
	char *name;
	TYPE_S *type;
	unsigned offset;
	int fbit, nbits;
};

struct def_type {
	struct def_type *next;
	char *name;
	char *key;
	int ctype;
	unsigned long size;
	TYPE_S *ref;
	struct def_enumconst *ec;
	struct def_member *members;
};

struct def_symbol {
	struct def_symbol *next;
	const char *name;
	TYPE_S *type;
	unsigned long long val;
};

static bool ready;
static int abi = ABI_X86;
static int nbpw = sizeof(int);
static int sign;

static char *memory;
static size_t memory_size;

static struct def_type *types;
static struct def_symbol *syms;

static int lookup_ctype(int ctype, char *name, TYPE_S *tout, bool bykey)
{
	struct def_type *type;

	for (type = types; type; type = type->next) {
		const char *s = bykey ? type->key : type->name;
		if (type->ctype == ctype &&
		    s && !strcmp(s, name))
			break;
	}
	if (!type)
		return 0;

	switch (type->ctype) {
	case V_BASE:
		eppic_inttype(tout, type->size);
		break;
	case V_REF:
		eppic_duptype(tout, type->ref);
		eppic_pushref(tout, 1);
		break;
	case V_ENUM:
		eppic_type_mkenum(tout);
		eppic_type_setsize(tout, type->size);
		if (type->name)
			eppic_type_setidxbyname(tout, (char*) type->name);
		break;
	case V_UNION:
		eppic_type_mkunion(tout);
		eppic_type_setsize(tout, type->size);
		if (type->name)
			eppic_type_setidxbyname(tout, (char*) type->name);
		break;
	case V_STRUCT:
		eppic_type_mkstruct(tout);
		eppic_type_setsize(tout, type->size);
		if (type->name)
			eppic_type_setidxbyname(tout, (char*) type->name);
		break;
	case V_TYPEDEF:
		eppic_type_mktypedef(tout);
		eppic_type_setref(tout, 0, eppic_type_gettype(type->ref));
		break;
	case V_ARRAY:
		break;
	default:
		fprintf(stderr, "Unhandled ctype %d\n", type->ctype);
		return 0;
	}

	return 1;
}

static int apigetmem(ull iaddr, void *p, int nbytes)
{
	if (iaddr >= memory_size)
		return 0;
	if (nbytes > (ull)memory_size - iaddr)
		nbytes = memory_size - iaddr;
	memcpy(p, memory + iaddr, nbytes);
	return 1;
}

static int apiputmem(ull iaddr, void *p, int nbytes)
{
	if (iaddr >= memory_size)
		return 0;
	if (nbytes > (ull)memory_size - iaddr)
		nbytes = memory_size - iaddr;
	memcpy(memory + iaddr, p, nbytes);
	return 1;
}

static int apimember(char *sname, char *mname, void **stmp)
{
	struct def_member *member;
	struct def_type *type;

	for (type = types; type; type = type->next) {
		if ((type->ctype == V_STRUCT || type->ctype == V_UNION) &&
		    type->name && !strcmp(type->name, sname))
			break;
	}
	if (!type)
		return 0;

	for (member = type->members; member; member = member->next) {
		if (*mname && strcmp(member->name, mname))
			continue;
		eppic_new_member(stmp, member->name ?: "");
		eppic_member_info(stmp, member->offset,
				  eppic_type_getsize(member->type),
				  member->fbit, member->nbits);
		eppic_duptype(eppic_stm_type(stmp), member->type);
	}

	return 1;
}

static int apigetctype(int ctype, char *name, TYPE_S *tout)
{
	return lookup_ctype(ctype, name, tout, false);
}

static int apigetval(char *name, ull *addr, VALUE_S *v)
{
	struct def_symbol *sym;
	TYPE_S *stype;

	for (sym = syms; sym; sym = sym->next) {
		if (!strcmp(sym->name, name))
			break;
        }
	if (!sym)
		return 0;
	if (addr)
		*addr = sym->val;

	stype = eppic_gettype(v);
	eppic_duptype(stype, sym->type);
	eppic_pushref(stype, 1);
	eppic_setmemaddr(v, sym->val);
	eppic_do_deref(v, v);
	return 1;
}

static int apigetenum(char *name, ENUM_S *enums)
{
	struct def_enumconst *ec;
	struct def_type *type;

	for (type = types; type; type = type->next)
		if (type->ctype == V_ENUM && type->name &&
		    !strcmp(type->name, name))
			break;
	if (!type)
		return 0;

	for (ec = type->ec; ec; ec = ec->next)
		enums = eppic_add_enum(enums, ec->name, ec->value);

	return 1;
}

static DEF_S *apigetdefs(void)
{
	return 0;
}

static char* apifindsym(char *p)
{
	return NULL;
}

static apiops testops= {
	apigetmem,
	apiputmem,
	apimember,
	apigetctype,
	apigetval,
	apigetenum,
	apigetdefs,
        NULL,
        NULL,
        NULL,
        NULL,
        apifindsym
};

static TYPE_S *get_eppic_type(char *origspec)
{
	char spec[strlen(origspec) + 1];
	TYPE_S *type;
	unsigned ref;
	char *endp;

        strcpy(spec, origspec);

	ref = 0;
	endp = spec + strlen(spec);
	do {
		*endp-- = 0;
		while (*endp == '*') {
			++ref;
			*endp-- = 0;
		}
	} while (isspace(*endp));

        endp = spec;
	while (*endp && !isspace(*endp))
		++endp;
	while (isspace(*endp))
		*endp++ = 0;

	type = eppic_newtype();
	if (!type) {
		fprintf(stderr, "Cannot allocate type\n");
		return NULL;
	}

	if (*endp) {
		if (!strcmp(spec, "struct")) {
			if (lookup_ctype(V_STRUCT, endp, type, true))
				goto found;
		} else if (!strcmp(spec, "union")) {
			if (lookup_ctype(V_UNION, endp, type, true))
				goto found;
		} else if (!strcmp(spec, "enum")) {
			if (lookup_ctype(V_ENUM, endp, type, true))
				goto found;
		}
	} else {
		if (lookup_ctype(V_BASE, spec, type, true))
			goto found;
		if (lookup_ctype(V_REF, spec, type, true))
			goto found;
		if (lookup_ctype(V_TYPEDEF, spec, type, true))
			goto found;
	}

	ref = 0;
	if (eppic_parsetype(origspec, type, ref))
		goto found;

        eppic_freetype(type);
	return NULL;

found:
	if (ref)
		eppic_pushref(type, ref);
	return type;
}

/************************************************************
 * Tokenizer
 */

struct tok {
	const char *fname;
	FILE *f;
	char *line, *ptr;
	size_t alloc, len;
	char *endp;
	unsigned lineno;
};

static bool tok_init(struct tok *tok, const char *fname)
{
	tok->fname = fname;
	tok->f = fopen(fname, "rt");
	if (!tok->f) {
		fprintf(stderr, "Cannot open %s: %s\n",
			fname, strerror(errno));
		return false;
	}

	tok->line = tok->ptr = NULL;
	tok->alloc = tok->len = 0;
	tok->lineno = 0;
	return true;
}

static void tok_cleanup(struct tok *tok)
{
	free(tok->line);
	if (tok->f)
		fclose(tok->f);
}

static int tok_err(struct tok *tok, const char *fmt, ...)
{
        va_list ap;
	int ret;

	va_start(ap, fmt);
	fprintf(stderr, "%s:%u: ", tok->fname, tok->lineno);
	ret = vfprintf(stderr, fmt, ap);
	va_end(ap);
	return ret;
}

static void tok_skipws(struct tok *tok)
{
	while (tok->len && isspace(*tok->ptr)) {
		--tok->len;
		++tok->ptr;
	}
}

static void tok_trimws(struct tok *tok)
{
        char *endp;

	tok_skipws(tok);
	endp = tok->ptr + tok->len - 1;
        while (endp > tok->ptr && isspace(*endp)) {
		--tok->len;
		*endp-- = 0;
	}
}

static bool tok_fill(struct tok *tok)
{
	while (!tok->len) {
		ssize_t rd;

		++tok->lineno;
		rd = getline(&tok->line, &tok->alloc, tok->f);
		if (ferror(tok->f)) {
			tok_err(tok, "Read error: %s\n", strerror(errno));
			return false;
		} else if (rd <= 0)
			break;
		tok->len = rd;
		tok->ptr = tok->line;
		tok_trimws(tok);
	}
	return true;
}

static bool tok_fill_noeof(struct tok *tok)
{
	if (!tok_fill(tok))
		return false;
	if (!tok->len) {
		tok_err(tok, "Unexpected EOF\n");
		return false;
	}
	return true;
}

static char tok_nextchar(struct tok *tok)
{
	char c = *tok->ptr;

        --tok->len;
	++tok->ptr;
	tok_skipws(tok);
	return c;
}

static void tok_flush(struct tok *tok)
{
	tok->len = 0;
	tok->ptr = tok->line;
}

static char *tok_eol(struct tok *tok)
{
	tok->len = 0;
	return tok->ptr;
}

static char *tok_delim(struct tok *tok, char delim)
{
	char *endp;
        char *ret;

	ret = tok->ptr;
	while (tok->len && *tok->ptr != delim) {
		--tok->len;
		++tok->ptr;
	}
	if (!tok->len) {
		tok_err(tok, "Delimiter '%c' not found.\n", delim);
		return NULL;
	}
	endp = tok->ptr - 1;
        while (endp > ret && isspace(*endp))
		*endp-- = 0;
        --tok->len;
	*tok->ptr++ = 0;
	tok_skipws(tok);

        return ret;
}

/************************************************************
 * Config file parser
 */

typedef bool parse_key_fn(struct tok *tok, char *key);

static bool global_key(struct tok *tok, char *key)
{
	char *value, *endp;
	int *param;

	if (!strcmp(key, "abi"))
		param = &abi;
	else if (!strcmp(key, "nbpw"))
		param = &nbpw;
	else if (!strcmp(key, "sign"))
		param = &sign;
	else {
		tok_err(tok, "No such global key: %s\n", key);
		return false;
	}
	value = tok_eol(tok);
	*param = strtoul(value, &endp, 0);
	if (!*value || *endp) {
		tok_err(tok, "Invalid %s value: %s\n", key, value);
		return false;
	}
	return true;
}

static bool memory_key(struct tok *tok, char *key)
{
        char *value, *addrstr, *endp;
	unsigned long long addr;

	value = tok_eol(tok);
	if (!strcmp(key, "size")) {
		unsigned long size;

		size = strtoul(value, &endp, 0);
		if (!*value || *endp) {
			tok_err(tok, "Invalid memory size: %s\n", value);
			return false;
		}
		memory = realloc(memory, size);
		if (!memory) {
			perror("Cannot allocate memory");
			return false;
		}
		memory_size = size;
		return true;
	}

	addrstr = strchr(key, '@');
	if (addrstr) {
		endp = addrstr;
		do {
			*endp-- = 0;
		} while (endp >= addrstr && isspace(*endp));
		do {
			++addrstr;
		} while (isspace(*addrstr));
	} else {
		addrstr = key;
		key = "STRING";
	}
	addr = strtoull(addrstr, &endp, 0);
	if (!*addrstr || *endp) {
		tok_err(tok, "Invalid address: %s\n", addrstr);
		return false;
	}

	if (!strcmp(key, "STRING")) {
		memcpy(memory + addr, value, strlen(value));
		value = "";
	} else if (!strcmp(key, "CSTRING")) {
		memcpy(memory + addr, value, strlen(value) + 1);
		value = "";
        } if (!strcmp(key, "BYTE")) {
		while (*value) {
			unsigned long byte = strtoul(value, &endp, 0);

                        if (endp == value)
				break;
			memory[addr++] = byte;
			for (value = endp; isspace(*value); ++value);
		}
	} else if (!strcmp(key, "WORD")) {
		while (*value) {
			unsigned long word = strtoul(value, &endp, 0);

                        if (endp == value)
				break;
			*(uint16_t*)(memory + addr) = word;
			addr += 2;
			for (value = endp; isspace(*value); ++value);
		}
	} else if (!strcmp(key, "DWORD")) {
		while (*value) {
			unsigned long dword = strtoul(value, &endp, 0);

                        if (endp == value)
				break;
			*(uint32_t*)(memory + addr) = dword;
			addr += 4;
			for (value = endp; isspace(*value); ++value);
		}
	} else if (!strcmp(key, "QWORD")) {
		while (*value) {
			unsigned long long qword = strtoul(value, &endp, 0);

                        if (endp == value)
				break;
			*(uint64_t*)(memory + addr) = qword;
			addr += 8;
			for (value = endp; isspace(*value); ++value);
		}
	}
	if (*value) {
		tok_err(tok, "Invalid memory content: %s\n", value);
		return false;
	}

	return true;
}

static bool parse_base(struct tok *tok, struct def_type *type)
{
	char *sizestr = tok_eol(tok);
	char *endp;

	type->ctype = V_BASE;
	type->size = strtoul(sizestr, &endp, 0);
	if (!*sizestr || *endp) {
		tok_err(tok, "Invalid size: %s\n", sizestr);
		return false;
	}
	return true;
}

static bool parse_type(struct tok *tok, struct def_type *type, int ctype)
{
	char *base = tok_eol(tok);

	type->ctype = ctype;
 	type->ref = get_eppic_type(base);
	if (!type->ref) {
		tok_err(tok, "Unknown type: %s\n", base);
		return false;
	}
	return true;
}

static bool check_brace(struct tok *tok)
{
	char *p = tok_delim(tok, '{');
	if (!p)
		return false;
	if (*p) {
		tok_err(tok, "Garbage before '{': %s\n", p);
		return false;
	}
	return true;
}

static bool parse_enum(struct tok *tok, struct def_type *type)
{
	struct def_enumconst *ec;
	char *id, *val, *endp;
	unsigned long num;

	if (!check_brace(tok))
		return false;

	type->ctype = V_ENUM;
        type->size = nbpw;

	num = 0;
        while (tok_fill_noeof(tok)) {
		if (*tok->ptr == '}') {
			tok_nextchar(tok);
			return true;
		}

		id = tok_eol(tok);
		if (!*id)
			continue;
		endp = strchr(id, '=');

                if (endp) {
			val = endp;
			do {
				*endp-- = 0;
			} while (isspace(*endp));
			do {
				++val;
			} while (isspace(*val));
			num = strtoul(val, &endp, 0);
			if (!*val || *endp) {
				tok_err(tok, "Invalid number: %s\n", val);
				break;
			}
		}

		ec = malloc(sizeof *ec);
		if (!ec) {
			perror("Cannot allocate enumeration constant");
			break;
		}
		ec->name = strdup(id);
		if (!ec->name) {
			free(ec);
			perror("Cannot allocate enumeration constant");
			break;
		}
		ec->value = num++;
		ec->next = type->ec;
		type->ec = ec;
	}

	free(id);
        return false;
}

static bool parse_compound(struct tok *tok, struct def_type *type, int ctype)
{
	struct def_member *member;
	char *typestr, *id, *endp;
	unsigned off, bitnum;
	unsigned long sz;

	if (!check_brace(tok))
		return false;

	type->ctype = ctype;
	member = NULL;
	off = 0;
	bitnum = 0;
        while (tok_fill_noeof(tok)) {
		if (*tok->ptr == '}') {
			tok_nextchar(tok);
			return true;
		}

		member = calloc(1, sizeof *member);
		if (!member) {
			perror("Cannot allocate member data");
			break;
		}

		typestr = tok_delim(tok, ':');
		if (!typestr)
			break;
		member->type = get_eppic_type(typestr);
		if (!member->type) {
			tok_err(tok, "Unknown type: %s\n", typestr);
			break;
                }
		sz = eppic_type_getsize(member->type);
		if (ctype == V_UNION && type->size < sz)
			type->size = sz;

                id = tok_eol(tok);
		endp = strchr(id, ':');
		if (endp) {
			member->nbits = strtoul(id, &endp, 0);
			if (*id == ':' || *endp != ':') {
				tok_err(tok, "Invalid member id: %s\n", id);
				break;
			}
			id = endp + 1;
			while (isspace(*id))
				++id;

			if (ctype == V_STRUCT) {
				if (bitnum == 0)
					off = type->size;
				if (type->size < off + sz)
					type->size = off + sz;
				if (bitnum + member->nbits > 8 * sz) {
					off += (bitnum + 7) / 8;
					bitnum = 0;
				}
			}
			member->fbit = bitnum;
		} else {
			member->fbit = -1;
			if (ctype == V_STRUCT) {
				off = type->size;
				type->size += sz;
			}
			bitnum = 0;
		}
		member->offset = (ctype == V_STRUCT ? off : 0);

		member->name = strdup(id);
		if (!member->name) {
			perror("Cannot allocate member name");
			break;
		}

                member->next = type->members;
		type->members = member;
		bitnum += member->nbits;
		member = NULL;
	}

	if (member) {
		if (member->type)
			eppic_freetype(member->type);
		free(member->name);
		free(member);
	}
	return false;
}

static bool type_key(struct tok *tok, char *key)
{
	struct def_type *type;
	char *typestr;

	typestr = tok_delim(tok, ':');
	if (!typestr)
		return false;

	type = calloc(1, sizeof *type);
	if (!type) {
		perror("Type allocation failed");
		return false;
	}
	type->key = strdup(key);
        type->name = (*key == '#' ? NULL : type->key);

	if (!strcmp(typestr, "BASE")) {
		if (!parse_base(tok, type))
			goto err;
	} else if (!strcmp(typestr, "TYPEDEF")) {
		if (!parse_type(tok, type, V_TYPEDEF))
			goto err;
	} else if (!strcmp(typestr, "REF")) {
		if (!parse_type(tok, type, V_REF))
			goto err;
	} else if (!strcmp(typestr, "ENUM")) {
		if (!parse_enum(tok, type))
			goto err;
	} else if (!strcmp(typestr, "UNION")) {
		if (!parse_compound(tok, type, V_UNION))
			goto err;
	} else if (!strcmp(typestr, "STRUCT")) {
		if (!parse_compound(tok, type, V_STRUCT))
			goto err;
	} else {
		tok_err(tok, "Unhandled type %s\n", typestr);
		goto err;
	}

	type->next = types;
	types = type;
	return true;

err:
	if (type->ref)
		eppic_freetype(type->ref);
	if (type->key)
		free(type->key);
	free(type);
	return false;
}

static bool symbol_key(struct tok *tok, char *key)
{
        char *typestr, *val, *endp;
	unsigned long long symval;
	struct def_symbol *sym;
	TYPE_S *type;

	typestr = tok_delim(tok, ':');
	if (!typestr)
                return false;

	val = tok_eol(tok);
	symval = strtoull(val, &endp, 0);
	if (!*val || *endp) {
		tok_err(tok, "Invalid symbol value for %s: %s\n", key, val);
		return false;
	}

	type = get_eppic_type(typestr);
	if (!type) {
		tok_err(tok, "Unknown type: %s\n", typestr);
		return false;
	}

	sym = calloc(1, sizeof *sym);
	if (!sym) {
		perror("Symbol allocation failed");
		return false;
	}

	sym->next = syms;
	sym->name = strdup(key);
	sym->type = type;
	sym->val = symval;
        syms = sym;
	return true;
}

static parse_key_fn *sect_parser(const char *name)
{
	if (!strcmp(name, "memory"))
		return memory_key;
	else if (!strcmp(name, "type"))
		return type_key;
	else if (!strcmp(name, "symbol"))
		return symbol_key;
	return NULL;
}

static bool parse_config(struct tok *tok)
{
	parse_key_fn *parse_key = global_key;

	while (tok_fill(tok) && tok->len) {
		if (*tok->ptr == ';') {
			tok_flush(tok);
		} else if (*tok->ptr == '[') {
			char *sect;

			tok_nextchar(tok);
			sect = tok_delim(tok, ']');
                        if (!sect)
				return false;
                        if (tok->len) {
				fprintf(stderr,
					"%s:%u: Garbage at end of line: %s\n",
					tok->fname, tok->lineno, tok->ptr);
				return false;
			}

                        parse_key = sect_parser(sect);
			if (!parse_key) {
				tok_err(tok, "Unknown section name: %s\n", sect);
                                return false;
			}

			if (!ready) {
				eppic_apiset(&testops, abi, nbpw, sign);
				ready = true;
			}
		} else {
			char *key;

			key = tok_delim(tok, '=');
			if (!key)
				return false;

                        if (!parse_key(tok, key))
				return false;
			tok_flush(tok);
		}
	}

        return (tok->len == 0);
}

/************************************************************
 * Main program
 */

enum {
	OPT_CONFIG = 'c',
	OPT_HELP = 'h',
	OPT_USAGE = 256,
};

static struct option longopts[] = {
	{ "help", no_argument, 0, OPT_HELP },
	{ "usage", no_argument, 0, OPT_USAGE },
	{ "config",  required_argument, 0, OPT_CONFIG },
	{ 0, 0, 0, 0 }
};

static void usage(FILE *fout)
{
	fprintf(fout, "Usage: %s --config <cfgfile> program.c\n",
		PROGRAM_NAME);
}

int main(int argc, char **argv)
{
	const char *config_fname = NULL;
	char *eppic_fname;
	struct tok tok;
	bool success;
	int c;

	while ((c =getopt_long(argc, argv, "c:h",
			       longopts, NULL)) != -1) {
		switch(c) {
		case OPT_CONFIG:
			config_fname = optarg;
                        break;

		case OPT_HELP:
		case OPT_USAGE:
			usage(stdout);
			return 0;

		case '?':
		default:
			usage(stderr);
			return 1;
		}
        }

	if (!config_fname) {
		fputs("Mandatory option --config is missing!\n", stderr);
                return 1;
	}
	if (!argv[optind]) {
		fputs("EPPIC file name is missing!\n", stderr);
		return 1;
	}
	eppic_fname = argv[optind];

	eppic_open();
	eppic_setofile(stdout);

        if (!tok_init(&tok, config_fname))
		return 2;
	success = parse_config(&tok);
	tok_cleanup(&tok);
        if (!success)
		return 2;

        eppic_load(eppic_fname);
        return eppic_cmd("main", argv + optind + 1, argc - optind - 1);
}
