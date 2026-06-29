#include "ir_translator.hpp"

#include <cassert>

std::string IRTranslator::new_temp() {
  static int temp_count = 0;
  return "T" + std::to_string(temp_count++);
}

std::string IRTranslator::new_label() {
  static int label_count = 0;
  return "L" + std::to_string(label_count++);
}

std::string IRTranslator::var_name(const AST::VarDefPtr& node) const {
  return node->symbol ? node->symbol->unique_name : node->ident;
}

std::string IRTranslator::var_name(const AST::LValPtr& node) const {
  return node->symbol ? node->symbol->unique_name : node->ident;
}

std::string IRTranslator::param_name(const AST::FuncParamPtr& node) const {
  return node->symbol ? node->symbol->unique_name : node->ident;
}

std::string IRTranslator::scalar_addr(const std::string& name) const {
  return name + ".addr";
}

bool is_relop(BinaryOp op) {
  return op == BinaryOp::Lt || op == BinaryOp::Gt ||
         op == BinaryOp::Le || op == BinaryOp::Ge ||
         op == BinaryOp::Eq || op == BinaryOp::Neq;
}
int calc_array_size(std::vector<int> dims) {
  int size = 1;
  for(auto dim : dims) {
    size *= dim;
  }
  return size;

}

// 判断是否是数组
bool is_array_def(const AST::VarDefPtr& node) {
  return !node->dims.empty();
}

static int calc_array_element_count(const std::vector<int>& dims) {
  int count = 1;
  for (int dim : dims) {
    count *= dim;
  }
  return count;
}

// 计算一个变量需要的字节数
static int calc_var_size(const AST::VarDefPtr& node) {
  if (node->dims.empty()) {
    return 4;
  }
  return calc_array_element_count(node->dims) * 4;
}

// 把嵌套的初始化列表变成一个一维列表
static void flatten_init(AST::NodePtr init, std::vector<AST::NodePtr>& values) {
  if (!init) {
    return;
  }

  if (auto init_list = std::dynamic_pointer_cast<AST::InitList>(init)) {
    for (auto& value : init_list->values) {
      flatten_init(value, values);
    }
  } else {
    values.push_back(init);
  }
}

static size_t array_capacity(const std::vector<int>& dims, size_t start = 0) {
  size_t result = 1;
  for (size_t i = start; i < dims.size(); ++i) {
    result *= static_cast<size_t>(dims[i]);
  }
  return result;
}

static std::vector<AST::NodePtr> flatten_array_init(
    const std::vector<int>& dims, AST::InitListPtr init) {
  std::vector<AST::NodePtr> values;
  size_t total = array_capacity(dims);

  if (!init) {
    values.resize(total);
    return values;
  }

  for (auto& value : init->values) {
    if (values.size() >= total) {
      break;
    }

    auto nested = std::dynamic_pointer_cast<AST::InitList>(value);
    if (!nested) {
      values.push_back(value);
      continue;
    }

    bool matched = false;
    for (size_t start = 1; start < dims.size(); ++start) {
      size_t sub_size = array_capacity(dims, start);
      if (sub_size != 0 && values.size() % sub_size == 0) {
        std::vector<int> sub_dims(dims.begin() + start, dims.end());
        auto sub_values = flatten_array_init(sub_dims, nested);
        values.insert(values.end(), sub_values.begin(), sub_values.end());
        matched = true;
        break;
      }
    }

    if (!matched) {
      flatten_init(value, values);
    }
  }

  if (values.size() > total) {
    values.resize(total);
  }
  values.resize(total);
  return values;
}

//将全局变量初始化节点转换成整数值
// int g = 5;用于将5返回
static int eval_global_init_int(AST::NodePtr node) {
  if (!node) {
    return 0;
  }
  auto int_const = std::dynamic_pointer_cast<AST::IntConst>(node);
  ASSERT(int_const != nullptr, "global initializer should be IntConst");
  return int_const->value;
}

IR::Code IRTranslator::translate(AST::NodePtr node) {
  if (!node) {
    return {};
  }
#define TRANSLATE_NODE(type)                                 \
  if (auto n = std::dynamic_pointer_cast<AST::type>(node)) { \
    return translate##type(n);                               \
  }
  // 递归翻译 AST 的每个节点
  // 如果你添加了新的 AST 节点类型，记得在这里添加对应的翻译函数

  TRANSLATE_NODE(CompUnit)
  TRANSLATE_NODE(FuncDef)
  TRANSLATE_NODE(Block)
  TRANSLATE_NODE(VarDecl)
  TRANSLATE_NODE(VarDef)
  TRANSLATE_NODE(AssignStmt)
  TRANSLATE_NODE(ReturnStmt)
  TRANSLATE_NODE(LVal)
  TRANSLATE_NODE(BinaryExp)
  TRANSLATE_NODE(UnaryExp)
  TRANSLATE_NODE(FuncCall)
  TRANSLATE_NODE(IntConst)
  TRANSLATE_NODE(IfStmt)
  TRANSLATE_NODE(WhileStmt)

#warning Add more AST node types if needed

#undef TRANSLATE_NODE

  ASSERT(false,
         "Unknown AST node type " + node->to_string() + " in IR translation");
}

// 把exp表达式的计算结果放到place这个临时变量中
IR::Code IRTranslator::translateExp(AST::NodePtr node,
                                    const std::string& place) {
#define TRANSLATE_EXP_NODE(type)                             \
  if (auto n = std::dynamic_pointer_cast<AST::type>(node)) { \
    return translate##type(n, place);                        \
  }

  TRANSLATE_EXP_NODE(BinaryExp)
  TRANSLATE_EXP_NODE(UnaryExp)
  TRANSLATE_EXP_NODE(FuncCall)
  TRANSLATE_EXP_NODE(IntConst)
  TRANSLATE_EXP_NODE(LVal)

#warning Add more AST node types i needed

#undef TRANSLATE_EXP_NODE

  ASSERT(false, "No translateExp for node " + node->to_string());
}

IR::Code IRTranslator::translateCompUnit(AST::CompUnitPtr node) {
  IR::Code ir;
  for (auto& unit : node->units) {
    if (!std::dynamic_pointer_cast<AST::VarDecl>(unit)) {
      continue;
    }
    auto unit_ir = translate(unit);
    std::move(unit_ir.begin(), unit_ir.end(), std::back_inserter(ir));
  }
  for (auto& unit : node->units) {
    if (std::dynamic_pointer_cast<AST::VarDecl>(unit)) {
      continue;
    }
    auto unit_ir = translate(unit);
    std::move(unit_ir.begin(), unit_ir.end(), std::back_inserter(ir));
  }
  return ir;
}

std::string IRTranslator::get_global_addr(const std::string& name) {
  auto it = global_addr_cache.find(name);
  if (it == global_addr_cache.end()) {
    std::string base_place = new_temp();
    global_addr_cache[name] = base_place;
    current_func_load_addrs.push_back(IR::LoadAddr::create(base_place, name));
    return base_place;
  }
  return it->second;
}

IR::Code IRTranslator::translateFuncDef(AST::FuncDefPtr node) {
  IR::Code ir;
  ir.push_back(IR::Function::create(node->name));
  
  global_addr_cache.clear();
  current_func_load_addrs.clear();

  if(node->params) {
    for(auto& param : node->params->params) {
      auto name = param_name(param);
      if (param->is_array) {
        array_dims[name] = param->array_dims;
        ir.push_back(IR::Param::create(name));
      } else {
        auto incoming = name + ".param";
        scalar_vars.insert(name);
        ir.push_back(IR::Param::create(incoming));
        ir.push_back(IR::Dec::create(scalar_addr(name), 4));
        ir.push_back(IR::Store::create(scalar_addr(name), incoming));
      }
    }
  }

  bool old_global_scope = in_global_scope;
  in_global_scope = false;

  auto block_ir = translate(node->block);
  std::move(current_func_load_addrs.begin(), current_func_load_addrs.end(), std::back_inserter(ir));
  std::move(block_ir.begin(), block_ir.end(), std::back_inserter(ir));
  ir.push_back(IR::Return::create());
  in_global_scope = old_global_scope;
  return ir;
}

IR::Code IRTranslator::translateBlock(AST::BlockPtr node) {
  IR::Code ir;
  for (auto& stmt : node->stmts) {
    auto stmt_ir = translate(stmt);
    std::move(stmt_ir.begin(), stmt_ir.end(), std::back_inserter(ir));
  }
  return ir;
}

IR::Code IRTranslator::translateCond(
                                      AST::NodePtr node, 
                                      const std::string& true_label, 
                                      const std::string& false_label) {
  IR::Code ir;

  if(auto binary = std::dynamic_pointer_cast<AST::BinaryExp>(node)) {
    if(binary->op == BinaryOp::And) {
      auto mid_label = new_label();
      
      auto left_ir = translateCond(binary->left, mid_label, false_label);
      std::move(left_ir.begin(), left_ir.end(), std::back_inserter(ir));

      ir.push_back(IR::Label::create(mid_label));

      auto right_ir = translateCond(binary->right, true_label, false_label);
      std::move(right_ir.begin(), right_ir.end(), std::back_inserter(ir));

      return ir;
    }

    if(binary->op == BinaryOp::Or) {
      auto mid_label = new_label();

      auto left_ir = translateCond(binary->left, true_label, mid_label);
      std::move(left_ir.begin(), left_ir.end(), std::back_inserter(ir));
      ir.push_back(IR::Label::create(mid_label));

      auto right_ir = translateCond(binary->right, true_label, false_label);
      std::move(right_ir.begin(), right_ir.end(), std::back_inserter(ir));

      return ir;
    }

    if (is_relop(binary->op)) {
      auto left_place = new_temp();
      auto right_place = new_temp();

      auto left_ir = translateExp(binary->left, left_place);
      auto right_ir = translateExp(binary->right, right_place);

      std::move(left_ir.begin(), left_ir.end(), std::back_inserter(ir));
      std::move(right_ir.begin(), right_ir.end(), std::back_inserter(ir));

      ir.push_back(IR::If::create(left_place, binary->op, right_place, true_label));
      ir.push_back(IR::Goto::create(false_label));

      return ir;
    }
  }

  if(auto unary = std::dynamic_pointer_cast<AST::UnaryExp>(node)) {
    if(unary->op == UnaryOp::Lnot) {
      auto not_ir = translateCond(unary->exp, false_label, true_label);
      std::move(not_ir.begin(), not_ir.end(), std::back_inserter(ir));
      return ir;
    }
  }
  // 普通表达式作为条件：if(x)
  auto cond_place = new_temp();
  auto zero_place = new_temp();

  auto cond_ir = translateExp(node, cond_place);
  std::move(cond_ir.begin(), cond_ir.end(), std::back_inserter(ir));

  ir.push_back(IR::LoadImm::create(zero_place, 0));
  ir.push_back(IR::If::create(cond_place, BinaryOp::Neq, zero_place, true_label));
  ir.push_back(IR::Goto::create(false_label));

  return ir;
}

IR::Code IRTranslator::translateIfStmt(AST::IfStmtPtr node) {
  IR::Code ir;

  auto true_label = new_label();
  auto false_label = new_label();
  auto end_label = new_label();

  auto cond_ir = translateCond(node->cond, true_label, false_label);
  std::move(cond_ir.begin(), cond_ir.end(), std::back_inserter(ir));

  ir.push_back(IR::Label::create(true_label));

  auto then_ir = translate(node->then_stmt);
  std::move(then_ir.begin(), then_ir.end(), std::back_inserter(ir));
  
  ir.push_back(IR::Goto::create(end_label));
  ir.push_back(IR::Label::create(false_label));

  if (node->else_stmt) { 
    auto else_ir = translate(node->else_stmt);
    std::move(else_ir.begin(), else_ir.end(), std::back_inserter(ir));
  }

  ir.push_back(IR::Label::create(end_label));

  return ir;
}

IR::Code IRTranslator::translateWhileStmt(AST::WhileStmtPtr node) { 
  IR::Code ir;

  auto begin_label = new_label();
  auto body_label = new_label();
  auto end_label = new_label();

  ir.push_back(IR::Label::create(begin_label));

  auto cond_ir = translateCond(node->cond, body_label, end_label);
  std::move(cond_ir.begin(), cond_ir.end(), std::back_inserter(ir));

  ir.push_back(IR::Label::create(body_label));

  auto body_ir = translate(node->stmt);
  std::move(body_ir.begin(), body_ir.end(), std::back_inserter(ir));

  ir.push_back(IR::Goto::create(begin_label));
  ir.push_back(IR::Label::create(end_label));

  return ir;
}

IR::Code IRTranslator::translateVarDecl(AST::VarDeclPtr node) {
  IR::Code ir;
  for (auto& def : node->defs) {
    auto def_ir = translate(def);
    std::move(def_ir.begin(), def_ir.end(), std::back_inserter(ir));
  }
  return ir;
}

IR::Code IRTranslator::translateVarDef(AST::VarDefPtr node) {
  IR::Code ir;

  auto name = var_name(node);
  bool is_array = !node->dims.empty();
  if(is_array) {
    array_dims[name] = node->dims;
  } else {
    scalar_vars.insert(name);
  } 
  if(in_global_scope) {
    global_names.insert(name);
  }
  int size = calc_var_size(node);
  int element_count = is_array ? calc_array_element_count(node->dims) : 1;

  std::vector<AST::NodePtr> init_values;
  if (node->init) {
    if (is_array) {
      init_values = flatten_array_init(
          node->dims, std::dynamic_pointer_cast<AST::InitList>(node->init));
    } else {
      flatten_init(node->init, init_values);
    }
  }

  // 1. 全局变量 / 全局数组
  if (in_global_scope) {
    std::vector<int> global_init_values;

    if (node->init) {
      if (is_array) {
        for (int i = 0; i < element_count; ++i) {
          if (i < static_cast<int>(init_values.size())) {
            global_init_values.push_back(eval_global_init_int(init_values[i]));
          } else {
            global_init_values.push_back(0);
          }
        }
      } else {
        if (!init_values.empty()) {
          global_init_values.push_back(eval_global_init_int(init_values[0]));
        } else {
          global_init_values.push_back(0);
        }
      }
    }

    ir.push_back(IR::Global::create(name, size, global_init_values));
    return ir;
  }

  // 2. 局部数组
  if (is_array) {
    ir.push_back(IR::Dec::create(name, size));

    if (node->init) {
      for (int i = 0; i < element_count; ++i) {
        auto value_place = new_temp();

        if (i < static_cast<int>(init_values.size()) && init_values[i]) {
          auto value_ir = translateExp(init_values[i], value_place);
          std::move(value_ir.begin(), value_ir.end(), std::back_inserter(ir));
        } else {
          ir.push_back(IR::LoadImm::create(value_place, 0));
        }

        ir.push_back(IR::StoreOffset::create(
            name,
            i * 4,
            value_place));
        // 用于给局部数组的某个元素赋初值 *(数组名 + #偏移量) = 值
      }
    }

    return ir;
  }

  // 3. 局部普通变量
  ir.push_back(IR::Dec::create(scalar_addr(name), 4));
  if (node->init) {
    auto value_place = new_temp();

    if (!init_values.empty()) {
      auto value_ir = translateExp(init_values[0], value_place);
      std::move(value_ir.begin(), value_ir.end(), std::back_inserter(ir));
    } else {
      ir.push_back(IR::LoadImm::create(value_place, 0));
    }

    ir.push_back(IR::Store::create(scalar_addr(name), value_place));
  }

  return ir;
}

IR::Code IRTranslator::translateValAddr(AST::LValPtr node, const std::string& place) {
  IR::Code ir;
  
  ASSERT(!node->indices.empty(), "translateLValAddr requires array indices");
  auto name = var_name(node);
  std::string base_place;

  if(global_names.count(name)) {
    base_place = get_global_addr(name);
  }else {
    base_place = name;
  }

  // offset = 0

  auto offset_place = new_temp();
  ir.push_back(IR::LoadImm::create(offset_place, 0));

  std::vector<int> dims;
  if(array_dims.count(name)) {
    dims = array_dims[name];
  }

  for(size_t i = 0; i < node->indices.size(); ++i) {
    auto index_place = new_temp();
    auto index_ir = translateExp(node->indices[i], index_place);

    std::move(index_ir.begin(), index_ir.end(), std::back_inserter(ir));

    int stride = 4;

    // 多维数组按行有限展开
    // a[i][j], 如果dims = {m,n}
    //offset = i * n * 4 + j * 4
    if(!dims.empty() && i + 1 < dims.size()) {
      stride = 4;
      for(size_t j = i + 1; j < dims.size(); ++j) {
        stride *= dims[j];
      }
    }

    auto term_place = new_temp();
    ir.push_back(IR::Binary::create(term_place, index_place,
                                    BinaryOp::Mul, "#" + std::to_string(stride)));
    auto new_offset_place = new_temp();
    ir.push_back(IR::Binary::create(new_offset_place, offset_place, BinaryOp::Add, term_place));

    offset_place = new_offset_place;
  }
  // place = base * offset
  ir.push_back(IR::Binary::create(place, base_place, BinaryOp::Add, offset_place));
  return ir;
}

IR::Code IRTranslator::translateAssignStmt(AST::AssignStmtPtr node) {
  IR::Code ir;

  auto rhs_place = new_temp();
  auto rhs_ir = translateExp(node->exp, rhs_place);
  std::move(rhs_ir.begin(), rhs_ir.end(), std::back_inserter(ir));

  // ASSERT(node->lval->indices.empty(), "array assignment is not implemented yet");
  if(node->lval->indices.empty()){
    auto lhs_name = var_name(node->lval);
    if(global_names.count(lhs_name)) {
      auto addr_place = get_global_addr(lhs_name);
      ir.push_back(IR::Store::create(addr_place, rhs_place));
    }else {
      ir.push_back(IR::Store::create(scalar_addr(lhs_name), rhs_place));
    }

    return ir;
  } 

  // 数组元素赋值：a[i] = value /mat[i][j] = value
  auto addr_place = new_temp();
  auto addr_ir = translateValAddr(node->lval, addr_place);
  std::move(addr_ir.begin(), addr_ir.end(), std::back_inserter(ir));

  ir.push_back(IR::Store::create(addr_place, rhs_place));
  return ir;


  
  // auto lnode = node->lval;
  // auto rnode = node->exp;

  // // 翻译左值和右值
  // auto right_place = new_temp();
  // auto right_ir = translateExp(rnode, right_place);
  // std::move(right_ir.begin(), right_ir.end(), std::back_inserter(ir));

  // ir.push_back(IR::Assign::create(node->lval->ident, right_place));

  // return ir;
}

IR::Code IRTranslator::translateReturnStmt(AST::ReturnStmtPtr node) {
  IR::Code ir;

  // 翻译返回值
  // 如果有返回值，则：
  // place = new_temp();
  // auto exp_ir = translateExp(node->exp, place);
  // return exp_ir + [RETURN place];
  // 否则：
  // return [RETURN];
  if(node->exp) {
    auto place = new_temp();
    auto exp_ir = translateExp(node->exp, place);
    std::move(exp_ir.begin(), exp_ir.end(), std::back_inserter(ir));
    ir.push_back(IR::Return::create(place));
  } else {
    ir.push_back(IR::Return::create());
  }

  return ir;
}

IR::Code IRTranslator::translateLVal(AST::LValPtr node,
                                     const std::string& place) {
  IR::Code ir;

  if (place.empty()) {
    return ir;
  }
  auto name = var_name(node);

  // 数组变量本身作为参数传递：foo(a)
  // 此时 a 表示数组首地址，不是 a[0] 的值
  if (node->indices.empty() && array_dims.count(name)) {
    if (global_names.count(name)) {
      auto addr_place = get_global_addr(name);
      ir.push_back(IR::Assign::create(place, addr_place));
    } else {
      ir.push_back(IR::Assign::create(place, name));
    }
    return ir;
  }

  // 普通变量读取
  if (node->indices.empty()) {
    if (global_names.count(name)) {
      auto addr_place = get_global_addr(name);
      ir.push_back(IR::Load::create(place, addr_place));
    } else {
      ir.push_back(IR::Load::create(place, scalar_addr(name)));
    }
    return ir;
  }

  // 数组元素读取，或 a[i] 作为子数组实参时返回地址。
  auto addr_place = new_temp();
  auto addr_ir = translateValAddr(node, addr_place);
  std::move(addr_ir.begin(), addr_ir.end(), std::back_inserter(ir));

  auto dims_it = array_dims.find(name);
  if (dims_it != array_dims.end() && node->indices.size() < dims_it->second.size()) {
    ir.push_back(IR::Assign::create(place, addr_place));
  } else {
    ir.push_back(IR::Load::create(place, addr_place));
  }

  return ir;
}

IR::Code IRTranslator::translateBinaryExp(AST::BinaryExpPtr node,
                                          const std::string& place) {
  IR::Code ir;
  auto left_place = new_temp();
  auto right_place = new_temp();

  // 翻译左右子表达式
  auto left_ir = translateExp(node->left, left_place);
  auto right_ir = translateExp(node->right, right_place);

  std::move(left_ir.begin(), left_ir.end(), std::back_inserter(ir));
  std::move(right_ir.begin(), right_ir.end(), std::back_inserter(ir));

  // 添加二元运算指令
  if (!place.empty()) {
    ir.push_back(IR::Binary::create(place, left_place, node->op, right_place));
  }
  return ir;
}

IR::Code IRTranslator::translateUnaryExp(AST::UnaryExpPtr node,
                                         const std::string& place) {
  IR::Code ir;

  // 翻译子表达式

  auto src_place = new_temp();
  auto exp_ir = translateExp(node->exp, src_place);
  std::move(exp_ir.begin(), exp_ir.end(), std::back_inserter(ir));
  // 如果 place 不为空，则将 UnaryExp 的值赋给 place

  if(!place.empty()) {
    if (node->op == UnaryOp::Lnot) {
      auto result_addr = new_temp() + ".addr";
      auto true_label = new_label();
      auto false_label = new_label();
      auto end_label = new_label();
      auto zero_place = new_temp();

      ir.push_back(IR::Dec::create(result_addr, 4));
      ir.push_back(IR::LoadImm::create(zero_place, 0));
      ir.push_back(IR::If::create(src_place, BinaryOp::Eq, zero_place, true_label));
      ir.push_back(IR::Goto::create(false_label));

      ir.push_back(IR::Label::create(true_label));
      auto one_place = new_temp();
      ir.push_back(IR::LoadImm::create(one_place, 1));
      ir.push_back(IR::Store::create(result_addr, one_place));
      ir.push_back(IR::Goto::create(end_label));

      ir.push_back(IR::Label::create(false_label));
      auto false_zero_place = new_temp();
      ir.push_back(IR::LoadImm::create(false_zero_place, 0));
      ir.push_back(IR::Store::create(result_addr, false_zero_place));

      ir.push_back(IR::Label::create(end_label));
      ir.push_back(IR::Load::create(place, result_addr));
    } else {
      ir.push_back(IR::Unary::create(place, node->op, src_place));
    }
  }


  return ir;
}

IR::Code IRTranslator::translateFuncCall(AST::FuncCallPtr node,
                                         const std::string& place) {
  IR::Code ir;
  std::vector<std::string> arg_places;

  // 首先翻译参数表达式，并存在临时变量中
  for(auto& arg : node->args) {
    auto arg_place = new_temp();
    auto arg_ir = translateExp(arg, arg_place);
    std::move(arg_ir.begin(), arg_ir.end(), std::back_inserter(ir));
    arg_places.push_back(arg_place);
  }
  // 接下来，添加参数传递指令和函数调用指令
  for(auto& arg_place : arg_places) {
    ir.push_back(IR::Arg::create(arg_place));
  }
  // 如果 place 不为空，则将函数调用的返回值赋给 place
  if(!place.empty()) {
    ir.push_back(IR::Call::create(place, node->name));  
  } else {
    ir.push_back(IR::Call::create(node->name));
  }
  return ir;
}

IR::Code IRTranslator::translateIntConst(AST::IntConstPtr node,
                                         const std::string& place) {
  IR::Code ir;
  // 添加赋值常量指令
  if (!place.empty()) {
    ir.push_back(IR::LoadImm::create(place, node->value));
  }
  return ir;
}
