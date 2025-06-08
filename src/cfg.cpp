#include <map>
#include <memory>
#include <variant>

#include "cfg.h"
#include "ast.h"

/* returns a reference to a cfg object */
cfg::Ref cfg_ref(cfg::Cfg &&cfg) {
    if (auto *basic = std::get_if<cfg::Basic>(&cfg)) {
        return std::make_shared<cfg::Basic>(
            std::move(basic->statements),
            std::move(basic->next)
        );
    } else if (auto *cond = std::get_if<cfg::Conditional>(&cfg)) {
        return std::make_shared<cfg::Conditional>(
            std::move(cond->statements),
            std::move(cond->guard),
            std::move(cond->tru),
            std::move(cond->fals)
        );
    } else if (auto *ret = std::get_if<cfg::Return>(&cfg)) {
        return std::make_shared<cfg::Return>(
            std::move(ret->statements)
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
                    cond
                });
            }
        } else if (auto *cond_s = std::get_if<Conditional>(&current_stmt)) {
            cfg::Conditional cond = cfg::Conditional {
                std::move(cfg_stmts),
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
            return cfg_ref(cfg::Return { std::move(cfg_stmts) });
        } else {
            cfg_stmts.emplace_back(std::move(*it));
        }
    }

    if (cfg_stmts.size() == 0) {
        return follow;
    }

    return cfg_ref(cfg::Basic { std::move(cfg_stmts), follow });
}

/* plzzzzzzzz work */
struct CfgRefOwnerLess {
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

/* for testing */
void print_cfgs(const Program &prog) {
    for (auto &[_, func] : prog.functions) {
        std::cout << "\nfunction " << func.id << ":\n";
        std::map<cfg::Ref, int, CfgRefOwnerLess> seen;
        std::vector<std::pair<cfg::Ref, int>> stack;
        int block_id = 0;

        cfg::Ref curr = func.cfg;
        int curr_indent = 0;
        stack.emplace_back(curr, curr_indent);

        while (!stack.empty()) {
            for (int i = 0; i < curr_indent; i++) {
                std::cout << " ";
            }

            if (seen.contains(curr)) {
                std::cout << "-- " << seen[curr] << " --\n";
            } else {
                seen.emplace(curr, block_id);

                if (auto *ret = std::get_if<std::shared_ptr<cfg::Return>>(&curr)) {
                    std::cout << block_id << ": return (" << ret->get()->statements.size() << ")\n";
                } else if (auto *basic = std::get_if<std::shared_ptr<cfg::Basic>>(&curr)) {
                    std::cout << block_id << ": basic (" << basic->get()->statements.size() << ")\n";
                    stack.emplace_back(basic->get()->next, curr_indent + 1);
                } else if (auto *cond = std::get_if<std::shared_ptr<cfg::Conditional>>(&curr)) {
                    std::cout << block_id << ": conditional (" << cond->get()->statements.size() << ")\n";
                    stack.emplace_back(cond->get()->fals, curr_indent + 1);
                    stack.emplace_back(cond->get()->tru, curr_indent + 1);
                } else if (std::holds_alternative<std::weak_ptr<cfg::Conditional>>(curr)) {
                    std::cout << "Actually got a weak CFG ref, please examine...\n";
                    std::exit(1);
                }
                block_id++;
            }

            curr = stack.back().first;
            curr_indent = stack.back().second;
            stack.pop_back();
        }
    }
}

void write_cfg_function(Function &func) {
    func.cfg = cfg_block(func.body.begin(), func.body.end(), std::make_shared<cfg::Return>());
}

void write_cfg(Program &prog) {
    for (auto &[_, func] : prog.functions) {
        write_cfg_function(func);
    }
}
