/* tst-exists.c
 *
 * Test the exists() built-in function.
 */

void test_exists(string name)
{
	printf("exists(\"%s\") = %d\n", name, exists(name));
}

char *p = hello_world;
char *q;
struct parial *s;

int main()
{
	/* image symbol */
	test_exists("hello_world");

	/* initialized eppic global variable */
        test_exists("p");

	/* uninitialized eppic global variable */
        test_exists("q");

	/* pointer to an incomplete type */
        test_exists("s");

        /* eppic function */
	test_exists("main");

	/* non-existent identifier */
        test_exists("nonexistent");

	return 0;
}
