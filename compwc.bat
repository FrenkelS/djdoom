if "%WATCOM%" == "" goto error

mkdir WCDM19
wmake -f makefile.wc WCDM19\wcdoom.exe
del *.err
goto end

:error
@echo Set the environment variables before running this script!

:end
