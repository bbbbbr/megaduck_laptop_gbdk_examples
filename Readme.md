# GBDK examples for the Mega Duck Laptop

How to interface with the special hardware on the Mega Duck Laptop models (Super QuiQue and Super Junior Computer).

## Keyboard example
- Initializing the external controller connected over the serial link port
- Polling the keyboard for input and processing the returned keycodes


## RTC example
- Initializing the external controller connected over the serial link port
- Polling the laptop RTC for date and time
- Setting a new date and time for the laptop RTC


# Emulation for Development and Testing
The [Super Junior SameDuck](https://github.com/bbbbbr/SuperJuniorSameDuck) fork of the [SameBoy](https://sameboy.github.io/) emulator can be used for running MegaDuck laptop programs, it has support for the Keyboard and RTC.

- When using it, make sure to enable the `--megaduck_laptop` flag on the command line to enable the relevant hardware emulation.
