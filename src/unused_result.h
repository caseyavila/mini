#pragma once

#include "cfg.h"

/* op -> block, instruction number */
using DefMap = std::unordered_map<aasm::Operand, std::pair<cfg::Ref, int>>;

DefMap definition_map(cfg::Function &func);
void unused_result(cfg::Program &prog);
