# ablc
x64 assembly compiler for a simple C-like language written in C. See grammar.txt and testdata for syntax details. The language
supports:
- int and void functions types (statically checked)
- recursive functions
- int variables (statically checked)
- while loops and if statements with boolean expressions
- expression printing

## Example program
Note that `main` must be present and have the signature `void main()`

Functions must be defined before they are called, i.e. in the example below `fib` must be above `main`.
```c++
int fib (int a) {
    if (a <= 2) {
        return 1;
    }
    return fib(a - 1) + fib(a - 2);
}

void main() {
    print(fib(10));
}
```

<details>

<summary>Compiler output</summary>

```asm
.data
format_string: .asciz "%ld\n"

.text
.global main

fib:
fib_prelude:
    pushq %rbp
    movq %rsp, %rbp
    subq $32, %rsp
    pushq %r12
    pushq %r13
    pushq %r14
fib_init:
    movq %rdi, %r12
fib_lab_1:
    movq %r12, %rax
    cmpq $2, %rax
    setle %al
    movzbq %al, %rax
    movq %rax, %rsi
    cmpq $1, %rsi
    je fib_lab_3
    jmp fib_lab_4
fib_lab_3:
    movq $1, %rax
    jmp fib_epilogue
fib_lab_4:
    jmp fib_lab_2
fib_lab_2:
    movq %r12, %rax
    subq $1, %rax
    movq %rax, %r13
    pushq %rbp
    movq %r13, %rdi
    callq fib
    addq $8, %rsp
    movq %rax, %r14
    movq %r12, %rax
    subq $2, %rax
    movq %rax, -8(%rbp)
    pushq %rbp
    movq -8(%rbp), %rdi
    callq fib
    addq $8, %rsp
    movq %rax, -16(%rbp)
    movq %r14, %rax
    addq -16(%rbp), %rax
    movq %rax, -24(%rbp)
    movq -24(%rbp), %rax
    jmp fib_epilogue
fib_epilogue:
    popq %r14
    popq %r13
    popq %r12
    addq $32, %rsp
    popq %rbp
    retq

main:
main_prelude:
    pushq %rbp
    movq %rsp, %rbp
    subq $0, %rsp
    pushq %r12
main_init:
main_lab_1:
    pushq %rbp
    movq $10, %rdi
    callq fib
    addq $8, %rsp
    movq %rax, %r12
    pushq %rbp
    leaq format_string(%rip), %rdi
    movq %r12, %rsi
    movq $0, %rax
    callq printf
    popq %rbp
main_epilogue:
    popq %r12
    addq $0, %rsp
    popq %rbp
    retq
```

</details>


## Building and running

### Building
1. > meson setup buildDir
2. > meson compile -C buildDir

### Running
The following instructions have been tested on Linux:

1.  cd into buildDir
> cd buildDir
2. compile src program to asm
> ./ablc ../testdata/ok_program.al --output t.asm
3. assemble
> as t.asm -o a.out
4. link with libc and generate startup code that calls main
> gcc a.out -o a
5. run 
> ./a

### Usage
> ./ablc <input_file.al> [--print-ast] [--print-ir] [--print-asm] <--skip-output | --output outputfile>

# Todos
- Instruction patching for x64, meaning some generated code might be invalid (as would flag for this).
- Additional data types for typechecking etc, for example an environment/map type would be useful
- Better error messages in parser and especially the typechecker
- Allow function declarations, to allow calling into custom C code
# Possible extensions/enhancements
1. More advanced register allocator
2. IR optimizations, such as removing empty 'jump/goto' blocks
3. Additional types
4. Arrays/pointers and structs
5. Compile files without `main` that can be linked against
