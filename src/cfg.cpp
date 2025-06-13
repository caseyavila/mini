#include <functional>
#include <memory>
#include <variant>

#include "cfg.h"
#include "ast.h"

/* returns a reference to a cfg object */
cfg::Ref cfg_ref(cfg::Cfg &&cfg) {
    if (auto *basic = std::get_if<cfg::Basic>(&cfg)) {
        return std::make_shared<cfg::Basic>(
            std::move(basic->statements),
            std::move(basic->instructions),
            std::move(basic->next)
        );
    } else if (auto *cond = std::get_if<cfg::Conditional>(&cfg)) {
        return std::make_shared<cfg::Conditional>(
            std::move(cond->statements),
            std::move(cond->instructions),
            std::move(cond->guard),
            std::move(cond->tru),
            std::move(cond->fals)
        );
    } else if (auto *ret = std::get_if<cfg::Return>(&cfg)) {
        return std::make_shared<cfg::Return>(
            std::move(ret->statements),
            std::move(ret->instructions)
        );
    }

    std::cerr << "Unhandled cfg reference creation. Quitting...\n";
    std::exit(1);
}

/* moving from the original ast block! */
cfg::Ref cfg_block(Block::iterator begin, Block::iterator end, const cfg::Ref &follow) {
    Block cfg_stmts;

    for (auto it = begin; it != end; it++) {
        Statement& current_stmt = *it;
        if (auto *loop_s = std::get_if<Loop>(&current_stmt)) {
            auto cond = std::make_shared<cfg::Conditional>(cfg::Conditional {
                {},
                {},
                std::move(loop_s->guard)
            });
            std::weak_ptr<cfg::Conditional> weak_cond = cond;

            cfg::Ref body_ref = cfg_block(loop_s->body.begin(), loop_s->body.end(), weak_cond);
            cond->tru = body_ref;

            cfg::Ref after_ref = cfg_block(it + 1, end, follow);
            cond->fals = after_ref;

            if (cfg_stmts.size() == 0) {
                return cond;
            } else {
                return cfg_ref(cfg::Basic {
                    std::move(cfg_stmts),
                    {},
                    cond
                });
            }
        } else if (auto *cond_s = std::get_if<Conditional>(&current_stmt)) {
            cfg::Conditional cond = cfg::Conditional {
                std::move(cfg_stmts),
                {},
                std::move(cond_s->guard)
            };

            cfg::Ref after_ref = cfg_block(it + 1, end, follow);
            cond.fals = after_ref;

            cfg::Ref then_ref = cfg_block(cond_s->then.begin(), cond_s->then.end(), after_ref);
            cond.tru = then_ref;

            if (cond_s->els.has_value()) {
                cfg::Ref els_ref = cfg_block(cond_s->els.value().begin(), cond_s->els.value().end(), after_ref);
                cond.fals = els_ref;
            }

            return cfg_ref(std::move(cond));
        } else if (std::holds_alternative<Return>(current_stmt)) {
            cfg_stmts.emplace_back(std::move(*it));
            return cfg_ref(cfg::Return { std::move(cfg_stmts), {} });
        } else {
            cfg_stmts.emplace_back(std::move(*it));
        }
    }

    if (cfg_stmts.size() == 0) {
        return follow;
    }

    return cfg_ref(cfg::Basic { std::move(cfg_stmts), {}, follow });
}

void cfg_traverse(const cfg::Ref &ref, std::function<void(cfg::Ref &)> lambda) {
    std::set<cfg::Ref, cfg::RefOwnerLess> seen;
    std::vector<cfg::Ref> stack;
    cfg::Ref curr = ref;

    stack.emplace_back(curr);

    while (!stack.empty()) {
        if (!seen.contains(curr)) {
            seen.emplace(curr);

            lambda(curr);

            if (auto *basic = std::get_if<std::shared_ptr<cfg::Basic>>(&curr)) {
                stack.emplace_back(basic->get()->next);
            } else if (auto *cond = std::get_if<std::shared_ptr<cfg::Conditional>>(&curr)) {
                stack.emplace_back(cond->get()->fals);
                stack.emplace_back(cond->get()->tru);
            } else if (std::holds_alternative<std::weak_ptr<cfg::Conditional>>(curr)) {
                std::cout << "Actually got a weak CFG ref, please examine...\n";
                std::exit(1);
            }
        }

        curr = stack.back();
        stack.pop_back();
    }
}

cfg::RefMap cfg_enumerate(const cfg::Program &prog) {
    cfg::RefMap map;
    int block_id = 0;

    auto lambda = [&](cfg::Ref &ref) {
        map.emplace(ref, block_id++);
    };

    for (auto &[_, func] : prog.functions) {
        cfg_traverse(func.body, lambda);
    }

    return map;
}

cfg::Function cfg_function(Function &&func) {
    return cfg::Function {
        std::move(func.id),
        std::move(func.parameters),
        std::move(func.return_type),
        std::move(func.declarations),
        cfg_block(func.body.begin(), func.body.end(), std::make_shared<cfg::Return>()),
        std::move(func.local_env)
    };
}

cfg::Program cfg_program(Program &&prog) {
    cfg::Functions funcs;
    for (auto &[id, func] : prog.functions) {
        funcs.emplace(id, cfg_function(std::move(func)));
    }

    return cfg::Program {
        std::move(prog.types),
        std::move(prog.declarations),
        std::move(funcs),
        std::move(prog.top_env)
    };
}
