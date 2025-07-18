#include <string>
#include <variant>

#include "print_aasm.h"
#include "aasm.h"
#include "ast.h"
#include "cfg.h"

std::string aasm_type(const Type &t) {
    if (std::holds_alternative<Int>(t)) {
        return "i64";
    } else if (std::holds_alternative<Bool>(t)) {
        return "i1";
    } else if (std::holds_alternative<Struct>(t) ||
               std::holds_alternative<Array>(t) ||
               std::holds_alternative<Null>(t)) {
        return "ptr";
    } else if (std::holds_alternative<Void>(t)) {
        return "void";
    } else {
        std::cerr << "Invalid AASM type. Quitting...\n";
        std::exit(1);
    }
}

void print_aasm_types(const TypeDeclarations &type_decls) {
    for (auto &[type_id, decls] : type_decls) {
        std::cout << "%struct." << type_id << " = type {";
        bool first = true;
        for (auto &decl : decls) {
            if (!first) std::cout << ", ";
            first = false;
            std::cout << aasm_type(decl.type);
        }

        std::cout << "}\n";
    }
}

void print_aasm_decls(const std::vector<Declaration> &decls) {
    for (auto &decl : decls) {
        std::cout << "@" << decl.id << " = common global " << aasm_type(decl.type);
        if (std::holds_alternative<Struct>(decl.type) ||
            std::holds_alternative<Array>(decl.type)) {
            std::cout << " null";
        } else {
            std::cout << " 0";
        }
        std::cout << ", align 4\n";
    }
}

std::string print_aasm_op(const Program &prog, const Function &func, const aasm::Operand &op) {
    if (auto *imm = std::get_if<aasm::Imm>(&op.value)) {
        return std::to_string(imm->val);
    } else if (auto *immb = std::get_if<aasm::ImmB>(&op.value)) {
        return immb->val ? "1" : "0";
    } else if (auto *var = std::get_if<aasm::Var>(&op.value)) {
        return "%" + std::to_string(var->id);
    } else if (std::holds_alternative<aasm::Null>(op.value)) {
        return "null";
    } else if (auto *id = std::get_if<aasm::Id>(&op.value)) {
        return "%" + id->id;
    } else if (auto *glob = std::get_if<aasm::Glob>(&op.value)) {
        return "@" + glob->id;
    }

    std::cerr << "Unhandled AASM operand. Quitting...\n";
    std::exit(1);
}

std::string gep_t(const Type &t) {
    if (auto *s = std::get_if<Struct>(&t)) {
        return "%struct." + s->id;
    } else {
        return "i64";
    }
}

void print_aasm_binary(const Program &prog, const Function &func, const aasm::Binary &bin) {
    auto p_op = [&](const aasm::Operand &op) {
        return print_aasm_op(prog, func, op);
    };

    std::string opcode;
    if (std::holds_alternative<aasm::Add>(bin.op)) opcode = "add";
    else if (std::holds_alternative<aasm::Sub>(bin.op)) opcode = "sub";
    else if (std::holds_alternative<aasm::Mul>(bin.op)) opcode = "mul";
    else if (std::holds_alternative<aasm::Div>(bin.op)) opcode = "sdiv";
    else if (std::holds_alternative<aasm::Xor>(bin.op)) opcode = "xor";
    else if (std::holds_alternative<aasm::And>(bin.op)) opcode = "and";
    else if (std::holds_alternative<aasm::Or>(bin.op)) opcode = "or";
    else if (std::holds_alternative<aasm::Eq>(bin.op)) opcode = "icmp eq";
    else if (std::holds_alternative<aasm::Ne>(bin.op)) opcode = "icmp ne";
    else if (std::holds_alternative<aasm::Gt>(bin.op)) opcode = "icmp sgt";
    else if (std::holds_alternative<aasm::Ge>(bin.op)) opcode = "icmp sge";
    else if (std::holds_alternative<aasm::Lt>(bin.op)) opcode = "icmp slt";
    else if (std::holds_alternative<aasm::Le>(bin.op)) opcode = "icmp sle";

    std::cout << p_op(bin.target) << " = "
              << opcode << " "
              << aasm_type(bin.left.type) << " "
              << p_op(bin.left) << ", "
              << p_op(bin.right) << "\n";
}

void print_aasm_insns(const Program &prog, const Function &func,
        const cfg::RefMap &ref_map, const cfg::Ref &ref, const std::vector<aasm::Ins> &insns) {

    auto p_op = [&](const aasm::Operand &op) {
        return print_aasm_op(prog, func, op);
    };

    for (auto &ins : insns) {
        if (auto *bin = std::get_if<aasm::Binary>(&ins)) {
            print_aasm_binary(prog, func, *bin);
        } else if (auto *load = std::get_if<aasm::Load>(&ins)) {
            std::cout << p_op(load->target) << " = load "
                      << aasm_type(load->target.type) << ", ptr "
                      << p_op(load->ptr) << "\n";
        } else if (auto *store = std::get_if<aasm::Store>(&ins)) {
            std::cout << "store " << aasm_type(store->value.type)
                      << " " << p_op(store->value)
                      << ", ptr " << p_op(store->ptr) << "\n";
        } else if (auto *ret = std::get_if<aasm::Ret>(&ins)) {
            if (ret->value.has_value()) {
                std::cout << "ret " << aasm_type(ret->value.value().type)
                          << " " << p_op(ret->value.value()) << "\n";
            } else {
                std::cout << "ret void\n";
            }
        } else if (auto *jmp = std::get_if<aasm::Jump>(&ins)) {
            std::cout << "br label %l" << ref_map.at(jmp->block) << "\n";
        } else if (auto *br = std::get_if<aasm::Br>(&ins)) {
            std::cout << "br i1 " << p_op(br->guard)
                      << ", label %l" << ref_map.at(br->tru)
                      << ", label %l" << ref_map.at(br->fals) << "\n";
        } else if (auto *gep = std::get_if<aasm::Gep>(&ins)) {
            std::cout << p_op(gep->target) << " = getelementptr "
                      << gep_t(gep->value.type) << ", ptr "
                      << p_op(gep->value) << ", ";
            if (std::holds_alternative<Struct>(gep->value.type)) {
                std::cout << "i1 0, i32 " << p_op(gep->index) << "\n";
            } else {
                std::cout << "i64 " << p_op(gep->index) << "\n";
            }
        } else if (auto *ns = std::get_if<aasm::NewS>(&ins)) {
            int size = prog.types.at(ns->id).size() * 8;
            std::cout << p_op(ns->target) << " = call ptr @malloc(i64 " << size << ")\n";
        } else if (auto *na = std::get_if<aasm::NewA>(&ins)) {
            std::cout << p_op(na->target) << " = call ptr @malloc(i64 " << na->size * 8 << ")\n";
        } else if (auto *free = std::get_if<aasm::Free>(&ins)) {
            std::cout << "call void @free(ptr " << p_op(free->value) << ")\n";
        } else if (auto *call = std::get_if<aasm::Call>(&ins)) {
            if (call->target.has_value()) {
                std::cout << p_op(call->target.value()) << " = ";
            }
            std::unordered_map<std::string, Type> runtime_map = {
                {"print", Void {}},
                {"println", Void {}},
                {"readnum", Int {}},
            };
            Type ret_t;
            if (runtime_map.contains(call->id)) {
                ret_t = runtime_map.at(call->id);
            } else {
                ret_t = prog.functions.at(call->id).return_type;
            }
            std::cout << "call " << aasm_type(ret_t) << " @" << call->id << "(";
            bool first = true;
            for (auto &arg : call->arguments) {
                if (!first) std::cout << ", ";
                first = false;
                std::cout << aasm_type(arg.type) << " " << p_op(arg);
            }
            std::cout << ") \n";
        } else if (auto *phi = std::get_if<aasm::Phi>(&ins)) {
            std::cout << p_op(phi->target) << " = phi " << aasm_type(phi->target.type) << " ";
            bool first = true;
            for (auto &[pred, op] : phi->bindings) {
                if (!first) {
                    std::cout << ", ";
                }
                first = false;

                std::cout << "[ " << p_op(op) << ", %l" << ref_map.at(pred) << " ]";
            }
            std::cout << "\n";
        } else {
            std::cerr << "Unhandled AASM instruction. Quitting...\n";
            std::exit(1);
        }
    }
}

void print_aasm_cfg(const Program &prog, const Function &func, const cfg::RefMap &ref_map) {
    auto print_aasm_ref = [&](cfg::Ref &ref) {
        std::cout << "\nl" << ref_map.at(ref) << ":\n";
        print_aasm_insns(prog, func, ref_map, ref, cfg_instructions(ref));
    };

    cfg_traverse(func.entry_ref, print_aasm_ref);
}

void print_aasm_function(const Program &prog, const Function &func, const cfg::RefMap &ref_map, bool ssa) {
    std::cout << "define " << aasm_type(func.return_type) << " @"
              << func.id << "(";
    bool first = true;
    int i = 0;
    for (auto &decl : func.parameters) {
        if (!first) std::cout << ", ";
        first = false;
        std::cout << aasm_type(decl.type) << " %" << (ssa ? decl.id : std::to_string(i++));
    }
    std::cout << ") {\n";

    if (!ssa) {
        for (auto &[id, type] : func.local_env) {
            std::cout << "%" << id << " = alloca " << aasm_type(type) << "\n";
        }
        i = 0;
        for (auto &param : func.parameters) {
            std::cout << "store " << aasm_type(param.type) << " %" << i++ << ", ptr %" << param.id << "\n";
        }
    }

    std::cout << "br label %l" << ref_map.at(func.entry_ref) << "\n";

    print_aasm_cfg(prog, func, ref_map);

    std::cout << "}\n";
}

void print_aasm_program(const Program &prog, bool ssa) {
    print_aasm_types(prog.types);
    std::cout << "\n";

    print_aasm_decls(prog.declarations);
    std::cout << "\n";

    cfg::RefMap ref_map = cfg_enumerate(prog);
    for (auto &[_, func] : prog.functions) {
        print_aasm_function(prog, func, ref_map, ssa);
        std::cout << "\n";
    }

    std::cout << "declare ptr @malloc(i64)\n";
    std::cout << "declare void @free(ptr)\n";
    std::cout << "declare void @print(i64)\n";
    std::cout << "declare void @println(i64)\n";
    std::cout << "declare i64 @readnum()\n";
}
