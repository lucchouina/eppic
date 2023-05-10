/* tst-union.c
 *
 * Test union members.
 */

int main()
{
	union test_union *p = &test_union;

        printf("qw = 0x%llx\n", p->qw);
	printf("ptr = %p\n", p->ptr);
	return 0;
}
