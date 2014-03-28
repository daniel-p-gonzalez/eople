#include "core.h"
#include "eople_text_buffer.h"
#include <gtk-2.0/gtk/gtk.h>
#include <thread>

namespace Eople
{

//
// Threading glue between gtk and Eople::TextBuffer
//



struct TextBufferData
{
  TextBufferData( TextBuffer* text_buffer, std::string text="" )
    : text_buffer(text_buffer), text(text)
  {
  }
  TextBuffer* text_buffer;
  std::string text;
};

gboolean append( gpointer _data )
{
  TextBufferData* data = (TextBufferData*)_data;
  data->text_buffer->InternalAppend( data->text.c_str() );
  return( FALSE );
}

// Append a string to the text buffer
void TextBuffer::Append( const char* text )
{
  g_idle_add( append, new TextBufferData( this, text ) );
}

gboolean clear( gpointer _data )
{
  TextBufferData* data = (TextBufferData*)_data;
  data->text_buffer->InternalClear();
  return( FALSE );
}

void TextBuffer::Clear()
{
  g_idle_add( clear, new TextBufferData( this ) );
}

gboolean upHistory( gpointer _data )
{
  TextBufferData* data = (TextBufferData*)_data;
  data->text_buffer->InternalUpHistory();
  return( FALSE );
}

void TextBuffer::UpHistory()
{
  g_idle_add( upHistory, new TextBufferData( this ) );
}

gboolean downHistory( gpointer _data )
{
  TextBufferData* data = (TextBufferData*)_data;
  data->text_buffer->InternalDownHistory();
  return( FALSE );
}

void TextBuffer::DownHistory()
{
  g_idle_add( downHistory, new TextBufferData( this ) );
}

gboolean flushInput( gpointer _data )
{
  TextBufferData* data = (TextBufferData*)_data;
  data->text_buffer->InternalFlushInput();
  return( FALSE );
}

void TextBuffer::FlushInput()
{
  g_idle_add( flushInput, new TextBufferData( this ) );
}

gboolean multiLineEnter( gpointer _data )
{
  TextBufferData* data = (TextBufferData*)_data;
  data->text_buffer->InternalMultiLineEnter();
  return( FALSE );
}

void TextBuffer::MultiLineEnter()
{
  g_idle_add( multiLineEnter, new TextBufferData( this ) );
}

gboolean pushInput( gpointer _data )
{
  TextBufferData* data = (TextBufferData*)_data;
  data->text_buffer->InternalPushInput(data->text);
  return( FALSE );
}

void TextBuffer::PushInput( std::string text )
{
  g_idle_add( pushInput, new TextBufferData( this, text ) );
}


//
// Implementation
//


// Append a string to the text buffer
void TextBuffer::InternalAppend( const char* text )
{
  if( m_text_view )
  {
    GtkTextIter ei;
    memset(&ei, 0, sizeof(GtkTextIter));
    auto text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(m_text_view));
    auto mark = gtk_text_buffer_get_mark(text_buffer, "output_end");
    gtk_text_buffer_get_iter_at_mark(text_buffer, &ei, mark);
    auto tag = gtk_text_tag_table_lookup( gtk_text_buffer_get_tag_table( text_buffer), "not_editable" );
    if( tag )
    {
      gtk_text_buffer_insert_with_tags(text_buffer, &ei, text, -1, tag, NULL);
    }
    //gtk_text_buffer_get_end_iter( text_buffer, &ei );
    //gtk_text_buffer_move_mark_by_name( text_buffer, "output_end", &ei );
  }
}

// Empty the text buffer
void TextBuffer::InternalClear()
{
  GtkTextIter start_iter;
  GtkTextIter end_iter;
  auto text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(m_text_view));
  gtk_text_buffer_set_text( text_buffer, "", -1 );
  gtk_text_buffer_get_end_iter(text_buffer, &end_iter);
  gtk_text_buffer_insert_with_tags_by_name( text_buffer, &end_iter, " ", -1, "partition", NULL );
  gtk_text_buffer_get_start_iter(text_buffer, &start_iter);

  gtk_text_buffer_move_mark_by_name( text_buffer, "output_end", &start_iter );
  gtk_text_buffer_move_mark_by_name( text_buffer, "input_end", &end_iter );
  gtk_text_buffer_move_mark_by_name( text_buffer, "input_start", &end_iter );
  gtk_text_buffer_move_mark_by_name( text_buffer, "scroll", &end_iter );

  gtk_text_buffer_get_bounds(text_buffer, &start_iter, &end_iter);
  gtk_text_buffer_apply_tag_by_name(text_buffer, "not_editable", &start_iter, &end_iter);
}

void TextBuffer::InternalUpHistory()
{
  auto text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(m_text_view));
  m_current_input_line = m_current_input_line ? m_current_input_line - 1 : m_current_input_line;

  if( m_current_input_line < m_input_lines.size() )
  {
    GtkTextIter end_iter;
    auto input_end = gtk_text_buffer_get_mark( text_buffer, "input_end" );
    gtk_text_buffer_get_iter_at_mark( text_buffer, &end_iter, input_end );
    while( gtk_text_buffer_backspace( text_buffer, &end_iter, TRUE, TRUE ) )
    {
    }
    gtk_text_iter_forward_char( &end_iter );
    gtk_text_buffer_insert( text_buffer, &end_iter, m_input_lines[m_current_input_line].c_str(), -1 );
  }
}

void TextBuffer::InternalDownHistory()
{
  auto text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(m_text_view));
  m_current_input_line = (m_current_input_line < m_input_lines.size()-1) ? m_current_input_line + 1 : m_current_input_line;

  if( m_current_input_line < m_input_lines.size() )
  {
    GtkTextIter end_iter;
    auto input_end = gtk_text_buffer_get_mark( text_buffer, "input_end" );
    gtk_text_buffer_get_iter_at_mark( text_buffer, &end_iter, input_end );
    while( gtk_text_buffer_backspace( text_buffer, &end_iter, TRUE, TRUE ) )
    {
    }
    gtk_text_iter_forward_char( &end_iter );
    gtk_text_buffer_insert( text_buffer, &end_iter, m_input_lines[m_current_input_line].c_str(), -1 );
  }
}

void TextBuffer::InternalFlushInput()
{
  auto text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(m_text_view));
  // quick workaround for buffer flush bug causing lost message
  std::chrono::milliseconds duration_ms( 5 );
  std::this_thread::sleep_for( duration_ms );

  GtkTextIter start_iter;
  GtkTextIter end_iter;

  auto input_start = gtk_text_buffer_get_mark( text_buffer, "input_start" );
  auto input_end = gtk_text_buffer_get_mark( text_buffer, "input_end" );
  gtk_text_buffer_get_iter_at_mark( text_buffer, &start_iter, input_start );
  gtk_text_buffer_get_iter_at_mark( text_buffer, &end_iter, input_end );

  std::string text = gtk_text_buffer_get_text( text_buffer, &start_iter, &end_iter, FALSE );
  m_input_lines.push_back(text);
  m_current_input_line = m_input_lines.size();

  m_input_message_lock.lock();
  AutoScope unlock( [=]()
  { 
    m_input_message_lock.unlock();
    m_input_wait_event.notify_one();
  } );
  m_input_messages.push(text);

  gtk_text_buffer_get_end_iter(text_buffer, &end_iter);
  gtk_text_buffer_insert(text_buffer, &end_iter, "\n", -1);

  // mark entered text as not editable since it's now part of output buffer
  gtk_text_buffer_apply_tag_by_name(text_buffer, "not_editable", &start_iter, &end_iter);

  // move output marker to current end of buffer (before partition)
  auto output_end = gtk_text_buffer_get_mark( text_buffer, "output_end" );

  // insert an invisible partition
  gtk_text_buffer_get_end_iter(text_buffer, &end_iter);
  gtk_text_buffer_insert_with_tags_by_name( text_buffer, &end_iter, " ", -1, "partition", NULL );
  gtk_text_iter_backward_char( &end_iter );
  gtk_text_buffer_move_mark( text_buffer, output_end, &end_iter );
  gtk_text_buffer_get_end_iter(text_buffer, &end_iter);

  // move input markers to new end of buffer (after partition)
  gtk_text_buffer_move_mark( text_buffer, input_start, &end_iter );
  gtk_text_buffer_move_mark( text_buffer, input_end, &end_iter );
}

void TextBuffer::InternalMultiLineEnter()
{
  auto text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(m_text_view));
  GtkTextIter end_iter;
  gtk_text_buffer_get_end_iter(text_buffer, &end_iter);
  gtk_text_buffer_insert(text_buffer, &end_iter, "\n       ", -1);
}

void TextBuffer::InternalPushInput( std::string input )
{
  m_input_message_lock.lock();
  {
    m_input_messages.push(input);
  }
  m_input_message_lock.unlock();
  m_input_wait_event.notify_all();
}

std::string TextBuffer::GetInput()
{
  std::unique_lock<std::mutex> input_lock(m_input_message_lock);
  if( m_input_messages.empty() )
  {
    m_input_wait_event.wait(input_lock, [this]{ return !m_input_messages.empty(); });
  }
  std::string input = m_input_messages.front();
  m_input_messages.pop();
  return input;
}

} // namespace Eople
