#pragma once

#include "parser.hpp"

class Generator {
   public:
    inline Generator(NodeExit root) : _root(std::move(root)) {
    }
    [[nodiscard]] std::string generate() const {
        std::stringstream output;

        output << "global _start\n";
        output << "_start:\n";
    }

   private:
    const NodeExit _root;
}