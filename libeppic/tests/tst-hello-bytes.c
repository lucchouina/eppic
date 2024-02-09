/* tst-hello-bytes.c
 *
 * Same as tst-hello.c, but using a base type defined in the image.
 */

int main()
{
	printf("%s\n", getstr(hello_bytes));
        return 0;
}
