#!/bin/sh

set -u

TAP_NAME="tap0"
TAP_ADDR="192.168.70.1/24"
GATEWAY_DEVICE="wlp0s20f3"

sudo ip tuntap add mode tap user $USER name $TAP_NAME
sudo ip addr add $TAP_ADDR dev $TAP_NAME
sudo ip link set $TAP_NAME up

echo 1 | sudo tee /proc/sys/net/ipv4/ip_forward >/dev/null
sudo iptables -A FORWARD -o $TAP_NAME -j ACCEPT
sudo iptables -A FORWARD -i $TAP_NAME -j ACCEPT
sudo iptables -t nat -A POSTROUTING -s $TAP_ADDR -o $GATEWAY_DEVICE -j MASQUERADE
