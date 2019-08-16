# MQTT-Vehicle-Presence
Vehicle Presence Detection for OpenHAB

# Purpose
Specifically written to run on a Wemos D1 Mini Pro (although should run on any ESP8266 chip) and interface with OpenHAB home automation software. They can be purchased here for less than US$5 https://www.aliexpress.com/item/32803725174.html It is best to utilise the Pro version due to it supporting an external antenna, which gives more accuracy over time when the vehicle is arriving or leaving your home.

The Wemos will reside in your vehicle, connected and powered by a USB cable. It should only be powered ON when the ignition/car is turned on, and power OFF when the ignition/car is off. Note that most cigarette lighter sockets are powered even with the ignition off, therefore consider wiring in something like this: https://www.aliexpress.com/item/32731673173.html

It will connect to your Wifi network and take (by default 5) samples of RSSI signal strength every 500msec when it first powers up and calculates an average RSSI. A buffer is then subtracted from the RSSI average (NOTE: RSSI is measure in negative dB) as we will assume the closer to your house the vehicle is, the stronger the signal strength, and conversely the further away the vehicle is, the weaker the signal strength. 

It will continue taking the same RSSI samples, average them and then...

- If the signal is weaker than the baseline, it assumes you are leaving (AWAY or "0") 
- If the signal is the same or stronger than the baseline, it assume you are arriving (HOME or "1")

Messages are sent utilising the MQTT protocol to OpenHAB (proxy) items. An OpenHAB rules file uses a combination of the proxy item "homeoraway" state as well as the LWT (last will and testament) "status", to more accurately determine whether the vehicle is home or away. For visual troubleshooting, the built-in Wemos LED light will illuminate when Wifi is connected, and off when disconnected. It will flash twice briefly when MQTT messages are sent.

Note: This is considered a beta release, as it has only been tested a few times with the vehicle coming and going. The external antenna (on the Pro version) is highly recommend in order to extend the range. In my case, when performing initial tests with the Wemos D1 Mini (non-Pro version) it only connected to my own Wireless Access Point about 2-3 seconds before I arrived home, parked in my garage and turned my car off (turning the car off any sooner would not have had a chance to connect and tell OpenHAB it was actually home)  
