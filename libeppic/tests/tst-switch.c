/* tst-switch.c
 *
 * Test switch statement.
 */

int main()
{
	int i;
	for (i = 0; i <= 5; ++i) {
		switch (i) {
		case 1:
			printf("one\n");
			break;
		case 2:
			printf("two\n");
			/* fallthrough */
		case 3:
		case 4:
			printf("many\n");
			break;

		case 0:
		default:
			printf("cannot handle %d\n", i);
			break;
		}
	}
	return 0;
}
