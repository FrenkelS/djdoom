@echo off

mkdir WCDM19
wmake.exe -f makefile.wc WCDM19\wcdoom.exe "appver_exedef = DM19" "use_apodmx = 0"
