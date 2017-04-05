#pragma once
#include "eople_core.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace Eople
{

struct TableEntry
{
  std::string ident;
  // if this identifier's type is a process or function reference, this is the name of the class/function
  std::string target_name;
  size_t      stack_index;
  type_t      type;
  bool        is_constant;
};

typedef size_t EntryId;

class SymbolTable
{
private:
  std::unordered_map<std::string, EntryId> m_table;
  std::vector<TableEntry>                  m_entries;
  std::string scope_name;

  // Register allocation:
  // constant registers
  size_t next_const_reg;
  // variable registers
  size_t next_var_reg;

  // support for rollback on REPL error
  size_t rollback_const_reg;
  size_t rollback_var_reg;

  // TODO: this should be cleaned up.
  // symbols will be forced into constant registers if true.
  // this is so the 'this' reference and function parameters appear below all else on the stack.
  // stack/register layout is [[this][parameters][constants][locals][temporaries]]
  bool force_constant;

public:
  static EntryId NOT_FOUND;

  SymbolTable()
    : next_const_reg(0), next_var_reg(0), force_constant(false)
  {
  }

  bool HasSymbol( const std::string &ident )
  {
    return m_table.find(ident) != m_table.end();
  }

  void PrepRollback()
  {
    rollback_const_reg = next_const_reg;
    rollback_var_reg   = next_var_reg;
  }

  void Rollback()
  {
    i32 count = (i32)((next_const_reg - rollback_const_reg) + (next_var_reg - rollback_var_reg));
    next_const_reg = rollback_const_reg;
    next_var_reg   = rollback_var_reg;

    assert( count <= m_entries.size() );
    for( auto it = m_table.begin(); it != m_table.end(); )
    {
      if( it->second > (m_entries.size()-1-count) )
      {
        it = m_table.erase(it);
      }
      else
      {
        ++it;
      }
    }
    while( count-- > 0 )
    {
      m_entries.pop_back();
    }
  }

  void ConvertToConstant( EntryId entry_id )
  {
    auto &entry = TableEntryFromId(entry_id);
    if(!entry.is_constant)
    {
      entry.is_constant = true;
      // push to end of constants
      if(entry.stack_index == (next_var_reg-1))
      {
        --next_var_reg;
      }
      entry.stack_index += next_const_reg++;
    }
  }

  TableEntry& TableEntryFromId( EntryId entry_id )
  {
    assert(entry_id < m_entries.size());
    return m_entries[entry_id];
  }

  size_t GetTableEntryIndex( const std::string &ident, bool is_constant, bool must_exist=false )
  {
    is_constant = force_constant || is_constant;
    size_t index = 0;

    // try with and without scope.
    // TODO: this is probably not necessary - make sure scope is ALWAYS used, then remove the 'if'
    auto entry = m_table.find(scope_name + ident);
    if( entry == m_table.end() )
    {
      entry = m_table.find(ident);
    }

    // if we found the identifier, return the index. otherwise, create a new entry
    if( entry != m_table.end() )
    {
      index = entry->second;
    }
    else if( !must_exist )
    {
      TableEntry new_entry;
      new_entry.ident = scope_name + ident;
      new_entry.type = TypeBuilder::GetNilType();
      new_entry.stack_index = is_constant ? next_const_reg++ : next_var_reg++;
      new_entry.is_constant = is_constant;
      index = m_entries.size();
      m_entries.push_back(new_entry);
      m_table[new_entry.ident] = index;
    }
    else
    {
      return NOT_FOUND;
    }

    return index;
  }

  void SetScopeName( std::string name )
  {
    scope_name = name + "::";
  }

  void ClearTable()
  {
    m_entries.clear();
    m_table.clear();
    next_const_reg = 0;
    next_var_reg = 0;
  }

  void ForceConstant( bool force )
  {
    force_constant = force;
  }

  size_t GetRegisterIndex( EntryId entry_id )
  {
    auto &entry = m_entries[entry_id];
    return entry.stack_index + (entry.is_constant ? 0 : next_const_reg);
  }

  // returns total symbol count including both constants and locals
  size_t symbol_count()
  {
    return next_const_reg + next_var_reg;
  }

  size_t constant_count()
  {
    return next_const_reg;
  }

  void DumpTable()
  {
    for( auto &entry : m_entries )
    {
      Log::Print("[SYM_TABLE] %s : %s(%d)\n", entry.ident.c_str(), entry.is_constant ? "c" : "r", entry.stack_index);
    }
  }
};

} // namespace Eople
