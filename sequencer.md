

# An Arduino Sequencer for Amateur Radios
By Glen Overby, kc0iyt
October 20, 2026

## Introduction

Microcontrollers are extremely popular, thanks in part to
platforms like the Arduino which have made developing the software easy.
This paper presents an Arduino project 
used for sequencing transmit / receive relays, amplifiers and transverters
in order to prevent transmitting into a relay when it is switching, and to
protect recevier inputs from transmitters or amplifiers.

My first generation sequencer uses an 8-pin Atmel ATTiny45 chip and has been
in use on my 10GHz station since 2006. I developed software, in C, that is
uses a configuration table to determine, for each output, if it is active high,
active low, pulsed when switching from receive to transmit, or pulsed when
switching from transmit to receive. The regular outputs are used for driving
transistors to provide grounded ("PTT") outputs, or to turn on power.
The pulsed outputs are used for driving latching relays.

This version moves the code over to the Arduino environment, and onto chips
with more output pins.

## Hardware

Today there are a large variety of Arduino boards today, but I'm using
one of the early ones: an Arduino Pro using the Microchip (formerly Atmel)
MegaAVR.

The MegaAVR chips output pins are +5v (or +3.3v) and it isn't healthy for
the chip if the pin is connected to a pull-up to higher voltages.
The best practice is to put a transistor of some type between the
microcontroller and the device being controlled. 

### BJT or Mosfet?

There are a huge number of transistors to pick from, in two classes:
Bipolar Junction Transisors such as the venerable 2N2222A, 2N3904, or 2N3906
and Mosfets.  BJTs require (usually) two resistors for each transistor
while there is a class of Mosfets called Logic Level which can be driven
directly from the +5V (or +3.3V) outputs on microcontrollers.  I wanted
to keep the component count down, while providing enough current capacity
for most projects I'm likely to do so I chose to use mosfets.

I chose components that I had used on other projects: the VN2222A for
PTT type outputs which pull a device to ground.  This mosfet should sink
at least an amp of current when driven from +5V; this isn't it's maximum
current carrying capability but an amp for a PTT output is a lot of current.

For voltage outputs, I chose the VP3203 mosfet. This mosfet requires it's
gate to be pulled to the output voltage to fully turn off. Since this could
be 12V or more, I needed to put a BJT between the microprocessor and the
mosfet.  This can be seen on Q2+Q7 and Q6+Q8 on the schematic.  If you're
controling something that needs even more current, there are other mosfets
available, such as the IRF9Z14 (6.7A) or STP80PF55 (80A).

Which type of transistor is selected determines how the output port should
be configured: active high for the VN2222A and active high for the
2N2222A+VP3203 outputs.  A 2N3906 would use an active low port.

### Sequencer3 PCB

The board included in this project, which I call Sequencer3, has footprints
for four VN2222A transistors and two VP3203 transitors. It uses a SparkFun
Arduino Pro Mini Arduino board and tries to optimize board space as much
as possible.  It was designed to fit in a plastic box that I had on hand.

To save space, I used surface mount resistors and a surface mount version
of the 2N2222A, the MMBT2222. I find these components to be easily
solderable wit the help of a magnifier.

The PTT input is protected with a 5.1V Zener diode.

There is an extra input on header J4; this is intended to be used with relays
that have feedback contacts on them.  I haven't written any software to use
this, but the intent is to prevent switching on the amplifier if the feedback
doesn't indicate that the relay switched.

### Other Options

There are a number of Arduino shields (add-on boards) that have relays or
can drive stepper motors. The relay shields can be easily adapted to this
by configuring the correct ports to drive them.  The stepper drivers should
be usable for proving + voltages to devices. I haven't used them.

## Software

The "portinfo" structure defines how an output pin is controlled: Mode,
delay time when PTT turns on, delay time for PTT Off and an on time for
pulsed ports.

The "portinfo" array definition defines how the pins are configured.
I have a lot of different configurations that I've used.  The "General
Purplse" definition configures as may pins as possible - some active high,
others active low. Four pins are pulsed outputs (two active high, two
active low).  This configuration provides the most flexibility to wire up
to whatever device you select.  The configuration for "Sequencer 4+2 Board" is 
to be used with the PCB accompanying this project.  It provides four outputs
to VN2222A chips, with the outputs active high and two outputs to pulsed
relays using a 2N2222A+VP3203 transitor pair.

The heart of the sequencer is the function named `turn`.  This turns a port
on or off, based on the port's configuration and the PTT state passed as
the 2nd parameter (TRUE = on). It returns true if the port is a pulsed port
that is on, indicating to the caller that a timer needs to be set to turn
the port off after the pulse time has elapsed.

# Other Variations

I've used this basic design with other Arduino boards in several projects.
My 6M SDR transciever has a driver amplifier with this sequencer that drives
a T/R relay, VCC to the driver amplifier, and a PTT output to the main
amplifier. Another version controls a 70cm SDR transciver driver amplifier
and adds monitoring of output power whose status is displayed on a red/green
LED.

# Breaking Away

The ATMega328PB-AN costs about $1.65 in single quantities (October 2026),
while an Arduino board like the Sparkfun Arduino Pro Mini costs $11.25
(October 2026). That's a big difference! There are cheaper Arduno clones
available on the internet that close this price gap.
The Arduino board itself isn't really
offering anything this project needs.  This can be run using the internal
oscillator at 8MHz.
There are some "bread board" processor definitions that could be used to
program the microcontroller on a breadboard.
I then found an Arduino board definition called <???> that allowed me to
program a non-Arduino board using the IDE.
I made a PCB for the ATmega328 chip with an Arduino-compabible pinout and am
able to program it.  I've designed a PCB for a sequencer using just the
ATmega328 chip, but have not assembled and tested it yet.



# Appendix A: Commercial Arduino Relay Shield

HiLetgo 5V 4 Channel Relay Shield for UNO R3 
```
Relay 1 is controlled by digital pin 7
Relay 2 is controlled by digital pin 6
Relay 3 is controlled by digital pin 5
Relay 4 is controlled by digital pin 4
```

a high written to a pin turns the relay ON
low written to a pin turns the relay OFF

For example, if we are using an NC (normally closed) configuration, we
need to write a LOW signal to activate the relay. If we are using an NO
(normally open) configuration, we need to write a HIGH signal to activate
the relay.

