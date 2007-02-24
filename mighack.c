/*
 * GTK+ window migration hack preload library
 *
 * Copyright 2007       Benjamin Berg <benjamin@sipsolutions.net>
 * Copyright 2007       Johannes Berg <johannes@sipsolutions.net>
 *
 * GPLv2
 */

#define _GNU_SOURCE
#include <string.h>
#include <dlfcn.h>
#include <gtk/gtk.h>

#define PROP_SCREEN_MIGRATION 12345

static guint my_signal = 0;

/*
 * result must point to a buffer long enough to hold
 * the whole dname plus at least two characters more!
 */
static gchar *
sanitize_display_name (const gchar *dname)
{
  const gchar *res = dname;
  gchar *tmp;

  g_assert(dname);

  /* skip leading "localhost" */
  if (g_str_has_prefix (res, "localhost"))
    {
      res += 9;
    }

  tmp = g_strrstr (res, ":");
  /* something's wrong with it, just return the original */
  if (!tmp)
    {
      return g_strdup (dname);
    }

  /* if it has a trailing .X then return */
  if (strstr (tmp, "."))
    {
      return g_strdup (res);
    }

  return g_strconcat (res, ".0", NULL);
}

static void
my_window_set_screen (GObject *object, const gchar *display_str)
{
  gchar **cycle_displays;
  GSList *item, *list;
  GdkDisplay *display = NULL;
  GdkScreen *screen;
  gchar *cur_display, *switch_to = NULL;
  int i;

  if (display_str == NULL)
    return;

  cur_display = (gchar*) gdk_display_get_name (gdk_screen_get_display (gtk_window_get_screen (GTK_WINDOW (object))));
  cur_display = sanitize_display_name (cur_display);

  cycle_displays = g_strsplit_set (display_str, "#,;", 0);

  /* now try to find the display we are on */
  for (i = 0; cycle_displays[i]; i++)
    {
      gchar *cycle_display;
      cycle_display = sanitize_display_name (cycle_displays[i]);

      if (g_str_equal (cycle_display, cur_display))
        {
          gchar *next;

          /* if i+1 is the end then we need to wrap around */
          next = cycle_displays[i+1];
          if (!next)
            next = cycle_displays[0];
          switch_to = sanitize_display_name (next);
        }

      g_free (cycle_display);
    }
  g_free (cur_display);

  list = gdk_display_manager_list_displays (gdk_display_manager_get ());
  for (item = list; item; item = g_slist_next (item))
    {
      gchar *display_name;
      GdkDisplay *data = (GdkDisplay*) item->data;

      display_name = sanitize_display_name (gdk_display_get_name (data));

      if (g_str_equal (display_name, switch_to))
        {
          display = data;
          g_free (display_name);
          break;
        }
      g_free (display_name);
    }
  g_slist_free (list);

  /* Try to open the display if we don't have it open already */
  if (!display)
    {
      display = gdk_display_open (switch_to);
    }

  if (!display)
    {
      g_warning ("Failed to open display \"%s\"", switch_to);
      goto out_free;
    }

  screen = gdk_display_get_default_screen (display);
  gtk_window_set_screen (GTK_WINDOW (object), screen);

 out_free:
  g_free (switch_to);
  g_strfreev (cycle_displays);
}

static void
modify_class (GType type)
{
  GtkWindowClass *class;

  /* We _need_ to ref the class so that it exists. */
  class = g_type_class_ref (type);
  class->_gtk_reserved4 = (void*) my_window_set_screen;

  my_signal =
    g_signal_new ("screen_migrate",
                  G_OBJECT_CLASS_TYPE (class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkWindowClass, _gtk_reserved4),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1,
                  G_TYPE_STRING);
  g_type_class_unref (class);
}

GType
gtk_window_get_type (void)
{
  static GType result = 0;
  GType (*orig_gtk_window_get_type) (void) = NULL;

  if (!result && orig_gtk_window_get_type == NULL) {
    orig_gtk_window_get_type = dlsym (RTLD_NEXT, "gtk_window_get_type");

    result = orig_gtk_window_get_type ();

    modify_class (result);
  }
  return result;
}
