#include "tail_rec.h"
#include "ast.h"
#include "cfg.h"
#include <memory>
#include <variant>

/* climb my pyramid of doom */
void tail_rec_func(cfg::Function &func) {
    bool tailed = false;
    auto new_entry = std::make_shared<cfg::Basic>(cfg::Basic {
        {}, {}, func.entry_ref
    });

    auto lambda = [&](cfg::Ref &ref) {
        auto *basic = std::get_if<std::shared_ptr<cfg::Basic>>(&ref);
        if (!basic) return;

        Block &stmts = basic->get()->statements;
        cfg::Ref &next = basic->get()->next;

        if (!cfg_equals(next, func.ret_ref)) return;
        if (stmts.size() == 0) return;

        Invocation *inv;
        if (std::holds_alternative<Void>(func.return_type)) {
            inv = std::get_if<Invocation>(&stmts.back());
            if (!inv) return;
        } else {
            auto *ass = std::get_if<Assignment>(&stmts.back());
            if (!ass) return;

            auto *expr = std::get_if<Expression>(&ass->source);
            if (!expr) return;

            inv = std::get_if<Invocation>(expr);
            if (!inv) return;
        }

        if (inv->id == func.id) {
            std::cerr << "tail recursion: " << func.id << "\n";
            std::vector<Expression> temp_args;

            temp_args.reserve(inv->arguments.size());
            for (auto &expr : inv->arguments) {
                temp_args.push_back(std::move(expr));
            }

            stmts.pop_back();

            int i = 0;
            for (auto &decl : func.parameters) {
                func.local_env.emplace("_" + decl.id, decl.type);
                stmts.push_back(Assignment { "_" + decl.id, std::move(temp_args[i]) });
                i++;
            }
            for (auto &decl : func.parameters) {
                stmts.push_back(Assignment { decl.id, "_" + decl.id });
            }

            next = func.entry_ref;
            tailed = true;
        }
    };

    cfg_traverse(func.entry_ref, lambda);

    if (tailed) {
        func.entry_ref = new_entry;
    }
}

void tail_rec_program(cfg::Program &cfg_prog) {
    for (auto &[_, func]: cfg_prog.functions) {
        tail_rec_func(func);
    }
}
