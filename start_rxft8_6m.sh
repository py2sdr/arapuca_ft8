#!/bin/bash
# RTL-SDR FT8 Reverse Beacon
# Author: Edson Pereira, PY2SDR

# Run as ii user is started by root
if [ $UID -eq 0 ]; then
  user=pi
  dir=/home/pi
  cd $dir
  exec su "$user" "$0" -- "$@"
fi

# Run in RAMDISK
cd /run/user/$UID
mkdir -p sdr6m
cd sdr6m
PATH=$PATH:/usr/bin:/usr/local/bin

# Cleanup function at exit
cleanup() {
    trap - EXIT INT TERM  # Remove the trap to prevent recursion
    echo "Stopping..."
    kill 0  # kills all processes in the current process group
    exit
}
trap cleanup EXIT INT TERM

# Multicast configuration
MCAST_GROUP=239.0.0.11
MCAST_PORT=50001

# SDR configuration
LO=50200000     # Center Frequency
QRG=50313000    # RX Frequency
SR=2400000      # Sample Rate
SHIFT=$(printf "%.6f" $(echo "scale=6; ($LO-$QRG) / $SR" | bc)) # Frequency Shift

# SDR processing chain
rtl_sdr -s $SR -f $LO -g 25 - | \
csdr convert_u8_f | \
csdr shift_addition_cc $SHIFT | \
csdr fir_decimate_cc 50 0.005 HAMMING | \
csdr bandpass_fir_fft_cc 0.0005 0.066 0.005 | \
csdr realpart_cf | \
csdr convert_f_s16 | \
msend -a $MCAST_GROUP -p $MCAST_PORT -i dummy0 &

# Audio recording and FT8 processing
rxft8 -c py2sdr -l gg56tv -f 50313000 -g $MCAST_GROUP -p $MCAST_PORT -i dummy0 &

# Remote audio monitoring
mrecv -a $MCAST_GROUP -p $MCAST_PORT -i dummy0 | nmux -a 0.0.0.0 -p 30000 &

wait
