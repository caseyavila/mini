#ifndef UNUSED_RESULT_H
#define UNUSED_RESULT_H

#include "ast.h"

/* op -> block, instruction number */
using DefMap = std::unordered_map<aasm::Operand, std::pair<cfg::Ref, int>>;

DefMap definition_map(Function &func);
void unused_result(Program &prog);

#endif
