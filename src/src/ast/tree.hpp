#ifndef AST_TREE_HPP
#define AST_TREE_HPP

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "common.hpp"
#include "semantic/symbol_table.hpp"

extern int yylineno;

namespace AST {

class Node;
using NodePtr = std::shared_ptr<Node>;
class Node {
 public:
  int lineno;

  virtual std::vector<NodePtr> get_children() { return std::vector<NodePtr>(); }
  void print_tree(std::string prefix = "", std::string info_prefix = "");
  virtual std::string to_string() = 0;

  Node() : lineno(yylineno) {}
  virtual ~Node() = default;
};

class IntConst;
using IntConstPtr = std::shared_ptr<IntConst>;
class IntConst : public Node {
 public:
  int value;
  IntConst(int value) : value(value) {}
  std::string to_string() override {
    return "IntConst <value: " + std::to_string(value) + ">";
  }
};

class InitList;
using InitListPtr = std::shared_ptr<InitList>;

class InitList : public Node {
 public:
  std::vector<NodePtr> values;

  InitList() {}

  InitList(NodePtr value) { 
    add_value(value); 
  }

  void add_value(NodePtr value) { 
    values.push_back(value); 
  }

  std::string to_string() override { 
    return "InitList"; 
  }

  std::vector<NodePtr> get_children() override { 
    return values; 
  }
};

class DimList;
using DimListPtr = std::shared_ptr<DimList>;
class DimList : public Node {
 public:
  std::vector<int> dims;

  void add_dim(int dim) { dims.push_back(dim); }

  std::string to_string() override { return "DimList"; }
};

class IndexList;
using IndexListPtr = std::shared_ptr<IndexList>;
class IndexList : public Node {
 public:
  std::vector<NodePtr> indices;

  void add_index(NodePtr index) { indices.push_back(index); }

  std::string to_string() override { return "IndexList"; }
  std::vector<NodePtr> get_children() override { return indices; }
};

class LVal;
using LValPtr = std::shared_ptr<LVal>;
class LVal : public Node {
 public:
  std::string ident;
  SymbolPtr symbol;
  std::vector<NodePtr> indices;
  NodePtr index;   // first dimension index, nullptr if scalar
  NodePtr index2;  // second dimension index, nullptr if not 2D/3D array access
  NodePtr index3;  // third dimension index, nullptr if not 3D array access

  LVal(std::string ident, NodePtr index = nullptr, NodePtr index2 = nullptr,
       NodePtr index3 = nullptr)
      : ident(std::move(ident)), index(index), index2(index2), index3(index3) {
    if (index) indices.push_back(index);
    if (index2) indices.push_back(index2);
    if (index3) indices.push_back(index3);
  }
  LVal(std::string ident, std::vector<NodePtr> indices)
      : ident(std::move(ident)),
        indices(std::move(indices)),
        index(nullptr),
        index2(nullptr),
        index3(nullptr) {
    if (!this->indices.empty()) index = this->indices[0];
    if (this->indices.size() > 1) index2 = this->indices[1];
    if (this->indices.size() > 2) index3 = this->indices[2];
  }
  std::string to_string() override { return "LVal <ident: " + ident + ">"; }
  std::vector<NodePtr> get_children() override {
    return indices;
  }
};

class UnaryExp;
using UnaryExpPtr = std::shared_ptr<UnaryExp>;
class UnaryExp : public Node {
 public:
  UnaryOp op;
  NodePtr exp;
  UnaryExp(UnaryOp op, NodePtr exp) : op(op), exp(exp) {}
  std::string to_string() override {
    return "UnaryExp <op: " + std::string(op_to_string(op)) + ">";
  }
  std::vector<NodePtr> get_children() override { return {exp}; }
};

class BinaryExp;
using BinaryExpPtr = std::shared_ptr<BinaryExp>;
class BinaryExp : public Node {
 public:
  BinaryOp op;
  NodePtr left, right;

  BinaryExp(BinaryOp op, NodePtr left, NodePtr right)
      : op(op), left(left), right(right) {}
  std::string to_string() override {
    return "BinaryExp <op: " + std::string(op_to_string(op)) + ">";
  }
  std::vector<NodePtr> get_children() override { return {left, right}; }
};

class FuncCall;
using FuncCallPtr = std::shared_ptr<FuncCall>;
class FuncCall : public Node {
 public:
  std::string name;
  std::vector<NodePtr> args;
  SymbolPtr symbol;
  FuncCall(std::string name) : name(std::move(name)) {}
  FuncCall(NodePtr exp) { add_arg(exp); }
  void add_arg(NodePtr exp) { args.push_back(exp); }
  std::string to_string() override { return "FuncCall <name: " + name + ">"; }
  std::vector<NodePtr> get_children() override { return args; }
};

class Block;
using BlockPtr = std::shared_ptr<Block>;
class Block : public Node {
 public:
  std::vector<NodePtr> stmts;
  Block() {}
  Block(NodePtr stmt) { add_stmt(stmt); }
  void add_stmt(NodePtr stmt) { stmts.push_back(stmt); }
  std::string to_string() override { return "Block"; }
  std::vector<NodePtr> get_children() override { return stmts; }
};

class AssignStmt;
using AssignStmtPtr = std::shared_ptr<AssignStmt>;
class AssignStmt : public Node {
 public:
  LValPtr lval;
  NodePtr exp;
  AssignStmt(LValPtr lval, NodePtr exp) : lval(lval), exp(exp) {}
  std::string to_string() override { return "AssignStmt"; }
  std::vector<NodePtr> get_children() override { return {lval, exp}; }
};

class ReturnStmt;
using ReturnStmtPtr = std::shared_ptr<ReturnStmt>;
class ReturnStmt : public Node {
 public:
  NodePtr exp;
  ReturnStmt() : exp(nullptr) {}
  ReturnStmt(NodePtr exp) : exp(exp) {}
  std::string to_string() override { return "ReturnStmt"; }
  std::vector<NodePtr> get_children() override {
    return exp ? std::vector<NodePtr>{exp} : std::vector<NodePtr>();
  }
};

class VarDef;
using VarDefPtr = std::shared_ptr<VarDef>;
class VarDef : public Node {
 public:
  std::string ident;
  std::vector<int> dims;  // array dimensions, empty means scalar
  NodePtr init;  // initialization expression, nullptr if no init
  SymbolPtr symbol;
  VarDef(std::string ident) : ident(std::move(ident)), init(nullptr) {}
  VarDef(std::string ident, std::vector<int> dims) : ident(std::move(ident)), dims(std::move(dims)), init(nullptr) {}
  VarDef(std::string ident, NodePtr init) : ident(std::move(ident)), init(init) {}
  VarDef(std::string ident, std::vector<int> dims, NodePtr init) : ident(std::move(ident)), dims(std::move(dims)), init(init) {}
  std::string to_string() override { return "VarDef <ident: " + ident + ">"; }
  std::vector<NodePtr> get_children() override {
    if (init) return {init};
    return {};
  }
};

class VarDecl;
using VarDeclPtr = std::shared_ptr<VarDecl>;
class VarDecl : public Node {
 public:
  BasicType btype;
  std::vector<VarDefPtr> defs;
  VarDecl(VarDefPtr def) : btype(BasicType::Unknown) { add_def(def); }
  void add_def(VarDefPtr def) { defs.push_back(def); }
  std::string to_string() override {
    return "VarDecl <btype: " + std::string(type_to_string(btype)) + ">";
  }
  std::vector<NodePtr> get_children() override {
    return std::vector<NodePtr>(defs.begin(), defs.end());
  }
};

class FuncParam;
using FuncParamPtr = std::shared_ptr<FuncParam>;
class FuncParam : public Node {
 public:
  std::string ident;
  SymbolPtr symbol;
  BasicType btype;
  bool is_array;
  int array_second_dim;
  int array_third_dim;
  std::vector<int> array_dims;
  FuncParam(std::string ident, BasicType btype) : ident(std::move(ident)), btype(btype), is_array(false), array_second_dim(0), array_third_dim(0) {}
  FuncParam(std::string ident, BasicType btype, bool is_array, int second_dim = 0, int third_dim = 0) : ident(std::move(ident)), btype(btype), is_array(is_array), array_second_dim(second_dim), array_third_dim(third_dim) {
    if (is_array) {
      array_dims.push_back(0);
      if (second_dim > 0) array_dims.push_back(second_dim);
      if (third_dim > 0) array_dims.push_back(third_dim);
    }
  }
  FuncParam(std::string ident, BasicType btype, std::vector<int> array_dims) : ident(std::move(ident)), btype(btype), is_array(true), array_second_dim(0), array_third_dim(0), array_dims(std::move(array_dims)) {
    if (this->array_dims.empty()) {
      this->array_dims.push_back(0);
    }
    if (this->array_dims.size() > 1) array_second_dim = this->array_dims[1];
    if (this->array_dims.size() > 2) array_third_dim = this->array_dims[2];
  }
  std::string to_string() override {
    return "FuncParam <ident: " + ident + ", btype: " + std::string(type_to_string(btype)) + ">";
  }
};

class FuncParams;
using FuncParamsPtr = std::shared_ptr<FuncParams>;
class FuncParams : public Node {
 public:
  std::vector<FuncParamPtr> params;
  FuncParams() {}
  void add_param(FuncParamPtr param) { params.push_back(param); }
  std::string to_string() override { return "FuncParams"; }
  std::vector<NodePtr> get_children() override {
    return std::vector<NodePtr>(params.begin(), params.end());
  }
};

class FuncDef;
using FuncDefPtr = std::shared_ptr<FuncDef>;
class FuncDef : public Node {
 public:
  BasicType return_btype;
  std::string name;
  FuncParamsPtr params;
  BlockPtr block;
  SymbolPtr symbol;
  FuncDef(BasicType return_btype, std::string name, BlockPtr block)
      : return_btype(return_btype), name(std::move(name)), params(nullptr), block(block) {}
  FuncDef(BasicType return_btype, std::string name, FuncParamsPtr params, BlockPtr block)
      : return_btype(return_btype), name(std::move(name)), params(params), block(block) {}
  std::string to_string() override {
    return "FuncDef <return_btype: " +
           std::string(type_to_string(return_btype)) + ", name: " + name + ">";
  }
  std::vector<NodePtr> get_children() override {
    std::vector<NodePtr> children;
    if (params) {
      children = params->get_children();
    }
    children.push_back(block);
    return children;
  }
};

class CompUnit;
using CompUnitPtr = std::shared_ptr<CompUnit>;
class CompUnit : public Node {
 public:
  std::vector<NodePtr> units;  // FuncDef or VarDecl
  CompUnit(NodePtr unit) { add_unit(unit); }
  void add_unit(NodePtr unit) { units.push_back(unit); }
  std::string to_string() override { return "CompUnit"; }
  std::vector<NodePtr> get_children() override { return units; }
};


class IfStmt;
using IfStmtPtr = std::shared_ptr<IfStmt>;
class IfStmt : public Node {
  public:
    NodePtr cond;
    NodePtr then_stmt;
    NodePtr else_stmt; // nullptr if no else
    IfStmt(NodePtr cond, NodePtr then_stmt, NodePtr else_stmt = nullptr)
        : cond(cond), then_stmt(then_stmt), else_stmt(else_stmt) {}
    std::string to_string() override { return "IfStmt"; }
    std::vector<NodePtr> get_children() override {
      if(else_stmt) {
        return {cond, then_stmt, else_stmt};
      }
      return {cond, then_stmt};
    }
};

class WhileStmt;
using WhileStmtPtr = std::shared_ptr<WhileStmt>;
class WhileStmt : public Node {
  public:
    NodePtr cond;
    NodePtr stmt;
    WhileStmt(NodePtr cond, NodePtr stmt)
        : cond(cond), stmt(stmt) {}
    std::string to_string() override { return "WhileStmt"; }
    std::vector<NodePtr> get_children() override { return {cond, stmt}; }
};

}  // namespace AST

#endif  // AST_TREE_HPP
