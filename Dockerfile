# phusion
FROM phusion/baseimage:0.11

# Use baseimage-docker's init system.
CMD ["/sbin/my_init"]

# Set Env
ENV DEBIAN_FRONTEND noninteractive
ENV path /launchpad-pro

# Set the working directory
WORKDIR ${path}

# Copy the current directory contents into the container at ${path}
#ADD . ${path}

# Distro management
RUN apt-get update && apt-get install -y build-essential gcc-arm-none-eabi && \
	apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Run shell
CMD ["/bin/bash"]
