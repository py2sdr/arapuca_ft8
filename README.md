# Arapuca_FT8 - FT8 Reverse Beacon

A lightweight FT8 digital mode receiver and decoder for Raspberry Pi, designed for unattended FT8 monitoring.

## Overview

RXFT8 receives audio samples via UDP multicast (48 kHz, 16-bit signed integer, mono), decodes FT8 transmissions using the `jt9` decoder, and reports valid grids to the PSK Reporter network. It's optimized for running as a reverse beacon station on single-board computers like the Raspberry Pi.

The input audio is expected to be low-pass filtered with a stopband at 3.1 kHz.

## Features

- Real-time FT8 signal decoding
- Automatic PSK Reporter integration
- UDP multicast audio input
- Audio power level monitoring
- Automatic WAV file cleanup (optional retention)

## Requirements

### Hardware
- Raspberry Pi or Linux PC
- RTL-SDR USB dongle
- Antenna suitable for your target frequency

### Software Dependencies
- Qt5 Core and Network modules
- `jt9` decoder (from WSJT-X package)
- `rtl_sdr` (RTL-SDR tools)
- `csdr` (command-line DSP tools)
- `msend` (multicast send utility from mtools)
- `ntpd` (for accurate time synchronization)

## Installation

### Install System Dependencies

```bash
# Install Qt5 development libraries
sudo apt-get install qt5-default qtbase5-dev

# Install RTL-SDR tools
sudo apt-get install rtl-sdr

# Install WSJT-X (provides jt9 decoder)
sudo apt-get install wsjtx

# Install NTP for accurate time synchronization (critical for FT8)
sudo apt-get install ntp

# Install bc for shell calculations
sudo apt-get install bc

# Install CSDR build dependencies
sudo apt-get install build-essential autoconf automake libtool pkg-config libfftw3-dev
```

### Install mtools

The `msend` utility is required for sending audio via UDP multicast:

```bash
# Clone the mtools repository
git clone https://github.com/py2sdr/mtools.git
cd mtools

# Compile both programs
gcc -Wall -O2 -o msend msend.c
gcc -Wall -O2 -o mrecv mrecv.c

# Install to /usr/local/bin
sudo cp msend mrecv /usr/local/bin
```

### Install CSDR

CSDR is used for SDR signal processing. Install from the py2sdr repository:

```bash
# Clone the CSDR repository
git clone https://github.com/py2sdr/csdr.git
cd csdr

# Build and install
autoreconf -i
./configure
make
sudo make install
```

### Build RXFT8

```bash
# Clone the RXFT8 repository
git clone https://github.com/py2sdr/rxft8.git
cd rxft8

# Generate Makefile
qmake

# Compile
make

# Install (optional)
sudo make install
```

## Network Setup

### Creating a Dummy Network Interface

To prevent multicast data from being transmitted over WiFi or other physical networks, you need to set up a dummy network interface. This keeps all multicast traffic isolated to your local system.

For detailed instructions on setting up the `dummy0` interface, please refer to the **Local Dummy Network Setup** section in the [mtools repository](https://github.com/py2sdr/mtools#local-dummy-network-setup).

## Usage

### Basic Command

```bash
rxft8 -c CALLSIGN -l LOCATOR -f FREQUENCY -g MULTICAST_GROUP -p MULTICAST_PORT -i INTERFACE
```

### Command Line Options

- `-c, --callsign`: Your station callsign (required)
- `-l, --locator`: Your Maidenhead grid locator (required)
- `-f, --freq`: RX frequency in Hz (required)
- `-g, --group`: Multicast group address (required)
- `-p, --port`: Multicast port (required)
- `-i, --interface`: Network interface name (required, e.g., dummy0, eth0)

### Example

```bash
rxft8 -c PY2SDR -l GG56TV -f 50313000 -g 239.0.0.11 -p 50001 -i dummy0
```

## Integration with SDR

The following start script demonstrates a complete FT8 receiver chain using an RTL-SDR. Other SDR hardware can be used as long as the CSDR processing chain outputs 48 kHz, 16-bit signed integer, mono audio to the multicast stream.

```bash
#!/bin/bash
# RTL-SDR

if [ $UID -eq 0 ]; then
  user=pi
  dir=/home/pi
  cd $dir
  exec su "$user" "$0" -- "$@"
fi

cd /run/user/$UID
mkdir -p sdr6m
cd sdr6m
PATH=$PATH:/usr/bin:/usr/local/bin

# Cleanup function
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

LO=50200000     # Center Frequency
QRG=50313000    # RX Frequency
SR=2400000      # Sample Rate
SHIFT=$(printf "%.6f" $(echo "scale=6; ($LO-$QRG) / $SR" | bc)) # Frequency Shift

rtl_sdr -s $SR -f $LO -g 49 - | \
csdr convert_u8_f | \
csdr shift_addition_cc $SHIFT | \
csdr fir_decimate_cc 50 0.005 HAMMING | \
csdr bandpass_fir_fft_cc 0.0005 0.066 0.005 | \
csdr realpart_cf | \
csdr convert_f_s16 | \
msend -a $MCAST_GROUP -p $MCAST_PORT -i dummy0 &

rxft8 -c py2sdr -l gg56tv -f 50313000 -g $MCAST_GROUP -p $MCAST_PORT -i dummy0 &

mrecv -a $MCAST_GROUP -p $MCAST_PORT -i dummy0 | nmux -a 0.0.0.0 -p 30000 &

wait
```

**Note**: The script changes to `/run/user/$UID/sdr6m`, which uses RAM filesystem (tmpfs). This prevents excessive writes to the SD card, extending its lifespan. WAV files are created and deleted in RAM, avoiding SD card wear.

### Pipeline Explanation

1. **rtl_sdr**: Captures IQ samples from RTL-SDR at 2.4 MHz sample rate
2. **csdr convert_u8_f**: Converts unsigned 8-bit samples to float
3. **csdr shift_addition_cc**: Shifts frequency to center on target
4. **csdr fir_decimate_cc**: Decimates by 50x (2.4 MHz → 48 kHz)
5. **csdr bandpass_fir_fft_cc**: Applies a 3.2 kHz bandpass filter (USB demodulation)
6. **csdr realpart_cf**: Extracts real part (complex to real)
7. **csdr convert_f_s16**: Converts to 16-bit signed integers
8. **msend**: Sends audio via UDP multicast
9. **rxft8**: Receives multicast audio and decodes FT8
10. **mrecv | nmux**: Receives multicast audio and serves it via TCP for remote monitoring

## Output

### Console Output
RXFT8 prints decoded messages to stdout in the format:
```
HHMMSS SNR DT FREQ MESSAGE *
```

The asterisk (*) indicates the station was reported to PSK Reporter.

### Log Files
- `/var/tmp/rxft8_FREQUENCY.log`: Decoded messages log
- `/var/tmp/rxft8_FREQUENCY_pwr.log`: Audio power levels

### WAV Files
By default, WAV files are automatically deleted after decoding. To retain WAV files, create an empty file named `keepwav` in the working directory:

```bash
touch keepwav
```

**Warning**: Only enable `keepwav` temporarily for debugging purposes. Since the working directory (`/run/user/$UID/sdr6m`) is in RAM, keeping WAV files will quickly fill up available memory. Each FT8 period generates approximately 2.6 MB of WAV data, which accumulates rapidly during continuous operation.

## Technical Details

### Audio Processing
- Input: 48 kHz, 16-bit signed integer (int16_t), mono, little-endian
- Downsampling: 48 kHz → 12 kHz (4:1)
- Buffer: 15 seconds for FT8 (1,382,400 samples @ 48 kHz)

### Timing
- FT8 cycle: 15 seconds
- Synchronization: Aligned to system clock (00, 15, 30, 45 seconds)

### Network
- Protocol: UDP multicast
- Audio format: 48 kHz, 16-bit signed integer (int16_t), mono, little-endian

## PSK Reporter Integration

RXFT8 automatically reports decoded callsigns with valid grid locators to PSK Reporter (https://pskreporter.info).

## Troubleshooting

If RXFT8 is not decoding signals as expected, you can enable WAV file retention and manually test the decoder:

### Enable WAV File Retention

Create an empty file named `keepwav` in the working directory:

```bash
touch keepwav
```

This will prevent RXFT8 from automatically deleting WAV files after decoding.

### Manual Decoder Testing

Once you have retained WAV files, you can test the jt9 decoder manually using the same command that RXFT8 uses:

```bash
jt9 --ft8 -d 3 -L 0 -H 3000 your_file.wav
```

Where:
- `--ft8`: Decode FT8 mode
- `-d 3`: Decoder depth level (3 is aggressive)
- `-L 0`: Low frequency limit (0 Hz)
- `-H 3000`: High frequency limit (3000 Hz)
- `your_file.wav`: The WAV file to decode

This allows you to verify:
- WAV files are being generated correctly
- The jt9 decoder is working properly
- Audio levels and quality are adequate for decoding

If the manual decoder works but RXFT8 doesn't, check the error messages in the console output for process or file access issues.

## License

This project is licensed under the GNU General Public License v3.0 (GPLv3).
