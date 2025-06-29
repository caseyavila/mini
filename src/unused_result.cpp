#include "unused_result.h"
#include "aasm.h"
#include "cfg.h"
#include <unordered_set>
#include <variant>

/*
    implements SSA-based unused result elimination
*/

using Marks = std::map<cfg::Ref, std::unordered_set<int>, cfg::RefOwnerLess>;

void map_instruction(DefMap &def_map, aasm::Ins &ins, cfg::Ref &ref, int idx) {
    std::pair<cfg::Ref, int> here = { ref, idx };

    if (auto *load = std::get_if<aasm::Load>(&ins)) {
        def_map.emplace(load->target, here);
    } else if (auto *bin = std::get_if<aasm::Binary>(&ins)) {
        def_map.emplace(bin->target, here);
    } else if (auto *call = std::get_if<aasm::Call>(&ins)) {
        if (call->target.has_value()) {
            def_map.emplace(call->target.value(), here);
        }
    } else if (auto *phi = std::get_if<aasm::Phi>(&ins)) {
        def_map.emplace(phi->target, here);
    } else if (auto *gep = std::get_if<aasm::Gep>(&ins)) {
        def_map.emplace(gep->target, here);
    } else if (auto *ns = std::get_if<aasm::NewS>(&ins)) {
        def_map.emplace(ns->target, here);
    } else if (auto *na = std::get_if<aasm::NewA>(&ins)) {
        def_map.emplace(na->target, here);
    }
}

DefMap definition_map(cfg::Function &func) {
    DefMap def_map;

    auto lambda = [&](cfg::Ref &ref) {
        int i = 0;
        for (auto &ins : cfg_instructions(ref)) {
            map_instruction(def_map, ins, ref, i);
            i++;
        }
    };

    cfg_traverse(func.entry_ref, lambda);

    return def_map;
}

void in_op_traverse(aasm::Ins &ins, std::function<void(aasm::Operand &)> lambda) {
    if (auto *load = std::get_if<aasm::Load>(&ins)) {
        lambda(load->ptr);
    } else if (auto *str = std::get_if<aasm::Store>(&ins)) {
        lambda(str->ptr);
        lambda(str->value);
    } else if (auto *bin = std::get_if<aasm::Binary>(&ins)) {
        lambda(bin->left);
        lambda(bin->right);
    } else if (auto *call = std::get_if<aasm::Call>(&ins)) {
        for (auto &arg : call->arguments) {
            lambda(arg);
        }
    } else if (auto *phi = std::get_if<aasm::Phi>(&ins)) {
        std::vector<aasm::Operand> deps;
        for (auto &[_, op] : phi->bindings) {
            lambda(op);
        }
    } else if (auto *gep = std::get_if<aasm::Gep>(&ins)) {
        lambda(gep->index);
        lambda(gep->value);
    } else if (auto *del = std::get_if<aasm::Free>(&ins)) {
        lambda(del->value);
    } else if (auto *ret = std::get_if<aasm::Ret>(&ins)) {
        if (ret->value.has_value()) {
            lambda(ret->value.value());
        }
    } else if (auto *br = std::get_if<aasm::Br>(&ins)) {
        lambda(br->guard);
    }
}

std::unordered_set<aasm::Operand> useful_ops(cfg::Function &func) {
    std::unordered_set<aasm::Operand> useful;

    auto emplace = [&](aasm::Operand &op) {
        useful.emplace(op);
    };

    auto lambda = [&](cfg::Ref &ref) {
        for (auto &ins : cfg_instructions(ref)) {
            in_op_traverse(ins, emplace);
        }
    };

    cfg_traverse(func.entry_ref, lambda);

    return useful;
}

bool remove_instructions(cfg::Function &func, Marks &marks) {
    bool same = true;

    auto lambda = [&](cfg::Ref &ref) {
        std::vector<aasm::Ins> new_instructions;
        int i = 0;
        for (auto &ins : cfg_instructions(ref)) {
            if (!marks.contains(ref) || !marks[ref].contains(i)) {
                new_instructions.emplace_back(ins);
            } else {
                //std::cerr << "dce: eliminating instruction\n";
                same = false;
            }
            i++;
        }
        cfg_instructions(ref) = new_instructions;
    };

    cfg_traverse(func.entry_ref, lambda);

    return same;
}

void unused_result_function(cfg::Function &func) {
    bool done = false;

    while (!done) {
        DefMap def_map = definition_map(func);
        std::unordered_set<aasm::Operand> useful = useful_ops(func);
        Marks marks;

        for (auto &[op, loc] : def_map) {
            if (!useful.contains(op)) {
                marks[loc.first].emplace(loc.second);
            }
        }

        done = remove_instructions(func, marks);
    }
}

void unused_result(cfg::Program &prog) {
    for (auto &[_, func] : prog.functions) {
        unused_result_function(func);
    }
}
