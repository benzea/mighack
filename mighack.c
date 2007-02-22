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
static void sanitize_display_name(const char *dname, char *result)
{
  const char *res = dname;
  char *tmp;

  g_assert(dname);

  /* split off leading "localhost" */
  if (g_str_has_prefix (res, "localhost"))
    {
      res += 9;
    }

  tmp = g_strrstr (res, ":");
  /* something's wrong with it, but hey... */
  if (!tmp)
    {
      strcpy (result, res);
      return;
    }

  /* if it has a trailing .X then return */
  if (strstr (tmp, "."))
    {
      strcpy (result, res);
      return;
    }

  strcpy (result, res);
  /* add trailing .0 */
  strcat(result, ".0");
}

static void
my_window_set_screen (GObject *object, const gchar *display_str)
{
  gchar **cycle_displays;
  const gchar *tmpstr;
  GSList *item, *list;
  GdkDisplay *display = NULL;
  GdkScreen *screen;
  gchar cur_display[1024], buf[1024], switch_to[1024];
  int i;

  if (display_str == NULL)
    return;

  tmpstr = gdk_display_get_name (gdk_screen_get_display (gtk_window_get_screen (GTK_WINDOW (object))));
  sanitize_display_name (tmpstr, cur_display);

  cycle_displays = g_strsplit_set (display_str, "#,;", 0);

  /* now try to find the display we are on */
  for (i = 0; cycle_displays[i]; i++)
    {
      sanitize_display_name (cycle_displays[i], buf);
      if (g_str_equal (buf, cur_display))
        {
          /* if i+1 is the end then we need to wrap around */
          tmpstr = cycle_displays[i+1];
          if (!tmpstr)
            tmpstr = cycle_displays[0];
          sanitize_display_name(tmpstr, switch_to);
        }
    }

  list = gdk_display_manager_list_displays (gdk_display_manager_get ());
  for (item = list; item; item = g_slist_next (item))
    {
      GdkDisplay *tmp = (GdkDisplay*) item->data;

      sanitize_display_name (gdk_display_get_name (tmp), buf);

      if (g_str_equal (buf, switch_to))
        {
          display = tmp;
          break;
        }
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
