/* tst-hello.c
 *
 * A simple hello-world program. However, it also exercises these APIs:
 * - memory read access
 * - symbol resolution (hello_world)
 * - type declaration (#charp)
 * - printf() built-in function
 */

int main()
{
	printf("%s\n", getstr(hello_world));
        return 0;
}
