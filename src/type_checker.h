#pragma once

#include "ast.h"

void check_program(Program &program);
Type check_env(const Environment &lenv, const Environment &tenv, const std::string &id);
