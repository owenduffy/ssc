
call vars.bat

set CHIP=%1

if #%1==#ESP8266 goto ESP8266
if #%1==#ESP32 goto ESP32
goto end

:ESP8266
echo Processing ESP8266
rem generic (FS:64KB OTA:~470KB)
rem eagle.flash.1m64.ld
set FSSIZE=0x10000
set FSLOC=0x0eb000
rem set FSBKSIZE=4096
set FSBKSIZE=0x1000
goto makeit

:ESP32
rem check these
echo Processing ESP32
set FSSIZE=0x30000
set FSLOC=0x3d0000
set FSBKSIZE=8192
goto makeit

:makeit
mklittlefs -d 2 -c data -b %FSBKSIZE% -p 256 -s %FSSIZE% littlefs.%1.bin

rem exit /b
rem goto end

"%ESPTOOL%" -c %CHIP% -p %PORT% -b %SPEED% write_flash %FSLOC% littlefs.%1.bin

:end

exit /b
