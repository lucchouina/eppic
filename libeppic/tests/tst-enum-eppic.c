/* tst-enum-eppic.c
 *
 * Test an enumeration type defined by EPPIC program.
 */

enum {
	ENUM_EPPIC_0,
	ENUM_EPPIC_1,
	ENUM_EPPIC_REP_0 = 0,
	ENUM_EPPIC_REP_1,
	ENUM_EPPIC_2,
};

int main()
{
	enum test_enum x;

	printf("ENUM_EPPIC_0 = %d\n", ENUM_EPPIC_0);
	printf("ENUM_EPPIC_1 = %d\n", ENUM_EPPIC_1);
	printf("ENUM_EPPIC_REP_0 = %d\n", ENUM_EPPIC_REP_0);
	printf("ENUM_EPPIC_REP_1 = %d\n", ENUM_EPPIC_REP_1);
	printf("ENUM_EPPIC_2 = %d\n", ENUM_EPPIC_2);
	return 0;
}
