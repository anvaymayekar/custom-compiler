#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "tokenizer.hpp"

std::string tokensToASM(const std::vector<Token> &tokens) {
    std::stringstream output;

    output << "global _start\n";
    output << "_start:\n";
    for (size_t i = 0; i < tokens.size(); i++) {
        const Token &token = tokens[i];

        if (token.type == TokenType::_exit) {
            if (i + 2 < tokens.size() &&
                tokens[i + 1].type == TokenType::_int &&
                tokens[i + 2].type == TokenType::semi) {
                output << "    mov rax, 60\n";
                output << "    mov rdi, " << tokens[i + 1].value.value()
                       << "\n";
                output << "    syscall\n";

                i += 2;
            }

            else {
                std::cerr << "Invalid return statement\n";
                exit(EXIT_FAILURE);
            }
        }
    }

    return output.str();
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Incorrect Usage. Use: compiler <file.cx>\n";
        return EXIT_FAILURE;
    }

    std::string contents;

    {
        std::ifstream input(argv[1]);

        if (!input) {
            std::cerr << "Failed to open file: " << argv[1] << "\n";
            return EXIT_FAILURE;
        }

        std::stringstream contentsStream;
        contentsStream << input.rdbuf();

        contents = contentsStream.str();
    }
    Tokenizer tokenizer(std::move(contents));
    std::vector<Token> tokens = tokenizer.tokenize();
    {
        std::ofstream file("./out.asm");

        if (!file) {
            std::cerr << "Failed to create output file\n";
            return EXIT_FAILURE;
        }

        file << tokensToASM(tokens);
    }
    system("nasm -f elf64 out.asm");
    system("ld -o out out.o");
    std::cout << "Successfully assembled!" << std::endl;
    return EXIT_SUCCESS;
}