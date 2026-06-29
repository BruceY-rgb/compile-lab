#include "symbol_table.hpp"
#include <cstddef>
#include <string>

SymbolPtr SymbolTable::add_symbol(std::string name, TypePtr type) {
  // 实现符号表的插入操作
  // 并设置 symbol 的 unique_name 属性（你也可以等到 IR Translation 阶段再设置）
  // 对于局部变量和数组，最好为该标识符重新生成一个唯一名称
  // 对于全局变量和函数，直接使用原名称即可
  // 最后，如果插入成功，返回新的符号
  // 如果符号已经存在，返回 nullptr

  if(scopes.empty()) {
    enter_scope();
  }

  auto& current_scope = scopes.back(); // 取作用域栈的最后一层，也就是当前作用域

  // 同一作用域内不能重复定义
  if(current_scope.find(name) != current_scope.end()) {
    return nullptr;
  }

  int current_scope_id = scope_ids.back();
  auto symbol = Symbol::create(name, type, current_scope_id);

  // 全局变量和函数直接使用原名
  if(is_global_scope()) {
    symbol->unique_name = name;
  } else {
    // 局部变量生成唯一名，避免不同作用域的同名变量冲突
    symbol->unique_name = 
      name + "." + std::to_string(current_scope_id) + "." + std::to_string(next_unique_id++);
  }

  current_scope[name] = symbol;
  return symbol;
}

SymbolPtr SymbolTable::find_symbol(std::string name,
                                   bool in_current_scope) const {
  // 实现符号表的查找操作
  // 找到了返回对应的符号，否则返回 nullptr
  // in_current_scope 为 true 时，只在当前作用域查找

  if(scopes.empty()) {
    return nullptr;
  }

  // 只在当前作用域中查找，不需要取外层域查找
  if(in_current_scope) {
    const auto& current_scope = scopes.back();
    auto it = current_scope.find(name); // 查找名字为name的符号

    if(it != current_scope.end()) {
      return it->second; // 返回对应的SymbolPtr(对应符号的指针)
    }

    return nullptr; 
  }

  // 从当前作用域往外层作用域查找(反向遍历，从最后一层开始往前找：当前block->函数作用域->全局作用域)
  for(auto scope_it = scopes.rbegin(); scope_it != scopes.rend(); scope_it++) {
    auto symbol_it = scope_it->find(name);

    if(symbol_it != scope_it->end()) {
      return symbol_it->second;
    }
  }

  return nullptr;
}

void SymbolTable::enter_scope() {
  // 实现符号表的进入作用域操作
  // 需要创建一个新的作用域
  scopes.emplace_back();
  scope_ids.push_back(next_scope_id++);

}

void SymbolTable::exit_scope() {
  // 实现符号表的退出作用域操作
  // 需要删除当前作用域中的所有符号
  if(scopes.empty()) {
    return ;
  }

  // 通常会保留全局作用域，防止误删全局变量和内置函数
  if(scopes.size() == 1) {
    return ;
  }
  scopes.pop_back();
  scope_ids.pop_back();
}

