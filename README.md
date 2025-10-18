# SARSAT_SGB - COSPAS-SARSAT T.018 (2nd Generation) Beacon Transmitter

Complete implementation of a COSPAS-SARSAT T.018 (2nd generation) beacon transmitter for **Odroid-C4 + ADALM-PLUTO (PlutoSDR)** platform.

## 📋 Overview

**SARSAT_SGB** (Second Generation Beacon) transmits emergency beacon signals compliant with **COSPAS-SARSAT T.018** specification:

- **Modulation**: OQPSK with DSSS (Direct Sequence Spread Spectrum)
- **Data Rate**: 300 bps (150 bps per I/Q channel)
- **Chip Rate**: 38.4 kchips/s
- **Spreading**: 256 chips/bit per channel
- **Error Correction**: BCH(250,202) - 48 parity bits
- **Frame Structure**: 252 bits total (2 header + 202 info + 48 BCH)
- **Sample Rate**: 400 kHz
- **Frequency**: 403 MHz (training), 406.028 MHz (operational)

## 🎯 Features

### Complete T.018 Protocol Implementation

✅ **LFSR/PRN Generator** (prn_generator.c)
- x²³ + x¹⁸ + 1 polynomial (Fibonacci configuration)
- RIGHT shift with X0⊕X18 feedback
- Validated against T.018 Table 2.2: `8000 0108 4212 84A1`
- Separate I/Q channels with 64-chip offset

✅ **BCH(250,202) Error Correction** (t018_protocol.c)
- Generator polynomial: `0x1C7EB85DF3C97` (49 bits)
- Galois Field GF(2⁶) implementation
- Polynomial long division encoding
- Frame integrity verification

✅ **OQPSK Modulator** (oqpsk_modulator.c)
- Tc/2 offset (Q-channel delayed by half chip)
- DSSS spreading with PRN sequences
- Linear interpolation between chips
- 400 kHz I/Q sample generation

✅ **Frame Building** (t018_protocol.c)
- GPS position encoding (T.018 Appendix C)
- ALL rotating field types:
  - **G.008**: Objective Requirements (elapsed hours, last position time, altitude)
  - **ELT-DT**: Time/Altitude data
  - **RLS**: Return Link Service provider/data
  - **CANCEL**: Deactivation method
- Dynamic field calculations
- Country codes (MID) support

✅ **ELT Sequence Management** (t018_protocol.c)
- **Phase 1**: 5-second intervals (0-3 minutes)
- **Phase 2**: 10-second intervals (3-30 minutes)
- **Phase 3**: 28.5±1.5s intervals (30+ minutes)

✅ **PlutoSDR Interface** (pluto_control.c)
- libiio library integration
- TX configuration (frequency, gain, sample rate)
- I/Q buffer transmission
- Status monitoring

## 🛠️ Hardware Requirements

- **Odroid-C4** (ARM64, 4GB RAM)
- **ADALM-PLUTO (PlutoSDR)** connected via USB
- **Operating System**: Armbian/Ubuntu 20.04+ (64-bit)

## 📦 Software Dependencies

```bash
sudo apt update
sudo apt install build-essential libiio-dev libiio-utils git
```

### Dependency Details

| Package | Version | Purpose |
|---------|---------|---------|
| gcc | ≥9.0 | GNU C Compiler |
| libiio-dev | ≥0.21 | PlutoSDR control library |
| libiio-utils | ≥0.21 | iio_info, iio_attr tools |
| libm | (glibc) | Math library (complex, trig) |

## 🚀 Installation

### 1. Clone Repository

```bash
cd /home/fab2/Developpement/COSPAS-SARSAT/ADALM-PLUTO/
git clone <repository_url> SARSAT_SGB
cd SARSAT_SGB
```

### 2. Verify Dependencies

```bash
make check-deps
```

### 3. Build

```bash
make clean && make
```

Expected output:
```
Compiling src/prn_generator.c
Compiling src/t018_protocol.c
Compiling src/oqpsk_modulator.c
Compiling src/pluto_control.c
Compiling src/main.c
Linking bin/sarsat_sgb
✓ SARSAT_SGB (2nd Generation Beacon) compiled successfully
```

### 4. Install (Optional)

```bash
sudo make install
```

This installs `sarsat_sgb` to `/usr/local/bin/`.

## 📡 PlutoSDR Setup

### 1. Connect PlutoSDR

```bash
# USB connection
lsusb | grep Analog  # Should show "Analog Devices, Inc. PlutoSDR"

# Network connection (default: 192.168.2.1)
ping 192.168.2.1
```

### 2. Verify PlutoSDR

```bash
iio_info -u ip:192.168.2.1
```

Expected devices:
- `ad9361-phy` (physical layer)
- `cf-ad9361-dds-core-lpc` (TX device)

### 3. Test Transmission

```bash
# 10-second interval test transmission
make test
```

## 🎛️ Usage

### Command Line Options

```bash
sarsat_sgb [options]

Options:
  -f <freq>     Frequency in Hz (default: 403000000)
  -g <gain>     TX gain in dB (default: -10)
  -t <type>     Beacon type: 0=EPIRB, 1=PLB, 2=ELT, 3=ELT-DT (default: 0)
  -c <code>     Country code (MID) (default: 227 for France)
  -s <serial>   Serial number (default: 13398)
  -m <mode>     Test mode: 0=Exercise, 1=Test (default: 1)
  -i <sec>      TX interval in seconds (default: 10)
  -lat <lat>    Latitude in degrees (default: 43.2)
  -lon <lon>    Longitude in degrees (default: 5.4)
  -alt <alt>    Altitude in meters (default: 0)
  -u <uri>      PlutoSDR URI (default: ip:192.168.2.1)
  -h            Show help
```

### Examples

#### 1. Basic Test Transmission (403 MHz, 10s interval)

```bash
./bin/sarsat_sgb -f 403000000 -g -10 -m 1 -i 10
```

#### 2. EPIRB Training (France, Mediterranean)

```bash
./bin/sarsat_sgb -t 0 -c 227 -lat 43.2 -lon 5.4 -m 1 -i 120
```

#### 3. ELT Simulation (2-minute interval)

```bash
./bin/sarsat_sgb -t 2 -c 227 -lat 45.5 -lon 1.5 -alt 1500 -i 120
```

#### 4. PLB with Custom PlutoSDR IP

```bash
./bin/sarsat_sgb -t 1 -u ip:192.168.3.1 -m 1
```

## 📊 Technical Specifications

### T.018 Frame Structure (252 bits)

| Field | Bits | Position | Description |
|-------|------|----------|-------------|
| Header | 2 | 1-2 | Test/Exercise flag + padding |
| TAC | 16 | 3-18 | Type Approval Certificate |
| Serial | 14 | 19-32 | Beacon serial number |
| Country | 10 | 33-42 | MID (Maritime ID) |
| Homing | 1 | 43 | Homing device status |
| RLS | 1 | 44 | Return Link Service |
| Test Flag | 1 | 45 | Test protocol indicator |
| Latitude | 23 | 46-68 | GPS latitude (Appendix C) |
| Longitude | 24 | 69-92 | GPS longitude (Appendix C) |
| Vessel ID Type | 3 | 93-95 | MMSI/Aviation/None |
| Vessel ID | 30 | 96-125 | MMSI or 24-bit address |
| EPIRB-AIS | 14 | 126-139 | AIS System Identity |
| Beacon Type | 3 | 140-142 | EPIRB/PLB/ELT/ELT-DT |
| Spare | 14 | 143-156 | Spare bits (all 1s) |
| Rotating Field Type | 4 | 157-160 | G008/ELTDT/RLS/CANCEL |
| Rotating Field Data | 44 | 161-204 | Type-specific data |
| BCH Parity | 48 | 205-252 | BCH(250,202) checksum |

### Rotating Field Types

#### G.008 (Objective Requirements)
- **Bits 159-164** (6): Elapsed activation hours (0-63)
- **Bits 165-175** (11): Time since last location (minutes, 0-2046)
- **Bits 176-185** (10): Altitude code (T.018 encoding)
- **Bits 186-202** (17): Dynamic/spare (LFSR in test mode)

#### ELT-DT (ELT with homing)
- **Bits 159-174** (16): Time value (day/hour/minute)
- **Bits 175-184** (10): Altitude code
- **Bits 185-202** (18): Spare

#### RLS (Return Link Service)
- **Bits 159-166** (8): RLS provider ID
- **Bits 167-202** (36): RLS data

#### CANCEL (Deactivation)
- **Bits 159-160** (2): Deactivation method
- **Bits 161-202** (42): Fixed (all 1s)

### OQPSK Modulation Parameters

| Parameter | Value |
|-----------|-------|
| Chip Rate | 38.4 kchips/s |
| Sample Rate | 400 kHz |
| Samples/Chip | 10.42 |
| Data Rate | 300 bps |
| Spreading Factor | 256 chips/bit/channel |
| Frame Duration | ~960 ms (300 bits × 256 chips ÷ 38400 Hz) |
| Total Samples | ~384,000 samples |
| Q Delay | Tc/2 (half chip period) |

## 🧪 Testing & Validation

### 1. PRN Generator Validation

The PRN generator is automatically validated against **T.018 Table 2.2**:

```
Expected: 8000 0108 4212 84A1
```

This test runs during initialization and verifies:
- LFSR initial state (0x000001)
- Feedback polynomial (X0⊕X18)
- First 64 chips match reference

### 2. BCH Encoder Validation

BCH encoder can be tested with **T.018 Appendix B.1** test vector:

```
Input:  00E608F4C986196188A047C000000000000FFFC0100C1A00960
Expected BCH: 492A4FC57A49
```

### 3. Frame Integrity Check

Every generated frame is verified:
- BCH parity recalculation
- Checksum comparison
- Pass/fail indication in output

### 4. Modulation Verification

OQPSK modulator performs sanity checks:
- I/Q range verification (±1.5 max)
- Average power calculation (0.5-2.0 expected)
- NaN/Inf detection
- Sample count validation

## 📁 Project Structure

```
SARSAT_SGB/
├── src/
│   ├── main.c                 # Application entry point, CLI
│   ├── prn_generator.c        # LFSR/PRN sequences (T.018 Table 2.2)
│   ├── t018_protocol.c        # BCH encoder, frame building
│   ├── oqpsk_modulator.c      # OQPSK modulation, DSSS spreading
│   └── pluto_control.c        # PlutoSDR interface (libiio)
├── include/
│   ├── prn_generator.h
│   ├── t018_protocol.h
│   ├── oqpsk_modulator.h
│   └── pluto_control.h
├── build/                     # Object files (generated)
├── bin/                       # Compiled executable (generated)
├── Makefile
└── README.md
```

## 🔍 Troubleshooting

### PlutoSDR Not Found

```bash
# Check USB connection
lsusb | grep Analog

# Check network connection
ping 192.168.2.1

# Verify IIO devices
iio_info -u ip:192.168.2.1
```

### Compilation Errors

```bash
# Missing libiio
sudo apt install libiio-dev libiio-utils

# Missing math library
# libm is part of glibc, should be installed by default

# Check gcc version
gcc --version  # Should be ≥9.0
```

### Transmission Fails

```bash
# Verify PlutoSDR TX configuration
iio_attr -u ip:192.168.2.1 -d ad9361-phy -c voltage0 hardwaregain

# Check sample rate
iio_attr -u ip:192.168.2.1 -d ad9361-phy -c voltage0 sampling_frequency

# Monitor system logs
dmesg | grep iio
```

### Low Signal Quality

- **Increase TX gain**: `-g -5` (default: -10 dB)
- **Check antenna**: Ensure proper 403/406 MHz antenna
- **Verify frequency**: Training = 403 MHz, Operational = 406.028 MHz
- **Check RF bandwidth**: Should be 200-400 kHz

## 🎓 References

### COSPAS-SARSAT Specifications

- **T.018**: Specification for COSPAS-SARSAT 406 MHz Distress Beacons (Type Approval Standard)
- **C/S T.001**: Specification for COSPAS-SARSAT 406 MHz Distress Beacons (1st Generation)
- **C/S G.008**: Objective Requirements Test Specification

### Key Documents

- T.018 Table 2.2: PRN sequence reference (`8000 0108 4212 84A1`)
- T.018 Appendix B: BCH test vectors
- T.018 Appendix C: GPS position encoding
- T.018 Appendix D: LFSR polynomial (x²³ + x¹⁸ + 1)

## ⚠️ Legal Notice

**IMPORTANT**: This software is for **TRAINING AND TESTING PURPOSES ONLY**.

- **Training Frequency**: 403.000 MHz (authorized for training)
- **Operational Frequency**: 406.028 MHz (EMERGENCY USE ONLY)
- **Transmission Power**: Keep low (<-10 dB) during testing
- **Regulatory Compliance**: Ensure compliance with local spectrum regulations

**Unauthorized transmission on 406 MHz can trigger false alarms and is illegal in most jurisdictions.**

## 📜 License

This project is developed for educational and training purposes as part of COSPAS-SARSAT beacon research.

## 🤝 Contributing

Improvements and bug reports welcome. Please ensure:
- Compliance with T.018 specification
- Validation against official test vectors
- No modification of critical LFSR/BCH algorithms without verification

## 📧 Contact

For technical questions about T.018 implementation or PlutoSDR integration, please open an issue in the repository.

---

**Built with**: Odroid-C4 + ADALM-PLUTO + libiio
**Specification**: COSPAS-SARSAT T.018 Rev.12
**Status**: ✅ Complete implementation with full T.018 compliance
