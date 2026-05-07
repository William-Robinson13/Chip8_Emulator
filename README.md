# CHIP-8 Emulator

A CHIP-8 emulator written in C/C++ that runs classic games such as Tetris.

## Overview

This project is a from-scratch CHIP-8 emulator built as a learning project inspired by Queso Fuego’s YouTube series. The goal was to understand how low-level systems work, how instructions are interpreted and executed, and how to build software that is structured clearly and easy to debug.

## Repository Contents

This repository includes:
- `chip8.cpp`, `Chip8_NoLerpModel.cpp`, and `chip8_queso.cpp` for emulator-related source code.
- A `Makefile` for easier compilation.
- `chip8-roms-master/` containing CHIP-8 game ROMs.
- `chip8-test-suite-main/` containing test ROMs.
- Additional ROM files such as `BC_test.ch8`, `IBM Logo.ch8`, and `test_opcode.ch8`.

## Build

From the project root, compile the emulator with:

```bash
make
```

This uses the `Makefile` included in the repository root.

## Run a Game

To run a game, pass the ROM path as a command-line argument.

### Example: Tetris

```bash
./chip8 "./chip8-roms-master/games/Tetris [Fran Dachille, 1991].ch8"
```

This launches the emulator and runs the Tetris ROM.

## Other Example ROMs

You can also run other ROMs from the repository, for example:

```bash
./chip8 "./IBM Logo.ch8"
./chip8 "./BC_test.ch8"
./chip8 "./test_opcode.ch8"
```

## Controls

This emulator uses a typical CHIP-8 keyboard mapping:

```text
CHIP-8 keypad       Keyboard
1 2 3 C             1 2 3 4
4 5 6 D             Q W E R
7 8 9 E             A S D F
A 0 B F             Z X C V
```

## What I Learned

This project taught me a lot about both low-level computing and practical software development.

- How hardware works from the absolute basics by building a simple virtual machine.
- How to design software in a way that is easier to debug and reason about.
- How to use a debugger to step through execution and find problems more systematically.
- How to use makefiles to make compilation and iteration much easier.
- How to think about user interface details so a program feels nicer, more polished, and more playable.

One of the main lessons I took from this project is that small details matter. Clean structure, easier builds, better debugging flow, and a more polished interface all add up to a much better final result.

## Demo

A short demo of the emulator running Tetris and Space Invader:

[Watch the Space Invader demo](./videos/Space_Invader.mp4)
[Watch the Tetris demo] (./videos/Tetris.mp4) 

## Credits

This project was built as a learning exercise inspired by Queso Fuego’s CHIP-8 emulator videos.
