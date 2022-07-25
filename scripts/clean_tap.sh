#!/bin/sh

set -u

TAP_NAME="tap0"

sudo ip link delete $TAP_NAME
