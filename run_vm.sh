#!/bin/bash

# su root first

qemu-system-x86_64 \
    -m 512 \
    -net user,hostfwd=tcp::7777-:22 \
    -net nic \
    -hda /home/james/projects/hwmon-temper/Arch-Linux-x86_64-basic.qcow2 \
    -usb -device usb-host,vendorid=0x3553,productid=0xa001 \
    -cpu host \
    -accel kvm \
    -virtfs local,path=/home/james/projects/hwmon-temper,mount_tag=host0,security_model=none,id=host0
