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
    using Ref = std::variant<std::shared_ptr<Basic>,
                             std::shared_ptr<Conditional>,
                             std::shared_ptr<Return>,
                             std::weak_ptr<Basic>,
                             std::weak_ptr<Conditional>,
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

    struct Function {
	std::string id;
        std::vector<Declaration> parameters;
        Type return_type;
        std::vector<Declaration> declarations;
        Ref entry_ref;
        Ref ret_ref;
        Environment local_env;
    };
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
cfg::Ref cfg_ref(cfg::Cfg &&cfg);
template <typename T>
std::shared_ptr<T> cfg_get_if(const cfg::Ref *ref);
void cfg_traverse(const cfg::Ref &ref, std::function<void(cfg::Ref &)> lambda);
std::vector<aasm::Ins> &cfg_instructions(const cfg::Ref &ref);
bool cfg_equals(const cfg::Ref &ref1, const cfg::Ref &ref2);
const cfg::RefMap cfg_enumerate(const cfg::Program &prog);
const cfg::Ref ref_weaken(const cfg::Ref &ref);
