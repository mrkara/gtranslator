/*
 * Copyright (C) 2007   Ignacio Casal Quinteiro <nacho.resa@gmail.com>
 * 			Fatih Demir <kabalak@kabalak.net>
 *			Ross Golder <ross@golder.org>
 *			Gediminas Paulauskas <menesis@kabalak.net>
			Pablo Sanxiao <psanxiao@gmail.com>
 *
 *     This program is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation, either version 3 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANMSGILITY or FITNESS FOR A PARTICULAR PURMSGSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>

#include "actions.h"
#include "file-dialogs.h"
#include "notebook.h"
#include "po.h"
#include "tab.h"
#include "window.h"


static void gtranslator_save_file_dialog(GtkWidget *widget,
					 GtranslatorWindow *window);


/*
 * The main file opening function. Checks that the file isn't already open,
 * and if not, opens it in a new tab.
 */
gboolean 
gtranslator_open(const gchar *filename,
		 GtranslatorWindow *window,
		 GError **error)
{
	GtranslatorHeader *header;
	GtranslatorPo	*po;
	GtranslatorTab *tab;
	GList *current;
	GtranslatorView *active_view;
	gchar *project_id;

	/*
	 * If the filename can't be opened, pass the error back to the caller
	 * to handle.
	 */
	po = gtranslator_po_new();
	gtranslator_po_parse(po, filename, error);
	
	if(*error != NULL)
		return FALSE;

	header = gtranslator_po_get_header(po);
	project_id = gtranslator_header_get_prj_id_version(header);

	/*
	 * If not a crash/temporary file, add to the history.
	 */
	gtranslator_recent_add(window, filename, project_id);

	/*
	 * Create a page to add to our list of open files
	 */
	tab = gtranslator_window_create_tab(window, po);
	
	/*
	 * Show the current message.
	 */
	current = gtranslator_po_get_current_message(po);
	gtranslator_tab_message_go_to(tab, current);
	
	/*
	 * Grab the focus
	 */
	active_view = gtranslator_tab_get_active_view(tab);
	gtk_widget_grab_focus(GTK_WIDGET(active_view));
	
	gtranslator_window_update_progress_bar(window);
	
	return TRUE;
}

static void 
gtranslator_po_parse_file_from_dialog(GtkWidget * dialog,
				      GtranslatorWindow *window)
{
	gchar *po_file;
	GError *error = NULL;
	po_file = g_strdup(gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)));

	gtranslator_file_dialogs_store_directory(po_file);

	/*
	 * Open the file via our centralized opening function.
	 */
	if(!gtranslator_open(po_file, window, &error)) {
		if(error) 
		{
			/*
			 * We have to show the error in a dialog
			 */
			gtk_message_dialog_new(GTK_WINDOW(window),
					       GTK_DIALOG_DESTROY_WITH_PARENT,
					       GTK_MESSAGE_ERROR,
					       GTK_BUTTONS_CLOSE,
					       error->message);
			g_error_free(error);
		}
	}

	g_free(po_file);
	
	/*
	 * Destroy the dialog 
	 */
	gtk_widget_destroy(dialog);
}


static void
gtranslator_file_chooser_analyse(gpointer dialog,
				 FileselMode mode,
				 GtranslatorWindow *window)
{	
	gint reply;

	reply = gtk_dialog_run(GTK_DIALOG (dialog));
	switch (reply){
		case GTK_RESPONSE_ACCEPT:
			if (mode == FILESEL_OPEN){
				gtranslator_po_parse_file_from_dialog(GTK_WIDGET(dialog),
								      window);
			} else {
				gtranslator_save_file_dialog(GTK_WIDGET(dialog), window);
			}
			break;
		case GTK_RESPONSE_CANCEL:
			gtk_widget_hide(GTK_WIDGET(dialog));
			break;
		case GTK_RESPONSE_DELETE_EVENT:
			gtk_widget_hide(GTK_WIDGET(dialog));
			break;
		default:
			break;
	}
}


/*
 * The "Open file" dialog.
 */
void
gtranslator_open_file_dialog(GtkAction * action,
			     GtranslatorWindow *window)
{
	GtkWindow *dialog = NULL;

	if(dialog != NULL) {
		gtk_window_present(GTK_WINDOW(dialog));
		return;
	}
	dialog = gtranslator_file_chooser_new (GTK_WINDOW(window), 
					       FILESEL_OPEN,
					       _("Open file for translation"));	
	/*
	 * With the gettext parser/writer API, we can't currently read/write
	 * to remote files with gnome-vfs. Eventually, we should intercept
	 * remote requests and use gnome-vfs to retrieve a temporary file to 
	 * work on, and transmit it back when saved.
	 */
	gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(dialog), TRUE);

	gtranslator_file_chooser_analyse((gpointer) dialog, FILESEL_OPEN, window);
}


/*
 * A callback for Overwrite in Save as
 */
static void
gtranslator_overwrite_file(GtkWidget * widget,
			   GtranslatorWindow *window)
{
	GError *error;
	//gtranslator_save_file(current_page->po,current_page->po->filename, &error);
	/*
	 * TODO: Should close the file and open the new saved file
	 */
	//gtranslator_open_file(current_page->po->filename);
	//gtranslator_open(current_page->po->filename, window, &error);
}

/*
 * A callback for OK in Save as... dialog 
 */
static void 
gtranslator_save_file_dialog(GtkWidget *widget,
			     GtranslatorWindow *window)
{
	gchar *po_file,
	      *po_file_normalized;
	GtranslatorPo *po;
	
	//po = gtranslator_window_get_active_po(window);
	
 	po_file = g_strdup(gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(widget)));
	po_file_normalized = g_utf8_normalize( po_file, -1, G_NORMALIZE_DEFAULT_COMPOSE);
	g_free(po_file);
	po_file = po_file_normalized;

	if (g_file_test(po_file, G_FILE_TEST_EXISTS))
	{
		//gtranslator_po_set_filename(po, po_file);
		
		GtkWidget *dialog, *button;
	
		dialog = gtk_message_dialog_new (NULL,
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_QUESTION,
						 GTK_BUTTONS_CANCEL,
						 _("The file '%s' already exists. Do you want overwrite it?"),
						 po_file);
		
		button = gtk_dialog_add_button (GTK_DIALOG (dialog), "Overwrite", 1);
		
		g_signal_connect (G_OBJECT (button), "clicked",
			G_CALLBACK (gtranslator_overwrite_file), window);
		
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	}
	g_free(po_file);
	gtk_widget_destroy(GTK_WIDGET(widget));
}

/*
 * "Save as" dialog.
 */
void
gtranslator_save_file_as_dialog(GtkAction * action,
				GtranslatorWindow *window)
{
	GtkWindow *dialog = NULL;
	GtranslatorTab *current_page;
	GtranslatorPo *po;
	
	if(dialog != NULL) {
		gtk_window_present(GTK_WINDOW(dialog));
		return;
	}

	current_page = gtranslator_window_get_active_tab(window);
	po = gtranslator_tab_get_po(current_page);
	
	if(gtranslator_po_get_write_perms(po)==FALSE ||
	   strstr((const char*)gtranslator_po_get_filename, "/.gtranslator/"))
	{
		dialog = gtranslator_file_chooser_new(GTK_WINDOW(window),
						      FILESEL_SAVE,
						      _("Save file as..."));
	}
	else
	{
		dialog = gtranslator_file_chooser_new(GTK_WINDOW(window),
						      FILESEL_SAVE,
						      _("Save local copy of file as..."));
		
		/*
		 * Set a local filename in the users home directory with the 
		 *  same filename as the original but with a project prefix
		 *   (e.g. "gtranslator-tr.po").
		 */ 
		gtk_file_chooser_select_filename(GTK_FILE_CHOOSER(dialog),
						 gtranslator_po_get_filename(po));
		gtranslator_file_dialogs_store_directory(gtranslator_po_get_filename(po));
	}

	/*
	 * With the gettext parser/writer API, we can't currently read/write
	 * to remote files with gnome-vfs. Eventually, we should intercept
	 * remote requests and use gnome-vfs to retrieve a temporary file to 
	 * work on, and transmit it back when saved.
	 */
	gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(dialog), TRUE);

	gtranslator_file_chooser_analyse((gpointer) dialog, FILESEL_SAVE, window);
	
	//gtranslator_dialog_show(&dialog, "gtranslator -- save");
}

void 
gtranslator_file_close(GtkAction * widget,
		       GtranslatorWindow *window)
{
	GtranslatorNotebook *nb;
	GtranslatorTab *current_page;
	gint i;
	
	current_page = gtranslator_window_get_active_tab(window);
	nb = gtranslator_window_get_notebook(window);
	g_assert(current_page != NULL);

	/*
	 * If user doesn't know what to do with changed file, return
	 */
	/*if (!gtranslator_should_the_file_be_saved_dialog(current_page))
		return;*/

	/*
	 * "Remove" the stored "runtime/filename" key.
	 * Is this really neccessary with tabs?
	 */
	//gtranslator_config_set_string("runtime/filename", "--- No file ---");
	
	i = gtk_notebook_page_num(GTK_NOTEBOOK(nb), GTK_WIDGET(current_page));
	if (i != -1)
		gtk_notebook_remove_page(GTK_NOTEBOOK(nb), i);
	
	set_sensitive_according_to_window(window);
}

void
gtranslator_file_quit(GtkAction *action,
		      GtranslatorWindow *window)
{	
	GtranslatorNotebook *nb;
	gint pages;
	gint table_pane_position;
	
	nb = gtranslator_window_get_notebook(window);
	pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(nb));
	
	while(pages > 0)
	{
		gtranslator_file_close(NULL, window);
		pages--;
	}

	
	gtk_widget_destroy(GTK_WIDGET(window));
	/*
	 * Get the EPaned's position offset.
	 */
	//table_pane_position=gtk_paned_get_position(GTK_PANED(gtranslator_window_get_paned(window)));
	/*
	 * Store the pane position in the preferences.
	 */
	//gtranslator_config_set_int("interface/table_pane_position", table_pane_position);
	
	//gtranslator_utils_save_geometry();

	/*
	 * "Flush" our runtime config string for the current filename.
	 */
	//gtranslator_config_set_string("runtime/filename", "--- No file ---");

	/*
	 * Free the used GtrTranslator structure.
	 */
	//gtranslator_translator_free(gtranslator_translator);

	/*
	 * Free any lungering stuff 'round -- free prefs.
	 */
//	gtranslator_preferences_free();

	/*
	 * Remove any lungering temp. file.
	 */
	//gtranslator_utils_remove_temp_files();

	/*
	 * Free our used runtime config structure.
	 */
	//gtranslator_runtime_config_free(gtranslator_runtime_config);
	
	/*
	 * Store the current date.
	 */
//	gtranslator_config_set_last_run_date();

	/*
	 * Shutdown the eventually (non-)initialized stuff from GnomeVFS.
	 */
	/*if(gnome_vfs_initialized())
	{
		gnome_vfs_shutdown();
	}*/

	/*
	 * Quit with the normal Gtk+ quit.
	 */
	//gtk_main_quit();
}

/*
 * A callback for Save
 */
void 
gtranslator_save_current_file_dialog(GtkWidget * widget,
				     GtranslatorWindow *window)
{
	GError *error;
	GtranslatorTab *current;
	GtranslatorPo *po;
	
	current = gtranslator_window_get_active_tab(window);
	po = gtranslator_tab_get_po(current);
	
	//g_return_if_fail(current_page->po->file_changed);

	if (!gtranslator_po_save_file(po, gtranslator_po_get_filename(po), &error)) {
		GtkWidget *dialog;
		g_assert(error != NULL);
		dialog = gtk_message_dialog_new(
			GTK_WINDOW(window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_WARNING,
			GTK_BUTTONS_OK,
			error->message);
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		g_clear_error(&error);
		return;
	}
	
	/* We have to change the state of the tab */
	gtranslator_tab_set_state(current, GTR_TAB_STATE_SAVED);
}

static gboolean
is_duplicated_uri (const GSList *uris, 
		   const gchar  *u)
{
	while (uris != NULL)
	{
		if (strcmp (u, (const gchar*)uris->data) == 0)
			return TRUE;
			
		uris = g_slist_next (uris);
	}
	
	return FALSE;
}

static void
load_file_list(GtranslatorWindow *window,
	       const GSList *uris)
{
	GSList        *uris_to_load = NULL;
	const GSList  *l;
	GError *error = NULL;
	gchar *path;
	
	g_return_if_fail ((uris != NULL) && (uris->data != NULL));
		
	/* Remove the uris corresponding to documents already open
	 * in "window" and remove duplicates from "uris" list */
	l = uris;
	while (uris != NULL)
	{
		if (!is_duplicated_uri (uris_to_load, uris->data))
		{
			
			/*We need to now if is already loaded in any tab*/
			
			/*tab = get_tab_from_uri (win_docs, (const gchar *)uris->data);
			if (tab != NULL)
			{
				if (uris == l)
				{
					gedit_window_set_active_tab (window, tab);
					jump_to = FALSE;

					if (line_pos > 0)
					{
						GeditDocument *doc;
						GeditView *view;

						doc = gedit_tab_get_document (tab);
						view = gedit_tab_get_view (tab);

						gedit_document_goto_line (doc, line_pos);
						gedit_view_scroll_to_cursor (view);
					}
				}

				++loaded_files;
			}
			else
			{*/
				uris_to_load = g_slist_prepend (uris_to_load, 
								uris->data);
			//}
		}

		uris = g_slist_next (uris);
	}
	
	if (uris_to_load == NULL)
		return;
	
	uris_to_load = g_slist_reverse (uris_to_load);
	l = uris_to_load;
	
	while (uris_to_load != NULL)
	{
		g_return_if_fail (uris_to_load->data != NULL);

		path = g_filename_from_uri((const gchar *)uris_to_load->data,
					   NULL, NULL);
		if(!gtranslator_open(path, window, &error))
			break;
		
		g_free(path);
		uris_to_load = g_slist_next (uris_to_load);
	}
	
	/*
	 * Now if there are any error we have to manage it
	 * and free the path
	 */
	if(error != NULL)
	{
		g_free(path);
		/*
		 * We have to show the error in a dialog
		 */
		gtk_message_dialog_new(GTK_WINDOW(window),
				       GTK_DIALOG_DESTROY_WITH_PARENT,
				       GTK_MESSAGE_ERROR,
				       GTK_BUTTONS_CLOSE,
				       error->message);
		g_error_free(error);
	}
	
	/* Free uris_to_load. Note that l points to the first element of uris_to_load */
	g_slist_free ((GSList *)l);
}


/**
 * gtranslator_actions_load_uris:
 *
 * Ignore non-existing URIs 
 */
void
gtranslator_actions_load_uris (GtranslatorWindow *window,
			       const GSList        *uris)
{	
	g_return_if_fail (GTR_IS_WINDOW (window));
	g_return_if_fail ((uris != NULL) && (uris->data != NULL));
	
	load_file_list (window, uris);
}