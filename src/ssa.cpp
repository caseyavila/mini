#include "aasm.h"
#include "ast.h"
#include "cfg.h"
#include <algorithm>
#include <string>
#include <unordered_map>
#include <variant>

using EditBlocks = std::unordered_map<std::string, std::vector<cfg::Ref>>;
using RefToRefs = std::map<cfg::Ref, std::set<cfg::Ref, cfg::RefOwnerLess>, cfg::RefOwnerLess>;

/* blocks that edit each variable */
EditBlocks edit_blocks(const cfg::Function &func) {
    EditBlocks result;

    auto edit_ref = [&](cfg::Ref &ref) {
        auto edit_insns = [&](std::vector<aasm::Ins> &insns) {
            for (auto &ins : insns) {
                if (auto *store = std::get_if<aasm::Store>(&ins)) {
                    if (auto *idv = std::get_if<aasm::Id>(&store->ptr.value)) {
                        if (func.local_env.contains(idv->id)) {
                            result[idv->id].emplace_back(ref);
                        }
                    }
                }
            }
        };

        if (auto *ret = std::get_if<std::shared_ptr<cfg::Return>>(&ref)) {
            edit_insns(ret->get()->instructions);
        } else if (auto *basic = std::get_if<std::shared_ptr<cfg::Basic>>(&ref)) {
            edit_insns(basic->get()->instructions);
        } else if (auto *cond = std::get_if<std::shared_ptr<cfg::Conditional>>(&ref)) {
            edit_insns(cond->get()->instructions);
        }
    };

    cfg_traverse(func.entry_ref, edit_ref);

    return result;
}

std::pair<RefToRefs, RefToRefs> preds_succs(const cfg::Ref &ref) {
    RefToRefs preds;
    RefToRefs succs;

    auto lambda = [&](cfg::Ref &ref) {
        preds[ref];
        succs[ref];

        if (auto *basic = std::get_if<std::shared_ptr<cfg::Basic>>(&ref)) {
            succs[ref].emplace(basic->get()->next);
            preds[basic->get()->next].emplace(ref);
        } else if (auto *cond = std::get_if<std::shared_ptr<cfg::Conditional>>(&ref)) {
            succs[ref].emplace(cond->get()->tru);
            succs[ref].emplace(cond->get()->fals);
            preds[cond->get()->tru].emplace(ref);
            preds[cond->get()->fals].emplace(ref);
        }
    };

    cfg_traverse(ref, lambda);
    return { preds, succs };
}

std::set<cfg::Ref, cfg::RefOwnerLess> intersection(std::vector<std::set<cfg::Ref, cfg::RefOwnerLess>> &sets) {
    std::set<cfg::Ref, cfg::RefOwnerLess> result;
    if (sets.size() == 0) return result;

    auto smallest = std::min_element(sets.begin(), sets.end(),
        [](const auto& a, const auto& b) { return a.size() < b.size(); });

    result = *smallest;

    for (auto it = sets.begin(); it != sets.end(); ++it) {
        if (it == smallest) {
            continue;
        }

        std::set<cfg::Ref, cfg::RefOwnerLess> curr;
        std::set_intersection(result.begin(), result.end(),
                              it->begin(), it->end(),
                              std::inserter(curr, curr.begin()),
                              cfg::RefOwnerLess {});

        result = curr;

        if (result.empty()) {
            break;
        }
    }

    return result;
}

std::vector<cfg::Ref> pre_order(const cfg::Ref &ref) {
    std::vector<cfg::Ref> result;

    auto lambda = [&](cfg::Ref &ref) {
        result.emplace_back(ref);
    };

    cfg_traverse(ref, lambda);
    return result;
}

bool equals(std::set<cfg::Ref, cfg::RefOwnerLess> &s1,
            std::set<cfg::Ref, cfg::RefOwnerLess> &s2) {
    if (s1.size() != s2.size()) {
        return false;
    }
    for (auto &ref : s1) {
        if (!s2.contains(ref)) {
            return false;
        }
    }

    return true;
}

RefToRefs dominators(const cfg::Ref &entry, RefToRefs &preds) {
    RefToRefs doms;

    std::vector<cfg::Ref> all_nodes = pre_order(entry);
    std::set<cfg::Ref, cfg::RefOwnerLess> all_nodes_set(all_nodes.begin(), all_nodes.end());

    /* dom = {every block -> all blocks} */
    for (auto &outer : all_nodes) {
        doms.emplace(outer, all_nodes_set);
    }

    doms[entry] = { entry };

    bool changed = true;
    while (changed) {
        changed = false;
        for (auto &ref : all_nodes) {
            if (doms[entry].contains(ref)) {
                continue;
            }

            std::set<cfg::Ref, cfg::RefOwnerLess> new_dom;
            std::vector<std::set<cfg::Ref, cfg::RefOwnerLess>> p_doms;

            for (auto &pred : preds[ref]) {
                p_doms.emplace_back(doms[pred]);
            }

            new_dom = intersection(p_doms);
            new_dom.emplace(ref);

            if (!equals(new_dom, doms[ref])) {
                changed = true;
            }

            doms[ref] = new_dom;
        }
    }

    return doms;
}

RefToRefs dom_tree(RefToRefs &doms) {
    RefToRefs tree;
    for (auto &[domer, domees] : doms) {
        if (domees.size() > 0) {
            tree[*domees.begin()].emplace(domer);
        }
    }
    return tree;
}

RefToRefs imm_dom(const cfg::Ref &entry, RefToRefs &preds, RefToRefs &doms) {
    RefToRefs idom;

    auto lambda = [&](cfg::Ref &ref) {
        std::set<cfg::Ref, cfg::RefOwnerLess> s = doms[ref];
        s.erase(ref);

        while (s.size() > 1) {
            auto left = *s.begin();
            s.erase(s.begin());

            auto right = *s.begin();
            s.erase(s.begin());

            if (doms[left].contains(right)) {
                s.emplace(left);
            } else {
                s.emplace(right);
            }
        }

        idom[ref] = s;
    };

    cfg_traverse(entry, lambda);
    return idom;
}

RefToRefs frontiers(const cfg::Ref &entry, RefToRefs &preds, RefToRefs &idom) {
    RefToRefs fronts;

    auto lambda = [&](cfg::Ref &ref) {
        fronts[ref];

        if (preds[ref].size() >= 2) {
            for (auto &pred : preds[ref]) {
                cfg::Ref runner = pred;

                while (!idom[ref].contains(runner)) {
                    fronts[runner].emplace(ref);
                    runner = *idom[runner].begin();
                }
            }
        }
    };

    cfg_traverse(entry, lambda);
    return fronts;
}

void place_phi_block(std::string id, cfg::Function &func, cfg::Ref &ref, std::vector<aasm::Ins> &insns, RefToRefs &preds, int idx) {
    for (auto &ins : insns) {
        if (auto *phi = std::get_if<aasm::Phi>(&ins)) {
            if (phi->id == id) {
                return;
            }
        } else {
            break;
        }
    }

    aasm::Phi phi = aasm::Phi { { aasm::Id { id + "." + std::to_string(idx) }, func.local_env.at(id) }, id, {} };
    for (auto &pred : preds[ref]) {
        phi.bindings.emplace(ref_weaken(pred), aasm::Operand { aasm::Id { id }, func.local_env.at(id) });
    }

    /* entry phi */
    if (cfg_equals(ref, func.entry_ref)) {
        phi.bindings.emplace(ref_weaken(ref), aasm::Operand { aasm::Id { id }, func.local_env.at(id) });
    }

    insns.insert(insns.begin(), phi);
}

void place_phis(cfg::Function& func, EditBlocks &edits, RefToRefs &fronts, RefToRefs &preds) {
    for (auto &[var, _] : edits) {
        int i = 0;
        int idx = 0;
        while (i < edits[var].size()) {
            for (cfg::Ref block : fronts[edits[var][i]]) {
                std::set<cfg::Ref, cfg::RefOwnerLess> container(edits[var].begin(), edits[var].end());
                if (!container.contains(block)) {
                    if (auto *basic = std::get_if<std::shared_ptr<cfg::Basic>>(&block)) {
                        place_phi_block(var, func, block, basic->get()->instructions, preds, idx);
                    } else if (auto *cond = std::get_if<std::shared_ptr<cfg::Conditional>>(&block)) {
                        place_phi_block(var, func, block, cond->get()->instructions, preds, idx);
                    } else if (auto *wcond = std::get_if<std::weak_ptr<cfg::Conditional>>(&block)) {
                        place_phi_block(var, func, block, wcond->lock().get()->instructions, preds, idx);
                    } else if (auto *ret = std::get_if<std::shared_ptr<cfg::Return>>(&block)) {
                        place_phi_block(var, func, block, ret->get()->instructions, preds, idx);
                    }
                    idx++;
                    edits[var].emplace_back(block);
                }
            }
            i++;
        }
    }
}

std::string phi_name(const std::string &id) {
    std::string real = id;
    if (real.find(".") != std::string::npos) {
        real = real.substr(0, real.find("."));
    }
    return real;
}

void rename_insns(std::vector<aasm::Ins> &insns,
        std::unordered_map<std::string, std::vector<aasm::Operand>> &stack,
        std::unordered_map<int, aasm::Operand> &vack) {
    std::vector<aasm::Ins> new_insns;

    std::function<aasm::Operand(aasm::Operand &)> lookup = [&](aasm::Operand &op) {
        if (auto *id = std::get_if<aasm::Id>(&op.value)) {
            if (stack.contains(phi_name(id->id))) {
                return stack.at(phi_name(id->id)).back();
            }
        } else if (auto *var = std::get_if<aasm::Var>(&op.value)) {
            if (vack.contains(var->id)) {
                return lookup(vack.at(var->id));
            }
        }
        return op;
    };

    for (auto &insn : insns) {
        if (auto *load = std::get_if<aasm::Load>(&insn)) {
            if (auto *var = std::get_if<aasm::Var>(&load->target.value)) {
                if (std::holds_alternative<aasm::Id>(load->ptr.value)) {
                    vack.emplace(var->id, load->ptr);
                    continue;
                }
            }
            new_insns.emplace_back(aasm::Load { lookup(load->target), lookup(load->ptr) });
        } else if (auto *store = std::get_if<aasm::Store>(&insn)) {
            if (auto *id = std::get_if<aasm::Id>(&store->ptr.value)) {
                stack[phi_name(id->id)].emplace_back(lookup(store->value));
                continue;
            }
            new_insns.emplace_back(aasm::Store { lookup(store->value), lookup(store->ptr) });
        } else if (auto *phi = std::get_if<aasm::Phi>(&insn)) {
            stack[phi_name(phi->id)].emplace_back(phi->target);
            new_insns.emplace_back(insn);
        } else if (auto *bin = std::get_if<aasm::Binary>(&insn)) {
            new_insns.emplace_back(aasm::Binary { bin->op, bin->target, lookup(bin->left), lookup(bin->right) });
        } else if (auto *gep = std::get_if<aasm::Gep>(&insn)) {
            new_insns.emplace_back(aasm::Gep { gep->target, lookup(gep->value), lookup(gep->index) });
        } else if (auto *free = std::get_if<aasm::Free>(&insn)) {
            new_insns.emplace_back(aasm::Free { lookup(free->value) });
        } else if (auto *br = std::get_if<aasm::Br>(&insn)) {
            new_insns.emplace_back(aasm::Br { lookup(br->guard), br->tru, br->fals });
        } else if (auto *call = std::get_if<aasm::Call>(&insn)) {
            std::vector<aasm::Operand> new_ops;
            for (auto &arg : call->arguments) {
                new_ops.emplace_back(lookup(arg));
            }
            new_insns.emplace_back(aasm::Call { call->target, call->id, new_ops });
        } else if (auto *ret = std::get_if<aasm::Ret>(&insn)) {
            if (ret->value.has_value()) {
                new_insns.emplace_back(aasm::Ret { lookup(ret->value.value()) });
            } else {
                new_insns.emplace_back(insn);
            }
        } else {
            new_insns.emplace_back(insn);
        }
    }

    insns = new_insns;
}

void edit_phi(std::vector<aasm::Ins> &insns, const cfg::Ref &ref,
        std::unordered_map<std::string, std::vector<aasm::Operand>> &stack,
        std::unordered_map<int, aasm::Operand> &vack) {
    int i = 0;
    while (i < insns.size()) {
        if (auto *phi = std::get_if<aasm::Phi>(&insns[i])) {
            if (phi->bindings.contains(ref_weaken(ref))) {
                if (stack.contains(phi_name(phi->id))) {
                    phi->bindings.at(ref_weaken(ref)) = stack.at(phi_name(phi->id)).back();
                    i++;
                } else {
                    insns.erase(insns.begin() + i);
                }
            }
        } else {
            break;
        }
    }
}

/* stack must not be a ref */
void rename_cfg(const cfg::Ref &ref, RefToRefs &tree, RefToRefs &succs,
        std::unordered_map<std::string, std::vector<aasm::Operand>> stack) {
    std::unordered_map<int, aasm::Operand> vack;

    if (auto *basic = std::get_if<std::shared_ptr<cfg::Basic>>(&ref)) {
        rename_insns(basic->get()->instructions, stack, vack);
    } else if (auto *cond = std::get_if<std::shared_ptr<cfg::Conditional>>(&ref)) {
        rename_insns(cond->get()->instructions, stack, vack);
    } else if (auto *ret = std::get_if<std::shared_ptr<cfg::Return>>(&ref)) {
        rename_insns(ret->get()->instructions, stack, vack);
    } else if (auto *wcond = std::get_if<std::weak_ptr<cfg::Conditional>>(&ref)) {
        rename_insns(wcond->lock().get()->instructions, stack, vack);
    }

    for (auto &succ : succs[ref]) {
        if (auto *basic = std::get_if<std::shared_ptr<cfg::Basic>>(&succ)) {
            edit_phi(basic->get()->instructions, ref, stack, vack);
        } else if (auto *cond = std::get_if<std::shared_ptr<cfg::Conditional>>(&succ)) {
            edit_phi(cond->get()->instructions, ref, stack, vack);
        } else if (auto *ret = std::get_if<std::shared_ptr<cfg::Return>>(&succ)) {
            edit_phi(ret->get()->instructions, ref, stack, vack);
        } else if (auto *wcond = std::get_if<std::weak_ptr<cfg::Conditional>>(&succ)) {
            edit_phi(wcond->lock().get()->instructions, ref, stack, vack);
        }
    }

    for (auto &domee : tree[ref]) {
        rename_cfg(domee, tree, succs, stack);
    }
}

void ssa_function(cfg::Program &prog, cfg::Function &func) {
    EditBlocks edits = edit_blocks(func);

    auto [preds, succs] = preds_succs(func.entry_ref);
    RefToRefs doms = dominators(func.entry_ref, preds);
    RefToRefs idom = imm_dom(func.entry_ref, preds, doms);
    RefToRefs fronts = frontiers(func.entry_ref, preds, idom);
    RefToRefs tree = dom_tree(idom);

    place_phis(func, edits, fronts, preds);

    std::unordered_map<std::string, std::vector<aasm::Operand>> stack;
    for (auto &decl : func.parameters) {
        stack[decl.id] = { aasm::Operand { aasm::Id { decl.id }, decl.type } };
    }

    rename_cfg(func.entry_ref, tree, succs, stack);
}

void ssa_program(cfg::Program &prog) {
    for (auto &[_, func] : prog.functions) {
        ssa_function(prog, func);
    }
}
