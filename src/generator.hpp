#pragma once
#include <iostream>

#include "parser.hpp"

class Generator {
   public:
    inline Generator(NodeExit root) : _root(std::move(root)) {
    }
    [[nodiscard]] std::string generate() const {
        std::stringstream output;

        output << "global _start\n";
        output << "_start:\n";
        output << "    mov rax, 60\n";
        output << "    mov rdi, " << _root.expr._int.value.value() << "\n";
        output << "    syscall\n";
        return output.str();
    }

   private:
    const NodeExit _root;
};