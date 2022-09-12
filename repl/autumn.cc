#include <functional>
#include <iostream>
#include <map>
#include <stdio.h>

#include "color.h"
#include "lexer.h"
#include "parser.h"
#include "evaluator.h"
#include "run_code.cc"

#include <readline/readline.h>
#include <readline/history.h>

const std::string PROMPT = "> ";

void lexer_repl(const std::string& line);
void parser_repl(const std::string& line);
void eval_repl(const std::string& line);
void do_nothing(const std::string& line);

autumn::Evaluator evaluator;

const std::map<std::string, std::function<void(const std::string&)>> REPLS = {
    {"lexer", lexer_repl},    
    {"parser", parser_repl},    
    {"eval", eval_repl},
};


void run_code(const std::string& path);

int main(int argc, char* argv[]) {
    std::cout << argv[1] << " " << argv[2] << std::endl;
    if (argc > 2 && std::string(argv[1]) == std::string("run")) {
        run_code(argv[2]);
        return 0;
    }
    
    std::function<void(const std::string&)> repl(do_nothing);
    if (argc > 1) {
        auto it = REPLS.find(argv[1]);
        if (it != REPLS.end()) {
            repl = it->second;
        }
    }
    std::string line;
    while (true) {
        char* ln = readline(PROMPT.c_str());
        if (ln == nullptr) return 0;

        line = ln;
        free(ln);

        if (!line.empty()) {
            if (line == "q" || line == "quit") return 0;
            repl(line);
            add_history(line.c_str());
        }
    }

    return 0;
}

void lexer_repl(const std::string& line) {
    autumn::Lexer lexer(line);

    for (auto token = lexer.next_token();
            token.type != autumn::Token::END;
            token = lexer.next_token()) {
        std::cout << token << std::endl;
    }
}

void parser_repl(const std::string& line) {
    autumn::Parser parser;
    auto program = parser.parse(line);
    if (!parser.errors().empty()) {
        for (auto& error : parser.errors()) {
            std::cerr << autumn::color::light::red
                << "error: " << autumn::color::off
                << error << std::endl;
        }
        return;
    }
    std::cout << program->to_string() << std::endl;
}

void eval_repl(const std::string& line) {
    auto obj = evaluator.eval(line);
    if (obj == nullptr) {
        return;
    }

    std::cout << obj->inspect() << std::endl;
}

void do_nothing(const std::string& line) {
    std::cout << "do_nothing:" << line << std::endl;
}
