/* tst-dynarray.c
 *
 * Test dynamic arrays.
 */

int main()
{
        /* TODO: What should be the type of a dynamic array?
	 * Only base types seem to work, but then any other type can be
	 * used as index or value...
	 */
	int arr;
	int revarr;
	int i;

	arr[0] = "eenie";
	arr[1] = "meenie";
	arr[2] = "miney";
	arr[3] = "mo";
	arr[10] = "catch";
	arr[11] = "a tiger";
	arr[12] = "by the";
	arr[13] = "toe";

	for (i = 0; i <= 13; i++)
		if (i in arr)
			printf("%s%c", arr[i], i < 13 ? ' ' : '\n');
		else
			printf(". ");

	for (i in arr) {
		string s = arr[i];
		printf("[%d] = %s\n", i, s);
		revarr[s] = i;
	}

	/* TODO: We can iterate using an int variable,
	 * but is it intended to work?
	 */
        for (i in revarr)
		printf("[%s] = %d\n", i, revarr[i]);

	return 0;
}
