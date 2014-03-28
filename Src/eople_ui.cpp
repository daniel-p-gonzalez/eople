#include "core.h"
#include "eople_ui.h"
#include "eople_log.h"
#include <gtk-2.0/gtk/gtk.h>
#include <gtk-2.0/gdk/gdkkeysyms.h>


namespace Eople
{

static GdkPixbuf* create_pixbuf(const gchar * filename)
{
   GdkPixbuf* pixbuf;
   GError* error = NULL;
   pixbuf = gdk_pixbuf_new_from_file(filename, &error);
   if(!pixbuf)
   {
      fprintf(stderr, "%s\n", error->message);
      g_error_free(error);
   }

   return pixbuf;
}

static GtkWidget* GetScrolledTextWindow( GtkWidget* text_view, gboolean is_editable )
{
  PangoFontDescription* font_description = pango_font_description_from_string("Monospace 11");//"DejaVu Sans Mono 11");
  GdkColor text_background_color;
  GdkColor text_color;
  GdkColor background_color;
  gdk_color_parse("#000000", &background_color);
  gdk_color_parse("#0F0F0D", &text_background_color);
  gdk_color_parse("#EEEEE0", &text_color);

  auto scrolled_window = gtk_scrolled_window_new( NULL, NULL );
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GtkPolicyType::GTK_POLICY_AUTOMATIC, GtkPolicyType::GTK_POLICY_AUTOMATIC);
  gtk_widget_modify_font( text_view, font_description );
  gtk_widget_modify_base( text_view, GtkStateType::GTK_STATE_NORMAL, &text_background_color );
  gtk_widget_modify_bg( text_view, GtkStateType::GTK_STATE_NORMAL, &background_color );
  gtk_widget_modify_text( text_view, GtkStateType::GTK_STATE_NORMAL, &text_color );
  gtk_widget_modify_cursor( text_view, &text_color, &text_background_color );

  gtk_text_view_set_border_window_size( GTK_TEXT_VIEW(text_view), GTK_TEXT_WINDOW_LEFT,   12 );
  gtk_text_view_set_border_window_size( GTK_TEXT_VIEW(text_view), GTK_TEXT_WINDOW_RIGHT,  12 );
  gtk_text_view_set_border_window_size( GTK_TEXT_VIEW(text_view), GTK_TEXT_WINDOW_TOP,     4 );
  gtk_text_view_set_border_window_size( GTK_TEXT_VIEW(text_view), GTK_TEXT_WINDOW_BOTTOM, 12 );

  gtk_text_view_set_editable( GTK_TEXT_VIEW(text_view), is_editable );
  gtk_container_add( GTK_CONTAINER(scrolled_window), text_view );

  return scrolled_window;
}

static void do_auto_scroll(GtkTextBuffer* buffer, AppWindow* app)
{
  app->DoAutoScroll(buffer);
}

static gboolean on_key_press(GtkWidget* widget, GdkEventKey* key, AppWindow* app)
{
  return app->OnKeyPress( widget, key );
}

static void AboutMenuItem( AppWindow* app )
{
  app->OnMenuAbout();
}

void AppWindow::OnMenuAbout()
{
  static auto dialog = gtk_dialog_new();

  static bool initialized = false;
  if( !initialized )
  {
    gtk_window_set_modal( GTK_WINDOW( dialog ), FALSE );
    gtk_window_set_title( GTK_WINDOW( dialog ), "About Eople E.V.E." );
    gtk_dialog_add_button( GTK_DIALOG( dialog ), GTK_STOCK_OK, 1 );
  }

  static auto label = gtk_label_new( "Eople Virtual Environment - Version 0.1\n\nCopyright (C) 2014 Daniel Paul Gonzalez." );
  static auto box = gtk_dialog_get_content_area( GTK_DIALOG(dialog) );

  static auto frame = gtk_frame_new(NULL);

  static auto vbox = gtk_vbox_new( FALSE, 8 );

  if( !initialized )
  {
    initialized = true;
    gtk_frame_set_shadow_type( GTK_FRAME(frame), GTK_SHADOW_IN );

#if WIN32
    gtk_box_pack_start( GTK_BOX(vbox), gtk_image_new_from_file("eople.png"), FALSE, FALSE, 16 );
#else
    gtk_box_pack_start( GTK_BOX(vbox), gtk_image_new_from_file("/usr/share/icons/eople.png"), FALSE, FALSE, 16 );
#endif

    gtk_box_pack_start( GTK_BOX(vbox), label, FALSE, FALSE, 0 );

    gtk_container_set_border_width( GTK_CONTAINER(vbox), 24 );

    gtk_container_add( GTK_CONTAINER(frame), vbox );

    gtk_box_pack_start( GTK_BOX(box), frame, FALSE, FALSE, 0 );
  }

  g_signal_connect_swapped(dialog, "response", G_CALLBACK (gtk_widget_destroy), dialog);

  gtk_widget_show_all(dialog);
}

void AppWindow::DoAutoScroll( GtkTextBuffer* buffer )
{
  GtkTextIter iter;
  GtkTextMark *mark;

  gtk_text_buffer_get_end_iter(buffer, &iter);
  gtk_text_iter_set_line_offset(&iter, 0);
  mark = gtk_text_buffer_get_mark(buffer, "scroll");
  gtk_text_buffer_move_mark(buffer, mark, &iter);
  gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(m_text_view), mark);
}

bool AppWindow::OnKeyPress( GtkWidget* widget, GdkEventKey* key )
{
  auto text_view = GTK_TEXT_VIEW(widget);
  auto text_buffer = gtk_text_view_get_buffer(text_view);
  if( !text_view || !text_buffer )
  {
    return FALSE;
  }

  if( key->type == GDK_KEY_PRESS && !(key->state & GDK_CONTROL_MASK) )
  {
    switch( key->keyval )
    {
      case GDK_KEY_Up:
      {
        m_text_buffer_facade->UpHistory();
        return TRUE;
      }
      case GDK_KEY_Down:
      {
        m_text_buffer_facade->DownHistory();
        return TRUE;
      }
      case GDK_KEY_Left:
      {
        auto insert_mark = gtk_text_buffer_get_insert( text_buffer );
        auto select_bound = gtk_text_buffer_get_selection_bound( text_buffer );
        GtkTextIter insert_iter;
        gtk_text_buffer_get_iter_at_mark( text_buffer, &insert_iter, insert_mark );
        gtk_text_iter_backward_cursor_position( &insert_iter );
        if( !gtk_text_iter_can_insert( &insert_iter, TRUE ) )
        {
          while( !gtk_text_iter_can_insert( &insert_iter, TRUE ) )
          {
            gtk_text_iter_forward_cursor_position( &insert_iter );
          }
          gtk_text_buffer_move_mark( text_buffer, insert_mark, &insert_iter );
          if( !(key->state & GDK_SHIFT_MASK) )
          {
            gtk_text_buffer_move_mark( text_buffer, select_bound, &insert_iter );
          }
          return TRUE;
        }
        break;
      }
      case GDK_KEY_Home:
      {
        auto insert_mark = gtk_text_buffer_get_insert( text_buffer );
        auto select_bound = gtk_text_buffer_get_selection_bound( text_buffer );
        GtkTextIter insert_iter;
        gtk_text_buffer_get_iter_at_mark( text_buffer, &insert_iter, insert_mark );
        while( gtk_text_iter_can_insert( &insert_iter, TRUE ) )
        {
          gtk_text_iter_backward_cursor_position( &insert_iter );
        }
        gtk_text_iter_forward_cursor_position( &insert_iter );
        gtk_text_buffer_move_mark( text_buffer, insert_mark, &insert_iter );
        if( !(key->state & GDK_SHIFT_MASK) )
        {
          gtk_text_buffer_move_mark( text_buffer, select_bound, &insert_iter );
        }
        return TRUE;
      }
    	case GDK_KEY_Return:
      {
        m_text_buffer_facade->FlushInput();
        return TRUE;
      }
    	break;
    }
  }
  else if( key->type == GDK_KEY_PRESS && (key->state & GDK_CONTROL_MASK) && key->keyval == GDK_KEY_Return )
  {
    m_text_buffer_facade->MultiLineEnter();
    return TRUE;
  }

  return FALSE;
}

void onClose( AppWindow* window )
{
  window->Shutdown();
  // TODO: this is obviously a bug. VM thread will not get message to shutdown...
  exit(0);
}

AppWindow::AppWindow()
{
  gchar* rcfile = (gchar*)"gtkrc";
  int gtk_argc = 0;

  gtk_rc_add_default_file(rcfile);
  gtk_init(&gtk_argc, nullptr);

  m_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  g_signal_connect(G_OBJECT(m_window), "destroy", G_CALLBACK(onClose), this);   
  gtk_window_set_title(GTK_WINDOW(m_window), "Eople E.V.E.  (V:0.1)");

#if WIN32
  gtk_window_set_icon(GTK_WINDOW(m_window), create_pixbuf("eople.png"));
#else
  gtk_window_set_icon(GTK_WINDOW(m_window), create_pixbuf("/usr/share/icons/eople.png"));
#endif

  m_text_view = gtk_text_view_new();
  m_scrolled_window = GetScrolledTextWindow(m_text_view, TRUE);

  auto rt_text_view = gtk_text_view_new();
  auto rt_scrolled_window = GetScrolledTextWindow(rt_text_view, TRUE);

  GtkTextIter start_iter;
  GtkTextIter end_iter;

  m_text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(m_text_view));
  m_text_buffer_facade.reset( new TextBuffer(m_text_view) );
  Log::SetTextBuffer( m_text_buffer_facade );

  gtk_text_buffer_create_tag(m_text_buffer, "not_editable", "editable", FALSE, NULL);
  gtk_text_buffer_create_tag(m_text_buffer, "partition", "invisible", TRUE, NULL);

  gtk_text_buffer_get_end_iter(m_text_buffer, &end_iter);
  gtk_text_buffer_insert_with_tags_by_name( m_text_buffer, &end_iter, " ", -1, "partition", NULL );
  gtk_text_buffer_get_start_iter(m_text_buffer, &start_iter);

  auto output_end = gtk_text_buffer_create_mark( m_text_buffer, "output_end", &start_iter, FALSE );
  auto input_end = gtk_text_buffer_create_mark( m_text_buffer, "input_end", &end_iter, FALSE );
  auto input_start = gtk_text_buffer_create_mark( m_text_buffer, "input_start", &end_iter, TRUE );
  auto scroll = gtk_text_buffer_create_mark( m_text_buffer, "scroll", &end_iter, FALSE );
  gtk_text_mark_set_visible( output_end, FALSE );
  gtk_text_mark_set_visible( input_end, FALSE );
  gtk_text_mark_set_visible( input_start, FALSE );
  gtk_text_mark_set_visible( scroll, FALSE );

  gtk_text_buffer_get_bounds(m_text_buffer, &start_iter, &end_iter);
  gtk_text_buffer_apply_tag_by_name(m_text_buffer, "not_editable", &start_iter, &end_iter);

  g_signal_connect(G_OBJECT(m_text_buffer), "changed", G_CALLBACK(do_auto_scroll), this);
  g_signal_connect(m_text_view, "key_press_event", G_CALLBACK(on_key_press), this);

  auto notebook = gtk_notebook_new();
  gtk_notebook_append_page( GTK_NOTEBOOK(notebook), m_scrolled_window, gtk_label_new("Terminal") );
  gtk_notebook_append_page( GTK_NOTEBOOK(notebook), rt_scrolled_window, gtk_label_new("Scratchpad") );

  auto vbox = gtk_vbox_new( FALSE, 1 );

  auto menu_bar = gtk_menu_bar_new();

  auto help_menu = gtk_menu_new();
  auto about_item = gtk_menu_item_new_with_label("About");
  gtk_menu_append( GTK_MENU(help_menu), about_item );
  g_signal_connect( GTK_OBJECT(about_item), "activate", G_CALLBACK(AboutMenuItem), NULL );
  gtk_widget_show(about_item);

  auto help_item = gtk_menu_item_new_with_label("Help");
  gtk_widget_show(help_item);
  gtk_menu_item_set_submenu( GTK_MENU_ITEM(help_item), help_menu );
  gtk_menu_bar_append( GTK_MENU_BAR(menu_bar), help_item );

  auto status_bar = gtk_statusbar_new();
  auto context_id = gtk_statusbar_get_context_id( GTK_STATUSBAR(status_bar), "main context" );
  gtk_statusbar_push( GTK_STATUSBAR(status_bar), context_id, "0 modules loaded.  0 processes running." );

  gtk_box_pack_start( GTK_BOX(vbox), menu_bar, FALSE, FALSE, 1 );
  gtk_box_pack_start( GTK_BOX(vbox), notebook, TRUE, TRUE, 1 );
  gtk_box_pack_start( GTK_BOX(vbox), status_bar, FALSE, FALSE, 1 );

  gtk_container_add( GTK_CONTAINER(m_window), vbox );

  gtk_window_set_position(GTK_WINDOW(m_window), GTK_WIN_POS_CENTER);
  gtk_widget_set_usize(m_window, 1024, 900);

  gtk_widget_show_all(m_window);

  gtk_window_set_focus( GTK_WINDOW(m_window), m_text_view );

  gdk_threads_init();
}

std::weak_ptr<TextBuffer> AppWindow::GetTextBuffer()
{
  return m_text_buffer_facade;
}

void AppWindow::EnterMessageLoop()
{
  gtk_main();
}

void AppWindow::Shutdown()
{
  gtk_main_quit();
}

AppWindow::~AppWindow()
{
  if( m_window )
  {
    gtk_widget_destroy(m_window);
  }
  m_window = nullptr;
}

} // namespace Eople
