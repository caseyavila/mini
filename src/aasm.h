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

    struct Call {
        std::optional<Operand> target;
        std::string id;
        std::vector<Operand> arguments;
    };

    struct Ret {
        std::optional<Operand> value;
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

    struct Br;

    using Ins = std::variant<Load, Store, Binary, Call, Ret, Br, Jump, Free, NewS, NewA, Gep>;
    using Block = std::vector<Ins>;
}

#include "cfg.h"

namespace aasm {
    struct Br {
        Operand guard;
        cfg::Ref tru;
        cfg::Ref fals;
    };
}

void aasm_program(cfg::Program &prog);
