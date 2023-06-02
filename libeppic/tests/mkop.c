/*
 * Copyright (C) Petr Tesarik <petr@tesarici.cz>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>

/* Some useful macros... */
#define ARRAY_SIZE(arr)	(sizeof(arr)/sizeof((arr)[0]))
#define BITS_PER_BYTE	8

/* All section numbers refer to ISO/IEC 9899:201x. */

/* 6.5.3.3. Unary arithmetic operators. */
static const char *unary_ops[] = {
	"+",
	"-",
	"~",
};

/* Binary operators where usual arithmetic conversions are performed
 * on the operands, and the type of the result is that of the
 * conversion type.*/
static const char *arith_ops[] = {
	/* 6.5.5. Multiplicative operators */
	"*",
	"/",
	"%",

	/* 6.5.6. Additive operators */
	"+",
	"-",

	/* 6.5.10. Bitwise AND operator */
	"&",

	/* 6.5.11. Bitwise exclusive OR operator */
	"^",

	/* 6.5.12. Bitwise inclusive OR operator */
	"|",
};

/* Binary operators where integer promotions are performed on each
 * of the operands, and the type of the result is that of the promoted
 * left operand.
 */
static const char *shift_ops[] = {
	/* 6.5.7. Bitwise shift operators */
	"<<",
	">>",
};

/* Binary operators where usual arithmetic conversions are performed
 * on the operands, and the type of the result is int.
 */
static const char *rel_ops[] = {
	/* 6.5.8. Relational operators */
	"<",
	">",
	"<=",
	">=",

	/* 6.5.9. Equality operators */
	"==",
	"!=",
};

/* This enumeration is ordered by integer conversion rank.
 * Cf.  Section 6.3.
*/
enum type {
	TYPE_signed_char,
	TYPE_unsigned_char,
	TYPE_signed_short,
	TYPE_unsigned_short,
	TYPE_signed_int,
	TYPE_unsigned_int,
	TYPE_signed_long,
	TYPE_unsigned_long,
	TYPE_signed_long_long,
	TYPE_unsigned_long_long,
};

#define long_long	long long
#define STRINGIFY(tok)	#tok

#define TDEF(sign, btype, conv) \
	[TYPE_##sign##_##btype] = {		\
		sizeof(sign btype),		\
		#sign " " STRINGIFY(btype),	\
		conv,				\
		(~(sign)0 < 0)			\
	}

static const struct {
	size_t size;
	char *name;
	char conv[4];
	char sign;
} types[] = {
	TDEF(signed, char, "hh"),
	TDEF(unsigned, char, "hh"),
	TDEF(signed, short, "h"),
	TDEF(unsigned, short, "h"),
	TDEF(signed, int, ""),
	TDEF(unsigned, int, ""),
	TDEF(signed, long , "l"),
	TDEF(unsigned, long, "l"),
	TDEF(signed, long_long, "ll"),
	TDEF(unsigned, long_long, "ll"),
};

static int int_promotion(enum type type)
{
	/* If integer conversion rank is greater than the rank of int
	 * and unsigned int, the type is unchanged.
	 * Cf. Section 6.3.1.1.
	 */
	if (type >= TYPE_signed_int)
		return type;

	/* If an int can represent all values of the original type
	 * (as restricted by the width, for a bit-field), the value
	 * is converted to an int.
	 * Cf. Section 6.3.1.1.
	 */
	if (types[type].sign &&
	    types[type].size <= types[TYPE_signed_int].size)
		return TYPE_signed_int;
	if (!types[type].sign &&
	    types[type].size < types[TYPE_signed_int].size)
		return TYPE_signed_int;

	/* Otherwise, it is converted to an unsigned int. */
	return TYPE_unsigned_int;
}

static int arith_conversion(enum type left, enum type right)
{

	/* If both operands have the same type, then no conversion
	 * is needed.
	 */
	if (left == right)
		return left;

	/* Otherwise, if both operands have signed integer types or
	 * both have unsigned integer types, the operand with the
	 * type of lesser integer conversion rank is converted to
	 * the type of the operand with greater rank.
	 */
	if (types[left].sign == types[right].sign)
		return left > right ? left : right;

	/* Otherwise, if the operand that has unsigned integer type
	 * has rank greater or equal to the rank of the type of the
	 * other operand, then the operand with signed integer type
	 * is converted to the type of the operand with unsigned
	 * integer type.
	 */
	if (!types[left].sign && left >= right)
		return left;
	if (!types[right].sign && right >= left)
		return right;

	/* Otherwise, if the type of the operand with signed
	 * integer type can represent all of the values of the type
	 * of the operand with unsigned integer type, then the
	 * operand with unsigned integer type is converted to the
	 * type of the operand with signed integer type.
	 */
	if (types[left].sign && types[left].size > types[right].size)
		return left;
	if (types[right].sign && types[right].size > types[left].size)
		return right;

	/* Otherwise, both operands are converted to the unsigned
	 * integer type corresponding to the type of the operand
	 * with signed integer type.
	 */
	return types[left].sign
		? left + 1
		: right + 1;
}

static char decimal_conv(enum type type)
{
	return types[type].sign ? 'd' : 'u';
}

static void const_suffix(char sfx[4], enum type type)
{
	if (!types[type].sign)
		*sfx++ = 'U';
	if (type >= TYPE_signed_long)
		*sfx++ = 'L';
	if (type >= TYPE_signed_long_long)
		*sfx++ = 'L';
	*sfx = '\0';
}

static void unary_op(const char *op, enum type type, enum type result,
		     unsigned long long val)
{
	char sfx[4];

	const_suffix(sfx, type);
	printf("printf(\"%%s%%%s%c = %%%s%c\\n\", \"%s\", (%s)%llu%s, %s(%s)%llu%s);\n",
	       types[type].conv, decimal_conv(type),
	       types[result].conv, decimal_conv(result),
	       op, types[type].name, val, sfx,
	       op, types[type].name, val, sfx);
}

static unsigned long long maxval(enum type type)
{
	unsigned shift = BITS_PER_BYTE * (sizeof 0ULL - types[type].size);
	return ~0ULL >> (shift + types[type].sign);
}

static void arith_unop(const char *op, enum type type)
{
	unsigned result = int_promotion(type);

	unary_op(op, type, result, 1);
	unary_op(op, type, result, maxval(type));
}

static void binary_op(const char *op, enum type left, enum type right,
		      enum type result, unsigned long long val1,
		      unsigned long long val2)
{
	char leftsfx[4];
	char rightsfx[4];

	const_suffix(leftsfx, left);
	const_suffix(rightsfx, right);
	printf("printf(\"%%%s%c %%s %%%s%c = %%%s%c\\n\", (%s)%llu%s, \"%s\", (%s)%llu%s, (%s)%llu%s %s (%s)%llu%s);\n",
	       types[left].conv, decimal_conv(left),
	       types[right].conv, decimal_conv(right),
	       types[result].conv, decimal_conv(result),
	       types[left].name, val1, leftsfx, op,
	       types[right].name, val2, rightsfx,
	       types[left].name, val1, leftsfx, op,
	       types[right].name, val2, rightsfx);
}

static void arith_binop(const char *op, enum type left, enum type right)
{
	enum type result = arith_conversion(left, right);

	binary_op(op, left, right, result, maxval(left), 2);
	binary_op(op, left, right, result, 1, maxval(right));
}

static void shift_binop(const char *op, enum type left, enum type right)
{
	enum type result = int_promotion(left);
	unsigned bits = types[result].size * BITS_PER_BYTE;

	binary_op(op, left, right, result, 1, bits - 1);
	binary_op(op, left, right, result, maxval(left), bits - 1);
}

static void rel_binop(const char *op, enum type left, enum type right)
{
	enum type result = arith_conversion(left, right);

	binary_op(op, left, right, result, 0, 0);
	binary_op(op, left, right, result, 0, 1);
	binary_op(op, left, right, result, 0, maxval(right));
	binary_op(op, left, right, result, maxval(left), 0);
	binary_op(op, left, right, result, maxval(left), maxval(right));
}

int main()
{
	unsigned op;
	enum type left, right;

	puts("/* This file was generated by mkop");
	puts(" *");
	puts(" * Test operators.");
	puts(" *");
	puts(" * Also test:");
	puts(" * - large integer constants");
	puts(" * - constant suffixes");
	puts(" * - #ifndef / #endif");
	puts(" * - eppic pre-defined macro");
	puts(" */");

	puts("#ifndef eppic");
	puts("# include <stdio.h>");
	puts("#endif");

	puts("int main()");
	puts("{");

	for (left = 0; left < ARRAY_SIZE(types); ++left) {
		printf("printf(\"%%s:\\n\", \"%s\");\n", types[left].name);
		for (op = 0; op < ARRAY_SIZE(unary_ops); ++op)
			arith_unop(unary_ops[op], left);

		unary_op("!", left, TYPE_signed_int, 0);
		unary_op("!", left, TYPE_signed_int, maxval(left));

		for (right = 0; right < ARRAY_SIZE(types); ++right) {
			printf("printf(\"%%s, %%s:\\n\", \"%s\", \"%s\");\n",
			       types[left].name, types[right].name);

			for (op = 0; op < ARRAY_SIZE(arith_ops); ++op)
				arith_binop(arith_ops[op], left, right);

			for (op = 0; op < ARRAY_SIZE(shift_ops); ++op)
				shift_binop(shift_ops[op], left, right);

			for (op = 0; op < ARRAY_SIZE(rel_ops); ++op)
				rel_binop(rel_ops[op], left, right);
		}
	}

	puts("return 0;");
	puts("}");

	return 0;
}
