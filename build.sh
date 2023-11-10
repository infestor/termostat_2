#!/bin/bash

MCU=atmega328p

OUTDIR=bin
#mkdir -p $OUTDIR >/dev/null

CESTA="/Users/evil/prg/arduino/Arduino.app/Contents/Java/hardware/tools/avr"

CC="$CESTA/bin/avr-c++"
CPP="$CESTA/bin/avr-c++"

## Options common to compile, link and assembly rules
COMMON="-mmcu=$MCU"

## Compile options common for all C compilation units.
#CFLAGS=$(COMMON) -Wall -g0 -gdwarf-2 -DF_CPU=16000000UL -Os -ffreestanding -fno-tree-scev-cprop -mcall-prologues -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums -fno-jump-tables -fdata-sections -ffunction-sections -fno-split-wide-types
#LDFLAGS="$(COMMON) -Wl,--gc-sections -Wl,--relax"

CFLAGS="-Wall -W"
#CLAGS+=" -pedantic"
CFLAGS+=" -g3 -gdwarf-2 -gstrict-dwarf"
CFLAGS+=" -DF_CPU=16000000UL -Os"
CFLAGS+=" -ffreestanding"
#CFLAGS+=" -mshort-calls"
#CFLAGS+=" -msize"
CFLAGS+=" -fno-tree-scev-cprop"
#CFLAGS+=" -mcall-prologues"
CFLAGS+=" -fno-jump-tables"
CFLAGS+=" -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums"
CFLAGS+=" -fno-split-wide-types"
#CFLAGS+=" -fwhole-program"
CFLAGS+=" -Wl,--gc-sections"
CFLAGS+=" -ffunction-sections"
CFLAGS+=" -fdata-sections"
CFLAGS+=" -Wl,--relax"
CFLAGS+=" -Wa,-a,-ad"
#CFLAGS+=" -nostartfiles"

$CPP -mmcu=$MCU $CFLAGS TM1637Display.c main.cpp -o termostat.elf  > termostat.lst

$CESTA/bin/avr-objcopy -O ihex -R .eeprom -R .fuse -R .lock -R .signature termostat.elf termostat.hex
$CESTA/bin/avr-size --mcu=$MCU --format=avr termostat.elf
$CESTA/bin/avr-objdump -h -S termostat.elf > termostat.lss

#echo $CFLAGS
