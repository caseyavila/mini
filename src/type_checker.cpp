#include "type_checker.h"
#include "ast.h"
#include <concepts>
#include <cstdlib>
#include <unordered_map>
#include <unordered_set>
#include <variant>

template <typename T, typename K>
concept HasContains = requires(T t, K k) {
    { t.contains(k) } -> std::convertible_to<bool>;
};

bool check_type(const Type &t, const HasContains<std::string> auto &types) {
    if (auto *s = std::get_if<Struct>(&t)) {
        if (!types.contains(s->id)) {
            std::cerr << "Invalid type: '" << s->id << "'\n";
            return false;
        }
    }

    return true;
}

bool check_declarations(const std::vector<Declaration> &decls,
        const HasContains<std::string> auto &types) {
    std::unordered_set<std::string> ids;

    for (auto &decl : decls) {
        if (ids.contains(decl.id)) {
            std::cerr << "Duplicate declaration: '" << decl.id << "'\n";
            return false;
        }

        if (!check_type(decl.type, types)) {
            std::cerr << "in declaration: '" << decl.id << "'\n";
            return false;
        }

        ids.insert(decl.id);
    }

    return true;
}

Environment environment(const std::vector<Declaration> &decls) {
    Environment env;

    for (auto &decl : decls) {
        env.emplace(decl.id, decl.type);
    }

    return env;
}

Type check_env(const Environment &lenv, const Environment &tenv, const std::string &id) {
    auto it = lenv.find(id);
    if (it != lenv.end()) {
        return it->second;
    }
    it = tenv.find(id);
    if (it != tenv.end()) {
        return it->second;
    }
    std::cerr << "Use of undeclared variable: '" << id << "'\n";
    std::exit(1);
}

Type check_expr(const Program &prog, const Environment &tenv, const Function &func,
        const Environment &lenv, const Expression &expr);

Type check_unary(const Program &prog, const Environment &tenv, const Function &func,
        const Environment &lenv, const Unary &unary) {

    auto recurse = [&](const Expression &e) { return check_expr(prog, tenv, func, lenv, e); };

    if (std::holds_alternative<Negative>(unary.op)) {
        if (!std::holds_alternative<Int>(recurse(*unary.expr))) {
            std::cerr << recurse(*unary.expr) << " invalid operand for '-' (negation)\n";
            std::exit(1);
        }
        return Int {};
    }
    if (std::holds_alternative<Not>(unary.op)) {
        if (!std::holds_alternative<Bool>(recurse(*unary.expr))) {
            std::cerr << recurse(*unary.expr) << " invalid operand for '!'\n";
            std::exit(1);
        }
        return Bool {};
    }

    std::cerr << "Unhandled unary typecheck, quitting...\n";
    std::exit(1);
}

Type check_binary(const Program &prog, const Environment &tenv, const Function &func,
        const Environment &lenv, const Binary &binary) {

    auto recurse = [&](const Expression &e) { return check_expr(prog, tenv, func, lenv, e); };

    if (std::holds_alternative<Add>(binary.op) ||
        std::holds_alternative<Sub>(binary.op) ||
        std::holds_alternative<Mul>(binary.op) ||
        std::holds_alternative<Div>(binary.op)) {
        if (!std::holds_alternative<Int>(recurse(*binary.left)) ||
            !std::holds_alternative<Int>(recurse(*binary.right))) {
            std::cerr << "Non-int operands for '+', '-', '*', '/'\n";
            std::exit(1);
        }
        return Int {};
    }

    if (std::holds_alternative<Grt>(binary.op) ||
        std::holds_alternative<Geq>(binary.op) ||
        std::holds_alternative<Lst>(binary.op) ||
        std::holds_alternative<Leq>(binary.op)) {
        if (!std::holds_alternative<Int>(recurse(*binary.left)) ||
            !std::holds_alternative<Int>(recurse(*binary.right))) {
            std::cerr << "Non-int operands for '>', '>=', '<', '<='\n";
            std::exit(1);
        }
        return Bool {};
    }

    if (std::holds_alternative<And>(binary.op) ||
        std::holds_alternative<Or>(binary.op)) {
        if (!std::holds_alternative<Bool>(recurse(*binary.left)) ||
            !std::holds_alternative<Bool>(recurse(*binary.right))) {
            std::cerr << "Non-bool operands for '&&', '||'\n";
            std::exit(1);
        }
        return Bool {};
    }

    if (std::holds_alternative<Eq>(binary.op) ||
        std::holds_alternative<Neq>(binary.op)) {
        if (recurse(*binary.left) != recurse(*binary.right)) {
            std::cerr << "Different operand types with '==', '!='\n";
            std::exit(1);
        }
        return Bool {};
    }

    std::cerr << "Unhandled unary typecheck, quitting...\n";
    std::exit(1);
}

Type check_dot(const Program &prog, const Environment &tenv, const Function &func,
        const Environment &lenv, const Dot &dot) {

    auto recurse = [&](const Expression &e) { return check_expr(prog, tenv, func, lenv, e); };

    auto expr_t = recurse(*dot.expr);
    if (auto *struct_t = std::get_if<Struct>(&expr_t)) {
        for (auto &member : prog.types.at(struct_t->id)) {
            if (member.id == dot.id) {
                return member.type;
            }
        }
        std::cerr << "Type '" << struct_t->id << "' does not have member '" << dot.id << "'\n";
        std::exit(1);
    } else {
        std::cerr << "Cannot use '.' operator on " << expr_t << "\n";
        std::exit(1);
    }
}

Type check_index(const Program &prog, const Environment &tenv, const Function &func,
        const Environment &lenv, const Index &idx) {

    auto recurse = [&](const Expression &e) { return check_expr(prog, tenv, func, lenv, e); };

    if (!std::holds_alternative<Array>(recurse(*idx.left))) {
        std::cerr << "Cannot index " << recurse(*idx.left) << "\n";
        std::exit(1);
    }
    if (!std::holds_alternative<Int>(recurse(*idx.index))) {
        std::cerr << "Cannot index array with " << recurse(*idx.index) << "\n";
        std::exit(1);
    }

    return Int {};
}

void check_arguments(const Program &prog, const Environment &tenv, const Function &caller,
        const Environment &lenv, const Invocation &inv, const Function &callee) {

    auto recurse = [&](const Expression &e) { return check_expr(prog, tenv, caller, lenv, e); };

    if (callee.parameters.size() != inv.arguments.size()) {
        std::cerr << "Function '" << inv.id << "' expects "
                  << callee.parameters.size() << " arguments but received "
                  << inv.arguments.size() << "\n";
        std::exit(1);
    }

    int i = 0;
    for (auto &param : callee.parameters) {
        if (param.type != recurse(inv.arguments[i])) {
            std::cerr << "Parameter " << param.id << " of " << inv.id << " is of "
                      << param.type << " but got " << recurse(inv.arguments[i]) << "\n";
            std::exit(1);
        }
        i++;
    }
}

Type check_invocation(const Program &prog, const Environment &tenv, const Function &func,
        const Environment &lenv, const Invocation &inv) {
    if (!prog.functions.contains(inv.id)) {
        std::cerr << "Function '" << inv.id << "' does not exist\n";
        std::exit(1);
    }

    check_arguments(prog, tenv, func, lenv, inv, prog.functions.at(inv.id));

    return prog.functions.at(inv.id).return_type;
}

Type check_expr(const Program &prog, const Environment &tenv, const Function &func,
        const Environment &lenv, const Expression &expr) {

    if (std::holds_alternative<int>(expr)) {
        return Int {};
    } else if (std::holds_alternative<True>(expr) ||
               std::holds_alternative<False>(expr)) {
        return Bool {};
    } else if (std::holds_alternative<Null>(expr)) {
        return Null {};
    } else if (std::holds_alternative<NewArray>(expr)) {
        return Array {};
    } else if (auto *new_s = std::get_if<NewStruct>(&expr)) {
        Type s_t =  Struct { new_s->id };
        if (!check_type(s_t, prog.types)) {
            std::cerr << "in 'new' declaration\n";
            std::exit(1);
        }
        return s_t;
    } else if (auto *id = std::get_if<std::string>(&expr)) {
        return check_env(lenv, tenv, *id);
    } else if (auto *u = std::get_if<Unary>(&expr)) {
        return check_unary(prog, tenv, func, lenv, *u);
    } else if (auto *b = std::get_if<Binary>(&expr)) {
        return check_binary(prog, tenv, func, lenv, *b);
    } else if (auto *d = std::get_if<Dot>(&expr)) {
        return check_dot(prog, tenv, func, lenv, *d);
    } else if (auto *idx = std::get_if<Index>(&expr)) {
        return check_index(prog, tenv, func, lenv, *idx);
    } else if (auto *inv = std::get_if<Invocation>(&expr)) {
        return check_invocation(prog, tenv, func, lenv, *inv);
    }

    std::cerr << "Unhandled expression typecheck, qutting...\n";
    std::exit(1);
}

void check_stmt(const Program &prog, const Environment &tenv, const Function &func,
        const Environment &lenv, const Statement &stmt);

void check_block(const Program &prog, const Environment &tenv, const Function &func,
        const Environment &lenv, const Block &block) {

    for (auto &stmt : block) {
        check_stmt(prog, tenv, func, lenv, stmt);
    }
}

Type check_lvalue(const Program &prog, const Environment &tenv, const Function &func,
        const Environment &lenv, const LValue &lvalue) {

    auto check_e = [&](const Expression &e) { return check_expr(prog, tenv, func, lenv, e); };
    auto recurse = [&](const LValue &l) { return check_lvalue(prog, tenv, func, lenv, l); };

    if (auto *id = std::get_if<std::string>(&lvalue)) {
        return check_env(lenv, tenv, *id);
    } else if (auto *lv_i = std::get_if<LValueIndex>(&lvalue)) {
        if (!std::holds_alternative<Array>(recurse(*lv_i->lvalue))) {
            std::cerr << "Cannot index " << recurse(*lv_i->lvalue) << "\n";
            std::exit(1);
        }
        if (!std::holds_alternative<Int>(check_e(lv_i->index))) {
            std::cerr << "Cannot index array with " << check_e(lv_i->index) << "\n";
            std::exit(1);
        }
        return Int {};
    } else if (auto *lv_d = std::get_if<LValueDot>(&lvalue)) {
        auto expr_t = recurse(*lv_d->lvalue);
        if (auto *struct_t = std::get_if<Struct>(&expr_t)) {
            for (auto &member : prog.types.at(struct_t->id)) {
                if (member.id == lv_d ->id) {
                    return member.type;
                }
            }
            std::cerr << "Type '" << struct_t->id << "' does not have member '" << lv_d ->id << "'\n";
            std::exit(1);
        } else {
            std::cerr << "Cannot use '.' operator on " << expr_t << "\n";
            std::exit(1);
        }
    }

    std::cerr << "Unhandled lvalue typecheck, qutting...\n";
    std::exit(1);
}

void check_stmt(const Program &prog, const Environment &tenv, const Function &func,
        const Environment &lenv, const Statement &stmt) {
    auto check_e = [&](const Expression &e) { return check_expr(prog, tenv, func, lenv, e); };

    if (auto *print = std::get_if<Print>(&stmt)) {
        if (!std::holds_alternative<Int>(check_e(print->expr))) {
            std::cerr << "Cannot print " << check_e(print->expr) << "\n";
            std::exit(1);
        }
        return;
    } else if (auto *print_ln = std::get_if<PrintLn>(&stmt)) {
        if (!std::holds_alternative<Int>(check_e(print_ln->expr))) {
            std::cerr << "Cannot print " << check_e(print_ln->expr) << "\n";
            std::exit(1);
        }
        return;
    } else if (auto *del = std::get_if<Delete>(&stmt)) {
        if (!std::holds_alternative<Struct>(check_e(del->expr))) {
            std::cerr << "Cannot delete " << check_e(del->expr) << "\n";
            std::exit(1);
        }
        return;
    } else if (auto *ret = std::get_if<Return>(&stmt)) {
        Type expr_t = Void {};
        if (ret->expr.has_value()) {
            expr_t = check_e(ret->expr.value());
        }
        if (expr_t != func.return_type) {
            std::cerr << "Cannot return " << expr_t << " from " << func.id << "\n";
            std::exit(1);
        }
        return;
    } else if (auto *loop = std::get_if<Loop>(&stmt)) {
        if (!std::holds_alternative<Bool>(check_e(loop->guard))) {
            std::cerr << "Cannot use " << check_e(loop->guard) << " as 'while' guard\n";
            std::exit(1);
        }
        check_block(prog, tenv, func, lenv, loop->body);
        return;
    } else if (auto *cond = std::get_if<Conditional>(&stmt)) {
        if (!std::holds_alternative<Bool>(check_e(cond->guard))) {
            std::cerr << "Cannot use " << check_e(cond->guard) << " as 'if' guard\n";
            std::exit(1);
        }
        check_block(prog, tenv, func, lenv, cond->then);
        if (cond->els.has_value()) {
            check_block(prog, tenv, func, lenv, cond->els.value());
        }
        return;
    } else if (auto *ass = std::get_if<Assignment>(&stmt)) {
        Type lvalue_t = check_lvalue(prog, tenv, func, lenv, ass->lvalue);
        Type rvalue_t = Int {};
        if (auto *expr = std::get_if<Expression>(&ass->source)) {
            rvalue_t = check_e(*expr);
        }
        if (lvalue_t != rvalue_t) {
            std::cerr << "Cannot assign " << lvalue_t << " to " << rvalue_t << "\n";
            std::exit(1);
        }
        return;
    } else if (auto *inv = std::get_if<Invocation>(&stmt)) {
        check_invocation(prog, tenv, func, lenv, *inv);
        return;
    }

    std::cerr << "Unhandled statement typecheck, qutting...\n";
    std::exit(1);
}

/* check if all paths return */
bool check_returns(const Block &block);

bool check_stmt_return(const Statement &stmt) {
    if (std::holds_alternative<Return>(stmt)) {
        return true;
    } else if (auto *cond = std::get_if<Conditional>(&stmt)) {
        if (cond->els.has_value()) {
            return check_returns(cond->then) && check_returns(cond->els.value());
        }
    }

    return false;
}

bool check_returns(const Block &block) {
    for (auto &stmt : block) {
        if (check_stmt_return(stmt)) {
            return true;
        }
    }

    return false;
}

void check_function(const Program &prog, const Environment &tenv,
        Function &func) {
    std::vector<Declaration> decls;
    decls.reserve(func.parameters.size() + func.declarations.size());

    /* combine decls and params for environment */
    decls.insert(decls.begin(), func.parameters.cbegin(), func.parameters.cend());
    decls.insert(decls.begin(), func.declarations.cbegin(), func.declarations.cend());

    if (!check_declarations(decls, prog.types)) {
        std::cerr << "in function declaration: '" << func.id << "'\n";
        std::exit(1);
    }

    Environment lenv = environment(decls);
    check_block(prog, tenv, func, lenv, func.body);

    if (!check_returns(func.body)) {
        if (std::holds_alternative<Void>(func.return_type)) {
            func.body.emplace_back(Return { std::nullopt });
        } else {
            std::cerr << "Not all control paths in '" << func.id << "' return\n";
            std::exit(1);
        }
    }

    func.local_env = lenv;
}

void check_functions(Program &prog, const Environment &tenv) {
    auto main_i = prog.functions.find("main");
    if (main_i == prog.functions.end() || !std::holds_alternative<Int>(main_i->second.return_type)) {
        std::cerr << "No function 'main' with return type 'int' found\n";
        std::exit(1);
    }

    for (auto &[_, func] : prog.functions) {
        check_function(prog, tenv, func);
    }
}

void check_program(Program &program) {
    for (auto &type : program.types) {
        if (!check_declarations(type.second, program.types)) {
            std::cerr << "in type declaration: '" << type.first << "'\n";
            std::exit(1);
        }
    }

    if (!check_declarations(program.declarations, program.types)) {
        std::cerr << "in top-level declaration\n";
        std::exit(1);
    }

    Environment tenv = environment(program.declarations);
    check_functions(program, tenv);

    program.top_env = tenv;
}
