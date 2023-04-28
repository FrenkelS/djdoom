# DJGPP Doom
Download the executables [here](https://github.com/FrenkelS/djdoom/releases).

Which compiler produces the fastest Doom code?
[DJGPP](https://github.com/andrewwutw/build-djgpp),
[Digital Mars](https://digitalmars.com/) (with [X32](https://github.com/Olde-Skuul/KitchenSink/tree/master/sdks/dos/x32)) or
[Watcom](https://github.com/open-watcom/open-watcom-v2)?

The code is based on [gamesrc-ver-recreation](https://bitbucket.org/gamesrc-ver-recreation/doom/src/master/)
and it took inspiration from
[FastDoom](https://github.com/viti95/FastDoom),
[doomgeneric](https://github.com/ozkl/doomgeneric) and
[Doom Vanille](https://github.com/AXDOOMER/doom-vanille).

All the assembly code is replaced by C code.
There's no sound, no joystick and no Logitech Cyberman support.
And in case of Digital Mars, no keyboard.

Use `compdj.bat` to compile with DJGPP, `compdm.bat` for Digital Mars and `compwc.bat` for Watcom.
