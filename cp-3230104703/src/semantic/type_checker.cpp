#include "type_checker.hpp"

#include "common.hpp"
#include "type.hpp"
#include <cstddef>
#include <iostream>
#include <cstdlib> // 使用std::exit(code)用指定的错误码退出
#include <memory>
#include <regex>
#include <vector>

void TypeChecker::error(int code, AST::NodePtr node,
                        const std::string& message) {
  std::cerr << "Semantic Error";

  if (node) {
    std::cerr << " at line " << node->lineno;
  }

  std::cerr << ": " << message << std::endl;

  std::exit(code);
}

bool TypeChecker::is_int(const TypePtr& type) const {
  auto primitive = std::dynamic_pointer_cast<PrimitiveType>(type);
  return primitive && primitive->basic_type == BasicType::Int;
}

bool TypeChecker::is_void(const TypePtr& type) const {
  auto primitive = std::dynamic_pointer_cast<PrimitiveType>(type);
  return primitive && primitive->basic_type == BasicType::Void;
}

size_t TypeChecker::array_capacity(const std::vector<int>& dims,
                                   size_t start) const {
  size_t result = 1;

  for (size_t i = start; i < dims.size(); i++) {
    result *= static_cast<size_t>(dims[i]);
  }

  return result;
}

void TypeChecker::check_scalar_initializer_in_array(AST::NodePtr init,
                                                    AST::NodePtr error_node) {
  auto init_list = std::dynamic_pointer_cast<AST::InitList>(init);

  if (init_list) {
    error(3, error_node, "excess elements in scalar initializer");
  }

  TypePtr value_type = check(init);

  if (!is_int(value_type)) {
    error(4, error_node, "incompatible conversion/assignment");
  }
}

size_t TypeChecker::check_array_initializer(const std::vector<int>& dims,
                                            AST::InitListPtr init) {
  size_t total = array_capacity(dims, 0);
  size_t filled = 0;

  for (auto& value : init->values) {
    if (filled >= total) {
      error(3, value, "excess elements in array initializer");
    }

    auto nested_list = std::dynamic_pointer_cast<AST::InitList>(value);

    if (!nested_list) {
      TypePtr value_type = check(value);

      if (!is_int(value_type)) {
        error(4, value, "incompatible conversion/assignment");
      }

      filled++;
      continue;
    }

    // 实验规定花括号列表只能初始化数组/子数组，不能初始化标量。
    if (dims.size() <= 1) {
      check_scalar_initializer_in_array(value, value);
      filled++;
      continue;
    }

    bool found_subarray = false;
    size_t chosen_start = 0;
    size_t chosen_size = 0;

    // 从更大的子数组开始尝试匹配
    for (size_t start = 1; start < dims.size(); start++) {
      size_t sub_size = array_capacity(dims, start);

      if (sub_size != 0 && filled % sub_size == 0) {
        found_subarray = true;
        chosen_start = start;
        chosen_size = sub_size;
        break;
      }
    }

    if (found_subarray) {
      if (filled + chosen_size > total) {
        error(3, value, "excess elements in array initializer");
      }

      std::vector<int> sub_dims(dims.begin() + chosen_start, dims.end());

      check_array_initializer(sub_dims, nested_list);

      // 即使 nested_list 没有写满，对应子数组剩余部分也会补 0
      filled += chosen_size;
    } else {
      error(3, value, "excess elements in scalar initializer");
    }
  }

  return filled;
}

void TypeChecker::check_initializer(const TypePtr& target_type,
                                    AST::NodePtr init,
                                    AST::NodePtr error_node) {
  auto init_list = std::dynamic_pointer_cast<AST::InitList>(init);
  auto array_type = std::dynamic_pointer_cast<ArrayType>(target_type);

  if (array_type) {
    if (!init_list) {
      error(12, error_node, "array initializer must be an initializer list");
    }

    check_array_initializer(array_type->dims, init_list);
    return;
  }

  if (init_list) {
    error(3, error_node, "excess elements in scalar initializer");
  }

  TypePtr init_type = check(init);

  if (!init_type || !target_type->compatible(init_type)) {
    error(4, error_node, "incompatible conversion/assignment");
  }
}

// 变量定义构造类型
TypePtr TypeChecker::build_var_type(BasicType btype, const std::vector<int>& dims) {
  TypePtr base = PrimitiveType::create(btype);

  if(dims.empty()){
    return base;
  }

  return ArrayType::create(base, dims);
}

// 构造函数参数类型
TypePtr TypeChecker::build_param_type(AST::FuncParamPtr param) {
  TypePtr base = PrimitiveType::create(param->btype);

  if(!param->is_array) {
    return base;
  }

  std::vector<int> dims = param->array_dims;
  if (dims.empty()) {
    dims.push_back(0);
  }

  return ArrayType::create(base, dims);
}

TypeChecker::TypeChecker() {
  // 你需要在这里对 symbol_table 进行初始化
  // 插入一些内置函数，如 read 和 write
  // 往全局符号表里面加入read,返回类型为int，参数列表为空
  symbol_table.add_symbol(
    "read",
    FuncType::create(PrimitiveType::Int, std::vector<TypePtr>{})
  );
  //向全局符号表中加入void write(int);
  /*
  函数名：write
  返回类型：void
  参数列表：一个 int
  */
  symbol_table.add_symbol (
    "write",
    FuncType::create(PrimitiveType::Void, std::vector<TypePtr>{PrimitiveType::Int})
  );

  current_return_type = nullptr; //表示当前没有进入任何函数
}

TypePtr TypeChecker::check(AST::NodePtr node) {
  if(!node) {
    return nullptr;
  }
#define CHECK_NODE(type)                                     \
  if (auto n = std::dynamic_pointer_cast<AST::type>(node)) { \
    return check##type(n);                                   \
  }

  // 递归检查 AST 的每个节点
  // 如果你添加了新的 AST 节点类型，记得在这里添加对应的检查函数
  CHECK_NODE(CompUnit)
  CHECK_NODE(FuncDef)
  CHECK_NODE(VarDecl)
  CHECK_NODE(Block)
  CHECK_NODE(AssignStmt)
  CHECK_NODE(ReturnStmt)
  CHECK_NODE(IfStmt)
  CHECK_NODE(WhileStmt)
  CHECK_NODE(LVal)
  CHECK_NODE(IntConst)
  CHECK_NODE(FuncCall)
  CHECK_NODE(UnaryExp)
  CHECK_NODE(BinaryExp)
  //CHECK_NODE(InitList)

#undef CHECK_NODE

  ASSERT(false, "Unknown AST node type " + node->to_string() +
                    " in type checking at line " +
                    std::to_string(node->lineno));
}

TypePtr TypeChecker::checkCompUnit(AST::CompUnitPtr node) {
  // 逐个检查全局变量和函数定义
  for (auto& unit : node->units) {
    check(unit);
  }
  return nullptr;
}

TypePtr TypeChecker::checkFuncDef(AST::FuncDefPtr node) {
  // 在这个函数中，你需要判断函数是否已经被定义过
  // 如果函数已经被定义过，你需要报错
  // 否则，你需要将函数插入符号表，并在符号表中创建一个新的作用域
  // 再将函数参数也插入符号表，并将符号表中对应的 symbol 挂到 FuncDef 节点上
  // 最后检查函数体的语句块

  std::vector<TypePtr> param_types;

  if(node->params) {
    for(auto& param : node->params->params) {
      param_types.push_back(build_param_type(param));
    }
  }

  TypePtr return_type = PrimitiveType::create(node->return_btype);
  TypePtr func_type = FuncType::create(return_type, param_types);

  auto func_symbol = symbol_table.add_symbol(node->name, func_type);

  if(!func_symbol) {
    error(11, node, "redefinition of identifier '" + node->name + "'");
  }

  node->symbol = func_symbol;

  TypePtr old_return_type = current_return_type;
  current_return_type = return_type;

  symbol_table.enter_scope();
  if (node->params) {
    for (auto& param : node->params->params) {
      TypePtr param_type = build_param_type(param);

      auto param_symbol = symbol_table.add_symbol(param->ident, param_type);

      if (!param_symbol) {
        error(11, param, "redefinition of identifier '" + param->ident + "'");
      }
      param->symbol = param_symbol;
    }
  }

  checkBlock(node->block, false);
  symbol_table.exit_scope();
  current_return_type = old_return_type;

  return nullptr;
}

TypePtr TypeChecker::checkVarDecl(AST::VarDeclPtr node) {
  for (auto var_def : node->defs) {
    checkVarDef(var_def, node->btype);
  }
  return nullptr;
}

TypePtr TypeChecker::checkVarDef(AST::VarDefPtr node, BasicType var_type) {
  // 你需要判断变量是否已经被定义过，并更新符号表
  // 判断变量是否已经被定义过
  // 如果有初始化表达式，你需要检查初始化表达式的类型是否和变量类型相同
  // 如果是数组，你还需要检查初始化表达式和数组的维度是否匹配，是否有溢出的情况
  // 将变量插入符号表，并将符号表中的 symbol 挂到 VarDef 节点上

  TypePtr type = build_var_type(var_type, node->dims);// 根据变量定义生成变量类型

  auto symbol = symbol_table.add_symbol(node->ident, type); //将变量加入当前作用域

  // 如果当前作用域已经有了同名符号，则返回nullptr
  if(!symbol) {
    error(11, node, "redefinition of idenifier '" + node->ident + "'");
  }
  
  node->symbol = symbol;// 把symbol挂到AST上
  // 如果变量有初始化，检查右边表达式的类型
  if (node->init) {
    check_initializer(type, node->init, node);
  }

  return nullptr;
}

TypePtr TypeChecker::checkBlock(AST::BlockPtr node, bool new_scope) {
  // 检查块内的每个语句
  // 如果 new_scope 为 true
  // 你需要在进入和退出块时更新符号表，创建、销毁新的作用域

  if(new_scope) {
    symbol_table.enter_scope();
  }

  for(auto& stmt : node->stmts) {
    if(stmt) {
      check(stmt);
    }
  }
  if(new_scope) {
    symbol_table.exit_scope();
  }
  return nullptr;
}

TypePtr TypeChecker::checkAssignStmt(AST::AssignStmtPtr node) {
  TypePtr lval_type = check(node->lval);
  TypePtr expr_type = check(node->exp);
  // 判断赋值号两边的类型是否相同
  // 我们实验中只支持 int 类型
  // 因此你需要判断 lval_type 和 expr_type 是否都为 int 类型
  if (!lval_type || !expr_type) {
    error(4, node, "incompatible conversion/assignment");
  }
  // 数组整体不能赋值
  if(std::dynamic_pointer_cast<ArrayType>(lval_type)) {
    error(2, node, "array type is not assignable");
  }
  // 普通类型不兼容
  if(!lval_type->compatible(expr_type)) {
    error(4, node, "incompatible conversion/assignment");
  }

  return lval_type;
}

TypePtr TypeChecker::checkReturnStmt(AST::ReturnStmtPtr node) {
  // 判断返回值类型是否和函数声明的返回值类型相同

  if(!current_return_type) {
    return nullptr;
  }
  // 函数没有返回表达式
  if(!node->exp) {
    if(!is_void(current_return_type)) {
      error(7, node, "return type mismatch");
    }
    return nullptr;
  }
  TypePtr expr_type = check(node->exp);

  if(!expr_type || !current_return_type->compatible(expr_type)) {
    error(7, node, "return type mismatch");
  }

  return nullptr;
}

TypePtr TypeChecker::checkLVal(AST::LValPtr node) {
  // 你需要在这里查找符号表，判断变量是否被定义过
  // 根据符号表中的信息设置 LVal 的类型
  // 若变量未定义，你需要报错
  // 否则，将符号表中的 symbol 挂到 LVal 节点上
  // 如果 LVal 是数组，你还需要根据下标索引来设置 LVal 的类型
  auto symbol = symbol_table.find_symbol(node->ident);

  if(!symbol) {
    error(10, node, "use of undeclared identifier '" + node->ident + "'");
  }

  node->symbol = symbol;

  std::vector<AST::NodePtr> indices = node->indices;

  TypePtr current_type = symbol->type;
  // 逐层处理下标
  for(auto& index:indices) {
    // 判断被下标的对象是不是数组
    auto array_type = std::dynamic_pointer_cast<ArrayType>(current_type);

    if(!array_type) {
      error(6, node, "subscripted value is not an array or pointer");
    }
    // 判断下标是不是整数
    TypePtr index_type = check(index);

    if(!is_int(index_type)) {
      error(8, index, "array subscript is not an integer");
    }
     //进行访问后的降维操作
    if(array_type->dims.size() <= 1) {
      current_type = array_type->element_type; // 当前数组只剩下一维，那么用下标访问之后，结果就是数组元素
    } else {
      std::vector<int> remain_dims(array_type->dims.begin()+1, array_type->dims.end());
      current_type = ArrayType::create(array_type->element_type, remain_dims);
    }

  }


  // 你需要返回 LVal 的类型
  return current_type;
}

TypePtr TypeChecker::checkIntConst(AST::IntConstPtr node) {
  // 整数常量的类型是 int
  return PrimitiveType::Int;
}

TypePtr TypeChecker::checkFuncCall(AST::FuncCallPtr node) {
  // 首先需要查找函数是否被定义过
    auto symbol = symbol_table.find_symbol(node->name);

    if(!symbol) {
      error(10, node, "use of undeclared identifier '" + node->name + "'");
    }

    node->symbol = symbol;
  // 然后需要判断函数调用的参数个数和类型是否和声明一致
  auto func_type = std::dynamic_pointer_cast<FuncType>(symbol->type);
  // 判断是不是函数
  if (!func_type) {
    error(9, node, "called object is not a function or function pointer");
  }
  // 判断参数数量
  if(node->args.size() != func_type->param_types.size()) {
    error(13, node, "function arguments number not matched");
  }

  // 最后设置函数调用表达式的类型为函数的返回值类型
  // 并将函数的 symbol 挂到 FuncCall 节点上
  auto compatible_for_arg = [](const TypePtr& expected,
                               const TypePtr& actual) -> bool {
    if(!expected || !actual) {
      return false;
    }
    
                                if (expected->compatible(actual)) {
      return true;
    }

    auto expected_array = std::dynamic_pointer_cast<ArrayType>(expected);
    auto actual_array = std::dynamic_pointer_cast<ArrayType>(actual);

    if (!expected_array || !actual_array) {
      return false;
    }

    if (!expected_array->element_type->compatible(actual_array->element_type)) {
      return false;
    }

    if (expected_array->dims.size() != actual_array->dims.size()) {
      return false;
    }

    for (size_t i = 0; i < expected_array->dims.size(); i++) {
      if (i == 0 && expected_array->dims[i] == 0) {
        continue;
      }

      if (expected_array->dims[i] != actual_array->dims[i]) {
        return false;
      }
    }

    return true;
  };

  for (size_t i = 0; i < node->args.size(); i++) {
    TypePtr arg_type = check(node->args[i]);

    if (!compatible_for_arg(func_type->param_types[i], arg_type)) {
      error(4, node->args[i], "incompatible conversion/assignment");
    }
  }

  return func_type->return_type;
}

TypePtr TypeChecker::checkUnaryExp(AST::UnaryExpPtr node) {
  auto type = check(node->exp);
  // 一元表达式只支持 int 类型，因此你需要判断 type 是否为 int

  if(!is_int(type)) {
    error(5, node, "invalid operands to unary expression");
  }
  return PrimitiveType::Int;
}

TypePtr TypeChecker::checkBinaryExp(AST::BinaryExpPtr node) {
  TypePtr left_type = check(node->left);
  TypePtr right_type = check(node->right);
  // 二元表达式只支持 int 类型，因此你需要判断左右表达式的类型是否为 int

  if(!is_int(left_type) || !is_int(right_type)) {
    error(5, node, "invalid operands to binary expression");
  }
  return PrimitiveType::Int;
}

TypePtr TypeChecker::checkIfStmt(AST::IfStmtPtr node) {
  TypePtr cond_type = check(node->cond);

  if (!is_int(cond_type)) {
    error(4, node->cond, "incompatible conversion/assignment");
  }

  check(node->then_stmt);

  if (node->else_stmt) {
    check(node->else_stmt);
  }

  return nullptr;
}

TypePtr TypeChecker::checkWhileStmt(AST::WhileStmtPtr node) {
  TypePtr cond_type = check(node->cond);

  if (!is_int(cond_type)) {
    error(4, node->cond, "incompatible conversion/assignment");
  }

  check(node->stmt);

  return nullptr;
}
