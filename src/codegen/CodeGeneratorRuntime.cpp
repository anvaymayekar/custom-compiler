#include "codegen/CodeGenerator.hpp"

namespace mr {

void CodeGenerator::emitPrintIntRoutine() {
    _out << "; print_int: writes the signed 64-bit value in rdi to stdout as\n";
    _out << "; decimal ASCII followed by a trailing newline.\n";
    _out << "print_int:\n";
    _out << "    push rbp\n";
    _out << "    mov rbp, rsp\n";
    _out << "    sub rsp, 32\n";
    _out << "    mov byte [rbp - 1], 10\n";
    _out << "    mov rax, rdi\n";
    _out << "    xor r8, r8\n";
    _out << "    cmp rax, 0\n";
    _out << "    jge .print_int_conv\n";
    _out << "    mov r8, 1\n";
    _out << "    neg rax\n";
    _out << ".print_int_conv:\n";
    _out << "    mov rbx, 10\n";
    _out << "    xor rcx, rcx\n";
    _out << "    lea r9, [rbp - 2]\n";
    _out << ".print_int_digit_loop:\n";
    _out << "    xor rdx, rdx\n";
    _out << "    div rbx\n";
    _out << "    add dl, '0'\n";
    _out << "    mov [r9], dl\n";
    _out << "    dec r9\n";
    _out << "    inc rcx\n";
    _out << "    test rax, rax\n";
    _out << "    jnz .print_int_digit_loop\n";
    _out << "    cmp r8, 0\n";
    _out << "    je .print_int_no_sign\n";
    _out << "    mov byte [r9], '-'\n";
    _out << "    dec r9\n";
    _out << "    inc rcx\n";
    _out << ".print_int_no_sign:\n";
    _out << "    inc r9\n";
    _out << "    mov rax, 1\n";
    _out << "    mov rdi, 1\n";
    _out << "    mov rsi, r9\n";
    _out << "    mov rdx, rcx\n";
    _out << "    inc rdx\n";
    _out << "    syscall\n";
    _out << "    mov rsp, rbp\n";
    _out << "    pop rbp\n";
    _out << "    ret\n";
}

void CodeGenerator::emitPrintStrRoutine() {
    _out
        << "; print_str: writes the length-prefixed string pointed to by rdi\n";
    _out
        << "; ([rdi] = 8-byte length, rdi+8.. = raw bytes) to stdout, then a\n";
    _out << "; trailing newline.\n";
    _out << "print_str:\n";
    _out << "    mov rdx, [rdi]\n";
    _out << "    lea rsi, [rdi + 8]\n";
    _out << "    mov rax, 1\n";
    _out << "    mov rdi, 1\n";
    _out << "    syscall\n";
    _out << "    mov rax, 1\n";
    _out << "    mov rdi, 1\n";
    _out << "    mov rsi, nl_byte\n";
    _out << "    mov rdx, 1\n";
    _out << "    syscall\n";
    _out << "    ret\n\n";
}

void CodeGenerator::emitPrintCharRoutine() {
    _out << "; print_char: writes the single byte in the low 8 bits of rdi\n";
    _out << "; to stdout, then a trailing newline.\n";
    _out << "print_char:\n";
    _out << "    push rdi\n";
    _out << "    mov rax, 1\n";
    _out << "    mov rdi, 1\n";
    _out << "    mov rsi, rsp\n";
    _out << "    mov rdx, 1\n";
    _out << "    syscall\n";
    _out << "    add rsp, 8\n";
    _out << "    mov rax, 1\n";
    _out << "    mov rdi, 1\n";
    _out << "    mov rsi, nl_byte\n";
    _out << "    mov rdx, 1\n";
    _out << "    syscall\n";
    _out << "    ret\n\n";
}

void CodeGenerator::emitPrintFloatRoutine() {
    _out << "; print_float: writes the IEEE-754 double whose bit pattern is\n";
    _out << "; in rdi to stdout as \"[-]digits.dddddd\" (fixed 6 fractional\n";
    _out
        << "; digits, like printf \"%.6f\") followed by a newline. This is a\n";
    _out << "; fixed-precision formatter, not a general dtoa; assumes the\n";
    _out << "; magnitude fits in an int64 (matches the rest of this "
            "int64-only\n";
    _out << "; backend) and does not special-case NaN/Infinity.\n";
    _out << "print_float:\n";
    _out << "    push rbp\n";
    _out << "    mov rbp, rsp\n";
    _out << "    sub rsp, 96\n";
    _out << "    lea r12, [rbp - 96]\n";
    _out << "    mov r15, r12\n";
    _out << "    mov rax, rdi\n";
    _out << "    movq xmm0, rax\n";
    _out << "    xorpd xmm1, xmm1\n";
    _out << "    xor r8, r8\n";
    _out << "    comisd xmm0, xmm1\n";
    _out << "    jae .pf_pos\n";
    _out << "    mov r8, 1\n";
    _out << "    btc rax, 63\n";
    _out << "    movq xmm0, rax\n";
    _out << ".pf_pos:\n";
    _out << "    cvttsd2si r10, xmm0\n";
    _out << "    cvtsi2sd xmm2, r10\n";
    _out << "    subsd xmm0, xmm2\n";
    _out << "    mov rax, 1000000\n";
    _out << "    cvtsi2sd xmm3, rax\n";
    _out << "    mulsd xmm0, xmm3\n";
    _out << "    mov rax, 0x3FE0000000000000\n";
    _out << "    movq xmm4, rax\n";
    _out << "    addsd xmm0, xmm4\n";
    _out << "    cvttsd2si r13, xmm0\n";
    _out << "    cmp r13, 1000000\n";
    _out << "    jl .pf_no_carry\n";
    _out << "    sub r13, 1000000\n";
    _out << "    inc r10\n";
    _out << ".pf_no_carry:\n";
    _out << "    cmp r8, 0\n";
    _out << "    je .pf_write_int\n";
    _out << "    mov byte [r12], '-'\n";
    _out << "    inc r12\n";
    _out << ".pf_write_int:\n";
    _out << "    lea r9, [rbp - 32]\n";
    _out << "    mov rax, r10\n";
    _out << "    mov rbx, 10\n";
    _out << "    xor rcx, rcx\n";
    _out << ".pf_int_digit_loop:\n";
    _out << "    xor rdx, rdx\n";
    _out << "    div rbx\n";
    _out << "    add dl, '0'\n";
    _out << "    dec r9\n";
    _out << "    mov [r9], dl\n";
    _out << "    inc rcx\n";
    _out << "    test rax, rax\n";
    _out << "    jnz .pf_int_digit_loop\n";
    _out << ".pf_copy_int:\n";
    _out << "    test rcx, rcx\n";
    _out << "    jz .pf_copy_int_done\n";
    _out << "    mov al, [r9]\n";
    _out << "    mov [r12], al\n";
    _out << "    inc r9\n";
    _out << "    inc r12\n";
    _out << "    dec rcx\n";
    _out << "    jmp .pf_copy_int\n";
    _out << ".pf_copy_int_done:\n";
    _out << "    mov byte [r12], '.'\n";
    _out << "    inc r12\n";
    for (const char *divisor : {"100000", "10000", "1000", "100", "10"}) {
        _out << "    mov rax, r13\n";
        _out << "    xor rdx, rdx\n";
        _out << "    mov rbx, " << divisor << "\n";
        _out << "    div rbx\n";
        _out << "    add al, '0'\n";
        _out << "    mov [r12], al\n";
        _out << "    inc r12\n";
        _out << "    mov r13, rdx\n";
    }
    _out << "    mov rax, r13\n";
    _out << "    add al, '0'\n";
    _out << "    mov [r12], al\n";
    _out << "    inc r12\n";
    _out << "    mov byte [r12], 10\n";
    _out << "    inc r12\n";
    _out << "    mov rdx, r12\n";
    _out << "    sub rdx, r15\n";
    _out << "    mov rax, 1\n";
    _out << "    mov rdi, 1\n";
    _out << "    mov rsi, r15\n";
    _out << "    syscall\n";
    _out << "    mov rsp, rbp\n";
    _out << "    pop rbp\n";
    _out << "    ret\n\n";
}

void CodeGenerator::emitStrConcatRoutine() {
    _out
        << "; str_concat: concatenates two length-prefixed strings (rdi = A,\n";
    _out << "; rsi = B, each pointer -> 8-byte length then raw bytes) into a\n";
    _out << "; fresh region of the bump-allocated str_heap and returns a\n";
    _out << "; pointer to the new length-prefixed string in rax. The heap is\n";
    _out << "; never freed - fine for short-lived programs, not for anything\n";
    _out << "; long-running or concatenation-heavy.\n";
    _out << "str_concat:\n";
    _out << "    push rbp\n";
    _out << "    mov rbp, rsp\n";
    _out << "    push rbx\n";
    _out << "    push r12\n";
    _out << "    push r13\n";
    _out << "    push r14\n";
    _out << "    mov r12, rdi\n";
    _out << "    mov r13, rsi\n";
    _out << "    mov r8, [r12]\n";
    _out << "    mov r9, [r13]\n";
    _out << "    lea r10, [r8 + r9]\n";
    _out << "    mov rax, [str_heap_offset]\n";
    _out << "    lea r14, [str_heap + rax]\n";
    _out << "    mov [r14], r10\n";
    _out << "    lea rbx, [r10 + 8 + 7]\n";
    _out << "    and rbx, -8\n";
    _out << "    add rax, rbx\n";
    _out << "    mov [str_heap_offset], rax\n";
    _out << "    lea rsi, [r12 + 8]\n";
    _out << "    lea rdi, [r14 + 8]\n";
    _out << "    mov rcx, r8\n";
    _out << "    rep movsb\n";
    _out << "    lea rsi, [r13 + 8]\n";
    _out << "    lea rdi, [r14 + r8 + 8]\n";
    _out << "    mov rcx, r9\n";
    _out << "    rep movsb\n";
    _out << "    mov rax, r14\n";
    _out << "    pop r14\n";
    _out << "    pop r13\n";
    _out << "    pop r12\n";
    _out << "    pop rbx\n";
    _out << "    mov rsp, rbp\n";
    _out << "    pop rbp\n";
    _out << "    ret\n\n";
}

void CodeGenerator::emitCharToStrRoutine() {
    _out << "; char_to_str: allocates a fresh 1-byte length-prefixed string\n";
    _out << "; on str_heap holding the single byte in the low 8 bits of rdi,\n";
    _out << "; and returns a pointer to it in rax. Does not touch rbx or\n";
    _out << "; r12-r15, so a caller may hold a pending value in one of those\n";
    _out << "; across this call (see genBinExpr's string-concat handling).\n";
    _out << "char_to_str:\n";
    _out << "    mov rax, [str_heap_offset]\n";
    _out << "    lea rdx, [str_heap + rax]\n";
    _out << "    mov qword [rdx], 1\n";
    _out << "    mov [rdx + 8], dil\n";
    _out << "    add rax, 16\n";
    _out << "    mov [str_heap_offset], rax\n";
    _out << "    mov rax, rdx\n";
    _out << "    ret\n\n";
}

void CodeGenerator::emitIntToStrRoutine() {
    _out << "; int_to_str: allocates a fresh length-prefixed decimal string\n";
    _out << "; on str_heap representing the signed 64-bit value in rdi, and\n";
    _out << "; returns a pointer to it in rax. Preserves rbx (uses r12/r13\n";
    _out << "; internally but saves/restores them), so a caller may hold a\n";
    _out << "; pending value in rbx across this call.\n";
    _out << "int_to_str:\n";
    _out << "    push rbp\n";
    _out << "    mov rbp, rsp\n";
    _out << "    sub rsp, 32\n";
    _out << "    push r12\n";
    _out << "    push r13\n";
    _out << "    mov rax, rdi\n";
    _out << "    xor r8, r8\n";
    _out << "    cmp rax, 0\n";
    _out << "    jge .its_conv\n";
    _out << "    mov r8, 1\n";
    _out << "    neg rax\n";
    _out << ".its_conv:\n";
    _out << "    mov r9, 10\n";
    _out << "    xor r12, r12\n";
    _out << "    lea r13, [rbp - 1]\n";
    _out << ".its_digit_loop:\n";
    _out << "    xor rdx, rdx\n";
    _out << "    div r9\n";
    _out << "    add dl, '0'\n";
    _out << "    dec r13\n";
    _out << "    mov [r13], dl\n";
    _out << "    inc r12\n";
    _out << "    test rax, rax\n";
    _out << "    jnz .its_digit_loop\n";
    _out << "    cmp r8, 0\n";
    _out << "    je .its_no_sign\n";
    _out << "    dec r13\n";
    _out << "    mov byte [r13], '-'\n";
    _out << "    inc r12\n";
    _out << ".its_no_sign:\n";
    _out << "    mov rax, [str_heap_offset]\n";
    _out << "    lea r10, [str_heap + rax]\n";
    _out << "    mov [r10], r12\n";
    _out << "    lea rdi, [r10 + 8]\n";
    _out << "    mov rsi, r13\n";
    _out << "    mov rcx, r12\n";
    _out << "    rep movsb\n";
    _out << "    lea rax, [r12 + 8 + 7]\n";
    _out << "    and rax, -8\n";
    _out << "    add rax, [str_heap_offset]\n";
    _out << "    mov [str_heap_offset], rax\n";
    _out << "    mov rax, r10\n";
    _out << "    pop r13\n";
    _out << "    pop r12\n";
    _out << "    mov rsp, rbp\n";
    _out << "    pop rbp\n";
    _out << "    ret\n\n";
}
}  // namespace mr
