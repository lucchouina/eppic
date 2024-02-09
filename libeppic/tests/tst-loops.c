/* tst-loops.c
 *
 * Test various loop constructs.
 */

int main()
{
	int i;

	printf("while loop:\n");
	i = 1;
	while (i < 10) {
		printf("%d\n", i);
		i <<= 1;
	}

	printf("do-while loop:\n");
	do {
		printf("%d\n", i);
		i >>= 1;
        } while (i);

	printf("for loop + continue:\n");
	for (i = 1; i < 10; ++i) {
		if (i % 3 == 0)
			continue;
		printf("%d\n", i);
	}

	printf("break:\n");
	while (1) {
		if (i % 3 == 0)
			break;
		printf("%d\n", i);
		--i;
        }

	return 0;
}
