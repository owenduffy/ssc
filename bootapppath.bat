
call vars.bat

rem set BOOTAPPPATH=C:\Users\owen\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.2/tools/partitions/
set BOOTAPPPATH=C:\Users\owen\AppData\Local\Arduino15\packages\esp32\hardware\esp32\

rem generate a unique workfile name
:tryworkfileagain
set /a workfile=%RANDOM%+100000
set workfile=work-%workfile:~-4%.tmp
echo rand is %workfile%
if exist %workfile% goto tryworkfileagain

rem get directory listing
dir /ad /od /b %BOOTAPPPATH%* >%workfile%
rem get the last entry 
for /F %%f in (%workfile%) do set version=%%f
del %workfile%
echo version at %version%
set BOOTAPPPATH=C:\Users\owen\AppData\Local\Arduino15\packages\esp32\hardware\esp32\%version%\tools\partitions\
echo BOOTAPPPATH=%BOOTAPPPATH%
