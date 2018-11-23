[![Build Status](https://travis-ci.org/dvhdr/launchpad-pro.svg?branch=master)](https://travis-ci.org/dvhdr/launchpad-pro)

# Launchpad Pro
Open source firmware for the Novation Launchpad Pro grid controller!  By customising this code, you can:

- Implement your own unique standalone apps
- Create chorders, sequencers, light shows, games and more
- Have fun!

You'll definitely need *some* C programming experience, but we've deliberately kept much of the firmwarey nastiness tucked away, to make the process a little friendlier.

# Philosophy
We could have released the full source for the factory shipping firmware, but we decided not to for a variety of reasons.  Instead, we created a simplified framework for developing "apps" on Launchpad Pro, which comprises a build environment, application entry points / API, and a library of low level source code.  Our reasoning is as follows:

- There is no value in customising low level routines such as LED multiplexing or ADC scanning - this code has been carefully tweaked over many months to deliver the best results, and is not something you'd want to mess with.
- There is very little value in customising main() or other low level features, and again these things are hard to do well.  Interrupt priorities? No.
- If we shipped the application firmware as-is, we'd have a support nightmare on our hands (imagine the phone calls - my "Launchpad Pro is behaving strangely...").  Instead, we wanted to create a clear boundary between "normal" usage with Ableton, and custom firmware.  As such, Ableton integration has been removed from this firmware, as has the setup / navigation functionality. In addition, the "Live" USB MIDI port has been removed, and the device has a different name and USB PID.
- If we left the Ableton integration and menu structure in place, open firmware developers would have to work around it.  They would also potentially consume precious RAM/CPU resources.  I've a feeling this isn't what you'd want, but we're interested to hear your feedback.
- Licensing requirements for the CMSIS library version we use are ambiguous.  Yes, we could port to the public version, but why bother, given the above reasoning - I'd prefer to spend my time on good documentation and examples.  As such, all the CMSIS code is compiled into launchpad_pro.a, and we do not need to distribute the headers.

I'm sure you'll have feedback for us, so please do get in touch!  I'm [blogging the process too](http://launchpadfirmware.tumblr.com/) if you'd like to read my musings.

# Setup the Development Environment

## Using Docker

If your system is running docker you can easily setup the environment with:

```
docker build -t novation-launchpad-pro-dev .
docker run -it -v $(pwd):/launchpad-pro novation-launchpad-pro-dev
make
```

## Using Vagrant

To use [Vagrant](https://www.vagrantup.com/) to manage the build environment you need to:

1. Clone this repository on your host computer (if using the command line, make sure you `git clone --recursive`).
2. Install [Vagrant](https://www.vagrantup.com/)
3. Install [VirtualBox](https://www.virtualbox.org/wiki/Downloads)
4. Open a command prompt, and navigate to the project directory
5. Type `vagrant up`, hit enter and grab a beverage of your choice.  Maybe two - it is building a lovely fresh development machine just for you!

If you have a poor internet connection, ummm, find a better one :)

### Building
Once your new "box" is up and running, you can build the app in one of two ways.  In the spirit of experimentation, we've created a full Eclipse development environment for you to use.  However, you might prefer to do things on the command line.

###  To use the command line interface:
1. SSH into the Vagrant "box" by doing `vagrant ssh`
2. At the command prompt, simply type `make`

### To build using Eclipse GUI

**Make sure you wait until the `vagrant up` command has fully completed** before logging in to your VM.  The GUI appears long before the provisioning script finishes.  If you don't, you'll have to log out and log back in again before Eclipse can see the correct path.

1. Log in to the Ubuntu GUI (the password is, as is the convention, **vagrant**).
2. Launch Eclipse from the doodah on the top left (it's a bit like Spotlight)
3. Accept the default when Eclipse asks you for a workspace.  I can't figure out how to store the workspace in source control, so you need to import it.
4. Click "Workbench" at the Eclipse startup screen.
5. In Eclipse, choose "File->Import..."
6. Under "C/C++", choose "Existing Code as Makefile Project", hit "Next"
7. Give the project any name you like (launchpad?)
8. Under "Existing Code Location" type `/vagrant`.  The toolchain isn't important, the compiler is part of the Makefile.
9. Hit Finish - you should now see your project.
10. Select your project by clicking on it.
11. Click the hammer icon at the top, and wait while the project builds.

Either of the above methods will generate the firmware image, `launchpad_pro.syx`, in the project `build` directory.  You can then upload this to your Launchpad Pro from the host!

## Using macOS

On macOS you can easily install the GCC ARM toolchain using the [homebrew package manager](http://brew.sh). The EABI tools are maintained in an external repository which you need to put on tap first. You can then run ```make``` to directly compile the code:

```
brew tap PX4/homebrew-px4
brew install gcc-arm-none-eabi
make
```

# Uploading to a Launchpad Pro
Now you've got some nice new code to run! To upload it to your Launchpad Pro, you'll need a sysex tool for your host platform (I'd love to get it working from the virtual machine, but that's for later).  I recommend [Sysex Librarian](http://www.snoize.com/SysExLibrarian/) on macOS, and [MIDI OX](http://www.midiox.com/) on Windows.  On Linux, I'll bet you already have a tool in mind.

I won't describe how to use these tools, I'm sure you already know - and if you don't, their documentation is superior to mine!  Here's what you need to do:

1. Unplug your Launchpad Pro
2. Hold the "Setup" button down while connecting it to your host via USB (ensure it's connected to the host, and not to a virtual machine!)
3. The unit will start up in "bootloader" mode
4. Send your launchpad_pro.syx file to the device MIDI port - it will briefly scroll "upgrading..." across the grid.
5. Wait for the update to complete, and for the device to reboot!

Tip - set the delay between sysex messages to as low a value as possible, so you're not waiting about for ages while the firmware uploads!

# Bricked it!
Don't worry - even if you upload toxic nonsense to the device, you cannot brick it - the bootloader is stored in a protected area of flash.  If your new firmware doesn't boot, you'll get stuck at step (3) above, or with a crashed unit. Simply repeat the above process with the shipping firmware image (`resources/Launchpad Pro-1.0.154.syx`) to restore your unit to the factory defaults.  Better yet, fix the bugs :)

# The API
The API works in two directions - from the HAL (hardware abstraction layer) to the app, and from the app to the HAL.  The HAL calls into your app to:

- Receive user events from the pads and buttons
- Receive messages from the MIDI/USB ports
- Receive a tick message to drive timer based code
- Be notified when someone connects or disconnects a MIDI cable

By calling into the HAL, your app can:

- Write colours to the LEDs
- Send messages to the MIDI/USB ports
- Store and recall a little bit of data on the Launchpad Pro's flash memory

The best way to learn about these is to read the documentation in `app.h`, and to study the (very basic) example code!

# Debugging
We decided not to support or encourage using a hardware debugger, as opening a Launchpad Pro to fit a debugging header can easily damage the FSR (force sensitive resistor) sheet.

Instead, you're going to have to do things the old fashioned way - by blinking LEDs or sending MIDI messages (though hopefully no need for a 'scope!).  For what it's worth, that's the way I've developed this version of the firmware - dogfooding all the way ;)

If do you want to debug interactively (and of course you do), you can use the interactive desktop simulator on macOS:

1. Build the Xcode project located in `/tools/osx`
2. Connect your Launchpad Pro
3. Install the factory firmware on your Launchpad Pro
4. Put the Launchpad Pro into "Programmer" mode using the Setup button (you'll get odd behaviour otherwise)
5. Start debugging in Xcode!

Currently it only supports button presses and LED messages - there's no setup button, flash storage or aftertouch (yet).  It has a really awful busywaiting timer for the 1kHz tick.  However, it does allow you to debug your application logic using Xcode!

You can also use the simple command-line simulator located in the `/tools` directory.  It is compiled and ran as part of the build process, so it serves as a very basic test of your app before it is baked into a sysex dump - more of a test harness.

To debug the simulator interactively in Eclipse:

1. Click the down arrow next to the little "bug" icon in the toolbar
2. Choose "Debug configurations..."
3. Right click "C/C++ Application" and choose "New...:
4. Under "C/C++ Application" click Browse... and locate the simulator binary at `/vagrant/build/simulator`
5. Hit "Debug"!

# The Hardware
The Launchpad Pro is based around an ARM Cortex M3 from STMicroelectronics.  Specifically, an [STM32F103RBT6](http://www.st.com/web/catalog/mmc/FM141/SC1169/SS1031/LN1565/PF164487).  It's clocked at 72MHz, and has 20k RAM (I'm not sure how much of this we're using in the open build yet - should be a fair amount left but I haven't measured it).  The low level LED multiplexing and pad/switch scanning consume a fair bit of CPU time in interrupt mode, but have changed a little in the open firmware library (so again, I don't have measurements for how many cycles they're using).

It has 128k of flash memory, but we won't be exposing all of it as part of this API (dangerously easy to corrupt things!).

# Vagrant tips
When you're done developing, simply type `vagrant suspend` to halt your VM without destroying it - this will make `vagrant up` a lot quicker next time.  If you're really finished, `vagrant destroy` will completely remove the VM from your system (but not any of your code).

If you only want to build using the command line, you might want to run your Vagrant box headless, which you can do by modifying the Vagrantfile: `vb.gui = false`.  You can also add more CPUs, RAM etc. if you want.

If prefer, you can install the gcc-arm toolchain on your local machine, or you might already have it.  You can find all you need [here](http://gnuarmeclipse.livius.net/).

If your connection drops out while updating the Vagrant box, you can get stuck, unable to `vagrant up`.  To resolve, you need to delete the temp file - `~/vagrant.d/tmp`.

# Firmware development tips
OK - we're not going to need to use the MISRA rules, but there are a few things to avoid.  Dynamic memory allocation is a no (well it will work, but it's best avoided). Floating point will work, but it's implemented in software and will be slooooow.   C++ ought to work, but you'll definitely want to avoid exceptions and RTTI!
