/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nemo
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Nemo is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nemo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Authors: Darin Adler <darin@bentspoon.com>
 */

#include <config.h>
#include "nemo-desktop-window.h"
#include "nemo-window-private.h"
#include "nemo-actions.h"

#include <X11/Xatom.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

#include <eel/eel-vfs-extensions.h>
#include <libnemo-private/nemo-file-utilities.h>
#include <libnemo-private/nemo-icon-names.h>
#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-ui-utilities.h>

struct NemoDesktopWindowDetails {
	gulong size_changed_id;

	gboolean loaded;
};

static void
desktop_background_cb (GtkAction * action, gpointer user_data)
{
	g_spawn_command_line_async("gnome-control-center background", NULL);
	return;
}

static void
ubuntu_docs_cb (GtkAction * action, gpointer user_data)
{
	g_spawn_command_line_async("yelp", NULL);
	return;
}

static const GtkActionEntry desktop_entries[] = {
	/* name, stock id, label */ { "Change Desktop Background", NULL, N_("Change Desktop _Background"),
	/* accel, tooltip */          NULL, NULL,
								  G_CALLBACK(desktop_background_cb)},
	/* name, stock id, label */ { "Ubuntu Documentation",      NULL, N_("Ubuntu Help"),
	/* accel, tooltip */          NULL, NULL,
								  G_CALLBACK(ubuntu_docs_cb)}
};

static void set_wmspec_desktop_hint (GdkWindow *window);

G_DEFINE_TYPE (NemoDesktopWindow, nemo_desktop_window, 
	       NEMO_TYPE_WINDOW);

static void
nemo_desktop_window_dispose (GObject *obj)
{
	NemoDesktopWindow *window = NEMO_DESKTOP_WINDOW (obj);

	g_signal_handlers_disconnect_by_func (nemo_preferences,
					      nemo_desktop_window_update_directory,
					      window);

	G_OBJECT_CLASS (nemo_desktop_window_parent_class)->dispose (obj);
}

static void
nemo_desktop_window_constructed (GObject *obj)
{
	GtkActionGroup *action_group;
	GtkAction *action;
	AtkObject *accessible;
	NemoDesktopWindow *window = NEMO_DESKTOP_WINDOW (obj);
	NemoWindow *nwindow = NEMO_WINDOW (obj);

	G_OBJECT_CLASS (nemo_desktop_window_parent_class)->constructed (obj);
	
	gtk_widget_hide (nwindow->details->statusbar);
	gtk_widget_hide (nwindow->details->menubar);

	action_group = nemo_window_get_main_action_group (nwindow);

	/* Don't allow close action on desktop */
	action = gtk_action_group_get_action (action_group,
					      NEMO_ACTION_CLOSE);
	gtk_action_set_sensitive (action, FALSE);

	/* Don't allow new tab on desktop */
	action = gtk_action_group_get_action (action_group,
					      NEMO_ACTION_NEW_TAB);
	gtk_action_set_sensitive (action, FALSE);

 	UbuntuMenuProxy * proxy = ubuntu_menu_proxy_get();
 	if (proxy != NULL) {
 		ubuntu_menu_proxy_insert(proxy, GTK_WIDGET(window), NEMO_WINDOW(window)->details->menubar, 0);
 	}

 	/* Add actions for the desktop */
 	GtkActionGroup * desktop_agroup = gtk_action_group_new("DesktopActions");
 	gtk_action_group_set_translation_domain(desktop_agroup, GETTEXT_PACKAGE);
 	gtk_action_group_add_actions(desktop_agroup, desktop_entries, G_N_ELEMENTS(desktop_entries), window);
 	gtk_ui_manager_insert_action_group(nemo_window_get_ui_manager(NEMO_WINDOW(window)),
 	                                   desktop_agroup, 0);
 	g_object_unref(desktop_agroup);
 
 	GtkUIManager * ui_manager = nemo_window_get_ui_manager(NEMO_WINDOW(window));
 	gtk_ui_manager_add_ui_from_resource (ui_manager, "/org/gnome/nemo/nemo-desktop-window-ui.xml", NULL);

 	/* Hide actions that don't make sense on the desktop */
 	GList * agroups = gtk_ui_manager_get_action_groups(NEMO_WINDOW(window)->details->ui_manager);
 	while (agroups != NULL) {
 		GtkActionGroup * agroup = GTK_ACTION_GROUP(agroups->data);
 		const gchar * name = gtk_action_group_get_name(agroup);
 
 		if (g_strcmp0(name, "LaunchpadIntegration") == 0) {
 			gtk_action_group_set_visible(agroup, FALSE);
 		} else if (g_strcmp0(name, "SpatialActions") == 0) {
 			GtkAction * action = NULL;
 
 			action = gtk_action_group_get_action(agroup, "Close Parent Folders");
 			gtk_action_set_visible(action, FALSE);
 
 			action = gtk_action_group_get_action(agroup, "Close All Folders");
 			gtk_action_set_visible(action, FALSE);
 		} else if (g_strcmp0(name, "ShellActions") == 0) {
 			GtkAction * action = NULL;
 
 			action = gtk_action_group_get_action(agroup, "Close All Windows");
 			gtk_action_set_visible(action, FALSE);

 			action = gtk_action_group_get_action(agroup, NEMO_ACTION_CLOSE);
 			gtk_action_set_visible(action, FALSE);
 
 			action = gtk_action_group_get_action(agroup, NEMO_ACTION_STOP);
 			gtk_action_set_visible(action, FALSE);
 
 			action = gtk_action_group_get_action(agroup, NEMO_ACTION_RELOAD);
 			gtk_action_set_visible(action, FALSE);
 
 			action = gtk_action_group_get_action(agroup, "Preferences");
 			gtk_action_set_visible(action, FALSE);
 
 			action = gtk_action_group_get_action(agroup, "About Nemo");
 			gtk_action_set_visible(action, FALSE);
 
 			action = gtk_action_group_get_action(agroup, "Up");
 			gtk_action_set_visible(action, FALSE);

 			action = gtk_action_group_get_action(agroup, "New Window");
 			gtk_action_set_visible(action, FALSE);

 			action = gtk_action_group_get_action(agroup, "New Tab");
 			gtk_action_set_visible(action, FALSE);

 			action = gtk_action_group_get_action(agroup, NEMO_ACTION_BACK);
 			gtk_action_set_visible(action, FALSE);

 			action = gtk_action_group_get_action(agroup, NEMO_ACTION_FORWARD);
 			gtk_action_set_visible(action, FALSE);

 			action = gtk_action_group_get_action(agroup, NEMO_ACTION_SHOW_HIDE_EXTRA_PANE);
 			gtk_action_set_visible(action, FALSE);

 			action = gtk_action_group_get_action(agroup, "SplitViewSameLocation");
 			gtk_action_set_visible(action, FALSE);

 			action = gtk_action_group_get_action(agroup, "Go to Location");
 			gtk_action_set_visible(action, FALSE);

 			action = gtk_action_group_get_action(agroup, "Search");
 			gtk_action_set_visible(action, FALSE);

 			action = gtk_action_group_get_action(agroup, "Bookmarks");
 			gtk_action_set_visible(action, FALSE);

 			action = gtk_action_group_get_action(agroup, "Show Hide Sidebar");
 			gtk_action_set_visible(action, FALSE);

 			action = gtk_action_group_get_action(agroup, "Show Hide Statusbar");
 			gtk_action_set_visible(action, FALSE);

 			action = gtk_action_group_get_action(agroup, "Show Hide Toolbar");
 			gtk_action_set_visible(action, FALSE);

 			action = gtk_action_group_get_action(agroup, "Sidebar Places");
 			gtk_action_set_visible(action, FALSE);

 			action = gtk_action_group_get_action(agroup, "Sidebar Tree");
 			gtk_action_set_visible(action, FALSE);

 			action = gtk_action_group_get_action(agroup, "NemoHelp");
 			gtk_action_set_visible(action, FALSE);

 			action = gtk_action_group_get_action(agroup, "NemoHelpSearch");
 			gtk_action_set_visible(action, FALSE);

 			action = gtk_action_group_get_action(agroup, "NemoHelpSort");
 			gtk_action_set_visible(action, FALSE);

 			action = gtk_action_group_get_action(agroup, "NemoHelpLost");
 			gtk_action_set_visible(action, FALSE);

 			action = gtk_action_group_get_action(agroup, "NemoHelpShare");
 			gtk_action_set_visible(action, FALSE);
 		}
 
 		agroups = g_list_next(agroups);
 	}

	/* Set the accessible name so that it doesn't inherit the cryptic desktop URI. */
	accessible = gtk_widget_get_accessible (GTK_WIDGET (window));

	if (accessible) {
		atk_object_set_name (accessible, _("Desktop"));
	}

	/* Monitor the preference to have the desktop */
	/* point to the Unix home folder */
	g_signal_connect_swapped (nemo_preferences, "changed::" NEMO_PREFERENCES_DESKTOP_IS_HOME_DIR,
				  G_CALLBACK (nemo_desktop_window_update_directory),
				  window);
}

static void
nemo_desktop_window_init (NemoDesktopWindow *window)
{
	window->details = G_TYPE_INSTANCE_GET_PRIVATE (window, NEMO_TYPE_DESKTOP_WINDOW,
						       NemoDesktopWindowDetails);

	gtk_window_move (GTK_WINDOW (window), 0, 0);

	/* shouldn't really be needed given our semantic type
	 * of _NET_WM_TYPE_DESKTOP, but why not
	 */
	gtk_window_set_resizable (GTK_WINDOW (window),
				  FALSE);

	g_object_set_data (G_OBJECT (window), "is_desktop_window", 
			   GINT_TO_POINTER (1));

 
}

static gint
nemo_desktop_window_delete_event (NemoDesktopWindow *window)
{
	/* Returning true tells GTK+ not to delete the window. */
	return TRUE;
}

void
nemo_desktop_window_update_directory (NemoDesktopWindow *window)
{
	GFile *location;

	g_assert (NEMO_IS_DESKTOP_WINDOW (window));

	window->details->loaded = FALSE;
	location = g_file_new_for_uri (EEL_DESKTOP_URI);
	nemo_window_go_to (NEMO_WINDOW (window), location);
	window->details->loaded = TRUE;

	g_object_unref (location);
}

static void
nemo_desktop_window_screen_size_changed (GdkScreen             *screen,
					     NemoDesktopWindow *window)
{
	int width_request, height_request;

	width_request = gdk_screen_get_width (screen);
	height_request = gdk_screen_get_height (screen);
	
	g_object_set (window,
		      "width_request", width_request,
		      "height_request", height_request,
		      NULL);
}

NemoDesktopWindow *
nemo_desktop_window_new (GdkScreen *screen)
{
	NemoDesktopWindow *window;
	int width_request, height_request;

	width_request = gdk_screen_get_width (screen);
	height_request = gdk_screen_get_height (screen);

	window = g_object_new (NEMO_TYPE_DESKTOP_WINDOW,
			       "disable-chrome", TRUE,
			       "width_request", width_request,
			       "height_request", height_request,
			       "screen", screen,
			       NULL);

	/* Special sawmill setting*/
	gtk_window_set_wmclass (GTK_WINDOW (window), "desktop_window", "Nemo");

	g_signal_connect (window, "delete_event", G_CALLBACK (nemo_desktop_window_delete_event), NULL);

	/* Point window at the desktop folder.
	 * Note that nemo_desktop_window_init is too early to do this.
	 */
	nemo_desktop_window_update_directory (window);

	return window;
}

static void
map (GtkWidget *widget)
{
	/* Chain up to realize our children */
	GTK_WIDGET_CLASS (nemo_desktop_window_parent_class)->map (widget);
	gdk_window_lower (gtk_widget_get_window (widget));
}

static void
unrealize (GtkWidget *widget)
{
	NemoDesktopWindow *window;
	NemoDesktopWindowDetails *details;
	GdkWindow *root_window;

	window = NEMO_DESKTOP_WINDOW (widget);
	details = window->details;

	root_window = gdk_screen_get_root_window (
				gtk_window_get_screen (GTK_WINDOW (window)));

	gdk_property_delete (root_window,
			     gdk_atom_intern ("NEMO_DESKTOP_WINDOW_ID", TRUE));

	if (details->size_changed_id != 0) {
		g_signal_handler_disconnect (gtk_window_get_screen (GTK_WINDOW (window)),
					     details->size_changed_id);
		details->size_changed_id = 0;
	}

	GTK_WIDGET_CLASS (nemo_desktop_window_parent_class)->unrealize (widget);
}

static void
set_wmspec_desktop_hint (GdkWindow *window)
{
	GdkAtom atom;

	atom = gdk_atom_intern ("_NET_WM_WINDOW_TYPE_DESKTOP", FALSE);
        
	gdk_property_change (window,
			     gdk_atom_intern ("_NET_WM_WINDOW_TYPE", FALSE),
			     gdk_x11_xatom_to_atom (XA_ATOM), 32,
			     GDK_PROP_MODE_REPLACE, (guchar *) &atom, 1);
}

static void
set_desktop_window_id (NemoDesktopWindow *window,
		       GdkWindow             *gdkwindow)
{
	/* Tuck the desktop windows xid in the root to indicate we own the desktop.
	 */
	Window window_xid;
	GdkWindow *root_window;

	root_window = gdk_screen_get_root_window (
				gtk_window_get_screen (GTK_WINDOW (window)));

	window_xid = GDK_WINDOW_XID (gdkwindow);

	gdk_property_change (root_window,
			     gdk_atom_intern ("NEMO_DESKTOP_WINDOW_ID", FALSE),
			     gdk_x11_xatom_to_atom (XA_WINDOW), 32,
			     GDK_PROP_MODE_REPLACE, (guchar *) &window_xid, 1);
}

static void
realize (GtkWidget *widget)
{
	NemoDesktopWindow *window;
	NemoDesktopWindowDetails *details;

	window = NEMO_DESKTOP_WINDOW (widget);
	details = window->details;

	/* Make sure we get keyboard events */
	gtk_widget_set_events (widget, gtk_widget_get_events (widget) 
			      | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);
			      
	/* Do the work of realizing. */
	GTK_WIDGET_CLASS (nemo_desktop_window_parent_class)->realize (widget);

	/* This is the new way to set up the desktop window */
	set_wmspec_desktop_hint (gtk_widget_get_window (widget));

	set_desktop_window_id (window, gtk_widget_get_window (widget));

	details->size_changed_id =
		g_signal_connect (gtk_window_get_screen (GTK_WINDOW (window)), "size_changed",
				  G_CALLBACK (nemo_desktop_window_screen_size_changed), window);
}

static NemoIconInfo *
real_get_icon (NemoWindow *window,
	       NemoWindowSlot *slot)
{
	return nemo_icon_info_lookup_from_name (NEMO_ICON_DESKTOP, 48);
}

static void
real_sync_title (NemoWindow *window,
		 NemoWindowSlot *slot)
{
	/* hardcode "Desktop" */
	gtk_window_set_title (GTK_WINDOW (window), _("Desktop"));
}

static void
real_window_close (NemoWindow *window)
{
	/* stub, does nothing */
	return;
}

static void
nemo_desktop_window_class_init (NemoDesktopWindowClass *klass)
{
	GtkWidgetClass *wclass = GTK_WIDGET_CLASS (klass);
	NemoWindowClass *nclass = NEMO_WINDOW_CLASS (klass);
	GObjectClass *oclass = G_OBJECT_CLASS (klass);

	oclass->constructed = nemo_desktop_window_constructed;
	oclass->dispose = nemo_desktop_window_dispose;

	wclass->realize = realize;
	wclass->unrealize = unrealize;
	wclass->map = map;

	nclass->sync_title = real_sync_title;
	nclass->get_icon = real_get_icon;
	nclass->close = real_window_close;

	g_type_class_add_private (klass, sizeof (NemoDesktopWindowDetails));
}

gboolean
nemo_desktop_window_loaded (NemoDesktopWindow *window)
{
	return window->details->loaded;
}
