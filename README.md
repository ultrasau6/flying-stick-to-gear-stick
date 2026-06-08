# Joy-to-gear
This project aims to transform any joystick with force feedback into a gear stick for sim racing cheaply.
It is currently developed on Linux, and there is no plan to make a Windows version.

## How it works.
This project uses libevdev to get a device's position and create a new device with 6 buttons; this new device will be updated by the position of the real device.
The device can be selected using a GTK interface.
