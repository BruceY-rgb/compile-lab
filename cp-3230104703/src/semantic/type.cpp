#include "type.hpp"
#include <memory>

#include "common.hpp"

bool PrimitiveType::compatible(const TypePtr& other) const {
  auto other_type = std::dynamic_pointer_cast<PrimitiveType>(other);
  return other_type && basic_type == other_type->basic_type;
}

bool FuncType::compatible(const TypePtr& other) const {
  auto other_type = std::dynamic_pointer_cast<FuncType>(other);
  if (!other_type || !return_type->compatible(other_type->return_type) ||
      param_types.size() != other_type->param_types.size()) {
    return false;
  }
  for (size_t i = 0; i < param_types.size(); i++) {
    if (!param_types[i]->compatible(other_type->param_types[i])) {
      return false;
    }
  }
  return true;
}

bool ArrayType::compatible(const TypePtr& other) const {
  auto other_type = std::dynamic_pointer_cast<ArrayType>(other);

  if(!other_type || dims != other_type->dims || !element_type || !other_type->element_type) {
    return false;
  }

  return element_type->compatible(other_type->element_type);
}

/*
不兼容：
对方不是数组
维度不一样
元素类型不一样
*/

