#ifndef IR_IR_HPP
#define IR_IR_HPP

#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "common.hpp"

namespace IR {

class Node;
using NodePtr = std::shared_ptr<Node>;
class Node {
 public:
  virtual std::string to_string() const = 0;
  virtual ~Node() = default;  // make the class polymorphic
};

class LoadImm;
using LoadImmPtr = std::shared_ptr<LoadImm>;
class LoadImm : public Node {
 public:
  std::string x;
  int k;

  LoadImm(const std::string& x, int k) : x(x), k(k) {}
  static LoadImmPtr create(const std::string& x, int k) {
    return std::make_shared<LoadImm>(x, k);
  }

  std::string to_string() const override {
    return x + " = #" + std::to_string(k);
  }
};

class Assign;
using AssignPtr = std::shared_ptr<Assign>;
class Assign : public Node {
 public:
  std::string x;
  std::string y;

  Assign(const std::string& x, const std::string& y) : x(x), y(y) {}
  static AssignPtr create(const std::string& x, const std::string& y) {
    return std::make_shared<Assign>(x, y);
  }

  std::string to_string() const override { return x + " = " + y; }
};

class Binary;
using BinaryPtr = std::shared_ptr<Binary>;
class Binary : public Node {
 public:
  std::string x;
  std::string y;
  BinaryOp op;
  std::string z;

  Binary(const std::string& x, const std::string& y, const BinaryOp& op,
         const std::string& z)
      : x(x), y(y), op(op), z(z) {}

  static BinaryPtr create(const std::string& x, const std::string& y,
                          const BinaryOp& op, const std::string& z) {
    return std::make_shared<Binary>(x, y, op, z);
  }

  std::string to_string() const override {
    return x + " = " + y + " " + op_to_string(op) + " " + z;
  }
};

class Unary;
using UnaryPtr = std::shared_ptr<Unary>;
class Unary : public Node {
 public:
  std::string x;
  UnaryOp op;
  std::string y;
  Unary(const std::string& x, const UnaryOp& op, const std::string& y)
      : x(x), op(op), y(y) {}

  static UnaryPtr create(const std::string& x, const UnaryOp& op,
                         const std::string& y) {
    return std::make_shared<Unary>(x, op, y);
  }

  std::string to_string() const override {
    return x + " = " + op_to_string(op) + " " + y;
  }
};

class Label;
using LabelPtr = std::shared_ptr<Label>;
class Label : public Node {
 public:
  std::string label;

  Label(const std::string& label) : label(label) {}

  static LabelPtr create(const std::string& label) {
    return std::make_shared<Label>(label);
  }

  std::string to_string() const override { return "LABEL " + label + ":"; }
};

class Goto;
using GotoPtr = std::shared_ptr<Goto>;
class Goto : public Node {
 public:
  std::string label;

  Goto(const std::string& label) : label(label) {}

  static GotoPtr create(const std::string& label) {
    return std::make_shared<Goto>(label);
  }

  std::string to_string() const override { return "GOTO " + label; }
};

class If;
using IfPtr = std::shared_ptr<If>;
class If : public Node {
  public:
    std::string x;
    BinaryOp op;
    std::string y;
    std::string label;

    If(const std::string& x, 
       const BinaryOp& op,
       const std::string& y,
       const std::string& label 
    ) :x(x), op(op), y(y), label(label) {}

    static IfPtr create(
      const std::string& x, 
      const BinaryOp& op,
      const std::string& y, 
      const std::string& label) {
        return std::make_shared<If>(x, op, y, label);
    }

    std::string to_string() const override {
      return "IF " + x + " " + op_to_string(op) + " " + y + " GOTO " + label;
    }
};

class Function;
using FunctionPtr = std::shared_ptr<Function>;
class Function : public Node {
 public:
  std::string name;

  Function(const std::string& func) : name(func) {}

  static FunctionPtr create(const std::string& func) {
    return std::make_shared<Function>(func);
  }

  std::string to_string() const override { return "FUNCTION " + name + ":"; }
};

class Call;
using CallPtr = std::shared_ptr<Call>;
class Call : public Node {
 public:
  std::string func;
  std::string x;  // 返回值存放的位置，空字符串表示无返回值

  Call(const std::string& func) : func(func) {}
  Call(const std::string& x, const std::string& func) : func(func), x(x) {}

  static CallPtr create(const std::string& func) {
    return std::make_shared<Call>(func);
  }
  static CallPtr create(const std::string& x, const std::string& func) {
    return std::make_shared<Call>(x, func);
  }

  std::string to_string() const override {
    if (x.empty()) {
      return "CALL " + func;
    }
    return x + " = CALL " + func;
  }
};

class Arg;
using ArgPtr = std::shared_ptr<Arg>;
class Arg : public Node {
 public:
  std::string x;
  std::string func;
  int k;

  Arg(const std::string& x): x(x), func(""), k(0) {}
  Arg(const std::string& x, const std::string& func, int k)
      : x(x), func(func), k(k) {}

  static ArgPtr create(const std::string& x) {
    return std::make_shared<Arg>(x);
  }

  static ArgPtr create(const std::string& x, const std::string& func, int k) {
    return std::make_shared<Arg>(x, func, k);
  }

  std::string to_string() const override { return "ARG " + x; }
};

class Param;
using ParamPtr = std::shared_ptr<Param>;
class Param : public Node {
  public:
    std::string x; 

    Param(const std::string& x):x(x){}

    static ParamPtr create(const std::string& x){
      return std::make_shared<Param>(x);
    }

    std::string to_string() const override { return "PARAM " + x; }
};

class Return;
using ReturnPtr = std::shared_ptr<Return>;
class Return : public Node {
 public:
  std::string x;  // 返回值，空字符串表示无返回值

  Return(const std::string& x = "") : x(x) {}

  static ReturnPtr create(const std::string& x = "") {
    return std::make_shared<Return>(x);
  }

  std::string to_string() const override {
    if (x.empty()) {
      return "RETURN";
    }
    return "RETURN " + x;
  }
};

class Dec;
using Decptr = std::shared_ptr<Dec>;
class Dec : public Node {
 public:
  std::string x;
  int k;

  Dec (const std::string& x, const int& k):x(x), k(k){}

  static Decptr create(const std::string x, const int& k){
    return std::make_shared<Dec>(x, k);
  }

  std::string to_string() const override {
    return "DEC " + x + " #" + std::to_string(k);
  }

};

class Global;
using GlobalPtr = std::shared_ptr<Global>;
class Global : public Node {
 public:
  std::string x;
  int size;
  std::vector<int> init_values;

  Global(const std::string& x, int size) : x(x), size(size) {}
  Global(const std::string& x, int size, const std::vector<int>& init_values)
      : x(x), size(size), init_values(init_values) {}

  static GlobalPtr create(const std::string& x, int size) {
    return std::make_shared<Global>(x, size);
  }

  static GlobalPtr create(const std::string& x, int size,
                          const std::vector<int>& init_values) {
    return std::make_shared<Global>(x, size, init_values);
  }

  std::string to_string() const override {
    std::string result = "GLOBAL " + x + ": #" + std::to_string(size);
    if (!init_values.empty()) {
      result += " = ";
      for (size_t i = 0; i < init_values.size(); ++i) {
        if (i > 0) {
          result += ", ";
        }
        result += "#" + std::to_string(init_values[i]);
      }
    }
    return result;
  }
};

class LoadAddr;
using LoadAddrPtr = std::shared_ptr<LoadAddr>;
class LoadAddr : public Node {
 public:
  std::string x;
  std::string label;

  LoadAddr(const std::string& x, const std::string& label)
      : x(x), label(label) {}

  static LoadAddrPtr create(const std::string& x, const std::string& label) {
    return std::make_shared<LoadAddr>(x, label);
  }

  std::string to_string() const override { return x + " = &" + label; }
};

class Load;
using LoadPtr = std::shared_ptr<Load>;
class Load : public Node {
 public:
  std::string x;
  std::string y;

  Load(const std::string& x, const std::string& y) : x(x), y(y) {}

  static LoadPtr create(const std::string& x, const std::string& y) {
    return std::make_shared<Load>(x, y);
  }

  std::string to_string() const override { return x + " = *" + y; }
};

class Store;
using StorePtr = std::shared_ptr<Store>;
class Store : public Node {
 public:
  std::string x;
  std::string y;

  Store(const std::string& x, const std::string& y) : x(x), y(y) {}

  static StorePtr create(const std::string& x, const std::string& y) {
    return std::make_shared<Store>(x, y);
  }

  std::string to_string() const override { return "*" + x + " = " + y; }
};

class LoadOffset;
using LoadOffsetPtr = std::shared_ptr<LoadOffset>;
class LoadOffset : public Node {
 public:
  std::string x;
  std::string y;
  int offset;

  LoadOffset(const std::string& x, const std::string& y, int offset)
      : x(x), y(y), offset(offset) {}

  static LoadOffsetPtr create(const std::string& x, const std::string& y,
                              int offset) {
    return std::make_shared<LoadOffset>(x, y, offset);
  }

  std::string to_string() const override {
    return x + " = *(" + y + " + #" + std::to_string(offset) + ")";
  }
};

class StoreOffset;
using StoreOffsetPtr = std::shared_ptr<StoreOffset>;
class StoreOffset : public Node {
 public:
  std::string x;
  int offset;
  std::string y;

  StoreOffset(const std::string& x, int offset, const std::string& y)
      : x(x), offset(offset), y(y) {}

  static StoreOffsetPtr create(const std::string& x, int offset,
                               const std::string& y) {
    return std::make_shared<StoreOffset>(x, offset, y);
  }

  std::string to_string() const override {
    return "*(" + x + " + #" + std::to_string(offset) + ") = " + y;
  }
};

using Code = std::list<NodePtr>;

}  // namespace IR



inline std::ostream& operator<<(std::ostream& os, const IR::Code& code) {
  for (const auto& node : code) {
    if (auto func = std::dynamic_pointer_cast<IR::Function>(node)) {
      os << func->to_string() << std::endl;
    } else if (auto label = std::dynamic_pointer_cast<IR::Label>(node)) {
      os << "  " << label->to_string() << std::endl;
    } else {
      os << "    " << node->to_string() << std::endl;
    }
  }
  return os;
}

#endif  // IR_IR_HPP