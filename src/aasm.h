#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "ast.h"

namespace aasm {
    struct Imm {
        int val;
        bool operator==(const Imm& other) const { return val == other.val; }
    };
    struct ImmB {
        bool val;
        bool operator==(const ImmB& other) const { return val == other.val; }
    };
    struct Var {
        int id;
        bool operator==(const Var& other) const { return id == other.id; }
    };
    struct Id {
        std::string id;
        bool operator==(const Id& other) const { return id == other.id; }
    };
    struct Glob {
        std::string id;
        bool operator==(const Glob& other) const { return id == other.id; }
    };
    struct Null {
        bool operator==(const Null& other) const { return true; }
    };

    using Value = std::variant<Imm, ImmB, Var, Id, Glob, Null>;

    struct Operand {
        Value value;
        Type type;
        bool operator==(const Operand& other) const {
              return value == other.value;
        }
    };

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
        Operand index;
    };

    struct Jump;
    struct Br;
    struct Phi;

    using Ins = std::variant<Load, Store, Binary, Call, Ret, Br, Jump, Free, NewS, NewA, Gep, Phi>;
    using Block = std::vector<Ins>;
}

#include "cfg.h"

namespace aasm {
    struct Jump { cfg::Ref block; };

    struct Br {
        Operand guard;
        cfg::Ref tru;
        cfg::Ref fals;
    };

    struct Phi {
        Operand target;
        std::string id;
        std::map<cfg::Ref, Operand, cfg::RefOwnerLess> bindings;
    };
}

void aasm_program(cfg::Program &prog);
