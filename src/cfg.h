#ifndef CFG_H
#define CFG_H

#include <functional>
#include <memory>
#include <unordered_map>
#include <map>
#include <variant>
#include <vector>

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

    using RefMap = std::map<Ref, int, RefOwnerLess>;
}

#include "ast.h"
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

void cfg_program(Program &prog);
cfg::Ref cfg_ref(cfg::Cfg &&cfg);
template <typename T>
std::shared_ptr<T> cfg_get_if(const cfg::Ref *ref);
void cfg_traverse(const cfg::Ref &ref, std::function<void(cfg::Ref &)> lambda);
std::vector<aasm::Ins> &cfg_instructions(const cfg::Ref &ref);
bool cfg_equals(const cfg::Ref &ref1, const cfg::Ref &ref2);
const cfg::RefMap cfg_enumerate(const Program &prog);
const cfg::Ref ref_weaken(const cfg::Ref &ref);

#endif
