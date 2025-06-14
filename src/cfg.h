#pragma once

#include <functional>
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

    using WeakRef = std::variant<std::weak_ptr<Basic>, std::weak_ptr<Conditional>,
                                 std::weak_ptr<Return>>;

    /* plzzzzzzzz work */
    struct RefOwnerLess {
        template <typename T1, typename T2>
        bool operator()(const T1& lhs, const T2& rhs) const {
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
void cfg_traverse(const cfg::Ref &ref, std::function<void(cfg::Ref &)> lambda);
bool cfg_equals(const cfg::Ref &ref1, const cfg::Ref &ref2);
const cfg::RefMap cfg_enumerate(const cfg::Program &prog);
const cfg::WeakRef ref_weaken(const cfg::Ref &ref);
const cfg::Ref ref_strengthen(const cfg::WeakRef &ref);
