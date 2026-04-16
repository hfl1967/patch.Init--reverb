# Makefile for patch.Init() reverb
# APP_TYPE=BOOT_QSPI tells the build system to target the Daisy bootloader,
# so you can flash over USB by pressing the reset button..

# The name of your project (also the output binary name)
TARGET = reverb

# Bootloader flashing — mirrors the cumuloid setup
APP_TYPE = BOOT_QSPI

# Source files — just our one cpp file
CPP_SOURCES = reverb.cpp ../DaisySP/DaisySP-LGPL/Source/Effects/reverbsc.cpp

# Extra include paths — lets the compiler find headers in this folder
# and one level up. The -I prefix means "also look here for #include files"
C_INCLUDES += -I. -I..

# Where are the Daisy libraries relative to this project?
# ../ means one level up from init_reverb, into the Projects folder
LIBDAISY_DIR = ../libDaisy
DAISYSP_DIR  = ../DaisySP

# Points to the core/ subfolder inside libDaisy.
# This is where the startup file (startup_stm32h750xx.s) and linker scripts live.
# The startup file is the very first code that runs when the Daisy powers on —
# it sets up the processor before your main() function is called.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core

# Pull in the Daisy build system. This one line brings in all the compiler
# flags, linker settings, and build rules for the STM32H7 chip.
# It uses all the variables defined above, so this must come last.
include $(SYSTEM_FILES_DIR)/Makefile