#!/bin/bash
CAM_IP=10.0.52.1
echo "Copying to camera"
cp /tmp/rcpu/brainfuck.out /tmp/rcpu.bin
echo "put /tmp/compiled" | pftp "$CAM_IP"
echo "put /tmp/rcpu.bin" | pftp "$CAM_IP"
echo "Done"
echo "logging in"
/tmp/rcpu/input.sh | telnet "$CAM_IP"
echo "done"
