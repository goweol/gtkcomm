/* vim:tabstop=8:softtabstop=4:shiftwidth=4:noexpandtab:
 *
 * Copyright (C) 2002-2015, SungHyun Nam and various contributors
 */
#include "config.h"
#include <sys/time.h>

#include "pcMain.h"
#include "pcTerm.h"

static GIConv ConvEUC2UTF = (GIConv) -1;
static GIConv ConvUTF2EUC = (GIConv) -1;

/* gdk_time_get() {{{1 */
guint32
gdk_time_get(void)
{
    struct timeval end;
    struct timeval elapsed;
    guint32 milliseconds;

    static int f = 1;
    static struct timeval start;
    if (f)
    {
	f=0;
	gettimeofday (&start, NULL);
    }

    gettimeofday (&end, NULL);

    if (start.tv_usec > end.tv_usec)
    {
	end.tv_usec += 1000000;
	end.tv_sec--;
    }
    elapsed.tv_sec = end.tv_sec - start.tv_sec;
    elapsed.tv_usec = end.tv_usec - start.tv_usec;

    milliseconds = (elapsed.tv_sec * 1000) + (elapsed.tv_usec / 1000);

    return milliseconds;
}

/* PC_IconvOpen() {{{1 */
void
PC_IconvOpen(void)
{
    ConvEUC2UTF = g_iconv_open("UTF-8", "EUC-KR");
    if (ConvEUC2UTF == (GIConv) -1)
	g_error("ConvEUC2UTF: g_iconv_open() failed");

    ConvUTF2EUC = g_iconv_open("EUC-KR", "UTF-8");
    if (ConvUTF2EUC == (GIConv) -1)
	g_error("ConvUTF2EUC: g_iconv_open() failed");
}

/* PC_IconvClose() {{{1 */
void
PC_IconvClose(void)
{
    if (ConvEUC2UTF != (GIConv) -1)
    {
	g_iconv_close(ConvEUC2UTF);
	ConvEUC2UTF = (GIConv) -1;
    }
    if (ConvUTF2EUC != (GIConv) -1)
    {
	g_iconv_close(ConvUTF2EUC);
	ConvUTF2EUC = (GIConv) -1;
    }
}

/* PC_IconvStr() {{{1 */
char *
PC_IconvStr(const char *from, int fromlen)
{
    gchar *utf;

    g_return_val_if_fail(from && fromlen, (char *) from);

    if (fromlen < 0)
	fromlen = strlen(from);

    /* It seems there is no perfect solution, but using the
     * g_utf8_validate() seems reasonable.
     */
    if (g_utf8_validate(from, fromlen, NULL) == FALSE)
	if (gu_euckr_validate_conv(from, fromlen, &utf, NULL) == TRUE)
	    return utf;

    return (char *) from;
}

/* gu_utf8_validate_conv() {{{1 */
gboolean
gu_utf8_validate_conv(const gchar *str, gint len, gchar **euc, gsize *euc_len)
{
    gchar *p;
    gsize br, bw;

    g_return_val_if_fail(str && len, FALSE);

    if (len < 0)
	len = strlen(str);

#if 0
    p = g_convert_with_iconv(str, len, ConvUTF2EUC, &br, &bw, NULL);
    if (p != NULL)
    {
	if (len == (gint) br)
	{
	    gchar *p2;
	    gsize bw2;

	    p2 = g_convert_with_iconv(p, bw, ConvEUC2UTF, NULL, &bw2, NULL);
	    if (p2 != NULL)
	    {
		if (bw2 == (gsize) len && !memcmp(str, p2, len))
		{
		    if (euc)
		    {
			*euc = p;
			if (euc_len)
			    *euc_len = bw;
		    }
		    else
			g_free(p);
		    g_free(p2);
		    return TRUE;
		}
		g_free(p2);
	    }
	}
	g_free(p);
    }
#else
    p = g_convert_with_iconv(str, len, ConvUTF2EUC, &br, &bw, NULL);
    if (p != NULL)
    {
	if (len == (gint) br)
	{
	    if (euc)
	    {
		*euc = p;
		if (euc_len)
		    *euc_len = bw;
	    }
	    else
		g_free(p);

	    return TRUE;
	}
	g_free(p);
    }
#endif

    return FALSE;
}

/* gu_euckr_validate_conv() {{{1 */
gboolean
gu_euckr_validate_conv(const gchar *str, gint len, gchar **utf, gsize *utf_len)
{
    gchar *p;
    gsize br, bw;

    g_return_val_if_fail(str && len, FALSE);

    if (len < 0)
	len = strlen(str);

#if 0
    p = g_convert_with_iconv(str, len, ConvEUC2UTF, &br, &bw, NULL);
    if (p != NULL)
    {
	if (len == (gint) br)
	{
	    gchar *p2;
	    gsize bw2;

	    p2 = g_convert_with_iconv(p, bw, ConvUTF2EUC, NULL, &bw2, NULL);
	    if (p2 != NULL)
	    {
		if (bw2 == (gsize) len && !memcmp(str, p2, len))
		{
		    if (utf)
		    {
			*utf = p;
			if (utf_len)
			    *utf_len = bw;
		    }
		    else
			g_free(p);
		    g_free(p2);
		    return TRUE;
		}
		g_free(p2);
	    }
	}
	g_free(p);
    }
#else
    p = g_convert_with_iconv(str, len, ConvEUC2UTF, &br, &bw, NULL);
    if (p != NULL)
    {
	if (len == (gint) br)
	{
	    if (utf)
	    {
		*utf = p;
		if (utf_len)
		    *utf_len = bw;
	    }
	    else
		g_free(p);

	    return TRUE;
	}
	g_free(p);
    }
#endif

    return FALSE;
}

/* marshal_VOID__OBJECT_OBJECT() {{{1 */
/* VOID:OBJECT,OBJECT (gtkmarshalers.list:69) */
void
marshal_VOID__OBJECT_OBJECT (GClosure     *closure,
			     GValue       *return_value,
			     guint         n_param_values,
			     const GValue *param_values,
			     gpointer      invocation_hint,
			     gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__OBJECT_OBJECT) (gpointer     data1,
                                                    gpointer     arg_1,
                                                    gpointer     arg_2,
                                                    gpointer     data2);
  register GMarshalFunc_VOID__OBJECT_OBJECT callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;

  UNUSED(return_value);
  UNUSED(invocation_hint);

  g_return_if_fail (n_param_values == 3);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__OBJECT_OBJECT) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_value_get_object (param_values + 1),
            g_value_get_object (param_values + 2),
            data2);
}

