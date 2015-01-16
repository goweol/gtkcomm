/*
 * gtk utils
 *
 * Copyright (C) 2003-2004, Nam SungHyun and various contributors
 */
#include "config.h"
#include "gtkutil.h"
#include "mbyte.h"
#include "pcSetting.h"

/* gu_draw_new() {{{1 */
GUDraw *
gu_draw_new(GtkWidget *w, const char *fontname)
{
    GUDraw *d;
    PangoFontMetrics *metrics;
    PangoLayout *layout;
    int width, height, ascent, descent;

    d = g_new0(GUDraw, 1);

    d->attrList = pango_attr_list_new();
    d->fontDesc = pango_font_description_from_string(fontname);
    if (d->fontDesc == NULL)
    {
	if (g_ascii_strcasecmp(fontname, DEFAULT_FONT) != 0)
	{
	    g_warning("Cannot get pango font description from '%s'\n"
		      "Try to use '%s'\n", fontname, DEFAULT_FONT);
	    d->fontDesc = pango_font_description_from_string(DEFAULT_FONT);
	}
	if (d->fontDesc == NULL)
	    g_error("Cannot get pango font description from '%s'\n",
		    DEFAULT_FONT);
    }

    /* pango_context_load_font() bails out if no font size is set */
    if (pango_font_description_get_size(d->fontDesc) <= 0)
	pango_font_description_set_size(d->fontDesc, 10 * PANGO_SCALE);

    d->context = gtk_widget_create_pango_context(GTK_WIDGET(w));
    pango_context_set_base_dir(d->context, PANGO_DIRECTION_LTR);
    pango_context_set_font_description(d->context, d->fontDesc);

    layout = pango_layout_new(d->context);
    pango_layout_set_font_description(layout, d->fontDesc);

    pango_layout_set_text(layout, "MW", 2);
    pango_layout_get_size(layout, &width, &height);

    /*
     * Set char_width to half the width obtained from pango_layout_get_size()
     * for CJK fixed_width/bi-width fonts.  An unpatched version of Xft leads
     * Pango to use the same width for both non-CJK characters (e.g. Latin
     * letters and numbers) and CJK characters.  This results in 's p a c e d
     * o u t' rendering when a CJK 'fixed width' font is used. To work around
     * that, divide the width returned by Pango by 2 if cjk_width is equal to
     * width for CJK fonts.
     *
     * For related bugs, see:
     * http://bugzilla.gnome.org/show_bug.cgi?id=106618
     * http://bugzilla.gnome.org/show_bug.cgi?id=106624
     *
     * With this, for all four of the following cases, Vim works fine:
     *     guifont=CJK_fixed_width_font
     *     guifont=Non_CJK_fixed_font
     *     guifont=Non_CJK_fixed_font,CJK_Fixed_font
     *     guifont=Non_CJK_fixed_font guifontwide=CJK_fixed_font
     */
    if (is_cjk_font(d->context, d->fontDesc))
    {
	int cjk_width;

	/* measure the text extent of U+4E00 and U+4EBC */
	pango_layout_set_text(layout, "\xe4\xb8\x80\xe4\xba\x8c", -1);
	pango_layout_get_size(layout, &cjk_width, NULL);

	if (width == cjk_width) /* XFT not patched */
	    width /= 2;
    }
    g_object_unref(layout);

    metrics = pango_context_get_metrics(d->context, d->fontDesc,
					pango_context_get_language(d->context));
    ascent = pango_font_metrics_get_ascent(metrics);
    descent = pango_font_metrics_get_descent(metrics);
    pango_font_metrics_unref(metrics);

    d->fontAscent = PANGO_PIXELS(ascent);
    d->fontDescent = PANGO_PIXELS(descent);
    d->fontWidth = (width / 2 + PANGO_SCALE - 1) / PANGO_SCALE;
    d->fontHeight = PANGO_PIXELS(height);

    return d;
}

/* gu_draw_free() {{{1 */
void
gu_draw_free(GUDraw *d)
{
    pango_font_description_free(d->fontDesc);
    pango_attr_list_unref(d->attrList);
    g_object_unref(d->context);
    g_free(d);
}

/* gu_draw_string() {{{1 */
int
gu_draw_string(GUDraw *d, GdkWindow *win, GdkGC *gc,
	       int x, int y, const guchar *str, int len)
{
    int j;
    GList *itemList;
    PangoGlyphString *glyphs = pango_glyph_string_new();
    int last_glyph_rbearing = PANGO_SCALE * d->fontWidth;
    int cells = 0;

    /* Break the text into segments with consistent directional level
     * and shaping engine.  Pure Latin text needs only a single
     * segment, so there's no need to worry about the loop's
     * efficiency.  Better try to optimize elsewhere, e.g. reducing
     * exposes and stuff :)
     */
    itemList = pango_itemize(d->context, (char*) str, 0, len, d->attrList,
			     NULL);
    while (itemList != NULL)
    {
	PangoItem *item;
	int item_cells = 0; /* item length if cells */

	item = (PangoItem *) itemList->data;
	itemList = g_list_delete_link(itemList, itemList);

	/* Increment the bidirectional embedding level by 1 if it is
	 * not even. An odd number means the output will be RTL, but
	 * we don't want that since Vim handles right-to-left text on
	 * its own.  It would probably be sufficient to just set level
	 * = 0, but you can never know :)
	 *
	 * Unfortunately we can't take advantage of Pango's ability to
	 * render both LTR and RTL at the same time.  In order to
	 * support that, Vim's main screen engine would have to make
	 * use of Pango functionality.
	 */
	item->analysis.level = (item->analysis.level + 1) & (~1U);

	pango_shape((char*) str + item->offset, item->length, &item->analysis,
		    glyphs);

	/* Fixed-width hack: iterate over the array and assign a fixed
	 * width to each glyph, thus overriding the choice made by the
	 * shaping engine.  We use utf_char2cells() to determine the
	 * number of cells needed.
	 *
	 * Also perform all kind of dark magic to get composing
	 * characters right (and pretty too of course).
	 */
	for (j = 0; j < glyphs->num_glyphs; j++)
	{
	    PangoGlyphInfo *glyph;

	    glyph = &glyphs->glyphs[j];

	    if (glyph->attr.is_cluster_start)
	    {
		int next;       /* glyph start index of next cluster */
		int start, end; /* string segment of current cluster */
		int uc;
		int cellcount = 0;
		const guchar *p;

		for (next = j + 1; next < glyphs->num_glyphs; ++next)
		    if (glyphs->glyphs[next].attr.is_cluster_start)
			break;

		start = item->offset + glyphs->log_clusters[j];
		end   = item->offset + ((next < glyphs->num_glyphs) ?
					glyphs->log_clusters[next] : item->length);

		for (p = str + start; p < (guchar *) (str + end);
		     p += utf_byte2len(*p))
		{
		    uc = utf_ptr2char(p);
		    if (uc < 0x100)
			++cellcount;
		    else if (!utf_iscomposing(uc))
			cellcount += utf_char2cells(uc);
		}

		if (cellcount > 0)
		{
		    glyph->geometry.width =
			cellcount * d->fontWidth * PANGO_SCALE;
		}
		else
		{
		    /* If there are only combining characters in the
		     * cluster, we cannot just change the width of the
		     * previous glyph since there is none.  Therefore
		     * some guesswork is needed, see below.
		     */
		    PangoRectangle ink_rect, logical_rect;

		    glyph->geometry.width = 0;

		    if (cells > 0) /* should never be 0 at this point */
		    {
			glyph->geometry.x_offset =
			    - cells * d->fontWidth * PANGO_SCALE;
		    }
		    pango_font_get_glyph_extents(item->analysis.font,
						 glyph->glyph,
						 &ink_rect, &logical_rect);
		    /*
		     * If ink_rect.x is negative Pango apparently has taken
		     * care of the composing by itself.  Actually setting
		     * x_offset = 0 should be sufficient then, but due to
		     * problems with composing from different fonts we
		     * still need to fine-tune x_offset to avoid uglyness.
		     *
		     * If ink_rect.x is not negative, force overstriking by
		     * pointing x_offset to the position of the previous
		     * glyph (done a few lines above).  Apparently this
		     * happens only with old X fonts which don't provide
		     * the special combining information needed by Pango.
		     */
		    if (ink_rect.x < 0)
		    {
			glyph->geometry.x_offset += last_glyph_rbearing;
			glyph->geometry.y_offset = logical_rect.height
			    - d->fontHeight * PANGO_SCALE;
		    }
		}

		item_cells += cellcount;
		cells = cellcount;
	    }
	    else if (j > 0) /* j == 0 "cannot happen" */
	    {
		/* There is a previous glyph, so we deal with combining
		 * characters the canonical way.  That is, setting the
		 * width of the previous glyph to 0.
		 */
		glyphs->glyphs[j - 1].geometry.width = 0;
		glyph->geometry.width = cells * d->fontWidth * PANGO_SCALE;
	    }
	    else
	    {
		glyph->geometry.width = 0;
	    }
	}

	/* If a certain combining mark had to be taken from a
	 * non-monospace font, we have to compensate manually by
	 * adapting x_offset according to the ink extents of the
	 * previous glyph.  Retrieving the ink extents is quite
	 * expensive, so let's do it only for the last glyph of the
	 * item and only if it's not the last item.
	 */
	if (j > 0 && itemList != NULL)
	{
	    PangoRectangle ink_rect;

	    do { --j; }
	    while (j > 0 && !glyphs->glyphs[j].attr.is_cluster_start);

	    pango_font_get_glyph_extents(item->analysis.font,
					 glyphs->glyphs[j].glyph,
					 &ink_rect, NULL);

	    last_glyph_rbearing = PANGO_RBEARING(ink_rect);
	}

	gdk_draw_glyphs(win, gc, item->analysis.font,
			x, y, glyphs);
	x += item_cells * d->fontWidth;
	pango_item_free(item);
    }
    pango_glyph_string_free(glyphs);

    return x;
}

/* is_cjk_font() {{{1 */
/*
 * Check if a given font is a CJK font. This is done in a very crude manner. It
 * just see if U+04E00 for zh and ja and U+AC00 for ko are covered in a given
 * font. Consequently, this function cannot  be used as a general purpose check
 * for CJK-ness for which fontconfig APIs should be used.  This is only used by
 * gui_mch_init_font() to deal with 'CJK fixed width fonts'.
 */
int
is_cjk_font(PangoContext *context, PangoFontDescription *font_desc)
{
    static const char * const cjk_langs[] =
    {
        "zh_CN", "zh_TW", "zh_HK", "ja", "ko" 
    };
    PangoFont   *font;
    unsigned    i;
    int         is_cjk = FALSE;

    font = pango_context_load_font(context, font_desc);
    if (font == NULL)
        return FALSE;

    for (i = 0; !is_cjk && i < G_N_ELEMENTS(cjk_langs); ++i)
    {
        PangoCoverage   *coverage;
        gunichar        uc;

        coverage = pango_font_get_coverage(
                font, pango_language_from_string(cjk_langs[i]));

        if (coverage != NULL)
        {
            uc = (cjk_langs[i][0] == 'k') ? 0xAC00 : 0x4E00;
            is_cjk = (pango_coverage_get(coverage, uc) == PANGO_COVERAGE_EXACT);
            pango_coverage_unref(coverage);
        }
    }

    g_object_unref(font);

    return is_cjk;
}

