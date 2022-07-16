call vars.bat

set CHIP=%1

if #%1==#ESP8266 goto ESP8266
if #%1==#ESP32 goto ESP32
goto end

:ESP32
rem goto end
set BIN=.pio\build\esp32dev-HT16K33\firmware.bin

echo Version:
%ESPTOOL% version

if #%2==# goto ESP32write

echo Merge:
%ESPTOOL% -c %CHIP% merge_bin -o esp32-merged.bin 0x1000 %USERPROFILE%\.platformio\packages\framework-arduinoespressif32\tools\sdk\esp32\bin\bootloader_dio_40m.bin 0x8000 .pio\build\esp32dev\partitions.bin 0xe000 %USERPROFILE%\.platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin  0x10000 %BIN%
goto end

:ESP32write

echo Write:
%ESPTOOL% -c %CHIP% -b %SPEED% -p %PORT% write_flash 0x00000 %BIN%
echo Verify:
%ESPTOOL% -c %CHIP% -b %SPEED% -p %PORT% verify_flash 0x00000 %BIN%
goto end

:ESP8266
set BIN=.pio\build\esp12e-HT16K33\firmware.bin

echo Version:
%ESPTOOL% version

echo Write:
%ESPTOOL% -c %CHIP% -b %SPEED% -p %PORT% write_flash 0x00000 %BIN%
echo Verify:
%ESPTOOL% -c %CHIP% -b %SPEED% -p %PORT% verify_flash 0x00000 %BIN%
goto end


rem read wifi 4MB
%ESPTOOL% -p %COM%  read_flash 0x3fe000 128 wifisettings.bin 
rem read wifi 512KB
rem %ESPTOOL% -p %COM%  read_flash 0x7e000 128 wifisettings.bin 



:end
exit /b

