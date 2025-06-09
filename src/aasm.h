#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "ast.h"

namespace aasm {
    struct Imm { int val; };
    struct ImmB { bool val; };
    struct Var { int id; };
    struct Id { std::string id; };
    struct PhId { std::string id; };
    struct Null {};

    using Operand = std::variant<Imm, ImmB, Var, Id, PhId, Null>;

    struct Load { Operand target; Operand ptr; };
    struct Store { Operand value; Operand ptr; };

    struct Add {};
    struct Sub {};
    struct Mul {};
    struct Div {};
    struct Xor {};
    struct And {};
    struct Or {};
    struct Gt {};
    struct Ge {};
    struct Lt {};
    struct Le {};
    struct Eq {};
    struct Ne {};

    using BinaryOp = std::variant<Add, Sub, Mul, Div, Xor, And,
                                  Or, Gt, Ge, Lt, Le, Eq, Ne>;

    struct Binary {
        BinaryOp op;
        Operand target;
        Operand left;
        Operand right;
    };

    struct Inv {
        std::optional<Operand> target;
        std::string id;
        std::vector<Operand> arguments;
    };

    struct Ret {
        std::optional<Operand> value;
    };

    struct Br {
        Operand guard;
        int tru;
        int fals;
    };

    struct Jump { int block; };
    struct Free { Operand target; };

    struct NewS {
        Operand target;
        std::string id;
    };

    struct NewA {
        Operand target;
        int size;
    };

    struct Gep {
        Operand target;
        Operand value;
        std::variant<std::string, Operand> index;
    };

    using Ins = std::variant<Load, Store, Binary, Inv, Ret, Br, Jump, Free, NewS, NewA, Gep>;

    using Block = std::vector<Ins>;
    using Function = GenericFunction<std::unordered_map<int, std::vector<Ins>>>;
    using Functions = std::unordered_map<std::string, Function>;
    using Program = GenericProgram<Functions>;
}

aasm::Program write_aasm(Program &&prog);
