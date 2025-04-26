#!/bin/bash

#Exit on any command failure
set -e

# Load cow brd module
echo "Loading cow_brd..."
insmod build/cow_brd.ko num_disks=1 num_snapshots=20 disk_size=204800 || exit 1

read -p "[*] cow_brd loaded. Press enter to load disk_wrapper..."

# Load disk wrapper module
echo "Loading disk_wrapper..."
insmod build/disk_wrapper.ko target_device_path=/dev/cow_ram_snapshot1_0 flags_device_path=/dev/sda || exit 1

read -p "[*] disk_wrapper loaded. Press enter to unload disk_wrapper..."


# Unload disk_wrapper
echo "Removing disk_wrapper..."
rmmod build/disk_wrapper.ko


echo "Done."
