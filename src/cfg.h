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

    struct Basic {
        Block statements;
        Ref next;
    };

    struct Conditional {
        Block statements;
        Expression guard;
        Ref tru;
        Ref fals;
    };

    struct Return {
        Block statements;
    };

    using Function = GenericFunction<Ref>;
    using Functions = std::unordered_map<std::string, Function>;
    using Program = GenericProgram<Functions>;
}

cfg::Program write_cfg(Program &&prog);
void print_cfgs(const cfg::Program &prog);
