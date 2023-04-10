## WARNING: DO NOT TRY TOO HARD TO BUILD ANY OF THE ORIGINAL EXECUTABLES! ##

Please remember that any little difference, no matter how small it is,
can lead to a vastly different EXE layout. This includes differences in:

- The development tools (or parts of such); For instance, a compiler, a linker,
an assembler, or even a support library or header. A version number is not
a promise for having the exact tool used to reproduce some executable.
- The order in which source code files are listed
in a given project or makefile.
- Project settings.
- Source file specific settings in such a project.
- The order in which object or library files are passed to a linker.
- Any modification to a source code file (especially a header file).
- More than any of the above.

Following the warning, a description of the ways in which the executables were
reproduced is given.

With the right tools, this codebase can be used
for reproducing the executables coming from the following
original releases of Doom, with a few caveats:

- Doom II prototype v1.666.
- Doom Shareware / Doom II early v1.666.
- Doom v1.666.
- Doom II v1.7.
- Doom II v1.7a.
- Doom II French v1.8.
- Doom / Doom II v1.8.
- Doom / Doom II v1.9.
- Doom v1.9 Special Edition prototype exe.
- The Ultimate Doom.
- Final Doom.
- Final Doom, later id Anthology revision.
- Chex Quest.

The MAKEFILE bundled with the Heretic sources was used as a base.
You shall *not* call "wmake" as-is. Instead, use DOBUILD.BAT.

List of releases by directory names
-----------------------------------

- DM1666P: Doom II prototype v1.666.
- DM1666E: Doom Shareware / Doom II early v1.666.
- DM1666: Doom v1.666.
- DM17: Doom II v1.7.
- DM17A: Doom II v1.7a.
- DM18FR: Doom II French v1.8.
- DM18: Doom / Doom II v1.8.
- DM19: Doom / Doom II v1.9.
- DM19UP: Doom v1.9 Special Edition prototype exe.
- DM19U: The Ultimate Doom.
- DM19F: Final Doom.
- DM19F2: Final Doom, later id Anthology revision.
- CHEX: Chex Quest.

Technical details of the original EXEs (rather than any recreated one)
----------------------------------------------------------------------

|     Version     | N. bytes |               MD5                |  CRC-32  |
|-----------------|----------|----------------------------------|----------|
| Prototype 1.666 |  686921  | b9c24975cb0c064a7c4f39c1d0b999ae | 3c2562f3 |
| Doom II 1.666   |  687001  | 22e3fc7854f030df977d26faf1e342be | 5f7ede6f |
| Doom 1.666      |  687001  | be2d05789beb3b56692a2ccd83f8c510 | 084677c4 |
| 1.7             |  686993  | ef9a1f197d56a174097a2af3ccdd7403 | 9116ce4c |
| 1.7a            |  686997  | 3de58b8f768e990856764df982ad5043 | e4eadf7e |
| French 1.8      |  687781  | 03edb841bb9ca99bd0fd5e197cf43e71 | 7c6fba71 |
| 1.8             |  709865  | b2d429377b0912475f1770d0e03f632d | 0563536b |
| 1.9             |  709905  | e2382b7dc47ae2433d26b6e6bc312999 | be91c727 |
| Doom SE proto.  |  710661  | fba2b8d81b1e5e6c5cd0bd886443f9ac | 4c18c327 |
| Ultimate Doom   |  715493  | 742a5f9953871687341e3794468894d8 | b5ceb8ac |
| Final Doom      |  722629  | f006de4fd282ba61d7d0af41a993f9ba | 12ee2811 |
| Final Doom alt. |  722629  | ccd2769f32b38d74d361085e905d8f9c | 503d9154 |
| Chex Quest      |  713017  | d58fc123f7ed425bcc060afb7269bc9c | dd3e1573 |

How to identify code changes (and what's this DOOMREV thing)?
-------------------------------------------------------------

Check out GAMEVER.H. Basically, for each EXE being built, the
macro APPVER_EXEDEF should be defined accordingly. For instance,
when building DM19, APPVER_EXEDEF is defined to be DM19.

Note that only C sources (and not ASM) are covered by the above, although
I_IBM_A.ASM and PLANAR.ASM are identical across all versions.

Other than GAMEVER.H, the APPVER_EXEDEF macro is not used *anywhere*.
Instead, other macros are used, mostly APPVER_DOOMREV.

Any new macro may also be introduced if useful.

APPVER_DOOMREV is defined in all builds, with different values. It is
intended to represent a revision of development of the Doom codebase.
Usually, this revision value is based on some evidenced date, or alternatively
a *guessed* date (say, an original modification date of the EXE).
Any other case is also a possibility.

These are two good reasons for using DOOMREV as described above, referring
to similar work done for Wolfenstein 3D EXEs (built with Borland C++):

- WL1AP12 and WL6AP11 share the same code revision. However, WL1AP11
is of an earlier revision. Thus, the usage of WOLFREV can be
less confusing.
- WOLFREV is a good way to describe the early March 1992 build. While
it's commonly called just "alpha" or "beta", GAMEVER_WOLFREV
gives us a more explicit description.

Is looking for "#if (APPVER_DOOMREV <= AV_DR_...)" (or >) sufficient?
---------------------------------------------------------------------

Nope!

Examples from Wolf3D/SOD:

For a project with GAMEVER_WOLFREV == GV_WR_SDMFG10,
the condition GAMEVER_WOLFREV <= GV_WR_SDMFG10 holds.
However, so does GAMEVER_WOLFREV <= GV_WR_WJ6IM14,
and also GAMEVER_WOLFREV > GV_WR_WL1AP10.
Furthermore, PML_StartupXMS (ID_PM.C) has two mentions of a bug fix
dated 10/8/92, for which the GAMEVER_WOLFREV test was chosen
appropriately. The exact range of WOLFREV values from this test
is not based on any specific build/release of an EXE.

What is this based on
---------------------

This codebase was originally based on the open-source release of LinuxDoom
and Heretic sources. Some portions of code were used from sources of
console Doom ports.

Special thanks go to John Romero, Simon Howard, Mike Swanson, Frank Sapone,
Evan Ramos and anybody else who deserves getting thanks.

What is *not* included
----------------------

As with id's original open source release, you won't find any of the files
from the DMX sound library. They're still required for making the EXEs in
such a way that their layouts will be as close to the originals' as possible.

Alternatively, to make a functioning EXE consisting of GPL-compatible sound
code for the purpose of having a test playthrough, you can use a replacement.
One which may currently be used is the Apogee Sound System backed DMX wrapper.
As expected, it'll sound different, especially the music.

Building any of the EXEs
========================

Required tools:

- For all game versions: Watcom C 9.5b and no other version in the 9.5 series.
- For all game versions: Turbo Assembler 3.1.

Additionally:

- For Doom (English) v1.8 and later revisions, dmx37 should be used
for mostly proper EXE layouts (albeit still with a few exceptions).
- Earlier revisions, the most recent one being Doom II French v1.8,
use variations of dmx34a with various fixes to FM driver.
- Alternatively, if you just want to get an EXE with entirely GPL-compatible
code for checking the game (e.g., watching the demos), you can use the
Apogee Sound System backed DMX wrapper. In such a case,
the Apogee Sound System v1.09 will also be used.

Notes before trying to build anything:

- As previously mentioned, the EXEs will surely have great
differences in the layouts without access to the original DMX files.
- Even with access to the correct versions of DMX, Watcom C and
Turbo Assembler, this may depend on luck. In fact, the compiler
may generate the contents of a function body a bit differently if
an additional environment variable is defined, or alternatively,
if a macro definition is added.
- Even if there are no such issues, versions of Watcom C like 9.5b
are known to insert data between C string literals, which appears to depend
on the environment, and/or the contents of the processed source codes.
- There will also be minor differences stemming from the use of the __LINE__
macro within I_Error.

Building any of the Doom EXEs
=============================

1. Use DOBUILD.BAT, selecting the output directory name (say DM19),
depending on the EXE that you want to build. In case you want to use
the DMX wrapper, enter "DOBUILD.BAT USE_APODMX".
2. Hopefully you should get an EXE essentially behaving like the original,
with a possible exception for the DMX sound code.

Expected differences from the original (even if matching DMX files are used):

- A few unused gaps, mostly between C string literals, seem to be filled
with values depending on the environment while running the Watcom C compiler
(e.g., the exact contents of each compilation unit). This seems to be related
to Watcom C 10.0b and earlier versions, and less to 10.5, 10.6 or 11.0.
- The created STRPDOOM.EXE file will require an external DOS4GW EXE
(or compatible). You may optionally use DOS/4GW Professional to bind
its loader to the EXE, but inspection of the EXE layout wasn't done.
- As stated above, there are a few mentions of the __LINE__ macro via I_Error,
which will translate to numbers differing from the originals in a portion of
the cases. Additionally, for prototype Doom II v1.666, a subset of the
global variables in the BSS will be ordered in a different manner.
- While this will probably not occur, in case of having a bit less luck,
global variables might get internally ordered in a bit different manner,
compared to the original EXE.
- Furthermore, again as stated earlier, the compiler might generate
a bit different code for specific function bodies.
