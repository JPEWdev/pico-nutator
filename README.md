# pico-nutator

This repository contains the source code and KiCad Schematic and PCB for a Pico
Pi powered scientific rocker (e.g. a nutator). The machine is designed to mix
small quantities of liquid (i.e. test tubes) by gently rocking them back and
forth or around in circles.

This is for a homemade rocker that I built. While the one I built is just a
rocker and not actually a [nutator](https://en.wikipedia.org/wiki/Nutation),
the software and hardware design could be utilized with different mechanical
designs.

## Basic hardware design

The PCB routes power from a 12V power brick, through a power switch, then out
to a 3 pin connector. This connector goes to a commonly available L298N dual
H-bridge board. This stepper motor driver provide back 5V power, courtesy of an
on board linear regulator.

A Raspberry Pi Pico is the brains of the system. It has 4 motor outputs driven
via PWM to L298N motor controller board to drive the 2 poles of a stepper motor
using the dual H-bridge. These outputs are driven via PWM to ensure that the
motor does not exceed its rated current limit. In addition there is a motor
enable output pin which goes to the L298N to completely disable the H-bridge,
allowing the motor to spin freely. This is important because the stepper motor
will draw current when it is locked in place, wasting power. If the rocker is
not running and has no inputs for 60 seconds, it will go into "sleep mode" and
disable the H-bridge to save energy.

For input, there are 3 buttons; a Start/Stop button to start and stop the
motor, and an up and down button to increase or decrease the target RPM of the
motor. The target RPM can be changed while the motor is stopped, or running.
The software will also apply acceleration to the motor RPM, so that it smoothly
ramps up or down to the target RPM when starting or when the RPM has changed.
When changing speed, the display will show the percentage of the target speed
the motor is currently running at.

For display, the Pico Pi is connected to a Newhaven K3Z family 2x16 LCD.

Finally, a fan output is enabled when the motor is enabled to cool the L298N,
as it can get hot while running. The fan turns off in sleep mode.

The Schematic and PCB were designed in [KiCad](https://www.kicad.org/)

## Software

The software is written in C using the [Pico Pi SDK](https://www.raspberrypi.com/documentation/pico-sdk/).
An initial prototype was written in Python using
[CircuitPython](https://circuitpython.org/), however it had to be re-written in
C since the overhead and jitter in the Python code made it unable to get
precise timing on motor output pins, and thus it did not run smoothly,
especially at high RPM (e.g. above 30)

## Compiling and Installing

To build the software yourself first ensure that you have a suitable cross
compiler installed to compile for the Pico Pi (I used GCC, but presume Clang
would work also). First, the Pico Pi SDK must be fetched as a submodule using:

```shell
git submodule update --init --recursive
```

Next, make a build directory, configure it using `cmake`, and build using
`ninja` (using `ninja` is recommended, as it will result in a faster build of
the SDK):

```shell
mkdir build
cd build
cmake .. -GNinja
ninja
```

To load the software onto the Pico Pi, ensure it is not powered, then hold down
the `BOOTSEL` button while plugging it into the USB port of your computer. The
Pico Pi will show up as a removal USB drive. Copy the `nutator.bin` file to
this USB drive, and the Pico Pi will automatically reboot and start executing
the program.
