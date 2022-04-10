/* wavbreaker - A tool to split a wave file up into multiple waves.
 * Copyright (C) 2002-2006 Timothy Robinson
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

#include <config.h>

#include <gtk/gtk.h>

#include "appinfo.h"

void
about_show(GtkWidget *main_window)
{
    gtk_show_about_dialog(GTK_WINDOW(main_window),
            "name", PACKAGE,
            "version", appinfo_version(),
            "copyright", appinfo_copyright(),
            "comments", appinfo_description(),
            "website", appinfo_url(),
            "website-label", appinfo_url(),
            "authors", appinfo_authors(),
            "logo-icon-name", "net.sourceforge.wavbreaker",
            NULL);
}
