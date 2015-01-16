/*
 * Code specifically for handling multi-byte characters.
 *
 * Copyright (C) 2003, Nam SungHyun and various contributors
 */

#include <glib.h>

extern int utf_byte2len(int b);
extern int utf_ptr2char(const guchar *p);
extern int utf_iscomposing(int c);
extern int utf_char2cells(int c);
extern int utf_printable(int c);
extern int utf_ptr2len_check(const guchar *p);

