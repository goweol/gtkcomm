/* vim:tabstop=8:softtabstop=4:shiftwidth=4:noexpandtab:
 *
 * gtk 1.x와 2.x 사이의 다른 점
 *
 * Copyright (C) 2002-2003, Nam SungHyun and various contributors
 */
#ifndef __PCCOMPAT_H
#define __PCCOMPAT_H 1

#include <gdk/gdkx.h> /* gdk_display */

extern guint32 gdk_time_get(void);

extern char *PC_IconvStr(const char *from, int fromlen);
extern void PC_IconvOpen(void);
extern void PC_IconvClose(void);
#define PC_IconvStrFree(a,b) G_STMT_START{ if (a!=b) g_free(b); }G_STMT_END

extern gboolean gu_utf8_validate_conv(const gchar *str, gint len,
				      gchar **euc, gsize *euc_len);
extern gboolean gu_euckr_validate_conv(const gchar *str, gint len,
				       gchar **utf, gsize *utf_len);

extern void marshal_VOID__OBJECT_OBJECT (GClosure     *closure,
					 GValue       *return_value,
					 guint         n_param_values,
					 const GValue *param_values,
					 gpointer      invocation_hint,
					 gpointer      marshal_data);

#endif /* __PCCOMPAT_H */
