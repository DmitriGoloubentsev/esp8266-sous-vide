# esp8266-sous-vide
Sous vide cooking using cheap esp8266 and Arduino for under 20$. In this project I wanted to play with esp8266 functionality. I'm pleased to know that it can connect to wifi access point and be access point at the same time. Can host primitive web server and save data into flash memory.

Hardware requirements

1) Isolated DS1820 temperature sensor or compatible
2) esp8266 board. I've used NodeMCU for this project.
3) Relay to switch heater appliance. I modified power bar and made one socket controllable by esp8266 using relay module.
4) Heater appliance. Can be a multi-cooker, rice cooker, electric grill or even a kettle!

Configuration

0) Connect to sous-vide cooker using wifi.
1) Set target temperature in Celsius.
2) With Sous-vide you want VERY slowly yo heat up your dish to desired temperature. Since most heater appliances are way more powerful then what is required we want to switch heater on for short period (Set time for ON state) and turn it off and wait for temperature to rise. Minimum time to wait is configured and you may want to adjust it for your appliance.

Cooking

"Time at target" tells you time we spent at desired temperature. Check your recipe!
