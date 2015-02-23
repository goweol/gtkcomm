/*
 * Code specifically for handling multi-byte characters.
 *
 * stolen from mbyte.c in VIM.
 */
#include "config.h"
#include <ctype.h>

#include "mbyte.h"

struct interval {
    unsigned short first;
    unsigned short last;
};

/* Lookup table to quickly get the length in bytes of a UTF-8 character from
 * the first byte of a UTF-8 string.  Bytes which are illegal when used as the
 * first byte have a one, because these will be used separately. */
static char utf8len_tab[256] = {
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /*bogus*/
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /*bogus*/
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,6,6,1,1,
};

static int intable(struct interval *table, int size, int c);

/*
 * Return length of UTF-8 character, obtained from the first byte.
 * "b" must be between 0 and 255!
 */
int
utf_byte2len(int b)
{
    return utf8len_tab[b];
}

/*
 * Convert a UTF-8 byte sequence to a wide character.
 * If the sequence is illegal or truncated by a NUL the first byte is
 * returned.
 * Does not include composing characters, of course.
 */
int
utf_ptr2char(const guchar *p)
{
    int		len;

    if (p[0] < 0x80)	/* be quick for ASCII */
	return p[0];

    len = utf8len_tab[p[0]];
    if ((p[1] & 0xc0) == 0x80)
    {
	if (len == 2)
	    return ((p[0] & 0x1f) << 6) + (p[1] & 0x3f);
	if ((p[2] & 0xc0) == 0x80)
	{
	    if (len == 3)
		return ((p[0] & 0x0f) << 12) + ((p[1] & 0x3f) << 6)
		    + (p[2] & 0x3f);
	    if ((p[3] & 0xc0) == 0x80)
	    {
		if (len == 4)
		    return ((p[0] & 0x07) << 18) + ((p[1] & 0x3f) << 12)
			+ ((p[2] & 0x3f) << 6) + (p[3] & 0x3f);
		if ((p[4] & 0xc0) == 0x80)
		{
		    if (len == 5)
			return ((p[0] & 0x03) << 24) + ((p[1] & 0x3f) << 18)
			    + ((p[2] & 0x3f) << 12) + ((p[3] & 0x3f) << 6)
			    + (p[4] & 0x3f);
		    if ((p[5] & 0xc0) == 0x80 && len == 6)
			return ((p[0] & 0x01) << 30) + ((p[1] & 0x3f) << 24)
			    + ((p[2] & 0x3f) << 18) + ((p[3] & 0x3f) << 12)
			    + ((p[4] & 0x3f) << 6) + (p[5] & 0x3f);
		}
	    }
	}
    }
    /* Illegal value, just return the first byte */
    return p[0];
}

/*
 * Return TRUE if "c" is a composing UTF-8 character.  This means it will be
 * drawn on top of the preceding character.
 * Based on code from Markus Kuhn.
 */
int
utf_iscomposing(int c)
{
    /* sorted list of non-overlapping intervals */
    static struct interval combining[] =
    {
	{0x0300, 0x034F}, {0x0360, 0x036F}, {0x0483, 0x0486}, {0x0488, 0x0489},
	{0x0591, 0x05A1}, {0x05A3, 0x05B9}, {0x05BB, 0x05BD}, {0x05BF, 0x05BF},
	{0x05C1, 0x05C2}, {0x05C4, 0x05C4}, {0x064B, 0x0655}, {0x0670, 0x0670},
	{0x06D6, 0x06DC}, {0x06DE, 0x06E4}, {0x06E7, 0x06E8}, {0x06EA, 0x06ED},
	{0x0711, 0x0711}, {0x0730, 0x074A}, {0x07A6, 0x07B0}, {0x0901, 0x0902},
	{0x093C, 0x093C}, {0x0941, 0x0948}, {0x094D, 0x094D}, {0x0951, 0x0954},
	{0x0962, 0x0963}, {0x0981, 0x0981}, {0x09BC, 0x09BC}, {0x09C1, 0x09C4},
	{0x09CD, 0x09CD}, {0x09E2, 0x09E3}, {0x0A02, 0x0A02}, {0x0A3C, 0x0A3C},
	{0x0A41, 0x0A42}, {0x0A47, 0x0A48}, {0x0A4B, 0x0A4D}, {0x0A70, 0x0A71},
	{0x0A81, 0x0A82}, {0x0ABC, 0x0ABC}, {0x0AC1, 0x0AC5}, {0x0AC7, 0x0AC8},
	{0x0ACD, 0x0ACD}, {0x0B01, 0x0B01}, {0x0B3C, 0x0B3C}, {0x0B3F, 0x0B3F},
	{0x0B41, 0x0B43}, {0x0B4D, 0x0B4D}, {0x0B56, 0x0B56}, {0x0B82, 0x0B82},
	{0x0BC0, 0x0BC0}, {0x0BCD, 0x0BCD}, {0x0C3E, 0x0C40}, {0x0C46, 0x0C48},
	{0x0C4A, 0x0C4D}, {0x0C55, 0x0C56}, {0x0CBF, 0x0CBF}, {0x0CC6, 0x0CC6},
	{0x0CCC, 0x0CCD}, {0x0D41, 0x0D43}, {0x0D4D, 0x0D4D}, {0x0DCA, 0x0DCA},
	{0x0DD2, 0x0DD4}, {0x0DD6, 0x0DD6}, {0x0E31, 0x0E31}, {0x0E34, 0x0E3A},
	{0x0E47, 0x0E4E}, {0x0EB1, 0x0EB1}, {0x0EB4, 0x0EB9}, {0x0EBB, 0x0EBC},
	{0x0EC8, 0x0ECD}, {0x0F18, 0x0F19}, {0x0F35, 0x0F35}, {0x0F37, 0x0F37},
	{0x0F39, 0x0F39}, {0x0F71, 0x0F7E}, {0x0F80, 0x0F84}, {0x0F86, 0x0F87},
	{0x0F90, 0x0F97}, {0x0F99, 0x0FBC}, {0x0FC6, 0x0FC6}, {0x102D, 0x1030},
	{0x1032, 0x1032}, {0x1036, 0x1037}, {0x1039, 0x1039}, {0x1058, 0x1059},
	{0x1160, 0x11FF},
	{0x1712, 0x1714}, {0x1732, 0x1734}, {0x1752, 0x1753}, {0x1772, 0x1773},
	{0x17B7, 0x17BD}, {0x17C6, 0x17C6}, {0x17C9, 0x17D3}, {0x180B, 0x180D},
	{0x18A9, 0x18A9}, {0x20D0, 0x20EA}, {0x302A, 0x302F}, {0x3099, 0x309A},
	{0xFB1E, 0xFB1E}, {0xFE00, 0xFE0F}, {0xFE20, 0xFE23}
    };

    return intable(combining, sizeof(combining) / sizeof(struct interval), c);
}

/*
 * For UTF-8 character "c" return 2 for a double-width character, 1 for others.
 * Returns 4 or 6 for an unprintable character.
 * Is only correct for characters >= 0x80.
 */
int
utf_char2cells(int c)
{
    if (c >= 0x100)
    {
	if (!utf_printable(c))
	    return 6;		/* unprintable, displays <xxxx> */
	if (c >= 0x1100
	    && (c <= 0x115f			/* Hangul Jamo */
		|| c == 0x2329
		|| c == 0x232a
		|| (c >= 0x2e80 && c <= 0xa4cf
		    && c != 0x303f)		/* CJK ... Yi */
		|| (c >= 0xac00 && c <= 0xd7a3)	/* Hangul Syllables */
		|| (c >= 0xf900 && c <= 0xfaff)	/* CJK Compatibility
						   Ideographs */
		|| (c >= 0xfe30 && c <= 0xfe6f)	/* CJK Compatibility Forms */
		|| (c >= 0xff00 && c <= 0xff60)	/* Fullwidth Forms */
		|| (c >= 0xffe0 && c <= 0xffe6)
		|| (c >= 0x20000 && c <= 0x2ffff)))
	    return 2;
    }

    /* Characters below 0x100 are influenced by 'isprint' option */
    else if (c >= 0x80)
    {
	if (c >= 0x100 && !utf_printable(c))
	    return 4;		/* unprintable, displays <xx> */
	else if (c >= 0x100 || (c > 0 && !isprint(c)))
	    return 4;		/* unprintable, displays <xx> */
    }

    return 1;
}

/*
 * Return TRUE if "c" is in "table[size]".
 */
static int
intable(struct interval *table, int size, int c)
{
    int mid, bot, top;

    /* first quick check for Latin1 etc. characters */
    if (c < table[0].first)
	return FALSE;

    /* binary search in table */
    bot = 0;
    top = size - 1;
    while (top >= bot)
    {
	mid = (bot + top) / 2;
	if (table[mid].last < c)
	    bot = mid + 1;
	else if (table[mid].first > c)
	    top = mid - 1;
	else
	    return TRUE;
    }
    return FALSE;
}

/*
 * Return TRUE for characters that can be displayed in a normal way.
 * Only for characters of 0x100 and above!
 */
int
utf_printable(int c)
{
    /* sorted list of non-overlapping intervals */
    static struct interval nonprint[] =
    {
	{0x070f, 0x070f}, {0x180b, 0x180e}, {0x200b, 0x200f}, {0x202a, 0x202e},
	{0x206a, 0x206f}, {0xfeff, 0xfeff}, {0xfff9, 0xfffb}
    };

    return !intable(nonprint, sizeof(nonprint) / sizeof(struct interval), c);
}

/*
 * Get the length of a UTF-8 byte sequence, not including any following
 * composing characters.
 * Returns 0 for "".
 * Returns 1 for an illegal byte sequence.
 */
int
utf_ptr2len_check(const guchar *p)
{
    int		len;
    int		i;

    if (*p == '\0')
	return 0;
    len = utf8len_tab[*p];
    for (i = 1; i < len; ++i)
	if ((p[i] & 0xc0) != 0x80)
	    return 1;
    return len;
}

