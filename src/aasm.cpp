#include "aasm.h"
#include "ast.h"
#include "cfg.h"
#include <variant>

aasm::Operand aasm_expr(Expression &expr, aasm::Block &insns, int &var);

aasm::Operand aasm_unary(Unary &unary, aasm::Block &insns, int &var) {
    aasm::Operand opd = aasm_expr(*unary.expr, insns, var);
    aasm::Operand target = aasm::Var { var++ };
    aasm::Ins ins;

    if (std::holds_alternative<Negative>(unary.op)) {
        ins = aasm::Binary { aasm::Sub {}, target, aasm::Imm { 0 }, opd };
    } else if (std::holds_alternative<Not>(unary.op)) {
        ins = aasm::Binary { aasm::Xor {}, target, aasm::ImmB { true }, opd };
    } else {
        std::cerr << "Unhandled AASM unary expression. Quitting...\n";
        std::exit(1);
    }

    insns.emplace_back(ins);
    return target;
}

aasm::Operand aasm_binary(Binary &binary, aasm::Block &insns, int &var) {
    aasm::Operand opd_l = aasm_expr(*binary.left, insns, var);
    aasm::Operand opd_r = aasm_expr(*binary.right, insns, var);
    aasm::Operand target = aasm::Var { var++ };
    aasm::Ins ins;

    if (std::holds_alternative<Add>(binary.op)) {
        ins = aasm::Binary { aasm::Add {}, target, opd_l, opd_r };
    } else if (std::holds_alternative<Sub>(binary.op)) {
        ins = aasm::Binary { aasm::Sub {}, target, opd_l, opd_r };
    } else if (std::holds_alternative<Mul>(binary.op)) {
        ins = aasm::Binary { aasm::Mul {}, target, opd_l, opd_r };
    } else if (std::holds_alternative<Div>(binary.op)) {
        ins = aasm::Binary { aasm::Div {}, target, opd_l, opd_r };
    } else if (std::holds_alternative<And>(binary.op)) {
        ins = aasm::Binary { aasm::And {}, target, opd_l, opd_r };
    } else if (std::holds_alternative<Or>(binary.op)) {
        ins = aasm::Binary { aasm::Or {}, target, opd_l, opd_r };
    } else if (std::holds_alternative<Grt>(binary.op)) {
        ins = aasm::Binary { aasm::Gt {}, target, opd_l, opd_r };
    } else if (std::holds_alternative<Geq>(binary.op)) {
        ins = aasm::Binary { aasm::Ge {}, target, opd_l, opd_r };
    } else if (std::holds_alternative<Lst>(binary.op)) {
        ins = aasm::Binary { aasm::Lt {}, target, opd_l, opd_r };
    } else if (std::holds_alternative<Leq>(binary.op)) {
        ins = aasm::Binary { aasm::Le {}, target, opd_l, opd_r };
    } else if (std::holds_alternative<Eq>(binary.op)) {
        ins = aasm::Binary { aasm::Eq {}, target, opd_l, opd_r };
    } else if (std::holds_alternative<Neq>(binary.op)) {
        ins = aasm::Binary { aasm::Ne {}, target, opd_l, opd_r };
    } else {
        std::cerr << "Unhandled AASM binary expression. Quitting...\n";
        std::exit(1);
    }

    insns.emplace_back(ins);
    return target;
}

aasm::Operand aasm_invocation(Invocation &inv, bool returns, aasm::Block &insns, int &var) {
    std::vector<aasm::Operand> args;

    for (auto &arg : inv.arguments) {
        args.emplace_back(aasm_expr(arg, insns, var));
    }

    aasm::Ins ins;
    if (returns) {
        aasm::Operand target = aasm::Var { var++ };
        ins = aasm::Call { target, inv.id, args };
        insns.emplace_back(ins);
        return target;
    } else {
        ins = aasm::Call { std::nullopt, inv.id, args };
        insns.emplace_back(ins);
        return aasm::Operand {};
    }
}

aasm::Operand aasm_dot(Dot &dot, aasm::Block &insns, int &var) {
    aasm::Operand left = aasm_expr(*dot.expr, insns, var);
    aasm::Operand gep = aasm::Var { var++ };
    aasm::Operand target = aasm::Var { var++ };

    insns.emplace_back(aasm::Gep { gep, left, dot.id });
    insns.emplace_back(aasm::Load { target, gep });

    return target;
}

aasm::Operand aasm_index(Index &idx, aasm::Block &insns, int &var) {
    aasm::Operand left = aasm_expr(*idx.left, insns, var);
    aasm::Operand idx_op = aasm_expr(*idx.index, insns, var);
    aasm::Operand gep = aasm::Var { var++ };
    aasm::Operand target = aasm::Var { var++ };

    insns.emplace_back(aasm::Gep { gep, left, idx_op });
    insns.emplace_back(aasm::Load { target, gep });

    return target;
}

aasm::Operand aasm_expr(Expression &expr, aasm::Block &insns, int &var) {
    if (auto *id = std::get_if<std::string>(&expr)) {
        return aasm::Id { *id };
    } else if (auto *i = std::get_if<int>(&expr)) {
        return aasm::Imm { *i };
    } else if (std::holds_alternative<True>(expr)) {
        return aasm::ImmB { true };
    } else if (std::holds_alternative<False>(expr)) {
        return aasm::ImmB { false };
    } else if (std::holds_alternative<Null>(expr)) {
        return aasm::Null {};
    } else if (auto *un = std::get_if<Unary>(&expr)) {
        return aasm_unary(*un, insns, var);
    } else if (auto *bin = std::get_if<Binary>(&expr)) {
        return aasm_binary(*bin, insns, var);
    } else if (auto *inv = std::get_if<Invocation>(&expr)) {
        return aasm_invocation(*inv, true, insns, var);
    } else if (auto *dot = std::get_if<Dot>(&expr)) {
        return aasm_dot(*dot, insns, var);
    } else if (auto *idx = std::get_if<Index>(&expr)) {
        return aasm_index(*idx, insns, var);
    } else if (auto *ns = std::get_if<NewStruct>(&expr)) {
        aasm::Operand target = aasm::Var { var++ };
        insns.emplace_back(aasm::NewS { target, ns->id });
        return target;
    } else if (auto *na = std::get_if<NewArray>(&expr)) {
        aasm::Operand target = aasm::Var { var++ };
        insns.emplace_back(aasm::NewA { target, na->size });
        return target;
    } else {
        std::cerr << "Unhandled AASM expression. Quitting...\n";
        std::exit(1);
    }
}

aasm::Operand aasm_lvalue(LValue &lval, aasm::Block &insns, int &var) {
    if (auto *id = std::get_if<std::string>(&lval)) {
        return aasm::Id { *id };
    } else if (auto *dot = std::get_if<LValueDot>(&lval)) {
        aasm::Operand lval_op = aasm_lvalue(*dot->lvalue, insns, var);
        aasm::Operand load = aasm::Var { var++ };
        aasm::Operand gep = aasm::Var { var++ };

        insns.emplace_back(aasm::Load { load, lval_op });
        insns.emplace_back(aasm::Gep { gep, load, dot->id });
        return gep;
    } else if (auto *idx = std::get_if<LValueIndex>(&lval)) {
        aasm::Operand lval_op = aasm_lvalue(*idx->lvalue, insns, var);
        aasm::Operand idx_op = aasm_expr(idx->index, insns, var);
        aasm::Operand load = aasm::Var { var++ };
        aasm::Operand gep = aasm::Var { var++ };

        insns.emplace_back(aasm::Load { load, lval_op });
        insns.emplace_back(aasm::Gep { gep, load, idx_op });
        return gep;
    } else {
        std::cerr << "Unhandled AASM lvalue. Quitting...\n";
        std::exit(1);
    }
}

void aasm_block(Block &stmts, aasm::Block &insns, int &var) {
    for (auto &stmt : stmts) {
        if (auto *p = std::get_if<Print>(&stmt)) {
            aasm::Operand arg = aasm_expr(p->expr, insns, var);
            insns.emplace_back(aasm::Call { std::nullopt, "print", { arg } });
        } else if (auto *pln = std::get_if<PrintLn>(&stmt)) {
            aasm::Operand arg = aasm_expr(pln->expr, insns, var);
            insns.emplace_back(aasm::Call { std::nullopt, "println", { arg } });
        } else if (auto *del = std::get_if<Delete>(&stmt)) {
            aasm::Operand arg = aasm_expr(del->expr, insns, var);
            insns.emplace_back(aasm::Call { std::nullopt, "delete", { arg } });
        } else if (auto *ret = std::get_if<Return>(&stmt)) {
            if (ret->expr.has_value()) {
                aasm::Operand arg = aasm_expr(ret->expr.value(), insns, var);
                insns.emplace_back(aasm::Ret { arg });
            } else {
                insns.emplace_back(aasm::Ret { std::nullopt });
            }
        } else if (auto *inv = std::get_if<Invocation>(&stmt)) {
            aasm_invocation(*inv, false, insns, var);
        } else if (auto *ass = std::get_if<Assignment>(&stmt)) {
            aasm::Operand rvalue;
            if (auto *expr = std::get_if<Expression>(&ass->source)) {
                rvalue = aasm_expr(*expr, insns, var);
            } else {
                rvalue = aasm::Var { var++ };
                insns.emplace_back(aasm::Call { rvalue, "readnum", {} });
            }
            insns.emplace_back(aasm::Store { rvalue, aasm_lvalue(ass->lvalue, insns, var) });
        } else {
            std::cerr << "Unhandled AASM statement. Quitting...\n";
            std::exit(1);
        }
    }
}

void aasm_function(cfg::Function &func) {
    cfg::RefMap seen;
    std::vector<cfg::Ref> stack;
    int block_id = 0;
    int var = 0;

    cfg::Ref curr = func.body;
    stack.emplace_back(curr);

    while (!stack.empty()) {
        if (!seen.contains(curr)) {
            seen.emplace(curr, block_id);

            if (auto *ret = std::get_if<std::shared_ptr<cfg::Return>>(&curr)) {
                aasm_block(ret->get()->statements, ret->get()->instructions, var);
            } else if (auto *basic = std::get_if<std::shared_ptr<cfg::Basic>>(&curr)) {
                aasm_block(basic->get()->statements, basic->get()->instructions, var);
                stack.emplace_back(basic->get()->next);
            } else if (auto *cond = std::get_if<std::shared_ptr<cfg::Conditional>>(&curr)) {
                aasm_block(cond->get()->statements, cond->get()->instructions, var);
                stack.emplace_back(cond->get()->fals);
                stack.emplace_back(cond->get()->tru);

                aasm::Operand guard = aasm_expr(cond->get()->guard, cond->get()->instructions, var);
                cond->get()->instructions.emplace_back(aasm::Br { guard, cond->get()->tru, cond->get()->fals });
            } else if (std::holds_alternative<std::weak_ptr<cfg::Conditional>>(curr)) {
                std::cout << "Actually got a weak CFG ref, please examine...\n";
                std::exit(1);
            }
            block_id++;
        }

        curr = stack.back();
        stack.pop_back();
    }
}

void aasm_program(cfg::Program &prog) {
    for (auto &[id, func] : prog.functions) {
        aasm_function(func);
    }
}
