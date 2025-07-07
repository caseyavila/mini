#include "aasm.h"
#include "ast.h"
#include "cfg.h"
#include "type_checker.h"
#include <functional>
#include <variant>

/*
    honestly should have made prog/func globally accessible
    instead of tyring to pass it around everywhere for type info

    i give up...
*/

aasm::Operand aasm_expr(Program &prog, Function &func, Expression &expr, aasm::Block &insns, int &var);

aasm::Operand aasm_unary(Program &prog, Function &func, Unary &unary, aasm::Block &insns, int &var) {
    aasm::Operand opd = aasm_expr(prog, func, *unary.expr, insns, var);
    aasm::Operand target = aasm::Operand { aasm::Var { var++ } };
    aasm::Ins ins;

    if (std::holds_alternative<Negative>(unary.op)) {
        target.type = Int {};
        ins = aasm::Binary { aasm::Sub {}, target, aasm::Operand { aasm::Imm { 0 }, Int {} }, opd };
    } else if (std::holds_alternative<Not>(unary.op)) {
        target.type = Bool {};
        ins = aasm::Binary { aasm::Xor {}, target, aasm::Operand { aasm::ImmB { true }, Bool {} }, opd };
    } else {
        std::cerr << "Unhandled AASM unary expression. Quitting...\n";
        std::exit(1);
    }

    insns.emplace_back(ins);
    return target;
}

aasm::Operand aasm_binary(Program &prog, Function &func, Binary &binary, aasm::Block &insns, int &var) {
    aasm::Operand opd_l = aasm_expr(prog, func, *binary.left, insns, var);
    aasm::Operand opd_r = aasm_expr(prog, func, *binary.right, insns, var);
    aasm::Operand target = aasm::Operand { aasm::Var { var++ } };
    aasm::Ins ins;

    if (std::holds_alternative<Add>(binary.op)) {
        target.type = Int {};
        ins = aasm::Binary { aasm::Add {}, target, opd_l, opd_r };
    } else if (std::holds_alternative<Sub>(binary.op)) {
        target.type = Int {};
        ins = aasm::Binary { aasm::Sub {}, target, opd_l, opd_r };
    } else if (std::holds_alternative<Mul>(binary.op)) {
        target.type = Int {};
        ins = aasm::Binary { aasm::Mul {}, target, opd_l, opd_r };
    } else if (std::holds_alternative<Div>(binary.op)) {
        target.type = Int {};
        ins = aasm::Binary { aasm::Div {}, target, opd_l, opd_r };
    } else if (std::holds_alternative<And>(binary.op)) {
        target.type = Bool {};
        ins = aasm::Binary { aasm::And {}, target, opd_l, opd_r };
    } else if (std::holds_alternative<Or>(binary.op)) {
        target.type = Bool {};
        ins = aasm::Binary { aasm::Or {}, target, opd_l, opd_r };
    } else if (std::holds_alternative<Grt>(binary.op)) {
        target.type = Bool {};
        ins = aasm::Binary { aasm::Gt {}, target, opd_l, opd_r };
    } else if (std::holds_alternative<Geq>(binary.op)) {
        target.type = Bool {};
        ins = aasm::Binary { aasm::Ge {}, target, opd_l, opd_r };
    } else if (std::holds_alternative<Lst>(binary.op)) {
        target.type = Bool {};
        ins = aasm::Binary { aasm::Lt {}, target, opd_l, opd_r };
    } else if (std::holds_alternative<Leq>(binary.op)) {
        target.type = Bool {};
        ins = aasm::Binary { aasm::Le {}, target, opd_l, opd_r };
    } else if (std::holds_alternative<Eq>(binary.op)) {
        target.type = Bool {};
        ins = aasm::Binary { aasm::Eq {}, target, opd_l, opd_r };
    } else if (std::holds_alternative<Neq>(binary.op)) {
        target.type = Bool {};
        ins = aasm::Binary { aasm::Ne {}, target, opd_l, opd_r };
    } else {
        std::cerr << "Unhandled AASM binary expression. Quitting...\n";
        std::exit(1);
    }

    insns.emplace_back(ins);
    return target;
}

aasm::Operand aasm_invocation(Program &prog, Function &func, Invocation &inv, bool returns, aasm::Block &insns, int &var) {
    std::vector<aasm::Operand> args;

    for (auto &arg : inv.arguments) {
        args.emplace_back(aasm_expr(prog, func, arg, insns, var));
    }

    aasm::Ins ins;
    if (returns) {
        Type ret_t = prog.functions.at(inv.id).return_type;
        aasm::Operand target = aasm::Operand { aasm::Var { var++ }, ret_t };
        ins = aasm::Call { target, inv.id, args };
        insns.emplace_back(ins);
        return target;
    } else {
        var++; /* because llvm said so */
        ins = aasm::Call { std::nullopt, inv.id, args };
        insns.emplace_back(ins);
        return aasm::Operand {};
    }
}

aasm::Operand aasm_dot(Program &prog, Function &func, Dot &dot, aasm::Block &insns, int &var) {
    aasm::Operand left = aasm_expr(prog, func, *dot.expr, insns, var);
    Struct *struct_t = std::get_if<Struct>(&left.type);

    int index = 0;
    while (prog.types.at(struct_t->id)[index].id != dot.id) {
        index++;
    }
    Type member_t = prog.types.at(struct_t->id)[index].type;

    aasm::Operand gep_op = aasm::Operand { aasm::Var { var++ }, member_t };
    aasm::Operand target = aasm::Operand { aasm::Var { var++ }, member_t };

    insns.emplace_back(aasm::Gep { gep_op, left, aasm::Operand { aasm::Imm { index }, Int {} } });
    insns.emplace_back(aasm::Load { target, gep_op });

    return target;
}

aasm::Operand aasm_index(Program &prog, Function &func, Index &idx, aasm::Block &insns, int &var) {
    aasm::Operand left = aasm_expr(prog, func, *idx.left, insns, var);
    aasm::Operand idx_op = aasm_expr(prog, func, *idx.index, insns, var);
    aasm::Operand gep = aasm::Operand { aasm::Var { var++ }, Int {} };
    aasm::Operand target = aasm::Operand { aasm::Var { var++ }, Int {} };

    insns.emplace_back(aasm::Gep { gep, left, idx_op });
    insns.emplace_back(aasm::Load { target, gep });

    return target;
}

aasm::Operand aasm_expr(Program &prog, Function &func, Expression &expr, aasm::Block &insns, int &var) {
    if (auto *id = std::get_if<std::string>(&expr)) {
        Type type = check_env(func.local_env, prog.top_env, *id);
        aasm::Operand id_op;
        if (func.local_env.contains(*id)) {
            id_op = aasm::Operand { aasm::Id { *id }, type };
        } else {
            id_op = aasm::Operand { aasm::Glob { *id }, type };
        }
        aasm::Operand load = aasm::Operand { aasm::Var { var++ }, type };
        insns.emplace_back(aasm::Load { load, id_op });
        return load;
    } else if (auto *i = std::get_if<int>(&expr)) {
        return aasm::Operand { aasm::Imm { *i }, Int {} };
    } else if (std::holds_alternative<True>(expr)) {
        return aasm::Operand { aasm::ImmB { true }, Bool {} };
    } else if (std::holds_alternative<False>(expr)) {
        return aasm::Operand { aasm::ImmB { false }, Bool {} };
    } else if (std::holds_alternative<Null>(expr)) {
        return aasm::Operand { aasm::Null {}, Null {} };
    } else if (auto *un = std::get_if<Unary>(&expr)) {
        return aasm_unary(prog, func, *un, insns, var);
    } else if (auto *bin = std::get_if<Binary>(&expr)) {
        return aasm_binary(prog, func, *bin, insns, var);
    } else if (auto *inv = std::get_if<Invocation>(&expr)) {
        return aasm_invocation(prog, func, *inv, true, insns, var);
    } else if (auto *dot = std::get_if<Dot>(&expr)) {
        return aasm_dot(prog, func, *dot, insns, var);
    } else if (auto *idx = std::get_if<Index>(&expr)) {
        return aasm_index(prog, func, *idx, insns, var);
    } else if (auto *ns = std::get_if<NewStruct>(&expr)) {
        aasm::Operand target = aasm::Operand { aasm::Var { var++ }, Struct { ns->id } };
        insns.emplace_back(aasm::NewS { target, ns->id });
        return target;
    } else if (auto *na = std::get_if<NewArray>(&expr)) {
        aasm::Operand target = aasm::Operand { aasm::Var { var++ }, Array {} };
        insns.emplace_back(aasm::NewA { target, na->size });
        return target;
    } else {
        std::cerr << "Unhandled AASM expression. Quitting...\n";
        std::exit(1);
    }
}

aasm::Operand aasm_lvalue(Program &prog, Function &func, LValue &lval, aasm::Block &insns, int &var) {
    if (auto *id = std::get_if<std::string>(&lval)) {
        aasm::Value val;
        if (func.local_env.contains(*id)) {
            val = aasm::Id { *id };
        } else {
            val = aasm::Glob { *id };
        }
        return aasm::Operand { val, check_env(func.local_env, prog.top_env, *id) };
    } else if (auto *dot = std::get_if<LValueDot>(&lval)) {
        aasm::Operand lval_op = aasm_lvalue(prog, func, *dot->lvalue, insns, var);
        aasm::Operand load = aasm::Operand { aasm::Var { var++ }, lval_op.type };
        Struct *struct_t = std::get_if<Struct>(&lval_op.type);

        int index = 0;
        while (prog.types.at(struct_t->id)[index].id != dot->id) {
            index++;
        }
        Type member_t = prog.types.at(struct_t->id)[index].type;

        aasm::Operand gep = aasm::Operand { aasm::Var { var++ }, member_t };

        insns.emplace_back(aasm::Load { load, lval_op });
        insns.emplace_back(aasm::Gep { gep, load, aasm::Operand { aasm::Imm { index }, Int {} } });
        return gep;
    } else if (auto *idx = std::get_if<LValueIndex>(&lval)) {
        aasm::Operand lval_op = aasm_lvalue(prog, func, *idx->lvalue, insns, var);
        aasm::Operand idx_op = aasm_expr(prog, func, idx->index, insns, var);
        aasm::Operand load = aasm::Operand { aasm::Var { var++ }, lval_op.type };
        aasm::Operand gep = aasm::Operand { aasm::Var { var++ }, Int {} };

        insns.emplace_back(aasm::Load { load, lval_op });
        insns.emplace_back(aasm::Gep { gep, load, idx_op });
        return gep;
    } else {
        std::cerr << "Unhandled AASM lvalue. Quitting...\n";
        std::exit(1);
    }
}

void aasm_block(Program &prog, Function &func, Block &stmts, aasm::Block &insns, int &var) {
    for (auto &stmt : stmts) {
        if (auto *p = std::get_if<Print>(&stmt)) {
            aasm::Operand arg = aasm_expr(prog, func, p->expr, insns, var);
            insns.emplace_back(aasm::Call { std::nullopt, "print", { arg } });
        } else if (auto *pln = std::get_if<PrintLn>(&stmt)) {
            aasm::Operand arg = aasm_expr(prog, func, pln->expr, insns, var);
            insns.emplace_back(aasm::Call { std::nullopt, "println", { arg } });
        } else if (auto *del = std::get_if<Delete>(&stmt)) {
            aasm::Operand arg = aasm_expr(prog, func, del->expr, insns, var);
            insns.emplace_back(aasm::Free { arg } );
        } else if (auto *ret = std::get_if<Return>(&stmt)) {
            if (ret->expr.has_value()) {
                aasm::Operand arg = aasm_expr(prog, func, ret->expr.value(), insns, var);
                insns.emplace_back(aasm::Ret { arg });
            } else {
                insns.emplace_back(aasm::Ret { std::nullopt });
            }
        } else if (auto *inv = std::get_if<Invocation>(&stmt)) {
            aasm_invocation(prog, func, *inv, false, insns, var);
        } else if (auto *ass = std::get_if<Assignment>(&stmt)) {
            aasm::Operand rvalue;
            if (auto *expr = std::get_if<Expression>(&ass->source)) {
                rvalue = aasm_expr(prog, func, *expr, insns, var);
            } else {
                rvalue = aasm::Operand { aasm::Var { var++ }, Int {} };
                insns.emplace_back(aasm::Call { rvalue, "readnum", {} });
            }
            insns.emplace_back(aasm::Store { rvalue, aasm_lvalue(prog, func, ass->lvalue, insns, var) });
        } else {
            std::cerr << "Unhandled AASM statement. Quitting...\n";
            std::exit(1);
        }
    }
}

void aasm_function(Program &prog, Function &func) {
    int var = func.parameters.size() + 1;

    auto aasm_ref = [&](cfg::Ref &ref) {
        if (auto ret = cfg_get_if<cfg::Return>(&ref)) {
            aasm_block(prog, func, ret.get()->statements, ret.get()->instructions, var);
        } else if (auto basic = cfg_get_if<cfg::Basic>(&ref)) {
            aasm_block(prog, func, basic.get()->statements, basic.get()->instructions, var);
            basic.get()->instructions.emplace_back(aasm::Jump { basic.get()->next });
        } else if (auto cond = cfg_get_if<cfg::Conditional>(&ref)) {
            aasm_block(prog, func, cond.get()->statements, cond.get()->instructions, var);
            aasm::Operand guard = aasm_expr(prog, func, cond.get()->guard, cond.get()->instructions, var);
            cond.get()->instructions.emplace_back(aasm::Br { guard, cond.get()->tru, cond.get()->fals });
        }
    };

    cfg_traverse(func.entry_ref, aasm_ref);
}

void aasm_program(Program &prog) {
    for (auto &[_, func] : prog.functions) {
        aasm_function(prog, func);
    }
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
