#include <stdio.h>
#include <string.h>
#include <stdlib.h>

char *uri = "/file/foo";
char *uri2 = "/file/bar/";
char *uri3 = "file/barz";
char *slash = "/";

int
main()
{
	char *str, *tok, *delim, *saveptr;

	delim = slash;
	// iterate all tokens
	//
	str = strdup(uri2);
	printf("str: \"%s\"\n", str);
	tok = strtok_r(str, delim, &saveptr);
	printf("tok: \"%s\"\n", tok);
	printf("ptr: \"%s\"\n", saveptr);
	//
	str = NULL;
	tok = strtok_r(str, delim, &saveptr);
	printf("tok: \"%s\"\n", tok);
	printf("ptr: \"%s\"\n", saveptr);

	char *src = "/file/TODO.txt";
	char *dst = "/file";
	int res = strncmp(src, dst, strlen(dst));

	printf("res=%d\n", res);

	free(str);
	return 0;
}
