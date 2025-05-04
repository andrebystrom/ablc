# ablc
X64 compiler for a simple language written in C. See grammar.txt and testdata for syntax details. The language
supports:
1. int and void functions types (statically checked)
2. int variables (statically checked)
3. while loops and if statements with boolean expressions
4. expression printing

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
# Possible extensions/enhancements
1. More advanced register allocator
2. IR optimizations
3. Additional types
4. Arrays and structs
