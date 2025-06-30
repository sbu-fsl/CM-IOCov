#!/bin/bash

cd /tmp

wget http://kernel.ubuntu.com/~kernel-ppa/mainline/v6.12/amd64/linux-headers-6.12.0-061200_6.12.0-061200.202411220723_all.deb
wget http://kernel.ubuntu.com/~kernel-ppa/mainline/v6.12/amd64/linux-headers-6.12.0-061200-generic_6.12.0-061200.202411220723_amd64.deb
wget http://kernel.ubuntu.com/~kernel-ppa/mainline/v6.12/amd64/linux-image-unsigned-6.12.0-061200-generic_6.12.0-061200.202411220723_amd64.deb
wget http://kernel.ubuntu.com/~kernel-ppa/mainline/v6.12/amd64/linux-modules-6.12.0-061200-generic_6.12.0-061200.202411220723_amd64.deb

sudo dpkg -i *.deb

