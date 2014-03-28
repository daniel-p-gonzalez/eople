#pragma once
#include "eople_text_buffer.h"
#include <memory>
#include <atomic>
#include <gtk-2.0/gtk/gtk.h>

namespace Eople
{

class AppWindow
{
public:
  AppWindow();
  ~AppWindow();

  void DoAutoScroll( GtkTextBuffer* buffer );
  bool OnKeyPress( GtkWidget* widget, GdkEventKey* key );
  void OnMenuAbout();
  void EnterMessageLoop();
  void Shutdown();

  std::weak_ptr<TextBuffer> GetTextBuffer();

private:
  std::shared_ptr<TextBuffer> m_text_buffer_facade;
  GtkWidget*                  m_window;
  GtkWidget*                  m_text_view;
  GtkWidget*                  m_scrolled_window;
  GtkTextBuffer*              m_text_buffer;
};

} // namespace Eople
