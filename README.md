# CHIP-8 Emulator

A CHIP-8 emulator written in C/C++ that loads and runs classic CHIP-8 ROMs such as Tetris. [page:1]

## Overview

This project is a CHIP-8 emulator built from scratch as a learning project inspired by Queso Fuego’s YouTube series. It focuses on understanding how low-level systems work, how emulators map instructions to behavior, and how to build software that is testable and easy to debug. [page:1]

## Repository Contents

The repository includes:
- Source code files such as `chip8.cpp`, `Chip8_NoLerpModel.cpp`, and `chip8_queso.cpp`. [page:1]
- A `Makefile` for compilation. [page:1]
- ROM folders including `chip8-roms-master` and `chip8-test-suite-main`. [page:1]
- Test ROMs such as `BC_test.ch8`, `IBM Logo.ch8`, and `test_opcode.ch8`. [page:1]

## Build

From the project root, compile the emulator with:

```bash
make
```

This uses the included `Makefile` in the repository root. [page:1]

## Run a Game

To run a CHIP-8 game, pass the ROM path as a command-line argument.

Example: run Tetris

```bash
./chip8 "./chip8-roms-master/games/Tetris [Fran Dachille, 1991].ch8"
```

This launches the emulator and loads the Tetris ROM from the `chip8-roms-master/games` folder. [page:1]

## Other ROMs

You can also try other ROMs in the repository, for example:

```bash
./chip8 "./IBM Logo.ch8"
./chip8 "./BC_test.ch8"
./chip8 "./test_opcode.ch8"
```

The repository also includes additional ROM collections inside `chip8-roms-master` and test files in `chip8-test-suite-main`. [page:1]

## What I Learned

This project taught me several practical software engineering lessons:

- How computer hardware concepts work from the ground up by implementing a virtual machine.
- How to structure software in a way that makes debugging easier.
- How to use a debugger to trace execution and understand program behavior.
- How to use makefiles to simplify compilation and development workflows.
- How to think about user interface details so a program feels nicer, more polished, and more playable.

A big lesson from this project was that small details matter. Clear structure, easier builds, readable debugging flow, and a better-looking interface all add up to a much stronger user experience.

## Demo

I will also attach a short demo video on the GitHub repository showing the emulator running Tetris.

## Credits

This project was created as a learning project inspired by Queso Fuego’s CHIP-8 emulator videos. [page:1]
