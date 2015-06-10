#!/usr/bin/env bash

# setup as described here: http://gnuarmeclipse.livius.net/blog/toolchain-install/#GNULinux

# tools for running 32-bit binaries
apt-get update
sudo apt-get install -y lib32z1 lib32ncurses5 lib32bz2-1.0

# eclipse IDE
# sudo apt-get install -y eclipse

# download the compiler
wget -O /tmp/gcc-arm.tar.bz2 https://launchpad.net/gcc-arm-embedded/4.9/4.9-2015-q1-update/+download/gcc-arm-none-eabi-4_9-2015q1-20150306-linux.tar.bz2

# expand and add it to the path (yes, I know it says don't ever do this - but this is a Vagrant box ;)
cd /usr/local
sudo tar xjf /tmp/gcc-arm.tar.bz2

# cd to the /vagrant dir on login
echo "cd /vagrant" >> /home/vagrant/.bashrc

# set up path to arm gcc on login
echo "export PATH=/usr/local/gcc-arm-none-eabi-4_9-2015q1/bin:$PATH" >> /home/vagrant/.bashrc


