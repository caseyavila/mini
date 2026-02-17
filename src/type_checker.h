#ifndef TYPE_CHECKER_H
#define TYPE_CHECKER_H

#include "ast.h"

void check_program(Program &program);
Type check_env(const Environment &lenv, const Environment &tenv, const std::string &id);

#endif
