#!/bin/sh

set -u

TAP_NAME="tap0"

sudo ip tuntap add mode tap user $USER name $TAP_NAME
sudo ip addr add 192.168.70.1/24 dev $TAP_NAME
sudo ip link set $TAP_NAME up
