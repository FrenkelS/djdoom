# DJGPP Doom
Download the executables [here](https://github.com/FrenkelS/djdoom/releases).

The goal of this project is to:
* Examine which compiler generates the fastest Doom code for DOS:
  - [DJGPP](https://github.com/andrewwutw/build-djgpp)
  - [Digital Mars](https://digitalmars.com) (with [X32](https://github.com/Olde-Skuul/KitchenSink/tree/master/sdks/dos/x32))
  - [CC386](https://ladsoft.tripod.com/cc386_compiler.html)
  - [Watcom](https://github.com/open-watcom/open-watcom-v2)
* Show the modifications required in the code for it to be compilable by the compilers.

The code is based on [gamesrc-ver-recreation](https://bitbucket.org/gamesrc-ver-recreation/doom/src/master)
and it took inspiration from
[doomgeneric](https://github.com/ozkl/doomgeneric),
[Doom Vanille](https://github.com/AXDOOMER/doom-vanille),
[FastDoom](https://github.com/viti95/FastDoom),
[Quake](https://github.com/id-Software/Quake) and
[Quake 2](https://github.com/id-Software/Quake-2).

There is some assembly in the code that requires [NASM](https://www.nasm.us).
To build Doom only using C code, use the macro `C_ONLY`.

There's no sound, no joystick and no Logitech Cyberman support.

## How to add other compilers
The differences between compilers specific to the Doom source code are in `compiler.h`, `d_main.c`, `i_ibm.c` and `planar.asm`.
Search in those files for the pre-defined compiler macros and start hacking.

|Compiler    |Set environment variables|Compile code|Pre-defined compiler macro|
|------------|-------------------------|------------|--------------------------|
|DJGPP       |`setenvdj.bat`           |`compdj.bat`|`__DJGPP__`               |
|Digital Mars|`setenvdm.bat`           |`compdm.bat`|`__DMC__`                 |
|CC386       |`setenvoc.bat`           |`compoc.bat`|`__CCDL__`                |
|Watcom      |`setenvwc.bat`           |`compwc.bat`|`__WATCOMC__`             |
