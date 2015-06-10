# Launchpad Pro
Open source firmware for the Launchpad Pro grid controller!

# Installation
This project uses Vagrant to manage the build environment. As such, you need to:

1. Install Vagrant
2. Install VirtualBox (or another virtualisation platform of your choice)
3. vagrant up

# Building
SSH into the Vagrant "box" by doing `vagrant up
At the command prompt, type `make

This will generate the firmware .bin file, which is no use to you (yet).

You need to convert it to a sysex dump format (or we may be able to automate this).  Instructions to follow...

# Uploading to a Launchpad Pro
Bootloader firmware update instructions here (Sysex Librarian / MIDI-OX / ...?)

# Debugging
We decided not to support or encourage using a JLink or similar for debugging, as opening a Launchpad Pro to fit a debugging header can easily damage the FSR (force sensitive resistor) sheets.

Instead, you're going to have to do things the old fashioned way - by blinking LEDs or sending MIDI messages.

If you want to test code in detail, we suggest developing it on a host (we'd love to release our simulator environment at a later stage, but there are currently no plans to do so).

