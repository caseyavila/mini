#pragma once

#include <memory>
#include <unordered_map>
#include <variant>
#include <vector>

#include "ast.h"

namespace cfg {
    struct Basic;
    struct Conditional;
    struct Return;

    using Cfg = std::variant<Basic, Conditional, Return>;
    using Ref = std::variant<std::shared_ptr<Basic>, std::shared_ptr<Conditional>,
                             std::weak_ptr<Conditional>, std::shared_ptr<Return>>;

    /* plzzzzzzzz work */
    struct RefOwnerLess {
        bool operator()(const cfg::Ref& lhs, const cfg::Ref& rhs) const {
            return std::visit(
                [&](auto&& arg1, auto&& arg2) -> bool {
                    return std::owner_less<>{}(arg1, arg2);
                },
                lhs,
                rhs
            );
        }
    };

    using Function = GenericFunction<Ref>;
    using Functions = std::unordered_map<std::string, Function>;
    using Program = GenericProgram<Functions>;
    using RefMap = std::map<Ref, int, RefOwnerLess>;
}

#include "aasm.h"

namespace cfg {
    struct Basic {
        Block statements;
        aasm::Block instructions;
        Ref next;
    };


    struct Conditional {
        Block statements;
        aasm::Block instructions;
        Expression guard;
        Ref tru;
        Ref fals;
    };

    struct Return {
        Block statements;
        aasm::Block instructions;
    };
}

cfg::Program cfg_program(Program &&prog);
cfg::RefMap cfg_enumerate(const cfg::Program &prog, bool print);
