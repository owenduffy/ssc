# SNTP synchronised clock (ssc)    
Repository for code related to project described at See https://owenduffy.net/blog/?p=21631

The SNTP synchronised clock (ssc) is an ESP8266 based time of day clock with an LED display..

Design criteria
The design criteria are:

- small, portable, powered from a 5V USB power supply;
- synchronised to a SNPT (simple network time protocol) server;
- flexibility for a range of displays (74HC595 static, TM1637, HT16K33);
- configurable time zone offset and daylight saving offset;
- configurable from a web page;
- switchable HH:MM and MM:SS display formats; and
- switchable daylight saving mode.
- The concept is that it is synchronised to net time, and the only adjustment needed through the year is to flick the daylight saving mode.

Copyright: Owen Duffy 2021/04/14.


