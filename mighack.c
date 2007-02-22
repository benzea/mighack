
#define _GNU_SOURCE
#include <dlfcn.h>
#include <gtk/gtk.h>

#define PROP_SCREEN_MIGRATION 12345

static guint my_signal = 0;

static void
my_window_set_screen (GObject *object, const gchar *display_str)
{
  gchar **cycle_displays;
  gint cycle_offset = 0;
  gint next_cycle_offset = 0;
  const gchar *cur_display;
  GSList *item, *start;
  GdkDisplay *display;
  GdkScreen *screen;

  if (display_str == NULL)
    return;

  cur_display = gdk_display_get_name (gdk_screen_get_display (gtk_window_get_screen (GTK_WINDOW (object))));

  /* split the list */
  cycle_displays = g_strsplit (display_str, "#", 0);
  /* now try to find the display we are on */
  while (cycle_displays[cycle_offset] != NULL)
    {
      if (g_str_equal (cycle_displays[cycle_offset], cur_display))
        next_cycle_offset = cycle_offset+1;
      cycle_offset++;
    }

  /* cycle_offset contains the number of displays in the list ... */
  if (next_cycle_offset >= cycle_offset)
    next_cycle_offset = 0;

  start = gdk_display_manager_list_displays (gdk_display_manager_get ());
  item = start;
  while (item)
    {
      display = (GdkDisplay*) item->data;

      if (g_str_equal (cycle_displays[next_cycle_offset], gdk_display_get_name (display)))
        {
          break;
        }

      item = g_slist_next (item);
    }
  g_slist_free (start);

  /* Try to open the display if we did not do this earlier. */
  if (!item)
    display = gdk_display_open (cycle_displays[next_cycle_offset]);

  if (!display)
    {
      g_warning ("Failed to open display \"%s\"", cycle_displays[next_cycle_offset]);
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
