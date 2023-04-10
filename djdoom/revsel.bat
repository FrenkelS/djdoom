@echo off
if "%MSG%" == "" goto error
set CHOICE=
cls
echo [1] Doom II prototype v1.666
echo [2] Doom Shareware / Doom II early v1.666
echo [3] Doom v1.666
echo [4] Doom II v1.7
echo [5] Doom II v1.7a
echo [6] Doom II French v1.8
echo [7] Doom / Doom II v1.8
echo [8] Doom / Doom II v1.9
echo [9] Doom v1.9 Special Edition prototype exe
echo [A] The Ultimate Doom
echo [B] Final Doom
echo [C] Final Doom, later id Anthology revision
echo [D] Chex Quest
echo.
echo [0] Cancel and quit
echo.
echo * Watcom 9.5b should be used for a more accurate code generation,
echo along with TASM 3.1.
echo.
echo %MSG%
set MSG=
choice /C:123456789ABCD0 /N
echo.

if ERRORLEVEL 14 goto end
if ERRORLEVEL 13 goto CHEX
if ERRORLEVEL 12 goto DM19F2
if ERRORLEVEL 11 goto DM19F
if ERRORLEVEL 10 goto DM19U
if ERRORLEVEL 9 goto DM19UP
if ERRORLEVEL 8 goto DM19
if ERRORLEVEL 7 goto DM18
if ERRORLEVEL 6 goto DM18FR
if ERRORLEVEL 5 goto DM17A
if ERRORLEVEL 4 goto DM17
if ERRORLEVEL 3 goto DM1666
if ERRORLEVEL 2 goto DM1666E
if ERRORLEVEL 1 goto DM1666P

:DM1666P
set CHOICE=DM1666P
goto end
:DM1666E
set CHOICE=DM1666E
goto end
:DM1666
set CHOICE=DM1666
goto end
:DM17
set CHOICE=DM17
goto end
:DM17A
set CHOICE=DM17A
goto end
:DM18FR
set CHOICE=DM18FR
goto end
:DM18
set CHOICE=DM18
goto end
:DM19
set CHOICE=DM19
goto end
:DM19UP
set CHOICE=DM19UP
goto end
:DM19U
set CHOICE=DM19U
goto end
:DM19F
set CHOICE=DM19F
goto end
:DM19F2
set CHOICE=DM19F2
goto end
:CHEX
set CHOICE=CHEX
goto end

:error
echo This script shouldn't be run independently

:end
