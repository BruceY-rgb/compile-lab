#ifndef SEMANTIC_SYMBOL_TABLE_HPP
#define SEMANTIC_SYMBOL_TABLE_HPP

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "type.hpp"

class Symbol;
using SymbolPtr = std::shared_ptr<Symbol>;
class Symbol {
 public:
  /// @brief The name of the symbol
  std::string name;
  /// @brief The unique name of the symbol
  std::string unique_name;
  /// @brief The type of the symbol
  TypePtr type;
// #warning Symbol: need to add a member variable to store the symbol's scope
  /// @brief The scope id where the symbol is defined
  int scope_id;
  Symbol(std::string name, TypePtr type, int scope_id = 0) : name(std::move(name)), type(std::move(type)), scope_id(scope_id) {}
  static SymbolPtr create(std::string name, TypePtr type, int scope_id = 0) {
    return std::make_shared<Symbol>(std::move(name), std::move(type), scope_id);
  }
};

class SymbolTable {
 public:
  SymbolTable() {
    enter_scope(); // global scope
  }
  /// @brief Add a symbol to the table and return the unique name of the symbol
  /// @param name The name of the symbol
  /// @param type The type of the symbol
  /// @return The added symbol if added successfully, nullptr otherwise
  SymbolPtr add_symbol(std::string name, TypePtr type);

  /// @brief Find a symbol by name
  /// @param name The name of the symbol
  /// @param in_current_scope Whether to search only in the current scope
  /// @return The symbol if found, nullptr otherwise
  SymbolPtr find_symbol(std::string name, bool in_current_scope = false) const;

  /// @brief Enter a new scope
  void enter_scope();

  /// @brief Exit the current scope
  void exit_scope();

  bool is_global_scope() const {
    return scopes.size() == 1;
  }

 private:
  std::vector<std::unordered_map<std::string, SymbolPtr>> scopes;
  std::vector<int> scope_ids;

  int next_scope_id = 0;
  int next_unique_id = 0;
};

#endif  // SEMANTIC_SYMBOL_TABLE_HPP