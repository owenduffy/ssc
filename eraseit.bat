
call vars.bat
set CHIP=%1

if #%1==#ESP8266 goto ESP8266
if #%1==#ESP32 goto ESP32
rem goto end

:ESP8266
:ESP32

"%ESPTOOL%" -c %CHIP% -p %PORT% -b %SPEED% erase_flash

:end
exit /b
