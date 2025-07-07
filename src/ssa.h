#pragma once

#include "cfg.h"

using RefToRefs = std::map<cfg::Ref, std::set<cfg::Ref, cfg::RefOwnerLess>, cfg::RefOwnerLess>;

void ssa_program(Program &prog);
