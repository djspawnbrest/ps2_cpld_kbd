# PS/2 AVR CPLD keyboard adapter

PS/2 keyboard adapter for ZX Spectrum on Atmega328 + EPM7128STC100.
Designed to be connected directly instead of a mechanical keyboard.

Based on [Andy Karpov](https://github.com/andykarpov) [PS/2 AVR CPLD keyboard adapter](https://github.com/andykarpov/ps2_cpld_kbd)

![image](https://github.com/djspawnbrest/ps2_cpld_kbd/raw/master/docs/ps2_cpld_kbd.jpg)

## How to program

### Programming Atmega328p
1. disconnect EPM7128 from the power source by disconnecting jumper JP1
2. avrdude -p atmega328p -c USBasp -U flash:w:avr_kbd_mega328p.hex -U lfuse:w:0xbf:m -U hfuse:w:0xda:m -U efuse:w:0xfd:m
3. short JP1

### Programming EPM7128S
use quartus programmer to flash cpld_kbd.pof

## Available hotkeys
1. F12 and Ctrl+Alt+Del generate a short negative pulse on O_RESET pin. It's nice to connect this signal to N_RESET of your ZX Spectrum
2. Ctrl+Alt+Backspace produce a controller internal reset needed for some reason
3. PrintScreen generate a short negative pulse on O_MAGICK pin. If you have a BDI controller then you have to connect this signal to magick button.
4. Scroll Lock switch on / off the O_TURBO signal with keyboard led indication as Scroll Lock led
5. Pause/Break switch on / off the O_SPECIAL signal with keyboard led indication as Num Lock led, also Num Lock button is blocked!
