#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <gtk-2.0/gtk/gtk.h>

namespace Eople
{

//
// Simple layer over a ui text buffer
//
class TextBuffer
{
public:
  TextBuffer(GtkWidget* text_view) : m_text_view(text_view)
  {
  }

  // Append a string to the text buffer
  void Append( const char* text );
  // Empty the text buffer
  void Clear();
  // Get string of text entered into text buffer (blocking)
  std::string GetInput();

  // Manually push and flush an input
  void PushInput( std::string input );

  // Go back/forward in input history (user hit up/down arrow)
  void UpHistory();
  void DownHistory();
  // User hit enter after entering text
  void FlushInput();
  // User hit cntrl-enter for multi-line input
  void MultiLineEnter();

private:
  // The above are called from different threads, and just send messages to the main loop thread.
  // This is where the work is actually done, called from the main loop.
  // Necessary because gtk is not particular happy about being called from multiple threads,
  //    even when using gdk_threads_enter...
  void InternalAppend( const char* text );
  void InternalClear();
  void InternalUpHistory();
  void InternalDownHistory();
  void InternalFlushInput();
  void InternalPushInput( std::string input );
  void InternalMultiLineEnter();

  friend gboolean append( gpointer _data );
  friend gboolean clear( gpointer _data );
  friend gboolean getInput( gpointer _data );
  friend gboolean pushInput( gpointer _data );
  friend gboolean upHistory( gpointer _data );
  friend gboolean downHistory( gpointer _data );
  friend gboolean flushInput( gpointer _data );
  friend gboolean multiLineEnter( gpointer _data );

  GtkWidget*               m_text_view;
  std::mutex               m_input_wait_lock;
  std::mutex               m_input_message_lock;
  std::condition_variable  m_input_wait_event;
  std::queue<std::string>  m_input_messages;
  std::vector<std::string> m_input_lines;
  size_t                   m_current_input_line;
};

} // namespace Eople
