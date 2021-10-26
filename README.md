# NewMotion EV Charger Energy Sniffer
## Introduction
This project is an energy sniffer for NewMotion (Shell Recharge) charging stations. It listens in on the Modbus interface between the Interlnam MID certified energy meter and the charge controller.
This project was created for my own personal use, use it as you please but note I do not reccomend making any changes to your charger. Doing so may void your warranty and damange your charger.
## Hardware
The electronics I used for this project are:
- esp
- Connector for 12v power [Phoenix Contact 1840366](https://www.phoenixcontact.com/online/portal/us?uri=pxc-oc-itemdetail:pid=1840366&library=usen&tab=1)

I have a Home Advanced CHarger by NewMotion. In my case this charger internally uses an Inepro Metering PRO380-Mod MID-certified meter to measure current and power use. The modbus output is available on terminals 22/23 of this meter. I use this to listen in on the communication between the meter and the charge controller.
