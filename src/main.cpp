#include <cstdlib>
#include <filesystem>
#include <unordered_set>

#include "MiniLexer.h"
#include "MiniParser.h"

#include "aasm.h"
#include "sscp.h"
#include "tail_rec.h"
#include "unused_result.h"
#include "print_arm.h"
#include "print_llvm.h"
#include "ast.h"
#include "cfg.h"
#include "ssa.h"
#include "type_checker.h"
#include "error_listener.h"

constexpr std::string_view extension = ".mini";

void usage(char *cmd) {
    std::cerr << "Usage: " << cmd << " [-S] [--tail] [--arm] [--ssa [--sscp] [--unused]] <file>\n";
    std::exit(1);
}

std::unordered_set<std::string> parse_args(int argc, char *argv[]) {
    std::unordered_set<std::string> valid = {
        "-S",
        "--tail",
        "--ssa",
        "--sscp",
        "--unused",
        "--arm"
    };

    std::unordered_set<std::string> args;

    if (argc < 2) {
        usage(argv[0]);
    }

    for (int i = 1; i < argc - 1; i++) {
        if (!valid.contains(argv[i])) {
            std::cerr << "Unrecongnized option: '" << argv[i] << "'\n";
            usage(argv[0]);
        }
        args.emplace(argv[i]);
    }

    if (!args.contains("--ssa") && (args.contains("--sscp") || args.contains("--unused"))) {
        usage(argv[0]);
    }

    return args;
}

int main(int argc, char *argv[]) {
    std::unordered_set<std::string> args = parse_args(argc, argv);

    std::string mini_name = argv[argc - 1];
    if (std::filesystem::path(mini_name).extension() != extension) {
        std::cerr << "Can only compile files ending with '" << extension << "'\n";
        usage(argv[0]);
    }

	std::ifstream mini_file(mini_name);

	if (!mini_file.is_open()) {
        std::perror("Failed to open input file");
        return 1;
    }

    antlr4::ANTLRInputStream input(mini_file);
    MiniLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    MiniParser parser(&tokens);

    ErrorListener errorListener;
    parser.addErrorListener(&errorListener);
    lexer.addErrorListener(&errorListener);

	Program prog = parse_program(parser.program());
    mini_file.close();

	check_program(prog);
	cfg_program(prog);

	/* cfg optimizations */
	if (args.contains("--tail")) {
	    tail_rec_program(prog);
	}

	aasm_program(prog);

	/* aasm optimizations */
	if (args.contains("--ssa")) {
	    ssa_program(prog);
		if (args.contains("--sscp")) {
		    sscp_program(prog);
		}
		if (args.contains("--unused")) {
		    unused_result(prog);
		}
	}

	std::string exec_name = std::filesystem::path(mini_name).stem();
	std::string s_name = exec_name + (args.contains("--arm") ? ".s" : ".ll");
	std::ofstream s_file(s_name);

	/* save stdout and redirect to .ll */
	std::streambuf *stdout = std::cout.rdbuf();
	std::cout.rdbuf(s_file.rdbuf());

	if (args.contains("--arm")) {
   	    print_arm_program(prog, args.contains("--ssa"));
	} else {
   	    print_llvm_program(prog, args.contains("--ssa"));
	}

	std::cout.rdbuf(stdout);
	s_file.close();

	if (!args.contains("-S")) {
        std::system(("clang -Wno-override-module util.c " + s_name + " -o " + exec_name).c_str());
    	std::remove(s_name.c_str());
	}

    return 0;
}
