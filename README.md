# Smart Watering Hardware
I`m using a Particle Photon as the heart of this smart watering system project.

![Alt text](https://raw.githubusercontent.com/MyOrchard/SmartWatering-Hardware/master/images/the-heart.png "The heart of the system")

A list of some of the hardware i using in this project:

- [Particle Photon](https://www.particle.io/)
- [I2C soil moisture sensor] (https://www.tindie.com/products/miceuz/i2c-soil-moisture-sensor/)
- [Water pump] (https://www.conrad.se/L%e5gsp%e4nningspump-dr%e4nkbar-Barwig-0333-720-l%2fh-6-m.htm)
- [Compressed-air hose] (https://www.conrad.se/Tryckluftsslang-Norgren-PE0010025C-Polyeten-Natural-Ytter-%d8:-10-mm-Inre-diameter:-7-mm-11-bar.htm)
- [Box 120x80x55 (millimeter)] (http://www.electrokit.com/apparatlada-hammond-1591tsbk-120x80x55-svart.43171)
- [Relay board 5v/logic level operation 1 channel] (https://www.m.nu/relay-board-5vlogic-level-operation-channel-assembled-p-1653.html)
- Two 10k ohm resistor

Here is some sketch over the hardware connections i did, its only shows one I2C soil moisture sensor but i using four parallel connected with flat cables and using only two 10k ohm resistor for the four sensors.

![Alt text](https://raw.githubusercontent.com/MyOrchard/SmartWatering-Hardware/master/images/hardware-connections.png "The connections between the hardware")

The project is based on this repository and the code version 1.0.0: https://github.com/yerpj/SmartWatering

But with some modifications and added some functions to handle more then one i2c sensors.

![Alt text](https://raw.githubusercontent.com/MyOrchard/SmartWatering-Hardware/master/images/photon-with-i2c-sensor "Particle Photon with one I2C soil moisture sensor")

![Alt text](https://raw.githubusercontent.com/MyOrchard/SmartWatering-Hardware/master/images/i2csensor "I2C soil moisture sensors")