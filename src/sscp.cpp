#include "sscp.h"
#include "aasm.h"
#include "cfg.h"
#include "unused_result.h"
#include <unordered_map>
#include <variant>

bool val_eq(const sscp::Value &val, const bool &b) {
    if (auto val_b = std::get_if<bool>(&val)) {
        return *val_b == b;
    }
    return false;
}

using UseMap = std::unordered_map<aasm::Operand, std::vector<std::pair<cfg::Ref, int>>>;
using ValMap = std::unordered_map<aasm::Operand, sscp::Value>;

UseMap use_map(Function &func, DefMap &def_map) {
    UseMap u_map;

    auto lambda = [&](cfg::Ref &ref) {
        int i = 0;

        auto emplace_back = [&](aasm::Operand &op) {
            if (def_map.contains(op)) {
                u_map[op].emplace_back(ref, i);
            }
        };

        for (auto &ins : cfg_instructions(ref)) {
            in_op_traverse(ins, emplace_back);
            i++;
        }
    };

    cfg_traverse(func.entry_ref, lambda);

    return u_map;
}

sscp::Value op_value(const aasm::Operand &op, ValMap &value_map) {
    if (auto *imm = std::get_if<aasm::Imm>(&op.value)) {
        return imm->val;
    } else if (auto *immb = std::get_if<aasm::ImmB>(&op.value)) {
        return immb->val;
    } else if (std::holds_alternative<aasm::Glob>(op.value)) {
        return sscp::Bot {};
    } else if (std::holds_alternative<aasm::Id>(op.value)) {
        return sscp::Bot {};
    } else if (std::holds_alternative<aasm::Var>(op.value)) {
        if (value_map.contains(op)) {
            return value_map.at(op);
        } else {
            return sscp::Top {};
        }
    } else {
        return sscp::Null {};
    }
}

/* only considers phi, binary instructions */
std::unordered_set<aasm::Operand> update_value_map(ValMap &value_map, DefMap &def_map) {
    std::unordered_set<aasm::Operand> new_const;

    for (auto &[op, val] : value_map) {
        if (!std::holds_alternative<sscp::Top>(val)) {
            continue;
        }

        aasm::Ins ins = cfg_instructions(def_map.at(op).first)[def_map.at(op).second];

        if (auto *bin = std::get_if<aasm::Binary>(&ins)) {
            sscp::Value left_val = op_value(bin->left, value_map);
            sscp::Value right_val = op_value(bin->right, value_map);

            if (std::holds_alternative<aasm::Or>(bin->op)) {
                if (val_eq(left_val, true) || val_eq(right_val, true)) {
                    value_map[op] = true;
                    new_const.emplace(op);
                    continue;
                }
            }

            if (std::holds_alternative<aasm::And>(bin->op)) {
                if (val_eq(left_val, false) || val_eq(right_val, false)) {
                    value_map[op] = false;
                    new_const.emplace(op);
                    continue;
                }
            }

            if (std::holds_alternative<sscp::Bot>(left_val) ||
                    std::holds_alternative<sscp::Bot>(right_val)) {
                value_map[op] = sscp::Bot {};
                continue;
            }

            if (std::holds_alternative<sscp::Top>(left_val) ||
                    std::holds_alternative<sscp::Top>(right_val)) {
                value_map[op] = sscp::Top {};
                continue;
            }

            sscp::Value bin_val;

            if (std::holds_alternative<aasm::Add>(bin->op)) {
                bin_val = *std::get_if<int64_t>(&left_val) + *std::get_if<int64_t>(&right_val);
            } else if (std::holds_alternative<aasm::Sub>(bin->op)) {
                bin_val = *std::get_if<int64_t>(&left_val) - *std::get_if<int64_t>(&right_val);
            } else if (std::holds_alternative<aasm::Mul>(bin->op)) {
                bin_val = *std::get_if<int64_t>(&left_val) * *std::get_if<int64_t>(&right_val);
            } else if (std::holds_alternative<aasm::Div>(bin->op)) {
                if (*std::get_if<int64_t>(&right_val) == 0) {
                    bin_val = sscp::Bot {};
                } else {
                    bin_val = *std::get_if<int64_t>(&left_val) / *std::get_if<int64_t>(&right_val);
                }
            } else if (std::holds_alternative<aasm::And>(bin->op)) {
                bin_val = *std::get_if<bool>(&left_val) && *std::get_if<bool>(&right_val);
            } else if (std::holds_alternative<aasm::Or>(bin->op)) {
                bin_val = *std::get_if<bool>(&left_val) || *std::get_if<bool>(&right_val);
            } else if (std::holds_alternative<aasm::Xor>(bin->op)) {
                bin_val = *std::get_if<bool>(&left_val) != *std::get_if<bool>(&right_val);
            } else if (std::holds_alternative<aasm::Gt>(bin->op)) {
                bin_val = *std::get_if<int64_t>(&left_val) > *std::get_if<int64_t>(&right_val);
            } else if (std::holds_alternative<aasm::Ge>(bin->op)) {
                bin_val = *std::get_if<int64_t>(&left_val) >= *std::get_if<int64_t>(&right_val);
            } else if (std::holds_alternative<aasm::Lt>(bin->op)) {
                bin_val = *std::get_if<int64_t>(&left_val) < *std::get_if<int64_t>(&right_val);
            } else if (std::holds_alternative<aasm::Le>(bin->op)) {
                bin_val = *std::get_if<int64_t>(&left_val) <= *std::get_if<int64_t>(&right_val);
            } else if (std::holds_alternative<aasm::Ne>(bin->op)) {
                bin_val = left_val != right_val;
            } else if (std::holds_alternative<aasm::Eq>(bin->op)) {
                bin_val = left_val == right_val;
            }

            value_map[op] = bin_val;
            new_const.emplace(op);
        } else if (auto *phi = std::get_if<aasm::Phi>(&ins)) {
            /* first value */
            sscp::Value phi_val = op_value(phi->bindings.cbegin()->second, value_map);

            for (auto &[ref, bind_op] : phi->bindings) {
                sscp::Value bind_val = op_value(bind_op, value_map);
                if (std::holds_alternative<sscp::Bot>(bind_val)) {
                    phi_val = sscp::Bot {};
                } else if (std::holds_alternative<sscp::Top>(bind_val)) {
                    if (!std::holds_alternative<sscp::Bot>(phi_val)) {
                        phi_val = sscp::Top {};
                    }
                } else {
                    if (phi_val != bind_val) {
                        if (!std::holds_alternative<sscp::Bot>(phi_val)) {
                            phi_val = sscp::Top {};
                        }
                    }
                }
            }
            value_map[op] = phi_val;
            new_const.emplace(op);
        }
    }

    return new_const;
}

void rewrite_ins(cfg::Ref &ref, aasm::Ins &ins, ValMap &value_map) {
    auto lambda = [&](aasm::Operand &op) {
        if (value_map.contains(op)) {
            //std::cerr << "sscp: propagating constant\n";
            if (auto *i = std::get_if<int64_t>(&value_map.at(op))) {
                op.value = aasm::Imm { *i };
            } else if (auto *b = std::get_if<bool>(&value_map.at(op))) {
                op.value = aasm::ImmB { *b };
                /* collapse constant branches */
                if (auto *br = std::get_if<aasm::Br>(&ins)) {
                    cfg::Ref next = *b ? br->tru : br->fals;
                    ins = aasm::Jump { next };
                }
            }
        }
    };

    in_op_traverse(ins, lambda);
}

void sscp_function(Function &func) {
    DefMap def_map = definition_map(func);
    UseMap u_map = use_map(func, def_map);
    ValMap value_map;

    for (auto &[op, _] : def_map) {
        aasm::Ins ins = cfg_instructions(def_map.at(op).first)[def_map.at(op).second];
        if (std::holds_alternative<aasm::Binary>(ins) ||
            std::holds_alternative<aasm::Binary>(ins)) {
            value_map[op] = sscp::Top {};
        }
    }

    std::unordered_set<aasm::Operand> new_const;
    do {
        new_const = update_value_map(value_map, def_map);
        for (auto &cons : new_const) {
            if (u_map.contains(cons)) {
                for (auto &use : u_map.at(cons)) {
                    rewrite_ins(use.first, cfg_instructions(use.first)[use.second], value_map);
                }
            }
        }
    } while (new_const.size() != 0);
}

void sscp_program(Program &prog) {
    for (auto &[_, func] : prog.functions) {
        sscp_function(func);
    }
}
