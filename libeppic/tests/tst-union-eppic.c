/* tst-union-eppic.c
 *
 * Test union defined in EPPIC.
 */

/* This must match test_union from testenv.conf */
union eppic_union {
	unsigned long long qw;
	void *ptr;
};

int main()
{
	union eppic_union *p = (union eppic_union*)&test_union;

        printf("qw = 0x%llx\n", p->qw);
	printf("ptr = %p\n", p->ptr);
	return 0;
}
