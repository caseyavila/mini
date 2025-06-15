#pragma once

#include "cfg.h"

using RefToRefs = std::map<cfg::Ref, std::set<cfg::Ref, cfg::RefOwnerLess>, cfg::RefOwnerLess>;

std::pair<RefToRefs, RefToRefs> preds_succs(const cfg::Ref &ref);
RefToRefs dominators(const cfg::Ref &entry, RefToRefs &preds, bool reorder);
RefToRefs imm_dom(const cfg::Ref &entry, RefToRefs &preds, RefToRefs &doms);
RefToRefs frontiers(const cfg::Ref &entry, RefToRefs &preds, RefToRefs &idom);
RefToRefs dom_tree(RefToRefs &doms);
void ssa_program(cfg::Program &prog);
