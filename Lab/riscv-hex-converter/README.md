# RISC-V Hex Converter

## Overview
The RISC-V Hex Converter is a tool designed to convert RISC-V assembly programs into hexadecimal format. It provides functionalities for initializing memory, loading programs, handling user commands, and printing the loaded program.

## Project Structure
```
riscv-hex-converter
├── Makefile
├── src
│   ├── risc-hex-converter.c
│   └── risc-hex-converter.h
└── README.md
```

## Files

### `src/risc-hex-converter.c`
This file contains the implementation of the RISC-V hex converter. It includes functions for:
- Initializing memory
- Loading a program from a file
- Handling user commands
- Printing the program loaded into memory

### `src/risc-hex-converter.h`
This header file defines:
- Constants for memory layout
- Enums for opcode types
- Structures for CPU state and memory regions

### `Makefile`
The Makefile automates the build process for the project. It compiles the source files and links them to create the executable. To build the project, run the command:
```
make
```

## Usage
1. Compile the project using the Makefile.
2. Run the executable with the input program file as an argument:
   ```
   ./riscv-hex-converter <input_program>
   ```
3. Follow the on-screen prompts to interact with the simulator.


## License
This project is licensed under the MIT License.
