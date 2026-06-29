#ifndef IR_IR_TRANSLATOR_HPP
#define IR_IR_TRANSLATOR_HPP

#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "ast/tree.hpp"
#include "ir/ir.hpp"

class IRTranslator {
 public:
  IR::Code translate(AST::NodePtr node);
  IR::Code translateExp(AST::NodePtr node, const std::string& place = "");

 private:
  IR::Code translateCompUnit(AST::CompUnitPtr node);
  IR::Code translateFuncDef(AST::FuncDefPtr node);
  IR::Code translateBlock(AST::BlockPtr node);
  IR::Code translateVarDecl(AST::VarDeclPtr node);
  IR::Code translateVarDef(AST::VarDefPtr node);
  IR::Code translateAssignStmt(AST::AssignStmtPtr node);
  IR::Code translateReturnStmt(AST::ReturnStmtPtr node);
  IR::Code translateLVal(AST::LValPtr node, const std::string& place = "");
  IR::Code translateBinaryExp(AST::BinaryExpPtr node,
                              const std::string& place = "");
  IR::Code translateUnaryExp(AST::UnaryExpPtr node,
                             const std::string& place = "");
  IR::Code translateFuncCall(AST::FuncCallPtr node,
                             const std::string& place = "");
  IR::Code translateIntConst(AST::IntConstPtr node,
                             const std::string& place = "");
  IR::Code translateCond(AST::NodePtr node, const std::string& true_label,
                             const std::string& false_label);
  IR::Code translateIfStmt(AST::IfStmtPtr node);
  IR::Code translateWhileStmt(AST::WhileStmtPtr node);
  IR::Code translateValAddr(AST::LValPtr node, const std::string& place); // 计算出具体的地址值，放到place里面
  std::string new_temp();
  std::string new_label();
  std::string var_name(const AST::VarDefPtr& node) const;
  std::string var_name(const AST::LValPtr& node) const;
  std::string param_name(const AST::FuncParamPtr& node) const;
  std::string scalar_addr(const std::string& name) const;

  bool in_global_scope = true;
  std::unordered_map<std::string, std::vector<int>> array_dims;  // 记录数组维度信息
  std::unordered_set<std::string> global_names; 
  std::unordered_set<std::string> scalar_vars;
  std::unordered_map<std::string, std::string> global_addr_cache;
  IR::Code current_func_load_addrs;
  std::string get_global_addr(const std::string& name);
};

#endif  // IR_IR_TRANSLATOR_HPP
