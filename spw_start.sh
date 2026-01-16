#!/bin/bash

# Read spw0_gpio
sudo devmem 0x80040008
# Read spw1_gpio
sudo devmem 0x80070008

sleep 0.5

sudo gpioset `sudo gpiofind "spw0_linkdis"`=0
sudo gpioset `sudo gpiofind "spw0_linkstart"`=1

sudo gpioset `sudo gpiofind "spw1_linkdis"`=1
sudo gpioset `sudo gpiofind "spw1_linkstart"`=1

sleep 0.5

# Read spw0_gpio
sudo devmem 0x80040008
# Read spw1_gpio
sudo devmem 0x80070008
