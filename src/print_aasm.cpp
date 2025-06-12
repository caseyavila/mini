#include "print_aasm.h"

void print_aasm_program(const cfg::Program &prog) {
    cfg::RefMap ref_map = cfg_enumerate(prog);

    std::cout << ref_map.size() << "\n";
}
