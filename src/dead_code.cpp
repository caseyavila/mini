#include "dead_code.h"
#include "aasm.h"
#include "cfg.h"
#include <unordered_map>
#include <variant>

/*
    implements critical instruction (mark-and-sweep)
    useless code elimination
*/

/* op -> block, instruction number */
using DefMap = std::unordered_map<aasm::Operand, std::pair<cfg::Ref, int>>;

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

std::vector<std::pair<cfg::Ref, int>> start_worklist(cfg::Function &func) {
    std::vector<std::pair<cfg::Ref, int>> worklist;

    auto lambda = [&](cfg::Ref &ref) {
        int i = 0;
        for (auto &ins : cfg_instructions(ref)) {
            if (std::holds_alternative<aasm::Call>(ins)) {
                worklist.emplace_back(ref, i);
            } else if (std::holds_alternative<aasm::Ret>(ins)) {
                worklist.emplace_back(ref, i);
            }
            i++;
        }
    };

    cfg_traverse(func.entry_ref, lambda);

    return worklist;
}

/* operands that an instruction depends on */
std::vector<aasm::Operand> dependencies(const cfg::Ref &ref, int idx) {
    aasm::Ins &ins = cfg_instructions(ref)[idx];

    if (auto *load = std::get_if<aasm::Load>(&ins)) {
        return { load->ptr };
    } else if (auto *str = std::get_if<aasm::Store>(&ins)) {
        return { str->ptr, str->value };
    } else if (auto *bin = std::get_if<aasm::Binary>(&ins)) {
        return { bin->left, bin->right };
    } else if (auto *call = std::get_if<aasm::Call>(&ins)) {
        return call->arguments;
    } else if (auto *phi = std::get_if<aasm::Phi>(&ins)) {
        std::vector<aasm::Operand> deps;
        for (auto &[_, op] : phi->bindings) {
            deps.emplace_back(op);
        }
        return deps;
    } else if (auto *gep = std::get_if<aasm::Gep>(&ins)) {
        return { gep->index, gep->value };
    } else if (auto *del = std::get_if<aasm::Free>(&ins)) {
        return { del->value };
    } else if (auto *ret = std::get_if<aasm::Ret>(&ins)) {
        if (ret->value.has_value()) {
            return { ret->value.value() };
        }
    } else if (auto *br = std::get_if<aasm::Br>(&ins)) {
        return { br->guard };
    }

    /* jmp, new */
    return { };
}

void dead_code_function(cfg::Function &func) {
    DefMap def_map = definition_map(func);
    std::vector<std::pair<cfg::Ref, int>> worklist = start_worklist(func);

    while (worklist.size() > 0) {
        auto [ref, idx] = worklist.back();
        worklist.pop_back();

        for (auto &dep : dependencies(ref, idx)) {

        }
    }
}

void dead_code_elim(cfg::Program &prog) {
    for (auto &[_, func] : prog.functions) {
        dead_code_function(func);
    }
}
