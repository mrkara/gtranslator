/*
 * Copyright (C) 2007  Ignacio Casal Quinteiro <nacho.resa@gmail.com>
 *               2008  Igalia
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Ignacio Casal Quinteiro <nacho.resa@gmail.com>
 *   Pablo Sanxiao <psanxiao@gmail.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gtr-actions.h"
#include "gtr-application.h"
#include "gtr-debug.h"
#include "gtr-dirs.h"
#include "gtr-header.h"
#include "gtr-msg.h"
#include "gtr-notebook.h"
#include "gtr-tab.h"
#include "gtr-plugins-engine.h"
#include "gtr-po.h"
#include "gtr-settings.h"
#include "gtr-statusbar.h"
#include "gtr-utils.h"
#include "gtr-window.h"
#include "gtr-window-activatable.h"
#include "gtr-profile-manager.h"
#include "gtr-status-combo-box.h"

#include "egg-toolbars-model.h"
#include "egg-toolbar-editor.h"
#include "egg-editable-toolbar.h"

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>
#include <libpeas/peas-extension-set.h>

#define GTR_STOCK_FUZZY_NEXT "gtranslator-fuzzy-next"
#define GTR_STOCK_FUZZY_PREV "gtranslator-fuzzy-prev"
#define GTR_STOCK_UNTRANS_NEXT "gtranslator-untranslated-next"
#define GTR_STOCK_UNTRANS_PREV "gtranslator-untranslated-prev"
#define GTR_STOCK_FUZZY_UNTRANS_NEXT "gtranslator-fuzzy-untranslated-next"
#define GTR_STOCK_FUZZY_UNTRANS_PREV "gtranslator-fuzzy-untranslated-prev"

#define PROFILE_DATA "GtrWidnowProfileData"

#define GTR_WINDOW_GET_PRIVATE(object)	(G_TYPE_INSTANCE_GET_PRIVATE ( \
					 (object),	\
					 GTR_TYPE_WINDOW,     \
					 GtrWindowPrivate))

static void gtr_window_cmd_edit_toolbar (GtkAction * action,
                                         GtrWindow * window);


G_DEFINE_TYPE (GtrWindow, gtr_window, GTK_TYPE_APPLICATION_WINDOW)

struct _GtrWindowPrivate
{
  GSettings *state_settings;

  GtkWidget *main_box;

  GtkWidget *menubar;
  GtkWidget *toolbar;
  GtkActionGroup *always_sensitive_action_group;
  GtkActionGroup *action_group;
  GtkActionGroup *documents_list_action_group;
  guint documents_list_menu_ui_id;

  GtkWidget *notebook;
  GtrTab *active_tab;

  GtkWidget *statusbar;

  GtkUIManager *ui_manager;
  GtkRecentManager *recent_manager;
  GtkWidget *recent_menu;

  gint width;
  gint height;
  GdkWindowState window_state;

  GtrProfileManager *prof_manager;
  GtkWidget *profile_combo;

  PeasExtensionSet *extensions;

  guint dispose_has_run : 1;
};

enum
{
  TARGET_URI_LIST = 100
};

static const GtkActionEntry always_sensitive_entries[] = {

  {"File", NULL, N_("_File")},
  {"Edit", NULL, N_("_Edit")},
  {"View", NULL, N_("_View")},
  {"Search", NULL, N_("_Search")},
  {"Go", NULL, N_("_Go")},
  {"Tools", NULL, N_("_Tools")},
  {"Documents", NULL, N_("_Documents")},

  /* File menu */
  {"FileOpen", "document-open", N_("_Open"), "<control>O",
   N_("Open a PO file"),
   G_CALLBACK (gtr_open_file_dialog)},
  {"FileRecentFiles", NULL, N_("_Recent Files"), NULL,
   NULL, NULL},

  /* Edit menu */
  {"EditToolbar", NULL, N_("T_oolbar"), NULL, NULL,
   G_CALLBACK (gtr_window_cmd_edit_toolbar)},
	  {"EditHeader", "document-properties", N_("_Header..."), NULL, NULL,
   G_CALLBACK (gtr_actions_edit_header)},
};

/* Normal items */
static const GtkActionEntry entries[] = {

  /* File menu */
  {"FileSave", "document-save", N_("_Save"), "<control>S",
   N_("Save the current file"),
   G_CALLBACK (gtr_save_current_file_dialog)},
  {"FileSaveAs", "document-save-as", N_("Save _As"), "<shift><control>S",
   N_("Save the current file with another name"),
   G_CALLBACK (gtr_save_file_as_dialog)},
  {"FileCloseWindow", "window-close", N_("_Close"), "<control>W",
   N_("Close the current file"),
   G_CALLBACK (gtr_file_close)},

  /* Edit menu */
  {"EditUndo", "edit-undo", N_("_Undo"), "<control>Z",
   N_("Undo last operation"),
   G_CALLBACK (gtr_actions_edit_undo)},
  {"EditRedo", "edit-redo", N_("_Redo"), "<shift><control>Z",
   N_("Redo last undone operation"),
   G_CALLBACK (gtr_actions_edit_redo)},
  {"EditCut", "edit-cut", N_("Cu_t"), "<control>X",
   N_("Cut the selected text"),
   G_CALLBACK (gtr_actions_edit_cut)},
  {"EditCopy", "edit-copy", N_("_Copy"), "<control>C",
   N_("Copy the selected text"),
   G_CALLBACK (gtr_actions_edit_copy)},
  {"EditPaste", "edit-paste", N_("_Paste"), "<control>V",
   N_("Paste the contents of the clipboard"),
   G_CALLBACK (gtr_actions_edit_paste)},
  {"EditClear", "edit-clear", N_("_Clear"), NULL,
   N_("Clear the selected translation"),
   G_CALLBACK (gtr_actions_edit_clear)},
  {"EditHeader", "document-properties", N_("_Header..."), NULL,
   N_("Edit the file header details"),
   G_CALLBACK (gtr_actions_edit_header)},
  {"EditMessage2Trans", NULL, N_("Copy _Message to Translation"),
   "<control>space",
   N_("Copy original message contents to the translation field"),
   G_CALLBACK (gtr_message_copy_to_translation)},
  {"EditFuzzy", NULL, N_("Toggle _Fuzzy Status"), "<control>U",
   N_("Toggle fuzzy status of a message"),
   G_CALLBACK (gtr_message_status_toggle_fuzzy)},
  {"EditTranslationMemory", NULL, N_("_Translation Memory"), NULL, NULL,
   NULL},


  /* View menu */
  {"ViewContext", NULL, N_("_Message Details"), "<control>J",
   N_("Show the Context panel"),
   G_CALLBACK (gtr_actions_view_context)},

  /* Go menu */
  {"GoPrevious", "go-previous", N_("_Previous Message"),
   "<alt>Left", N_("Move back one message"),
   G_CALLBACK (gtr_message_go_to_previous)},
  {"GoForward", "go-next", N_("_Next Message"),
   "<alt>Right", N_("Move forward one message"),
   G_CALLBACK (gtr_message_go_to_next)},
  {"GoJump", "go-jump", N_("_Go to Message..."),
   "<control>G", N_("Jumps to a specific message"),
   G_CALLBACK (gtr_message_jump)},
  {"GoFirst", "go-first", N_("_First Message"),
   "<alt>Home", N_("Go to the first message"),
   G_CALLBACK (gtr_message_go_to_first)},
  {"GoLast", "go-last", N_("_Last Message"),
   "<alt>End", N_("Go to the last message"),
   G_CALLBACK (gtr_message_go_to_last)},
  {"GoNextFuzzy", GTR_STOCK_FUZZY_NEXT, N_("Next Fuz_zy"),
   "<alt><control>Page_Down", N_("Go to the next fuzzy message"),
   G_CALLBACK (gtr_message_go_to_next_fuzzy)},
  {"GoPreviousFuzzy", GTR_STOCK_FUZZY_PREV, N_("Previous Fuzz_y"),
   "<alt><control>Page_Up", N_("Go to the previous fuzzy message"),
   G_CALLBACK (gtr_message_go_to_prev_fuzzy)},
  {"GoNextUntranslated", GTR_STOCK_UNTRANS_NEXT, N_("Next _Untranslated"),
   "<alt>Page_Down", N_("Go to the next untranslated message"),
   G_CALLBACK (gtr_message_go_to_next_untranslated)},
  {"GoPreviousUntranslated", GTR_STOCK_UNTRANS_PREV,
   N_("Previ_ous Untranslated"),
   "<alt>Page_Up", N_("Go to the previous untranslated message"),
   G_CALLBACK (gtr_message_go_to_prev_untranslated)},
  {"GoNextFuzzyUntranslated", GTR_STOCK_FUZZY_UNTRANS_NEXT,
   N_("Next Fu_zzy or Untranslated"),
   "<control><shift>Page_Down",
   N_("Go to the next fuzzy or untranslated message"),
   G_CALLBACK (gtr_message_go_to_next_fuzzy_or_untranslated)},
  {"GoPreviousFuzzyUntranslated", GTR_STOCK_FUZZY_UNTRANS_PREV,
   N_("Pre_vious Fuzzy or Untranslated"),
   "<control><shift>Page_Up",
   N_("Go to the previous fuzzy or untranslated message"),
   G_CALLBACK (gtr_message_go_to_prev_fuzzy_or_untranslated)},

  /* Search menu */
  {"SearchFind", "edit-find", N_("_Find"), "<control>F",
   N_("Search for text"),
   G_CALLBACK (_gtr_actions_search_find)},
  {"SearchReplace", "edit-find-replace", N_("Find and _Replace"), "<control>H",
   N_("Search for and replace text"),
   G_CALLBACK (_gtr_actions_search_replace)},

  /* Documents menu */
  {"FileSaveAll", "document-save", N_("_Save All"), "<shift><control>L",
   N_("Save all open files"),
   G_CALLBACK (_gtr_actions_file_save_all)},
  {"FileCloseAll", "window-close", N_("_Close All"), "<shift><control>W",
   N_("Close all open files"),
   G_CALLBACK (_gtr_actions_file_close_all)},
  {"DocumentsPreviousDocument", NULL, N_("_Previous Document"),
   "<alt><control>Page_Up",
   N_("Activate previous document"),
   G_CALLBACK (gtr_actions_documents_previous_document)},
  {"DocumentsNextDocument", NULL, N_("_Next Document"),
   "<alt><control>Page_Down",
   N_("Activate next document"),
   G_CALLBACK (gtr_actions_documents_next_document)}
};

static void          profile_combo_changed            (GtrStatusComboBox *combo,
                                                       GtkMenuItem       *item,
                                                       GtrWindow         *window);

static void
extension_update_state (PeasExtensionSet *extensions,
                        PeasPluginInfo   *info,
                        PeasExtension    *exten,
                        GtrWindow        *window)
{
  gtr_window_activatable_update_state (GTR_WINDOW_ACTIVATABLE (exten));
}

static void
extensions_update_state (GtrWindow *window)
{
  peas_extension_set_foreach (window->priv->extensions,
                              (PeasExtensionSetForeachFunc) extension_update_state,
                              window);
}

void
_gtr_window_set_sensitive_according_to_message (GtrWindow * window,
                                                GtrPo * po)
{
  GList *current;
  GtkAction *action;

  current = gtr_po_get_current_message (po);

  action = gtk_action_group_get_action (window->priv->action_group,
                                        "GoForward");
  gtk_action_set_sensitive (action, g_list_next (current) != NULL);

  action = gtk_action_group_get_action (window->priv->action_group,
                                        "GoPrevious");
  gtk_action_set_sensitive (action, g_list_previous (current) != NULL);

  action = gtk_action_group_get_action (window->priv->action_group,
                                        "GoFirst");
  gtk_action_set_sensitive (action, g_list_first (current) != current);

  action = gtk_action_group_get_action (window->priv->action_group, "GoLast");
  gtk_action_set_sensitive (action, g_list_last (current) != current);

  action = gtk_action_group_get_action (window->priv->action_group,
                                        "GoNextFuzzy");
  gtk_action_set_sensitive (action, gtr_po_get_next_fuzzy (po) != NULL);

  action = gtk_action_group_get_action (window->priv->action_group,
                                        "GoPreviousFuzzy");
  gtk_action_set_sensitive (action, gtr_po_get_prev_fuzzy (po) != NULL);

  action = gtk_action_group_get_action (window->priv->action_group,
                                        "GoNextUntranslated");
  gtk_action_set_sensitive (action, gtr_po_get_next_untrans (po) != NULL);

  action = gtk_action_group_get_action (window->priv->action_group,
                                        "GoPreviousUntranslated");
  gtk_action_set_sensitive (action, gtr_po_get_prev_untrans (po) != NULL);

  action = gtk_action_group_get_action (window->priv->action_group,
                                        "GoNextFuzzyUntranslated");
  gtk_action_set_sensitive (action,
                            gtr_po_get_next_fuzzy_or_untrans (po) != NULL);

  action = gtk_action_group_get_action (window->priv->action_group,
                                        "GoPreviousFuzzyUntranslated");
  gtk_action_set_sensitive (action,
                            gtr_po_get_prev_fuzzy_or_untrans (po) != NULL);
}

static void
set_sensitive_according_to_tab (GtrWindow * window, GtrTab * tab)
{
  GtrNotebook *notebook;
  GtrView *view;
  GtrPo *po;
  GtkSourceBuffer *buf;
  GtkAction *action;
  GtrPoState state;
  gint pages;
  gint current_page;

  notebook = gtr_window_get_notebook (window);
  pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->priv->notebook));
  view = gtr_tab_get_active_view (tab);
  po = gtr_tab_get_po (tab);
  buf = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));

  if (gtk_action_group_get_sensitive (window->priv->action_group) == FALSE)
    gtk_action_group_set_sensitive (window->priv->action_group, TRUE);

  /*File */
  state = gtr_po_get_state (po);
  action = gtk_action_group_get_action (window->priv->action_group,
                                        "FileSave");
  gtk_action_set_sensitive (action, state == GTR_PO_STATE_MODIFIED);

  /*Edit */
  action = gtk_action_group_get_action (window->priv->action_group,
                                        "EditUndo");
  gtk_action_set_sensitive (action, gtk_source_buffer_can_undo (buf));

  action = gtk_action_group_get_action (window->priv->action_group,
                                        "EditRedo");
  gtk_action_set_sensitive (action, gtk_source_buffer_can_redo (buf));

  action = gtk_action_group_get_action (window->priv->action_group,
                                        "EditCut");
  gtk_action_set_sensitive (action,
                            gtk_text_buffer_get_has_selection (GTK_TEXT_BUFFER
                                                               (buf)));

  action = gtk_action_group_get_action (window->priv->action_group,
                                        "EditCopy");
  gtk_action_set_sensitive (action,
                            gtk_text_buffer_get_has_selection (GTK_TEXT_BUFFER
                                                               (buf)));

  /*Go */
  current_page =
    gtk_notebook_page_num (GTK_NOTEBOOK (notebook), GTK_WIDGET (tab));
  g_return_if_fail (current_page >= 0);

  action = gtk_action_group_get_action (window->priv->action_group,
                                        "DocumentsPreviousDocument");
  gtk_action_set_sensitive (action, current_page != 0);

  action = gtk_action_group_get_action (window->priv->action_group,
                                        "DocumentsNextDocument");
  gtk_action_set_sensitive (action, current_page < pages - 1);

  _gtr_window_set_sensitive_according_to_message (window, po);

  extensions_update_state (window);
}

static void
set_sensitive_according_to_window (GtrWindow * window)
{
  gint pages;

  pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->priv->notebook));

  gtk_action_group_set_sensitive (window->priv->action_group, pages > 0);
}

/*
 * gtr_window_update_statusbar_message_count:
 *
 * This func is used to show the global status of the message list
 * in the statusbar widget.
 */
static void
gtr_window_update_statusbar_message_count (GtrTab * tab,
                                           GtrMsg * message,
                                           GtrWindow * window)
{
  GtrPo *po;
  gchar *msg;
  const gchar *status;
  gchar *status_msg;
  gchar *current;
  gchar *total;
  gchar *translated_msg;
  gchar *fuzzy_msg;
  gchar *untranslated_msg;
  gint pos, message_count, translated, fuzzy, untranslated;

  g_return_if_fail (GTR_IS_MSG (message));

  po = gtr_tab_get_po (tab);

  message_count = gtr_po_get_messages_count (po);
  pos = gtr_po_get_message_position (po);
  translated = gtr_po_get_translated_count (po);
  fuzzy = gtr_po_get_fuzzy_count (po);
  untranslated = gtr_po_get_untranslated_count (po);
  status = NULL;

  switch (gtr_msg_get_status (message))
    {
    case GTR_MSG_STATUS_UNTRANSLATED:
      status = _("Untranslated");
      break;
    case GTR_MSG_STATUS_TRANSLATED:
      status = _("Translated");
      break;
    case GTR_MSG_STATUS_FUZZY:
      status = _("Fuzzy");
      break;
    default:
      break;
    }

  status_msg = g_strdup_printf ("(%s)", status);
  current = g_strdup_printf (_("Current: %d"), pos);
  total = g_strdup_printf (_("Total: %d"), message_count);
  translated_msg = g_strdup_printf (ngettext ("%d translated",
                                              "%d translated",
                                              translated), translated);
  fuzzy_msg = g_strdup_printf (ngettext ("%d fuzzy",
                                         "%d fuzzy", fuzzy), fuzzy);
  untranslated_msg = g_strdup_printf (ngettext ("%d untranslated",
                                                "%d untranslated",
                                                untranslated), untranslated);

  msg = g_strconcat ("    ", current, " ", status_msg, "    ", total,
                     " (", translated_msg, ", ", fuzzy_msg, ", ",
                     untranslated_msg, ")", NULL);

  gtr_statusbar_pop (GTR_STATUSBAR (window->priv->statusbar), 0);

  gtr_statusbar_push (GTR_STATUSBAR (window->priv->statusbar), 0, msg);

  g_free (msg);
  g_free (current);
  g_free (status_msg);
  g_free (total);
  g_free (translated_msg);
  g_free (fuzzy_msg);
  g_free (untranslated_msg);

  /* We have to update the progress bar too */
  gtr_statusbar_update_progress_bar (GTR_STATUSBAR
                                     (window->priv->statusbar),
                                     (gdouble) translated,
                                     (gdouble) message_count);
}

static void
documents_list_menu_activate (GtkToggleAction * action, GtrWindow * window)
{
  gint n;

  if (gtk_toggle_action_get_active (action) == FALSE)
    return;

  n = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (action));
  gtk_notebook_set_current_page (GTK_NOTEBOOK (window->priv->notebook), n);
}

static gchar *
get_menu_tip_for_tab (GtrTab * tab)
{
  GtrPo *doc;
  gchar *uri;
  gchar *tip;
  GFile *file;

  doc = gtr_tab_get_po (tab);
  file = gtr_po_get_location (doc);

  uri = g_file_get_path (file);
  g_object_unref (file);

  /* Translators: %s is a URI */
  tip = g_strdup_printf (_("Activate '%s'"), uri);
  g_free (uri);

  return tip;
}

static void
update_documents_list_menu (GtrWindow * window)
{
  GtrWindowPrivate *p = window->priv;
  GList *actions, *l;
  gint n, i;
  guint id;
  GSList *group = NULL;

  g_return_if_fail (p->documents_list_action_group != NULL);

  if (p->documents_list_menu_ui_id != 0)
    gtk_ui_manager_remove_ui (p->ui_manager, p->documents_list_menu_ui_id);

  actions = gtk_action_group_list_actions (p->documents_list_action_group);
  for (l = actions; l != NULL; l = l->next)
    {
      g_signal_handlers_disconnect_by_func (GTK_ACTION (l->data),
                                            G_CALLBACK
                                            (documents_list_menu_activate),
                                            window);
      gtk_action_group_remove_action (p->documents_list_action_group,
                                      GTK_ACTION (l->data));
    }
  g_list_free (actions);

  n = gtk_notebook_get_n_pages (GTK_NOTEBOOK (p->notebook));

  id = (n > 0) ? gtk_ui_manager_new_merge_id (p->ui_manager) : 0;

  for (i = 0; i < n; i++)
    {
      GtkWidget *tab;
      GtkRadioAction *action;
      gchar *action_name;
      gchar *tab_name;
      gchar *name;
      gchar *tip;
      gchar *accel;

      tab = gtk_notebook_get_nth_page (GTK_NOTEBOOK (p->notebook), i);

      /* NOTE: the action is associated to the position of the tab in
       * the notebook not to the tab itself! This is needed to work
       * around the gtk+ bug #170727: gtk leaves around the accels
       * of the action. Since the accel depends on the tab position
       * the problem is worked around, action with the same name always
       * get the same accel.
       */
      action_name = g_strdup_printf ("Tab_%d", i);
      tab_name = _gtr_tab_get_name (GTR_TAB (tab));
      name = gtr_utils_escape_underscores (tab_name, -1);
      tip = get_menu_tip_for_tab (GTR_TAB (tab));

      /* alt + 1, 2, 3... 0 to switch to the first ten tabs */
      accel = (i < 10) ? g_strdup_printf ("<alt>%d", (i + 1) % 10) : NULL;

      action = gtk_radio_action_new (action_name, name, tip, NULL, i);

      if (group != NULL)
        gtk_radio_action_set_group (action, group);

      /* note that group changes each time we add an action, so it must be updated */
      group = gtk_radio_action_get_group (action);

      gtk_action_group_add_action_with_accel (p->documents_list_action_group,
                                              GTK_ACTION (action), accel);

      g_signal_connect (action,
                        "activate",
                        G_CALLBACK (documents_list_menu_activate), window);

      gtk_ui_manager_add_ui (p->ui_manager,
                             id,
                             "/MainMenu/DocumentsMenu/DocumentsListPlaceholder",
                             action_name, action_name,
                             GTK_UI_MANAGER_MENUITEM, FALSE);

      if (GTR_TAB (tab) == p->active_tab)
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

      g_object_unref (action);

      g_free (action_name);
      g_free (tab_name);
      g_free (name);
      g_free (tip);
      g_free (accel);
    }

  p->documents_list_menu_ui_id = id;
}

static GtrWindow *
get_drop_window (GtkWidget * widget)
{
  GtkWidget *target_window;

  target_window = gtk_widget_get_toplevel (widget);
  g_return_val_if_fail (GTR_IS_WINDOW (target_window), NULL);

  return GTR_WINDOW (target_window);
}

/* Handle drops on the GtrWindow */
static void
drag_data_received_cb (GtkWidget * widget,
                       GdkDragContext * context,
                       gint x,
                       gint y,
                       GtkSelectionData * selection_data,
                       guint info, guint time, gpointer data)
{
  GtrWindow *window;
  GSList *locations;

  window = get_drop_window (widget);

  if (window == NULL)
    return;

  if (info == TARGET_URI_LIST)
    {
      locations = gtr_utils_drop_get_locations (selection_data);
      gtr_actions_load_locations (window, locations);

      g_slist_free_full (locations, g_object_unref);
    }
}

static void
update_overwrite_mode_statusbar (GtkTextView * view, GtrWindow * window)
{
  if (view != GTK_TEXT_VIEW (gtr_window_get_active_view (window)))
    return;

  /* Note that we have to use !gtk_text_view_get_overwrite since we
     are in the in the signal handler of "toggle overwrite" that is
     G_SIGNAL_RUN_LAST
   */
  gtr_statusbar_set_overwrite (GTR_STATUSBAR
                               (window->priv->statusbar),
                               !gtk_text_view_get_overwrite (view));
}

static void
set_window_title (GtrWindow * window, gboolean with_path)
{
  GtrPo *po;
  GtrPoState state;
  GtrTab *active_tab;
  GFile *file;
  gchar *title;

  if (with_path)
    {
      gchar *path;

      active_tab = gtr_window_get_active_tab (window);
      po = gtr_tab_get_po (active_tab);
      state = gtr_po_get_state (gtr_tab_get_po (active_tab));
      po = gtr_tab_get_po (active_tab);
      file = gtr_po_get_location (po);
      path = g_file_get_path (file);

      if (state == GTR_PO_STATE_MODIFIED)
        /* Translators: this is the title of the window with a modified document */
        title = g_strdup_printf (_("*%s - gtranslator"), path);
      else
        /* Translators: this is the title of the window with a document opened */
        title = g_strdup_printf (_("%s - gtranslator"), path);

      g_free (path);
      g_object_unref (file);
    }
  else
    title = g_strdup (_("gtranslator"));

  gtk_window_set_title (GTK_WINDOW (window), title);
  g_free (title);
}

static void
notebook_switch_page (GtkNotebook * nb,
                      GtkWidget * page,
                      gint page_num, GtrWindow * window)
{
  GtrTab *tab;
  GList *msg;
  GtrView *view;
  GtrPo *po;
  GtrHeader *header;
  GtrProfile *profile;
  gint n_pages;
  GtkAction *action;
  gchar *action_name;
  GList *profile_items, *l;

  tab = GTR_TAB (gtk_notebook_get_nth_page (nb, page_num));
  if (tab == window->priv->active_tab)
    return;

  /*
   * Set the window title
   */
  n_pages = gtk_notebook_get_n_pages (nb);
  if (n_pages == 1)
    set_window_title (window, TRUE);
  else
    set_window_title (window, FALSE);

  window->priv->active_tab = tab;
  view = gtr_tab_get_active_view (tab);

  set_sensitive_according_to_tab (window, tab);


  /* sync the statusbar */
  gtr_statusbar_set_overwrite (GTR_STATUSBAR
                               (window->priv->statusbar),
                               gtk_text_view_get_overwrite
                               (GTK_TEXT_VIEW (view)));

  po = gtr_tab_get_po (tab);
  msg = gtr_po_get_current_message (po);
  gtr_window_update_statusbar_message_count (tab, msg->data, window);

  header = gtr_po_get_header (po);
  profile = gtr_header_get_profile (header);

  if (profile == NULL)
    profile = gtr_profile_manager_get_active_profile (window->priv->prof_manager);

  profile_items = gtr_status_combo_box_get_items (GTR_STATUS_COMBO_BOX (window->priv->profile_combo));

  for (l = profile_items; l != NULL; l = g_list_next (l))
    {
      GtrProfile *item_profile;

      item_profile = GTR_PROFILE (g_object_get_data (G_OBJECT (l->data),
                                                     PROFILE_DATA));

      if (item_profile == profile)
        {
          g_signal_handlers_block_by_func (window->priv->profile_combo,
                                           profile_combo_changed,
                                           window);
          gtr_status_combo_box_set_item (GTR_STATUS_COMBO_BOX (window->priv->profile_combo),
                                         GTK_MENU_ITEM (l->data));
          g_signal_handlers_unblock_by_func (window->priv->profile_combo,
                                             profile_combo_changed,
                                             window);
        }
    }

  /* activate the right item in the documents menu */
  action_name = g_strdup_printf ("Tab_%d", page_num);
  action =
    gtk_action_group_get_action (window->priv->documents_list_action_group,
                                 action_name);
  g_free (action_name);

  /* sometimes the action doesn't exist yet, and the proper action
   * is set active during the documents list menu creation
   * CHECK: would it be nicer if active_tab was a property and we monitored the notify signal?
   */
  if (action != NULL)
    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

  extensions_update_state (window);
}

static void
notebook_page_removed (GtkNotebook * notebook,
                       GtkWidget * child, guint page_num, GtrWindow * window)
{
  gint n_pages;

  /* Set the window title */
  n_pages = gtk_notebook_get_n_pages (notebook);
  if (n_pages == 1)
    set_window_title (window, TRUE);
  else
    set_window_title (window, FALSE);

  /* Hide the profile combo */
  if (n_pages == 0)
    gtk_widget_hide (window->priv->profile_combo);

  update_documents_list_menu (window);

  peas_engine_garbage_collect (PEAS_ENGINE (gtr_plugins_engine_get_default ()));
}

static void
notebook_tab_close_request (GtrNotebook * notebook,
                            GtrTab * tab, GtrWindow * window)
{
  /* Note: we are destroying the tab before the default handler
   * seems to be ok, but we need to keep an eye on this. */
  gtr_close_tab (tab, window);

  extensions_update_state (window);
}

static void
can_undo (GtkSourceBuffer * doc, GParamSpec * pspec, GtrWindow * window)
{
  GtkAction *action;
  gboolean sensitive;
  GtrView *view;
  GtkSourceBuffer *buf;

  view = gtr_window_get_active_view (window);
  buf = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));

  sensitive = gtk_source_buffer_can_undo (buf);

  if (doc != buf)
    return;

  action = gtk_action_group_get_action (window->priv->action_group,
                                        "EditUndo");
  gtk_action_set_sensitive (action, sensitive);
}

static void
can_redo (GtkSourceBuffer * doc, GParamSpec * spec, GtrWindow * window)
{
  GtkAction *action;
  gboolean sensitive;
  GtrView *view;
  GtkSourceBuffer *buf;

  view = gtr_window_get_active_view (window);
  buf = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));

  sensitive = gtk_source_buffer_can_redo (buf);

  if (doc != buf)
    return;

  action = gtk_action_group_get_action (window->priv->action_group,
                                        "EditRedo");
  gtk_action_set_sensitive (action, sensitive);
}

static void
sync_state (GtrPo * po, GParamSpec * pspec, GtrWindow * window)
{
  int n_pages = 0;

  set_sensitive_according_to_tab (window, gtr_tab_get_from_document (po));
  n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->priv->notebook));

  if (n_pages == 1)
    set_window_title (window, TRUE);
}

static void
showed_message_cb (GtrTab * tab, GtrMsg * msg, GtrWindow * window)
{
  g_return_if_fail (GTR_IS_TAB (tab));

  gtr_window_update_statusbar_message_count (tab, msg, window);

  _gtr_window_set_sensitive_according_to_message (window, gtr_tab_get_po (tab));
}

static void
selection_changed_cb (GtrTab * tab, GtrWindow * window)
{
  set_sensitive_according_to_tab (window, tab);
}

static void
notebook_tab_added (GtkNotebook * notebook,
                    GtkWidget * child, guint page_num, GtrWindow * window)
{
  GList *views;
  GtrTab *tab = GTR_TAB (child);
  GtkTextBuffer *buffer;
  gint n_pages;

  g_return_if_fail (GTR_IS_TAB (tab));

  /* Set the window title */
  n_pages = gtk_notebook_get_n_pages (notebook);
  if (n_pages == 1)
    set_window_title (window, TRUE);
  else
    set_window_title (window, FALSE);

  /* Show the profile combo */
  gtk_widget_show (window->priv->profile_combo);

  views = gtr_tab_get_all_views (tab, FALSE, TRUE);

  while (views)
    {
      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (views->data));


      g_signal_connect (GTK_SOURCE_BUFFER (buffer),
                        "notify::can-undo", G_CALLBACK (can_undo), window);

      g_signal_connect (GTK_SOURCE_BUFFER (buffer),
                        "notify::can-redo", G_CALLBACK (can_redo), window);

      g_signal_connect (views->data,
                        "toggle_overwrite",
                        G_CALLBACK (update_overwrite_mode_statusbar), window);

      views = views->next;
    }

  g_signal_connect_after (child,
                          "message_changed",
                          G_CALLBACK
                          (gtr_window_update_statusbar_message_count),
                          window);
  g_signal_connect_after (child, "showed_message",
                          G_CALLBACK (showed_message_cb), window);
  g_signal_connect (child, "selection-changed",
                    G_CALLBACK (selection_changed_cb), window);

  g_signal_connect (gtr_tab_get_po (tab),
                    "notify::state", G_CALLBACK (sync_state), window);

  update_documents_list_menu (window);

  extensions_update_state (window);
}

void
_gtr_recent_add (GtrWindow * window, GFile * location, gchar * project_id)
{
  GtkRecentData *recent_data;
  gchar *uri;
  gchar *path;
  gchar *display_name;

  uri = g_file_get_uri (location);
  path = g_file_get_path (location);
  display_name = gtr_utils_reduce_path ((const gchar *) path);

  recent_data = g_slice_new (GtkRecentData);

  recent_data->display_name = display_name;
  recent_data->description = NULL;
  recent_data->mime_type = "text/x-gettext-translation";
  recent_data->app_name = (gchar *) g_get_application_name ();
  recent_data->app_exec = g_strjoin (" ", g_get_prgname (), "%u", NULL);
  recent_data->groups = NULL;
  recent_data->is_private = FALSE;

  if (!gtk_recent_manager_add_full (window->priv->recent_manager,
                                    uri, recent_data))
    {
      g_warning ("Unable to add '%s' to the list of recently used documents",
                 uri);
    }

  g_free (uri);
  g_free (path);
  g_free (display_name);
  g_free (recent_data->app_exec);
  g_slice_free (GtkRecentData, recent_data);
}

/*
static void
gtr_recent_remove (GtrWindow * window, const gchar * path)
{
  gchar *uri;
  GError *error = NULL;

  uri = g_filename_to_uri (path, NULL, &error);
  if (error)
    {
      g_warning ("Could not convert uri \"%s\" to a local path: %s",
                 uri, error->message);
      g_error_free (error);
      return;
    }

  gtk_recent_manager_remove_item (window->priv->recent_manager, uri, &error);
  if (error)
    {
      g_warning ("Could not remove recent-files uri \"%s\": %s",
                 uri, error->message);
      g_error_free (error);
    }

  g_free (uri);
}
*/

static void
gtr_recent_chooser_item_activated_cb (GtkRecentChooser * chooser,
                                      GtrWindow * window)
{
  gchar *uri;
  GSList *list = NULL;
  GFile *location;

  uri = gtk_recent_chooser_get_current_uri (chooser);
  location = g_file_new_for_uri (uri);
  g_free (uri);

  list = g_slist_prepend (list, location);

  gtr_actions_load_locations (window, list);
  g_slist_free_full (list, g_object_unref);
}

static GtkWidget *
create_recent_chooser_menu (GtrWindow * window, GtkRecentManager * manager)
{
  GtkWidget *recent_menu;
  GtkRecentFilter *filter;

  recent_menu = gtk_recent_chooser_menu_new_for_manager (manager);

  gtk_recent_chooser_set_local_only (GTK_RECENT_CHOOSER (recent_menu), TRUE);
  gtk_recent_chooser_set_show_icons (GTK_RECENT_CHOOSER (recent_menu), FALSE);
  gtk_recent_chooser_set_sort_type (GTK_RECENT_CHOOSER (recent_menu),
                                    GTK_RECENT_SORT_MRU);
  gtk_recent_chooser_menu_set_show_numbers (GTK_RECENT_CHOOSER_MENU
                                            (recent_menu), TRUE);
  gtk_recent_chooser_set_show_tips (GTK_RECENT_CHOOSER (recent_menu), TRUE);

  filter = gtk_recent_filter_new ();
  gtk_recent_filter_add_application (filter, g_get_application_name ());
  gtk_recent_chooser_set_filter (GTK_RECENT_CHOOSER (recent_menu), filter);

  return recent_menu;
}

static void
gtr_window_cmd_edit_toolbar_cb (GtkDialog * dialog,
                                gint response, gpointer data)
{
  GtrWindow *window = GTR_WINDOW (data);
  egg_editable_toolbar_set_edit_mode
    (EGG_EDITABLE_TOOLBAR (window->priv->toolbar), FALSE);
  _gtr_application_save_toolbars_model (GTR_APP);
  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
gtr_window_cmd_edit_toolbar (GtkAction * action, GtrWindow * window)
{
  GtkWidget *dialog;
  GtkWidget *editor;
  GtkWidget *content_area;

  dialog = gtk_dialog_new_with_buttons (_("Toolbar Editor"),
                                        GTK_WINDOW (window),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        _("_Close"),
                                        GTK_RESPONSE_CLOSE, NULL);
  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);
  gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)), 5);
  gtk_box_set_spacing (GTK_BOX (content_area), 2);
  gtk_window_set_default_size (GTK_WINDOW (dialog), 500, 400);

  editor = egg_toolbar_editor_new (window->priv->ui_manager,
                                   EGG_TOOLBARS_MODEL
                                   (_gtr_application_get_toolbars_model
                                    (GTR_APP)));
  gtk_container_set_border_width (GTK_CONTAINER (editor), 5);
  gtk_box_set_spacing (GTK_BOX (EGG_TOOLBAR_EDITOR (editor)), 5);

  gtk_box_pack_start (GTK_BOX (content_area), editor, TRUE, TRUE, 0);

  egg_editable_toolbar_set_edit_mode
    (EGG_EDITABLE_TOOLBAR (window->priv->toolbar), TRUE);

  g_signal_connect (dialog, "response",
                    G_CALLBACK (gtr_window_cmd_edit_toolbar_cb), window);
  gtk_widget_show_all (dialog);
}

static void
menu_item_select_cb (GtkMenuItem * proxy, GtrWindow * window)
{
  GtkAction *action;
  const gchar *message;

  action = gtk_activatable_get_related_action (GTK_ACTIVATABLE (proxy));
  g_return_if_fail (action != NULL);

  message = gtk_action_get_tooltip (action);
  if (message)
    {
      gtr_statusbar_push_default (GTR_STATUSBAR (window->priv->statusbar),
                                  message);
    }
}

static void
menu_item_deselect_cb (GtkMenuItem * proxy, GtrWindow * window)
{
  gtr_statusbar_pop_default (GTR_STATUSBAR (window->priv->statusbar));
}

static void
connect_proxy_cb (GtkUIManager * manager,
                  GtkAction * action, GtkWidget * proxy, GtrWindow * window)
{
  if (GTK_IS_MENU_ITEM (proxy))
    {
      g_signal_connect (proxy, "select",
                        G_CALLBACK (menu_item_select_cb), window);
      g_signal_connect (proxy, "deselect",
                        G_CALLBACK (menu_item_deselect_cb), window);
    }
}

static void
disconnect_proxy_cb (GtkUIManager * manager,
                     GtkAction * action,
                     GtkWidget * proxy, GtrWindow * window)
{
  if (GTK_IS_MENU_ITEM (proxy))
    {
      g_signal_handlers_disconnect_by_func
        (proxy, G_CALLBACK (menu_item_select_cb), window);
      g_signal_handlers_disconnect_by_func
        (proxy, G_CALLBACK (menu_item_deselect_cb), window);
    }
}

static void
profile_combo_changed (GtrStatusComboBox *combo,
                       GtkMenuItem       *item,
                       GtrWindow         *window)
{
  GtrTab *tab;
  GtrPo *po;
  GtrHeader *header;
  GtrProfile *profile;

  tab = gtr_window_get_active_tab (window);

  if (tab == NULL)
    return;

  po = gtr_tab_get_po (tab);
  header = gtr_po_get_header (po);

  profile = GTR_PROFILE (g_object_get_data (G_OBJECT (item), PROFILE_DATA));

  gtr_header_set_profile (header, profile);
}

static void
fill_profile_combo (GtrWindow *window)
{
  GSList *profiles, *l;
  GtkWidget *menu_item;
  const gchar *name;

  profiles = gtr_profile_manager_get_profiles (window->priv->prof_manager);

  for (l = profiles; l != NULL; l = g_slist_next (l))
    {
      GtrProfile *profile = GTR_PROFILE (l->data);

      name = gtr_profile_get_name (profile);
      menu_item = gtk_menu_item_new_with_label (name);
      gtk_widget_show (menu_item);

      g_object_set_data (G_OBJECT (menu_item), PROFILE_DATA, profile);
      gtr_status_combo_box_add_item (GTR_STATUS_COMBO_BOX (window->priv->profile_combo),
                                     GTK_MENU_ITEM (menu_item),
                                     name);
    }

  if (profiles == NULL)
    {
      name = _("No profile");

      menu_item = gtk_menu_item_new_with_label (name);

      g_object_set_data (G_OBJECT (menu_item), PROFILE_DATA, NULL);
      gtr_status_combo_box_add_item (GTR_STATUS_COMBO_BOX (window->priv->profile_combo),
                                     GTK_MENU_ITEM (menu_item),
                                     name);
    }
}

static void
create_statusbar (GtrWindow *window)
{
  GtkWidget *hbox;

  /* hbox */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (window->priv->main_box), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  window->priv->statusbar = gtr_statusbar_new ();

  gtk_box_pack_end (GTK_BOX (hbox), window->priv->statusbar, TRUE, TRUE, 0);

  gtk_widget_show (window->priv->statusbar);

  window->priv->profile_combo = gtr_status_combo_box_new (_("Profile"));
  gtk_widget_set_tooltip_text (window->priv->profile_combo,
                               _("Profile for the active document"));
  gtk_box_pack_start (GTK_BOX (window->priv->statusbar),
                      window->priv->profile_combo, FALSE, TRUE, 0);

  g_signal_connect (window->priv->profile_combo, "changed",
                    G_CALLBACK (profile_combo_changed), window);

  fill_profile_combo (window);
}

static void
on_active_profile_changed (GtrProfileManager *manager,
                           GtrProfile        *profile,
                           GtrWindow         *window)
{
  GList *items, *l;
  GtrTab *tab;
  GtrPo *po;
  GtrHeader *header;

  tab = gtr_window_get_active_tab (window);

  if (tab == NULL)
    return;

  po = gtr_tab_get_po (tab);
  header = gtr_po_get_header (po);

  items = gtr_status_combo_box_get_items (GTR_STATUS_COMBO_BOX (window->priv->profile_combo));

  for (l = items; l != NULL; l = g_list_next (l))
    {
      GtkMenuItem *menu_item;
      GtrProfile *item_profile;

      menu_item = GTK_MENU_ITEM (l->data);

      item_profile = GTR_PROFILE (g_object_get_data (G_OBJECT (menu_item),
                                  PROFILE_DATA));

      if (item_profile == profile && gtr_header_get_profile (header) == NULL)
        {
          g_signal_handlers_block_by_func (window->priv->profile_combo,
                                           profile_combo_changed, window);
          gtr_status_combo_box_set_item (GTR_STATUS_COMBO_BOX (window->priv->profile_combo),
                                         menu_item);
          g_signal_handlers_unblock_by_func (window->priv->profile_combo,
                                             profile_combo_changed, window);
        }
    }
}

static void
on_profile_added (GtrProfileManager *manager,
                  GtrProfile        *profile,
                  GtrWindow         *window)
{
  GtkMenuItem *menu_item;
  GList *items;

  /* check that the item is not a "No profile" item */
  items = gtr_status_combo_box_get_items (GTR_STATUS_COMBO_BOX (window->priv->profile_combo));

  if (items->next == NULL &&
      (g_object_get_data (G_OBJECT (items->data), PROFILE_DATA) == NULL))
    {
      menu_item = GTK_MENU_ITEM (items->data);

      gtk_menu_item_set_label (menu_item,
                               gtr_profile_get_name (profile));

      g_object_set_data (G_OBJECT (menu_item), PROFILE_DATA, profile);
    }
  else
    {
      const gchar *name;

      name = gtr_profile_get_name (profile);
      menu_item = GTK_MENU_ITEM (gtk_menu_item_new_with_label (name));
      gtk_widget_show (GTK_WIDGET (menu_item));

      g_object_set_data (G_OBJECT (menu_item), PROFILE_DATA, profile);
      gtr_status_combo_box_add_item (GTR_STATUS_COMBO_BOX (window->priv->profile_combo),
                                     menu_item, name);
    }
}

static void
on_profile_removed (GtrProfileManager *manager,
                    GtrProfile        *profile,
                    GtrWindow         *window)
{
  GList *items, *l;

  items = gtr_status_combo_box_get_items (GTR_STATUS_COMBO_BOX (window->priv->profile_combo));

  for (l = items; l != NULL; l = g_list_next (l))
    {
      GtrProfile *prof;

      prof = GTR_PROFILE (g_object_get_data (G_OBJECT (l->data), PROFILE_DATA));
      if (prof == profile)
        gtr_status_combo_box_remove_item (GTR_STATUS_COMBO_BOX (window->priv->profile_combo),
                                          GTK_MENU_ITEM (l->data));
    }
}

static void
on_profile_modified (GtrProfileManager *manager,
                     GtrProfile        *old_profile,
                     GtrProfile        *new_profile,
                     GtrWindow         *window)
{
  GList *items, *l;

  items = gtr_status_combo_box_get_items (GTR_STATUS_COMBO_BOX (window->priv->profile_combo));

  for (l = items; l != NULL; l = g_list_next (l))
    {
      GtrProfile *profile;

      profile = GTR_PROFILE (g_object_get_data (G_OBJECT (l->data), PROFILE_DATA));
      if (profile == old_profile)
        {
          gtk_menu_item_set_label (GTK_MENU_ITEM (l->data),
                                   gtr_profile_get_name (new_profile));
          g_object_set_data (G_OBJECT (l->data), PROFILE_DATA, new_profile);

          return;
        }
    }
}

static void
extension_added (PeasExtensionSet *extensions,
                 PeasPluginInfo   *info,
                 PeasExtension    *exten,
                 GtrWindow        *window)
{
  gtr_window_activatable_activate (GTR_WINDOW_ACTIVATABLE (exten));
}

static void
extension_removed (PeasExtensionSet *extensions,
                   PeasPluginInfo   *info,
                   PeasExtension    *exten,
                   GtrWindow        *window)
{
  gtr_window_activatable_deactivate (GTR_WINDOW_ACTIVATABLE (exten));

  /* Ensure update of ui manager, because we suspect it does something
   * with expected static strings in the type module (when unloaded the
   * strings don't exist anymore, and ui manager updates in an idle
   * func) */
  gtk_ui_manager_ensure_update (window->priv->ui_manager);
}

static void
gtr_window_init (GtrWindow * window)
{
  GtkTargetList *tl;
  GtkWidget *widget;
  GError *error = NULL;
  GtkActionGroup *action_group;
  GtrWindowPrivate *priv;

  window->priv = GTR_WINDOW_GET_PRIVATE (window);
  priv = window->priv;

  priv->state_settings = g_settings_new ("org.gnome.gtranslator.state.window");

  /* Profile manager */
  priv->prof_manager = gtr_profile_manager_get_default ();

  g_signal_connect (priv->prof_manager, "active-profile-changed",
                    G_CALLBACK (on_active_profile_changed), window);
  g_signal_connect (priv->prof_manager, "profile-added",
                    G_CALLBACK (on_profile_added), window);
  g_signal_connect (priv->prof_manager, "profile-removed",
                    G_CALLBACK (on_profile_removed), window);
  g_signal_connect (priv->prof_manager, "profile-modified",
                    G_CALLBACK (on_profile_modified), window);

  /* Main box */
  priv->main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (window), priv->main_box);
  gtk_widget_show (priv->main_box);

  /* Menus */
  priv->ui_manager = gtk_ui_manager_new ();

  gtk_window_add_accel_group (GTK_WINDOW (window),
                              gtk_ui_manager_get_accel_group
                              (priv->ui_manager));

  priv->always_sensitive_action_group =
    gtk_action_group_new ("AlwaysSensitiveMenuActions");
  gtk_action_group_set_translation_domain
    (priv->always_sensitive_action_group, NULL);
  gtk_action_group_add_actions (priv->always_sensitive_action_group,
                                always_sensitive_entries,
                                G_N_ELEMENTS (always_sensitive_entries),
                                window);

  gtk_ui_manager_insert_action_group (priv->ui_manager,
                                      priv->always_sensitive_action_group, 0);

  priv->action_group = gtk_action_group_new ("MenuActions");
  gtk_action_group_set_translation_domain (priv->action_group, NULL);
  gtk_action_group_add_actions (priv->action_group, entries,
                                G_N_ELEMENTS (entries), window);

  gtk_ui_manager_insert_action_group (priv->ui_manager,
                                      priv->action_group, 0);

  /* list of open documents menu */
  action_group = gtk_action_group_new ("DocumentsListActions");
  gtk_action_group_set_translation_domain (action_group, NULL);
  priv->documents_list_action_group = action_group;
  gtk_ui_manager_insert_action_group (priv->ui_manager, action_group, 0);
  g_object_unref (action_group);

  gtk_ui_manager_add_ui_from_resource (priv->ui_manager,
				       "/org/gnome/gtranslator/ui/gtranslator-ui.xml",
				       &error);

  if (error != NULL)
    {
      g_warning ("Could not add ui definition: %s", error->message);
      g_error_free (error);
    }

  /* show tooltips in the statusbar */
  g_signal_connect (priv->ui_manager,
                    "connect_proxy", G_CALLBACK (connect_proxy_cb), window);
  g_signal_connect (priv->ui_manager,
                    "disconnect_proxy",
                    G_CALLBACK (disconnect_proxy_cb), window);

  priv->menubar = gtk_ui_manager_get_widget (priv->ui_manager, "/MainMenu");
  gtk_box_pack_start (GTK_BOX (priv->main_box),
                      priv->menubar, FALSE, FALSE, 0);

  /* Recent files */
  priv->recent_manager = gtk_recent_manager_get_default ();

  priv->recent_menu =
    create_recent_chooser_menu (window, priv->recent_manager);

  g_signal_connect (priv->recent_menu,
                    "item-activated",
                    G_CALLBACK (gtr_recent_chooser_item_activated_cb),
                    window);

  widget = gtk_ui_manager_get_widget (priv->ui_manager,
                                      "/MainMenu/FileMenu/FileRecentFilesMenu");
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (widget), priv->recent_menu);

  /* Toolbar */
  priv->toolbar = GTK_WIDGET
    (g_object_new (EGG_TYPE_EDITABLE_TOOLBAR,
                   "ui-manager", priv->ui_manager,
                   "model",
                   _gtr_application_get_toolbars_model (GTR_APP), NULL));

  gtk_style_context_add_class (gtk_widget_get_style_context (priv->toolbar),
                               GTK_STYLE_CLASS_PRIMARY_TOOLBAR);

  egg_editable_toolbar_show (EGG_EDITABLE_TOOLBAR (priv->toolbar),
                             "DefaultToolBar");
  gtk_box_pack_start (GTK_BOX (priv->main_box),
                      priv->toolbar, FALSE, FALSE, 0);
  gtk_widget_show (priv->toolbar);

  /* notebook */
  priv->notebook = GTK_WIDGET (gtr_notebook_new ());
  gtk_widget_show (priv->notebook);
  gtk_box_pack_start (GTK_BOX (priv->main_box), priv->notebook, TRUE, TRUE, 0);
  g_signal_connect (priv->notebook, "switch-page",
                    G_CALLBACK (notebook_switch_page), window);
  g_signal_connect (priv->notebook, "page-added",
                    G_CALLBACK (notebook_tab_added), window);
  g_signal_connect (priv->notebook, "page-removed",
                    G_CALLBACK (notebook_page_removed), window);
  g_signal_connect (priv->notebook,
                    "tab_close_request",
                    G_CALLBACK (notebook_tab_close_request), window);

  /* statusbar & progress bar */
  create_statusbar (window);


  set_sensitive_according_to_window (window);

  /* Drag and drop support, set targets to NULL because we add the
     default uri_targets below */
  gtk_drag_dest_set (GTK_WIDGET (window),
                     GTK_DEST_DEFAULT_MOTION |
                     GTK_DEST_DEFAULT_HIGHLIGHT |
                     GTK_DEST_DEFAULT_DROP, NULL, 0, GDK_ACTION_COPY);

  /* Add uri targets */
  tl = gtk_drag_dest_get_target_list (GTK_WIDGET (window));

  if (tl == NULL)
    {
      tl = gtk_target_list_new (NULL, 0);
      gtk_drag_dest_set_target_list (GTK_WIDGET (window), tl);
      gtk_target_list_unref (tl);
    }

  gtk_target_list_add_uri_targets (tl, TARGET_URI_LIST);

  /* Connect signals */
  g_signal_connect (window,
                    "drag_data_received",
                    G_CALLBACK (drag_data_received_cb), NULL);

  /* Plugins */
  window->priv->extensions = peas_extension_set_new (PEAS_ENGINE (gtr_plugins_engine_get_default ()),
                                                     GTR_TYPE_WINDOW_ACTIVATABLE,
                                                     "window", window,
                                                     NULL);
  peas_extension_set_foreach (window->priv->extensions,
                              (PeasExtensionSetForeachFunc) extension_added,
                              window);

  g_signal_connect (window->priv->extensions,
                    "extension-added",
                    G_CALLBACK (extension_added),
                    window);
  g_signal_connect (window->priv->extensions,
                    "extension-removed",
                    G_CALLBACK (extension_removed),
                    window);
}

static void
save_panes_state (GtrWindow * window)
{
  g_settings_set (window->priv->state_settings, GTR_SETTINGS_WINDOW_SIZE, "(ii)",
                  window->priv->width, window->priv->height);
  g_settings_set_int (window->priv->state_settings, GTR_SETTINGS_WINDOW_STATE,
                      window->priv->window_state);
}

static void
gtr_window_dispose (GObject * object)
{
  GtrWindow *window = GTR_WINDOW (object);
  GtrWindowPrivate *priv = window->priv;

  DEBUG_PRINT ("window dispose");

  /* First of all, force collection so that plugins
   * really drop some of the references.
   */
  peas_engine_garbage_collect (PEAS_ENGINE (gtr_plugins_engine_get_default ()));

  if (!priv->dispose_has_run)
    {
      save_panes_state (window);

      /* Note that unreffing the extensions will automatically remove
         all extensions which in turn will deactivate the extension */
      g_object_unref (priv->extensions);

      peas_engine_garbage_collect (PEAS_ENGINE (gtr_plugins_engine_get_default ()));

      priv->dispose_has_run = TRUE;
    }

  g_clear_object (&priv->state_settings);
  g_clear_object (&priv->ui_manager);
  g_clear_object (&priv->action_group);
  g_clear_object (&priv->prof_manager);

  /* Now that there have broken some reference loops,
   * force collection again.
   */
  peas_engine_garbage_collect (PEAS_ENGINE (gtr_plugins_engine_get_default ()));

  G_OBJECT_CLASS (gtr_window_parent_class)->dispose (object);
}

static void
gtr_window_finalize (GObject * object)
{
  DEBUG_PRINT ("Finalize window");

  G_OBJECT_CLASS (gtr_window_parent_class)->finalize (object);
}

static gboolean
gtr_window_configure_event (GtkWidget * widget, GdkEventConfigure * event)
{
  GtrWindow *window = GTR_WINDOW (widget);

  window->priv->width = event->width;
  window->priv->height = event->height;

  return GTK_WIDGET_CLASS (gtr_window_parent_class)->configure_event (widget,
                                                                      event);
}

#if 0
FIXME: for now we are giving priority to the window accels
/*
 * GtkWindow catches keybindings for the menu items _before_ passing them to
 * the focused widget. This is unfortunate and means that pressing ctrl+V
 * in an entry on a panel ends up pasting text in the TextView.
 * Here we override GtkWindow's handler to do the same things that it
 * does, but in the opposite order and then we chain up to the grand
 * parent handler, skipping gtk_window_key_press_event.
 */
static gboolean
gtr_window_key_press_event (GtkWidget   *widget,
                            GdkEventKey *event)
{
  static gpointer grand_parent_class = NULL;
  GtkWindow *window = GTK_WINDOW (widget);
  gboolean handled = FALSE;

  if (grand_parent_class == NULL)
    grand_parent_class = g_type_class_peek_parent (gtr_window_parent_class);

  /* handle focus widget key events */
  if (!handled)
    handled = gtk_window_propagate_key_event (window, event);

  /* handle mnemonics and accelerators */
  if (!handled)
    handled = gtk_window_activate_key (window, event);

  /* Chain up, invokes binding set */
  if (!handled)
    handled = GTK_WIDGET_CLASS (grand_parent_class)->key_press_event (widget, event);

  return handled;
}
#endif

static void
gtr_window_class_init (GtrWindowClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GtrWindowPrivate));

  object_class->finalize = gtr_window_finalize;
  object_class->dispose = gtr_window_dispose;

  widget_class->configure_event = gtr_window_configure_event;
  /*widget_class->key_press_event = gtr_window_key_press_event;*/
}

/***************************** Public funcs ***********************************/

/**
 * gtr_window_create_tab:
 * @window: a #GtrWindow
 * @po: a #GtrPo
 *
 * Adds a new #GtrTab to the #GtrNotebook and returns the
 * #GtrTab.
 *
 * Returns: (transfer none): a new #GtrTab object
 */
GtrTab *
gtr_window_create_tab (GtrWindow * window, GtrPo * po)
{
  GtrTab *tab;

  tab = gtr_tab_new (po);
  gtk_widget_show (GTK_WIDGET (tab));

  gtr_notebook_add_page (GTR_NOTEBOOK (window->priv->notebook), tab);

  return tab;
}

/**
 * gtr_window_get_active_tab:
 * @window: a #GtrWindow
 *
 * Gets the active #GtrTab of the @window.
 *
 * Returns: (transfer none): the active #GtrTab of the @window.
 */
GtrTab *
gtr_window_get_active_tab (GtrWindow * window)
{
  return gtr_notebook_get_page (GTR_NOTEBOOK (window->priv->notebook));
}

/**
 * gtr_window_get_all_tabs:
 * @window: a #GtrWindow
 *
 * Gets a list of all tabs in the @window or NULL if there is no tab opened.
 *
 * Returns: (transfer container) (element-type Gtranslator.Tab):
 * a list of all tabs in the @window or NULL if there is no tab opened.
 */
GList *
gtr_window_get_all_tabs (GtrWindow * window)
{
  gint num_pages;
  gint i = 0;
  GList *toret = NULL;

  num_pages =
    gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->priv->notebook));

  while (i < num_pages)
    {
      toret = g_list_append (toret,
                             gtk_notebook_get_nth_page (GTK_NOTEBOOK
                                                        (window->
                                                         priv->notebook), i));
      i++;
    }

  return toret;
}

/**
 * gtr_window_get_header_from_active_tab:
 * @window: a #GtrWindow
 *
 * Gets the #GtrHeader of the #GtrPo of in the active
 * #GtrTab.
 *
 * Returns: (transfer none): the #GtrHeader of the #GtrPo of in the active
 * #GtrTab
 */
GtrHeader *
gtr_window_get_header_from_active_tab (GtrWindow * window)
{
  GtrTab *current_page;
  GtrPo *po;
  GtrHeader *header;

  g_return_val_if_fail (GTR_IS_WINDOW (window), NULL);

  current_page = gtr_window_get_active_tab (window);
  if (!current_page)
    return NULL;

  po = gtr_tab_get_po (current_page);
  header = gtr_po_get_header (po);

  return header;
}

/**
 * gtr_window_get_notebook:
 * @window: a #GtrWindow
 *
 * Gets the main #GtrNotebook of the @window.
 *
 * Returns: (transfer none): the #GtrNotebook of the @window
 */
GtrNotebook *
gtr_window_get_notebook (GtrWindow * window)
{
  return GTR_NOTEBOOK (window->priv->notebook);
}

/**
 * gtr_window_get_statusbar:
 * @window: a #GtrWindow
 *
 * Gets the statusbar widget of the window.
 *
 * Returns: (transfer none): the statusbar widget of the window
 */
GtkWidget *
gtr_window_get_statusbar (GtrWindow * window)
{
  return window->priv->statusbar;
}

/**
 * gtr_window_get_ui_manager:
 * @window: a #GtrWindow
 *
 * Gets the #GtkUIManager of the window.
 *
 * Returns: (transfer none): the #GtkUIManager of the @window
 */
GtkUIManager *
gtr_window_get_ui_manager (GtrWindow * window)
{
  return window->priv->ui_manager;
}

/**
 * gtr_window_get_active_view:
 * @window: a #GtranslationWindow
 *
 * Gets the active translation view in the #GtranslationWindow or
 * NULL if there is not tab opened.
 *
 * Returns: (transfer none): the active translation view in the
 *          #GtranslationWindow or %NULL if there is not tab opened.
 **/
GtrView *
gtr_window_get_active_view (GtrWindow * window)
{
  GtrTab *current_tab;
  current_tab = gtr_window_get_active_tab (window);

  if (!current_tab)
    return NULL;

  return gtr_tab_get_active_view (current_tab);
}

/**
 * gtr_window_get_all_views:
 * @window: the #GtranslationWindow
 * @original: TRUE if you want original TextViews.
 * @translated: TRUE if you want transtated TextViews.
 *
 * Returns all the views currently present in #GtranslationWindow
 *
 * Return value: (transfer container) (element-type Gtranslator.View):
 * a newly allocated list of #GtranslationWindow objects
 **/
GList *
gtr_window_get_all_views (GtrWindow * window,
                          gboolean original, gboolean translated)
{
  gint numtabs;
  gint i;
  GList *views = NULL;
  GtkWidget *tab;

  g_return_val_if_fail (GTR_IS_WINDOW (window), NULL);

  numtabs = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->priv->notebook));
  i = numtabs - 1;

  while (i >= 0 && numtabs != 0)
    {
      tab = gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->priv->notebook),
                                       i);
      views =
        g_list_concat (views,
                       gtr_tab_get_all_views (GTR_TAB (tab), original,
                                              translated));
      i--;
    }

  return views;
}

/**
 * gtr_window_get_tab_from_location:
 * @window: a #GtrWindow
 * @location: the GFile of the po file of the #GtrTab
 *
 * Gets the #GtrTab of the #GtrWindows that matches with the
 * @location.
 *
 * Returns: (transfer none): the #GtrTab which @location matches with its po file.
 */
GtkWidget *
gtr_window_get_tab_from_location (GtrWindow * window, GFile * location)
{
  GList *tabs, *l;
  GtrPo *po;
  GFile *po_location;

  g_return_val_if_fail (GTR_IS_WINDOW (window), NULL);

  tabs = gtr_window_get_all_tabs (window);

  for (l = tabs; l != NULL; l = g_list_next (l))
    {
      po = gtr_tab_get_po (GTR_TAB (l->data));

      po_location = gtr_po_get_location (po);

      if (g_file_equal (location, po_location) == TRUE)
        {
          g_object_unref (po_location);

          return l->data;
        }
      g_object_unref (po_location);
    }

  return NULL;
}

/**
 * gtr_window_set_active_tab:
 * @window: a #GtrWindow
 * @tab: a #GtrTab
 *
 * Sets the active tab for the @window.
 */
void
gtr_window_set_active_tab (GtrWindow * window, GtkWidget * tab)
{
  gint page;

  page = gtk_notebook_page_num (GTK_NOTEBOOK (window->priv->notebook), tab);

  gtk_notebook_set_current_page (GTK_NOTEBOOK (window->priv->notebook), page);
}

/**
 * _gtr_window_close_tab:
 * @window: a #GtrWindow
 * @tab: a #GtrTab
 *
 * Closes the opened @tab of the @window and sets the right sensitivity of the
 * widgets.
 */
void
_gtr_window_close_tab (GtrWindow * window, GtrTab * tab)
{
  gint i;

  g_return_if_fail (GTR_IS_TAB (tab));

  i = gtk_notebook_page_num (GTK_NOTEBOOK (window->priv->notebook),
                             GTK_WIDGET (tab));
  if (i != -1)
    gtr_notebook_remove_page (GTR_NOTEBOOK (window->priv->notebook), i);

  /*
   * If there is only one file opened, we have to clear the statusbar
   */
  if (i == 0)
    {
      gtr_statusbar_push (GTR_STATUSBAR (window->priv->statusbar), 0, " ");
      gtr_statusbar_clear_progress_bar (GTR_STATUSBAR
                                        (window->priv->statusbar));
    }

  set_sensitive_according_to_window (window);
}
