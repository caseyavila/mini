#pragma once

#include "cfg.h"
#include <optional>

namespace sscp {
    struct Top {
        bool operator==(const Top& other) const { return true; }
    };
    struct Bot {
        bool operator==(const Bot& other) const { return true; }
    };
    struct Null {
        bool operator==(const Null& other) const { return true; }
    };

    using Value = std::variant<Top, Bot, Null, int64_t, bool>;

    bool operator==(const Value& val, const bool& b);
}

void sscp_program(cfg::Program &prog);
