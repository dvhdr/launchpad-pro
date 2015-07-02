# Launchpad Pro
Open source firmware for the Novation Launchpad Pro grid controller!  By customising this code, you can:

- Implement your own unique standalone apps
- Create chorders, sequencers, light shows, games and more
- Learn a little about software development!

You'll definiteley need *some* C programming experience, but we've deliberately kept much of the firmwarey nastiness tucked away, to make the process a little friendlier.

This project is still at an early stage, and no "interesting" example apps have yet been developed.  You might want to hang on until there's something more detailed before you get stuck in.  Or not!

# Philosophy
We could have released the full source for the factory shipping firmware, but we decided not to for a variety of reasons.  Instead, we created a simplified framework for developing "apps" on Launchpad, which comprises a build environment, application entry points / API, and a library of low level source code.  Our reasoning is as follows:

- There is no value in customising low level routines such as LED multiplexing or ADC scanning - this code has been carefully tweaked over many months to deliver the best results, and is not something you'd want to mess with.
- There is very little value in customising main() or other low level features, and again these things are hard to do well.  Interrupt priorities? No.
- If we shipped the application firmware as-is, we'd have a support nightmare on our hands (imagine the phonecalls - my "Launchpad Pro is behaving strangely...").  Instead, we wanted to create a clear boundary between "normal" usage with Ableton, and custom firmware.  As such, Ableton integration has been removed from this firmware, as has the setup / navigation functionality. In addition, the "Live" USB MIDI port has been removed, and the device has a different name and USB PID.
- If we left the Ableton integration and menu structure in place, open firmware developers would have to work around it.  They would also potentially consume precious RAM/CPU resources.  I've a feeling this isn't what you'd want, but we're interested to hear your feedback.
- Licensing requirements for the CMSIS library version we use are ambiguous.  Yes, we could port to the public version, but why bother, given the above reasoning - I'd prefer to spend my time on good documentation and examples.  As such, all the CMSIS code is compiled into launchpad_pro.a, and we do not need to distribute the headers.

I'm sure you'll have feedback for us, so please do get in touch!  I'm [blogging the process too](http://launchpadfirmware.tumblr.com/) if you'd like to read my musings.

# Installation
This project uses [Vagrant](https://www.vagrantup.com/) to manage the build environment. As such, you need to:

1. Clone this repository on your host computer
2. Install [Vagrant](https://www.vagrantup.com/)
3. Install [VirtualBox](https://www.virtualbox.org/wiki/Downloads)
4. Open a command prompt, and navigate to the project directory
5. Type `vagrant up`, hit enter and grab a beverage of your choice.  It's building a lovely fresh development machine just for you!

# Building
You can build the app in one of two ways.  In the spirit of experimenation, I've created a full Eclipse development environment for you to use.  However, you might prefer to do things on the command line.  (if you do, you might also want to run your Vagrant box headless, which you can do by modifying the Vagrantfile). 

# -To use the command line interface:
1. SSH into the Vagrant "box" by doing `vagrant ssh`
2. At the command prompt, simply type `make`

# -To build using the Eclipse GUI

1. Log in to the Ubuntu GUI (the password is, as is the convention, "vagrant").
2. Launch Eclipse from the doodah on the top left (it's a bit like Spotlight)
3. Accept the default when Eclipse asks you for a workspace.  I can't figure out how to store the workspace in source control, so you need to import it.
4. In Eclipse, choose "File->Import..."
5. Under "C/C++", choose "Existing Code as Makefile Project", hit "Next"
6. Give the project any name you like (launchpad?)
7. Under "Existing code location" type `/vagrant`
8. Hit Finish - you should now see your project.  If not, click "Workbench" and it should appear.
9. Click the hammer icon at the top, and wait while the project builds.

Either of the above methods will generate the firmware image, `launchpad_pro.syx`, in the project /build directory.  You can then upload this to your Launchpad Pro from the host!

# Uploading to a Launchpad Pro
Now you've got some nice new code to run! To upload it to your Launchpad Pro, you'll need a sysex tool for your host platform (I'd love to get it working from the virtual machine, but that's for later).  I recommend [Sysex Librarian](http://www.snoize.com/SysExLibrarian/) on OS X, and [MIDI OX](http://www.midiox.com/) on Windows.  On Linux, I'll bet you already have a tool in mind.

I won't describe how to use these tools, I'm sure you already know - and if you don't, their documentation is superior to mine!  Here's what you need to do:

1. Unplug your Launchpad Pro
2. Hold the "Setup" button down while connecting it to your host via USB (ensure it's connected to the host, and not to a virtual machine!)
3. The unit will start up in "bootloader" mode
4. Send your launchpad_pro.syx file to the device MIDI port - it should briefly scroll "updating..." across the grid.
5. Wait for the update to complete, and for the device to reboot!

# Bricked it!
Don't worry - even if you upload toxic nonsense to the device, you cannot brick it - the bootloader is stored in a protected area of flash.  If your new firmware doesn't boot, you'll get stuck at step (3) above, or with a crashed unit. Simply repeat the above process with the shipping firmware image (resources/Launchpad Pro-1.0.154.syx) to restore your unit to the factory defaults.  Better yet, fix the bugs :)

# API
The most crucial parts of the API are:

- Recieving messages from the pads and buttons
- Writing colours to the LEDs
- Sending and receiving messages from the MIDI ports
- Receiving a tick message to drive timer based code

The best way to learn about these is to read the documentation in app.h, and to study the example code!

# Debugging
We decided not to support or encourage using a hardware debugger, as opening a Launchpad Pro to fit a debugging header can easily damage the FSR (force sensitive resistor) sheet.

Instead, you're going to have to do things the old fashioned way - by blinking LEDs or sending MIDI messages.  FWIW, that's the way I've developed this version of the firmware - dogfooding all the way ;)

If you want to test code in detail, we suggest developing it on a host.  I want to develop a host app which presents the same API to app.c, but which forwards all messages to and from the hardware Launchpad Pro via MIDI - that way you'll be able to debug most code on the host very nicely.

# Vagrant tips
When you're done developing, simply type `vagrant suspend` to halt your VM without destroying it - this will make `vagrant up` a lot quicker next time.  If you're really finished, `vagrant destroy` will completely remove the VM from your system (but not any of your code).

