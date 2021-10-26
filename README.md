# NewMotion EV Charger Energy Sniffer
## Introduction
This project is an energy sniffer for NewMotion (Shell Recharge) charging stations. I have a Home Advanced Charger by NewMotion. In my case this charger internally uses an Inepro Metering PRO380-Mod MID-certified meter to measure current and power use. This project listens in on the Modbus interface between the internal MID certified energy meter and the charge controller. It then sends out these values over Wifi to an MQTT broker.

This project was created for my own personal use, use it as you please but note I do not recommend making any changes to your charger. Doing so may void your warranty and damange your charger. This documentation may also be incomplete, so double-check everything.

**Use at your own risk**
## Hardware
The electronics I used for this project are:
- [Adafruit ESP32 Feather](https://www.adafruit.com/product/3405)
- Connector for 12v power [Phoenix Contact 1840366](https://www.phoenixcontact.com/online/portal/us?uri=pxc-oc-itemdetail:pid=1840366&library=usen&tab=1)
- 12v to 5v DC/DC convertor with galvanic isolation
- RS485 interface [like this one](https://www.sparkfun.com/products/10124).
- Small electronics enclosure

## Installation
### Build electronics
- I connected the RS485 interface to the 3.3v and the serial port of the Feather. I did not connect the tx pin because I only want to "listen" to the communication and never want to send anything because that may cause problems with the charger.
- Then I connected the DC/DC convertor output to the USB-pin of the Feather to power both the Feather and the RS485 interface.
- Program the esp with the software in this repository using Arduino programming software.
### Connect to Charger
- **Disconnect power from your charger!** There is a lot of power in these things; power it off before you open it or it may try to kill you.
- My version of the CHarger PCB has a Phoenix Contact connector labeled CON13. I use this as the power input for my DC/DC convertor.
- I connect the RS485 port to terminals 22/23 of the PRO380-Mod meter.
### First use
- When you start the controller for the first time it will create an AP you can connect to. Then you can surf to the setup page and add settings for the Wifi and MQTT broker.
## How does it work?
I did not want to put my electronics "in between" charge controller and power measuring device. So the device passively listens to the messages being sent. Unfortunately the Modbus protocol used unfortunately does not easiliy allow to differentiate between messages sent by the master and those sent by the slave. To solve this I wait for a timeout in the communication, then after that I assume what follows is a new message by the master, followed by the reply from the slave.
This is a really messy, nasty way of doing things, but it works.
### Use with Home Assistant

This is my config for Home Assistant to get the values from the sniffer:
```
sensor:
  - platform: mqtt
    name: "Laadpaal Spanning F1"
    state_topic: "/newmotion/5002"
    unit_of_measurement: "V"
    unique_id: laadpaal_spanning_f1
  - platform: mqtt
    name: "Laadpaal Spanning F2"
    state_topic: "/newmotion/5004"
    unit_of_measurement: "V"
    unique_id: laadpaal_spanning_f2
  - platform: mqtt
    name: "Laadpaal Spanning F3"
    state_topic: "/newmotion/5006"
    unit_of_measurement: "V"
    unique_id: laadpaal_spanning_f3
  - platform: mqtt
    name: "Laadpaal Frequentie"
    state_topic: "/newmotion/5008"
    unit_of_measurement: "Hz"
    unique_id: laadpaal_frequentie
  - platform: mqtt
    name: "Laadpaal Actief Vermogen F1"
    state_topic: "/newmotion/5014"
    unit_of_measurement: "kW"
    unique_id: laadpaal_actief_vermogen_f1
  - platform: mqtt
    name: "Laadpaal Actief Vermogen F2"
    state_topic: "/newmotion/5016"
    unit_of_measurement: "kW"
    unique_id: laadpaal_actief_vermogen_f2
  - platform: mqtt
    name: "Laadpaal Actief Vermogen F3"
    state_topic: "/newmotion/5018"
    unit_of_measurement: "kW"
    unique_id: laadpaal_actief_vermogen_f3
  - platform: mqtt
    name: "Laadpaal Stroom F1"
    state_topic: "/newmotion/500c"
    unit_of_measurement: "A"
    unique_id: laadpaal_stroom_f1
  - platform: mqtt
    name: "Laadpaal Stroom F2"
    state_topic: "/newmotion/500e"
    unit_of_measurement: "A"
    unique_id: laadpaal_stroom_f2
  - platform: mqtt
    name: "Laadpaal Stroom F3"
    state_topic: "/newmotion/5010"
    unit_of_measurement: "A"
    unique_id: laadpaal_stroom_f3
  - platform: mqtt
    name: "Laadpaal Energie"
    state_topic: "/newmotion/6000"
    unit_of_measurement: "kWh"
    unique_id: laadpaal_actieve_energie
