#!/usr/bin/env bash

# setup as described here: http://gnuarmeclipse.livius.net/blog/toolchain-install/#GNULinux
# We're using Luna - need to update plugin install locations below if switching to Kepler etc.
ECLIPSE_URL=http://mirror.netcologne.de/eclipse//technology/epp/downloads/release/luna/SR2/eclipse-cpp-luna-SR2-linux-gtk-x86_64.tar.gz

# latest GCC-ARM version
GCC_ARM_URL=https://launchpad.net/gcc-arm-embedded/4.8/4.8-2014-q3-update/+download/gcc-arm-none-eabi-4_8-2014q3-20140805-linux.tar.bz2

# install tools for running 32-bit binaries
apt-get update
sudo apt-get install -y lib32z1 lib32ncurses5 lib32bz2-1.0

# install Java
sudo apt-get install -y openjdk-7-jdk

# download Eclipse
wget -O /tmp/eclipse.tar.gz $ECLIPSE_URL

# Extract files
sudo tar zxvf /tmp/eclipse.tar.gz -C /opt/
# Change ownership to root
sudo chown -R root:root /opt/eclipse/

# Create launch script in /usr/bin
echo '#!/bin/sh' | sudo tee /usr/bin/eclipse
echo 'export ECLIPSE_HOME="/opt/eclipse"' | sudo tee -a /usr/bin/eclipse
echo '$ECLIPSE_HOME/eclipse $*' | sudo tee -a /usr/bin/eclipse
sudo chmod 755 /usr/bin/eclipse

# Create menu entry
echo "[Desktop Entry]" | sudo tee /usr/share/applications/eclipse.desktop
echo "Name=Eclipse IDE" | sudo tee -a /usr/share/applications/eclipse.desktop
echo "Comment=Integrated Development Environment" | sudo tee -a /usr/share/applications/eclipse.desktop
echo "TryExec=/usr/bin/eclipse" | sudo tee -a /usr/share/applications/eclipse.desktop
echo "Exec=/usr/bin/eclipse" | sudo tee -a /usr/share/applications/eclipse.desktop
echo "Icon=/opt/eclipse/icon.xpm" | sudo tee -a /usr/share/applications/eclipse.desktop
echo "Categories=Development;IDE;Java;" | sudo tee -a /usr/share/applications/eclipse.desktop
echo "Terminal=false" | sudo tee -a /usr/share/applications/eclipse.desktop
echo "Type=Application" | sudo tee -a /usr/share/applications/eclipse.desktop
echo "StartupNotify=true" | sudo tee -a /usr/share/applications/eclipse.desktop

# Delete the eclipse tarball
rm /tmp/eclipse.tar.gz

# download the gcc-arm tools
wget -O /tmp/gcc-arm.tar.bz2 $GCC_ARM_URL

# expand and add it to the path (yes, I know it says don't ever do this - but this is a Vagrant box ;)
sudo tar xjf /tmp/gcc-arm.tar.bz2 -C /usr/local

# set up path to arm gcc on login
sudo echo "export PATH=/usr/local/gcc-arm-none-eabi-4_8-2014q3/bin:$PATH" >> /etc/profile.d/env.sh

# Delete the gcc-arm tarball
rm /tmp/gcc-arm.tar.bz2

# helpfully cd to the /vagrant dir on login
echo "cd /vagrant" >> /home/vagrant/.bashrc
