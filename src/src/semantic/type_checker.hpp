#ifndef SEMANTIC_TYPE_CHECKER_HPP
#define SEMANTIC_TYPE_CHECKER_HPP

#include <memory>

#include "ast/tree.hpp"
#include "symbol_table.hpp"
#include "type.hpp"

class TypeChecker {
 public:
  TypeChecker();

  TypePtr check(AST::NodePtr node);

 private:
  /// @brief The symbol table
  SymbolTable symbol_table;
  // 记录当前正在检查的函数应该返回什么类型
  TypePtr current_return_type;   
  // 一个统一的报错函数
  void error(int code, AST::NodePtr node, const std::string& message);

  bool is_int(const TypePtr& type) const;
  bool is_void(const TypePtr& type) const;
  // 计算数组容量
  size_t array_capacity(const std::vector<int>& dims, size_t start = 0) const;
  void check_scalar_initializer_in_array(AST::NodePtr init,
                                         AST::NodePtr error_node);
  // 统一检查普通变量/数组变量初始化
  void check_initializer(const TypePtr& target_type, AST::NodePtr init,
                         AST::NodePtr error_node);
  // 专门检查数组初始化列表
  size_t check_array_initializer(const std::vector<int>& dims,
                                 AST::InitListPtr init);

  TypePtr build_var_type(BasicType btype, const std::vector<int>& dims); // 根据变量定义生成真正的类型
  // 最后一个dims变量可以判断是变量还是数组，dims.empty()?变量:数组
  TypePtr build_param_type(AST::FuncParamPtr param);// 处理函数参数类型

  TypePtr checkIntConst(AST::IntConstPtr node);
  TypePtr checkLVal(AST::LValPtr node);
  TypePtr checkUnaryExp(AST::UnaryExpPtr node);
  TypePtr checkBinaryExp(AST::BinaryExpPtr node);
  TypePtr checkFuncCall(AST::FuncCallPtr node);
  TypePtr checkBlock(AST::BlockPtr node, bool new_scope = true);
  TypePtr checkAssignStmt(AST::AssignStmtPtr node);
  TypePtr checkReturnStmt(AST::ReturnStmtPtr node);
  TypePtr checkVarDef(AST::VarDefPtr node, BasicType var_type);
  TypePtr checkVarDecl(AST::VarDeclPtr node);
  TypePtr checkFuncDef(AST::FuncDefPtr node);
  TypePtr checkCompUnit(AST::CompUnitPtr node);
  TypePtr checkIfStmt(AST::IfStmtPtr node);
  TypePtr checkWhileStmt(AST::WhileStmtPtr node);
};

#endif  // SEMANTIC_TYPE_CHECKER_HPP