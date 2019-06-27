#!/bin/bash
set -e
set -x
VM_ADDRESS=osboxes@10.50.0.21


scp -r $PWD "$VM_ADDRESS:/tmp/"
echo "compiling over ssh"
ssh "$VM_ADDRESS" "cd /tmp/rcpu && echo compiling && /usr/local/cris/bin/cris-gcc -g -mcpu=v10 -mno-side-effects -mcc-init -static -nostdlib main.c -Wl,--entry=_main -o /tmp/compiled && ./copy_to_cam.sh"
