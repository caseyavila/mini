#ifndef AASM_H
#define AASM_H

#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace aasm {
    struct Imm {
        int64_t val;
        friend bool operator==(const Imm& lhs, const Imm& rhs) = default;
    };
    struct ImmB {
        bool val;
        friend bool operator==(const ImmB& lhs, const ImmB& rhs) = default;
    };
    struct Var {
        int id;
        friend bool operator==(const Var& lhs, const Var& rhs) = default;
    };
    struct Id {
        std::string id;
        friend bool operator==(const Id& lhs, const Id& rhs) = default;
    };
    struct Glob {
        std::string id;
        friend bool operator==(const Glob& lhs, const Glob& rhs) = default;
    };
    struct Null {
        friend bool operator==(const Null& lhs, const Null& rhs) { return true; };
    };

    using Value = std::variant<Imm, ImmB, Var, Id, Glob, Null>;

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

    struct Load;
    struct Store;
    struct Binary;
    struct Call;
    struct Ret;
    struct Free;
    struct NewS;
    struct NewA;
    struct Gep;
    struct Jump;
    struct Br;
    struct Phi;

    using Ins = std::variant<Load, Store, Binary, Call, Ret, Br, Jump, Free, NewS, NewA, Gep, Phi>;
    using Block = std::vector<Ins>;
}

#include "cfg.h"

namespace aasm {
    struct Operand {
        Value value;
        Type type;
        friend bool operator==(const Operand& lhs, const Operand& rhs) {
              return lhs.value == rhs.value;
        }
    };

    struct Load { Operand target; Operand ptr; };
    struct Store { Operand value; Operand ptr; };

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

    struct Free { Operand value; };

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

/* hashing for Operand */
namespace std {
    template<> struct hash<aasm::Imm> {
        size_t operator()(const aasm::Imm& imm) const {
            return hash<int64_t>()(imm.val);
        }
    };

    template<> struct hash<aasm::ImmB> {
        size_t operator()(const aasm::ImmB& immb) const {
            return hash<bool>()(immb.val);
        }
    };

    template<> struct hash<aasm::Var> {
        size_t operator()(const aasm::Var& var) const {
            return hash<int>()(var.id);
        }
    };

    template<> struct hash<aasm::Id> {
        size_t operator()(const aasm::Id& id) const {
            return hash<string>()(id.id);
        }
    };

    template<> struct hash<aasm::Glob> {
        size_t operator()(const aasm::Glob& glob) const {
            return hash<string>()(glob.id);
        }
    };

    template<> struct hash<aasm::Null> {
        size_t operator()(const aasm::Null&) const {
            return 0;
        }
    };

    template<> struct hash<aasm::Operand> {
        size_t operator()(const aasm::Operand& op) const {
            return hash<aasm::Value>()(op.value);
        }
    };
}

void in_op_traverse(aasm::Ins& ins, std::function<void(aasm::Operand&)> lambda);
void aasm_program(Program& prog);

#endif
