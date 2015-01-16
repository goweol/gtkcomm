/** =========================================================================
   gtkpcomm 관련 test routines
========================================================================== */
/* variable function test */
#include <stdio.h>
#include <gtk/gtk.h>

#define _FindSubString_TEST_	 0
#define _ScriptStringParse_TEST_ 0
#define _GetRealFilename_TEST_	 1

#ifndef FALSE
#define FALSE 0
#define TRUE (!FALSE)
#endif

#if _FindSubString_TEST_

static int NextPos = 0;

static int FindSubString(const char *buf, int len, const char *patt, int patLen)
{
    register int i, j;

    for (i = 0; i < len; i++)
    {
	for (j = 0; j < patLen; j++)
	{
	    if (buf[i + j] != patt[j])
		break;
	    if (i + j >= len - 1)
	    {
		NextPos = j + 1;
		return 1;
	    }
	}
	if (j == patLen)
	    return 0;
    }
    return -1;
}

int main(void)
{
    /* 0123456789 */
    static const char *s = "abcdefghijklmnlmnodkdkdkdd";
    int r;

    printf("r = %d\n", r = FindSubString(s, 10, "fgh", 3));
    printf("r = %d\n", r = FindSubString(s, 10, "dkdk", 3));
    printf("r = %d\n", r = FindSubString(s, 10, "ijklm", 5));
    if (r == 1)
    {
	printf("NextPos = %d\n", NextPos);
	printf("r = %d\n", r = FindSubString(s + 10, 10, "ijklm" + NextPos,
					     strlen("ijklm" + NextPos)));
    }
    printf("r = %d\n", r = FindSubString(s, 10, "ijklmlmn", 8));
    if (r == 1)
    {
	printf("NextPos = %d\n", NextPos);
	printf("r = %d\n", r = FindSubString(s + 10, 4, "ijklmlmn" + NextPos,
					     strlen("ijklmlmn" + NextPos)));
    }
/* result
	    r = 0
	    r = -1
	    r = 1
	    NextPos = 2
	    r = 0
	    r = 1
	    NextPos = 2
	    r = -1
*/
    return 0;
}

#endif /* _FindSubString_TEST_ */

#if _ScriptStringParse_TEST_
int main(void)
{
    static char buf[] = "waitfor \"dir\"; pause 3; transmit \"\n\"";
    int lastFlag;
    char *beginSentence, *s;

    s = buf;
    for (lastFlag = FALSE; !lastFlag;)
    {
	beginSentence = s;	/* 문장 처음 저장 */

	/* 문장끝을 찾는다. */
	for (; *s; s++)
	{
	    /* double quote 안에 있을 지도 모르는 문장끝을 의미하는 ';'를
	     * skip한다.
	     */
	    if (*s == '"')
	    {
		do
		{
		    if (*++s == '"')
		    {
			++s;
			break;
		    }
		}
		while (*s);
	    }
	    if (*s == ';')
		break;
	}

	if (*s == ';')
	    *s++ = '\0';
	else
	    lastFlag = TRUE;

	printf("    bS = %s\n", beginSentence);
    }
}
#endif /* _ScriptStringParse_TEST_ */

#if _GetRealFilename_TEST_
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
char *GetRealFilename(const char *filename, const char *path)
{
    char *s;
    struct stat st;

    if (filename && *filename)
    {
	if (!stat(filename, &st) && S_ISREG(st.st_mode)
	    && !access(filename, R_OK))
	{
	    return (char *) g_strdup(filename);
	}
	else if (*filename != '/' && path && *path)
	{
	    s = g_strconcat(path, "/", filename, NULL);
	    if (!stat(s, &st) && S_ISREG(st.st_mode) && !access(s, R_OK))
		return (char *) s;
	    g_free(s);
	}
    }
    return NULL;
}
int main(int argc, char *argv[])
{
    if (argc == 2)
    {
	char *s = GetRealFilename(argv[1], NULL);
	g_print("s = %s\n", s);
	g_free(s);
    }
    else if (argc == 3)
    {
	char *s = GetRealFilename(argv[1], argv[2]);
	g_print("s = %s\n", s);
	g_free(s);
    }
    return 0;
}
#endif /* _GetRealFilename_TEST_ */
