
call vars.bat
set CHIP=%1

echo %ESPTOOL% -c %CHIP% -b %SPEED% -p %PORT% flash_id
%ESPTOOL% -c %CHIP% -b %SPEED% -p %PORT% flash_id

exit /b
