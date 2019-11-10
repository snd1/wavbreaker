/* wavbreaker - A tool to split a wave file up into multiple wave.
 * Copyright (C) 2002-2005 Timothy Robinson
 * Copyright (C) 2007-2019 Thomas Perl
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <gtk/gtk.h>
#include <math.h>

#include "draw.h"

static void draw_sample_surface(struct WaveformSurface *self, struct WaveformSurfaceDrawContext *ctx);
static void draw_summary_surface(struct WaveformSurface *self, struct WaveformSurfaceDrawContext *ctx);

#define SAMPLE_COLORS 6
#define SAMPLE_SHADES 3

GdkRGBA sample_colors[SAMPLE_COLORS][SAMPLE_SHADES];
GdkRGBA bg_color;
GdkRGBA nowrite_color;

/**
 * The sample_colors_values array now uses colors from the
 * Tango Icon Theme Guidelines (except "Chocolate", as it looks too much like
 * "Orange" in the waveform sample view), see this URL for more information:
 *
 * http://tango.freedesktop.org/Tango_Icon_Theme_Guidelines
 **/
const double sample_colors_values[SAMPLE_COLORS][3] = {
    { 52 / 255.f, 101 / 255.f, 164 / 255.f }, // Sky Blue
    { 204 / 255.f, 0 / 255.f, 0 / 255.f },    // Scarlet Red
    { 115 / 255.f, 210 / 255.f, 22 / 255.f }, // Chameleon
    { 237 / 255.f, 212 / 255.f, 0 / 255.f },  // Butter
    { 245 / 255.f, 121 / 255.f, 0 / 255.f },  // Orange
    { 117 / 255.f, 80 / 255.f, 123 / 255.f }, // Plum
};

static inline void
set_cairo_source(cairo_t *cr, GdkRGBA *color)
{
    cairo_set_source_rgb(cr, color->red, color->green, color->blue);
}

static inline void
fill_cairo_rectangle(cairo_t *cr, GdkRGBA *color, int width, int height)
{
    set_cairo_source(cr, color);
    cairo_rectangle(cr, 0.f, 0.f, (float)width, (float)height);
    cairo_fill(cr);
}

static inline void
draw_cairo_line(cairo_t *cr, float x, float y0, float y1)
{
    cairo_move_to(cr, x-0.5f, y0);
    cairo_line_to(cr, x-0.5f, y1);
}

static void
waveform_surface_static_init()
{
    static gboolean inited = FALSE;
    if (inited) {
        return;
    }

    /* Set default colors up */
    bg_color.red   =
    bg_color.green =
    bg_color.blue  = 1.f;

    nowrite_color.red   =
    nowrite_color.green =
    nowrite_color.blue  = 0.86f;

    for (int i=0; i<SAMPLE_COLORS; i++) {
        for (int x=0; x<SAMPLE_SHADES; x++) {
            float factor_white = 0.5f*((float)x/(float)SAMPLE_SHADES);
            float factor_color = 1.f-factor_white;
            sample_colors[i][x].red = sample_colors_values[i][0]*factor_color+factor_white;
            sample_colors[i][x].green = sample_colors_values[i][1]*factor_color+factor_white;
            sample_colors[i][x].blue = sample_colors_values[i][2]*factor_color+factor_white;
        }
    }

    inited = TRUE;
}

struct WaveformSurface *waveform_surface_create_sample()
{
    waveform_surface_static_init();

    struct WaveformSurface *surface = calloc(sizeof(struct WaveformSurface), 1);

    surface->draw = draw_sample_surface;

    return surface;
}

struct WaveformSurface *waveform_surface_create_summary()
{
    waveform_surface_static_init();

    struct WaveformSurface *surface = calloc(sizeof(struct WaveformSurface), 1);

    surface->draw = draw_summary_surface;

    return surface;
}

void waveform_surface_draw(struct WaveformSurface *surface, struct WaveformSurfaceDrawContext *ctx)
{
    surface->draw(surface, ctx);
}

void waveform_surface_invalidate(struct WaveformSurface *surface)
{
    if (surface->surface) {
        cairo_surface_destroy(surface->surface);
        surface->surface = NULL;
    }

    surface->width = 0;
    surface->height = 0;
}

void waveform_surface_free(struct WaveformSurface *surface)
{
    if (surface->surface) {
        cairo_surface_destroy(surface->surface);
    }

    free(surface);
}

static void
draw_sample_surface(struct WaveformSurface *self, struct WaveformSurfaceDrawContext *ctx)
{
    int xaxis;
    int width, height;
    int y_min, y_max;
    int scale;
    long i;

    int shade;

    int count;
    int is_write;
    int index;
    GList *tbl;
    TrackBreak *tb_cur;
    unsigned int moodbar_pos = 0;
    GdkRGBA new_color;

    {
        GtkAllocation allocation;
        gtk_widget_get_allocation(ctx->widget, &allocation);

        width = allocation.width;
        height = allocation.height;
    }

    if (self->surface != NULL && self->width == width && self->height == height && self->offset == ctx->pixmap_offset &&
        (ctx->moodbarData && ctx->moodbarData->numFrames) == self->moodbar) {
        return;
    }

    if (self->surface) {
        cairo_surface_destroy(self->surface);
    }

    self->surface = gdk_window_create_similar_surface(gtk_widget_get_window(ctx->widget),
            CAIRO_CONTENT_COLOR, width, height);

    if (!self->surface) {
        printf("surface is NULL\n");
        return;
    }

    cairo_t *cr = cairo_create(self->surface);
    cairo_set_line_width(cr, 1.f);

    /* clear sample_surface before drawing */
    fill_cairo_rectangle(cr, &bg_color, width, height);

    if (ctx->graphData->data == NULL) {
        cairo_destroy(cr);
        return;
    }

    xaxis = height / 2;
    if (xaxis != 0) {
        scale = ctx->graphData->maxSampleValue / xaxis;
        if (scale == 0) {
            scale = 1;
        }
    } else {
        scale = 1;
    }

    /* draw sample graph */

    for (i = 0; i < width && i < ctx->graphData->numSamples; i++) {
        y_min = ctx->graphData->data[i + ctx->pixmap_offset].min;
        y_max = ctx->graphData->data[i + ctx->pixmap_offset].max;

        y_min = xaxis + fabs((double)y_min) / scale;
        y_max = xaxis - y_max / scale;

        /* find the track break we are drawing now */
        count = 0;
        is_write = 0;
        tb_cur = NULL;
        for (tbl = ctx->track_break_list; tbl != NULL; tbl = g_list_next(tbl)) {
            index = g_list_position(ctx->track_break_list, tbl);
            tb_cur = g_list_nth_data(ctx->track_break_list, index);
            if (tb_cur != NULL) {
                if (i + ctx->pixmap_offset > tb_cur->offset) {
                    is_write = tb_cur->write;
                    count++;
                } else {
                    /* already found */
                    break;
                }
            }
        }

        if (ctx->moodbarData && ctx->moodbarData->numFrames) {
            moodbar_pos = (unsigned int)(ctx->moodbarData->numFrames*((float)(i+ctx->pixmap_offset)/(float)(ctx->graphData->numSamples)));
            set_cairo_source(cr, &(ctx->moodbarData->frames[moodbar_pos]));
            draw_cairo_line(cr, i, 0.f, height);
            cairo_stroke(cr);
        }

        for( shade=0; shade<SAMPLE_SHADES; shade++) {
            if( is_write) {
                new_color = sample_colors[(count-1) % SAMPLE_COLORS][shade];
            } else {
                new_color = nowrite_color;
            }

            set_cairo_source(cr, &new_color);
            draw_cairo_line(cr, i, y_min+(xaxis-y_min)*shade/SAMPLE_SHADES, y_min+(xaxis-y_min)*(shade+1)/SAMPLE_SHADES);
            draw_cairo_line(cr, i, y_max-(y_max-xaxis)*shade/SAMPLE_SHADES, y_max-(y_max-xaxis)*(shade+1)/SAMPLE_SHADES);
            cairo_stroke(cr);
        }
    }

    cairo_destroy(cr);

    self->width = width;
    self->height = height;
    self->offset = ctx->pixmap_offset;
    self->moodbar = ctx->moodbarData && ctx->moodbarData->numFrames;
}

static void
draw_summary_surface(struct WaveformSurface *self, struct WaveformSurfaceDrawContext *ctx)
{
    int xaxis;
    int width, height;
    int y_min, y_max;
    int min, max;
    int scale;
    int i, k;
    int loop_end, array_offset;
    int shade;

    float x_scale;

    int count;
    int is_write;
    int index;
    GList *tbl;
    TrackBreak *tb_cur;

    unsigned int moodbar_pos = 0;
    GdkRGBA new_color;

    {
        GtkAllocation allocation;
        gtk_widget_get_allocation(ctx->widget, &allocation);
        width = allocation.width;
        height = allocation.height;
    }

    if (self->surface != NULL && self->width == width && self->height == height &&
        (ctx->moodbarData && ctx->moodbarData->numFrames) == self->moodbar) {
        return;
    }

    if (self->surface) {
        cairo_surface_destroy(self->surface);
    }

    self->surface = gdk_window_create_similar_surface(gtk_widget_get_window(ctx->widget),
            CAIRO_CONTENT_COLOR, width, height);

    if (!self->surface) {
        printf("summary_surface is NULL\n");
        return;
    }

    cairo_t *cr = cairo_create(self->surface);
    cairo_set_line_width(cr, 1.f);

    /* clear sample_surface before drawing */
    fill_cairo_rectangle(cr, &bg_color, width, height);

    if (ctx->graphData->data == NULL) {
        cairo_destroy(cr);
        return;
    }

    xaxis = height / 2;
    if (xaxis != 0) {
        scale = ctx->graphData->maxSampleValue / xaxis;
        if (scale == 0) {
            scale = 1;
        }
    } else {
        scale = 1;
    }

    /* draw sample graph */

    x_scale = (float)(ctx->graphData->numSamples) / (float)(width);
    if (x_scale == 0) {
        x_scale = 1;
    }

    for (i = 0; i < width && i < ctx->graphData->numSamples; i++) {
        min = max = 0;
        array_offset = (int)(i * x_scale);

        if (x_scale != 1) {
            loop_end = (int)x_scale;

            for (k = 0; k < loop_end; k++) {
                if (ctx->graphData->data[array_offset + k].max > max) {
                    max = ctx->graphData->data[array_offset + k].max;
                } else if (ctx->graphData->data[array_offset + k].min < min) {
                    min = ctx->graphData->data[array_offset + k].min;
                }
            }
        } else {
            min = ctx->graphData->data[i].min;
            max = ctx->graphData->data[i].max;
        }

        y_min = min;
        y_max = max;

        y_min = xaxis + fabs((double)y_min) / scale;
        y_max = xaxis - y_max / scale;

        count = 0;
        is_write = 0;
        tb_cur = NULL;
        for (tbl = ctx->track_break_list; tbl != NULL; tbl = g_list_next(tbl)) {
            index = g_list_position(ctx->track_break_list, tbl);
            tb_cur = g_list_nth_data(ctx->track_break_list, index);
            if (tb_cur != NULL) {
                if (array_offset > tb_cur->offset) {
                    is_write = tb_cur->write;
                    count++;
                }
            }
        }

        if (ctx->moodbarData && ctx->moodbarData->numFrames) {
            moodbar_pos = (unsigned int)(ctx->moodbarData->numFrames*((float)(array_offset)/(float)(ctx->graphData->numSamples)));
            set_cairo_source(cr, &(ctx->moodbarData->frames[moodbar_pos]));
            draw_cairo_line(cr, i, 0.f, height);
            cairo_stroke(cr);
        }

        for( shade=0; shade<SAMPLE_SHADES; shade++) {
            if( is_write) {
                new_color = sample_colors[(count-1) % SAMPLE_COLORS][shade];
            } else {
                new_color = nowrite_color;
            }

            if (ctx->moodbarData && ctx->moodbarData->numFrames) {
#define MB_OVL_MOODBAR 2
#define MB_OVL_WAVEFORM 7
#define MOODBAR_BLEND(waveform,moodbar) (((MB_OVL_WAVEFORM*waveform+MB_OVL_MOODBAR*moodbar))/(MB_OVL_MOODBAR+MB_OVL_WAVEFORM))
                new_color.red = MOODBAR_BLEND(new_color.red, ctx->moodbarData->frames[moodbar_pos].red);
                new_color.green = MOODBAR_BLEND(new_color.green, ctx->moodbarData->frames[moodbar_pos].green);
                new_color.blue = MOODBAR_BLEND(new_color.blue, ctx->moodbarData->frames[moodbar_pos].blue);
#undef MB_OVL_MOODBAR
#undef MB_OVL_WAVEFORM
#undef MOODBAR_BLEND
            }

            set_cairo_source(cr, &new_color);
            draw_cairo_line(cr, i, y_min+(xaxis-y_min)*shade/SAMPLE_SHADES, y_min+(xaxis-y_min)*(shade+1)/SAMPLE_SHADES);
            draw_cairo_line(cr, i, y_max-(y_max-xaxis)*shade/SAMPLE_SHADES, y_max-(y_max-xaxis)*(shade+1)/SAMPLE_SHADES);
            cairo_stroke(cr);
        }
    }

    cairo_destroy(cr);

    self->width = width;
    self->height = height;
    self->moodbar = ctx->moodbarData && ctx->moodbarData->numFrames;
}
