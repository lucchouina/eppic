/* tst-struct-eppic.c
 *
 * Test struct defined in EPPIC.
 */

/* This must match test_struct from testenv.conf */
struct eppic_struct {
	unsigned long long qw;
	unsigned int dw;
	unsigned short w;
	unsigned char b;
};

int main()
{
	struct eppic_struct *p = (struct eppic_struct*)&test_struct;

        printf("qw = 0x%llx\n", p->qw);
	printf("dw = 0x%x\n", p->dw);
	printf("w = 0x%x\n", p->w);
	printf("b = 0x%x\n", p->b);
	return 0;
}
