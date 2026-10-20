# Sequencer
An Arduino sequencer for amateur radio

This is an Arduino project used for sequencing transmit / receive relays,
amplifiers and transverters in order to prevent transmitting into a relay when
it is switching, and to protect recevier inputs from transmitters or amplifiers.

I've written [a full description of othe project.](sequencer.md)

The source code is in this repository under Software and the KiCad files
for a PCB are under Hardware/sequencer3. I had this board manufactured by
OshPark in 2024.

# Arduino Clone (sort of)

I wanted to reduce the cost of this project, and the MegaAVR chip itself is
about a tenth of the price of an Arduino board. I can get by with just the
AVR and a few support chips. I made an arduino style board for the ATMega328
in a TQFP package.

[Find it here](Hardware/Mega328basic1)

