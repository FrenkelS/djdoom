@echo off
set MSG=Please select what to build:
call REVSEL.BAT

if "%CHOICE%" == "" goto end

if "%1" == "USE_APODMX" goto apodmx

if not exist ..\dmx goto dmxerror
set USE_APODMX=0
goto task

:dmxerror
echo Can't recreate Doom EXE, you need a compatible version of
echo DMX headers under ..\dmx. You also need a compatible version of
echo the DMX library, again under ..\dmx. See makefile for more details.
echo Alternatively, run "DOBUILD.BAT USE_APODMX" to use APODMX instead.
goto end

:apodmx
if not exist ..\apodmx\apodmx.lib goto apodmxerror
REM AUDIO_WF.LIB is copied as a workaround for too long path
set USE_APODMX=1
goto task

:apodmxerror
echo Can't recreate Doom EXE, you need the APODMX headers
echo and the APODMX.LIB file under ..\apodmx ready, as well
echo as ..\audiolib\origexes\109\AUDIO_WF.LIB.
goto end

:task
REM Since environment variables may actually impact the compiler output,
REM use a helper script in order to try and refrain from them

mkdir %CHOICE%
echo wmake.exe %CHOICE%\newdoom.exe "appver_exedef = %CHOICE%" "use_apodmx = %USE_APODMX%" > BUILDTMP.BAT
echo del BUILDTMP.BAT >> BUILDTMP.BAT
set CHOICE=
set USE_APODMX=
BUILDTMP.BAT

:end
set CHOICE=
