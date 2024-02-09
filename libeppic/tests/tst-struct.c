/* tst-struct.c
 *
 * Test struct members.
 *
 * Also test name spaces, because the struct tag is identical to the variable
 * name.
 */

int main()
{
	struct test_struct *p = &test_struct;

        printf("qw = 0x%llx\n", p->qw);
	printf("dw = 0x%x\n", p->dw);
	printf("w = 0x%x\n", p->w);
	printf("b = 0x%x\n", p->b);
	return 0;
}
