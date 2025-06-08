#pragma once

#include <memory>
#include <variant>
#include <vector>

#include "ast.h"

void write_cfg(Program &prog);
void print_cfgs(const Program &prog);
