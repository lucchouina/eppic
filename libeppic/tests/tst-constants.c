/* tst-constants.c
 *
 * Test integer, character and string constants.
 */

int main()
{
	printf("012: 0%o\n", 012);
	printf("12: %d\n", 12);
	printf("12u: %u\n", 12u);
	printf("12U: %u\n", 12U);
	printf("12l: %ld\n", 12l);
	printf("12L: %ld\n", 12l);
	printf("12ul: %lu\n", 12ul);
	printf("12uL: %lu\n", 12uL);
	printf("12Ul: %lu\n", 12Ul);
	printf("12UL: %lu\n", 12UL);
	printf("12lu: %lu\n", 12lu);
	printf("12lU: %lu\n", 12lU);
	printf("12Lu: %lu\n", 12Lu);
	printf("12lU: %lu\n", 12lU);
	printf("12ull: %llu\n", 12ull);
	printf("12ULL: %llu\n", 12ULL);
	printf("12llu: %llu\n", 12llu);
	printf("12Llu: %llu\n", 12Llu);
	printf("12LLU: %llu\n", 12LLU);
	printf("-12: %d\n", -12);
	printf("0x12: 0x%x\n", 0x12);
	printf("0X12: 0X%x\n", 0X12);
	printf("0xabc: 0x%x\n", 0xabc);
	printf("0XABC: 0X%X\n", 0XABC);
	printf("'x': '%c'\n", 'x');
	printf("\"abc\": \"%s\"\n", "abc");
	return 0;
}
