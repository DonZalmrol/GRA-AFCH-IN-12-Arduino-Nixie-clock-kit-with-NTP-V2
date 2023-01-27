# GRA-AFCH-IN-12-Arduino-Nixie-clock-kit-with-NTP-V2
GRA &amp; AFCH : IN-12 Arduino : Nixie clock kit with NTP V2

While the build in GPS works well, it can be an issue within well insulated home to receive GPS/ DCF signals from the outside. Hence the wish to add NTP with the needed hardware and code to get a working NTP sync that uses DST (Daylight Savings Time).

Used Hardware:
* GRA-AFCH; IN-12 Arduino Shield+Adapter+Raspberry Pi shield Nixie Tubes Clock https://gra-afch.com/catalog/nixie-tubes-clock-without-cases/raspberry-pi-shield-nixie-tubes-clock-ncs312-for-in-12-nixie-tubes-options-tubes-gps-remote-arduino-columns/
* Arduino Mega2560 + Wifi R3 (ESP8266) https://www.aliexpress.com/item/32950536539.html?spm=a2g0s.9042311.0.0.29054c4d1L9QCd

First things first:
* Flash the ESP8266 to the latest firmware (V3.0.4 in my case), I've used option 2 from page 5 (post by Attila_r): https://forum.arduino.cc/index.php?topic=495840.60
* Added the ESP8266 library in Arduino
* Install the needed libraries

The arduino_secrets.h file allows you to set:
1. Your WiFi SSID network
2. Your WiFi SSID password
3. Your desired NTP server (e.g. router or NTP server)
4. The DST timezone (DST active period)
5. The standard timezone (DST off period)

The code will then update your clock's time with the fetch NTP (unix epoch) and display it accordingly.
It will check if your RTC is in sync with the fetched time and update it if not.

Besides NTP it also includes a basic webserver with information regarding your clock's time (NTP, local, RTC) etc...
