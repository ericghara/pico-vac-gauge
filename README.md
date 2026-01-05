### Pico Vacuum Gauge

Dual channel vacuum gauge for the Pi Pico.  This was designed to measure
intake vacuum of an internal combustion engine.

The high sampling rate allows capturing the distribution of pressure
for various events in a running engine (i.e. intake valves opening
and closing).  The immediate application of this sensor is to
perform throttle body syncing of an engine with independent throttle bodies.
Future applications could be for measuring the RPM of an internal combustion
engine without probing the engine's ignition system (for example reading
RPM for a dyno run on an old pre-EFI, wasted spark ignition engine).

### Design Goals
- Readings across channels are comparable (i.e. for same vacuum source each channel should read identically)
- High sample rate - 25,000 samples per channel per second

### Getting started

#### Hardware
- Requires a Raspberry Pi Pico
- 2 MPXV6115V set-up similar to datasheet reference schematic with resistor voltage dividers to scale their output from 0-5 to 0-3.3 volts.  Schematic coming soon.
- Computer to interface with Pico and log measurements over UART  

#### Software 

1. Init submodules: `git submodule update --init --recursive`
2. Build and transfer to Raspberry Pi Pico.
   1. `cmake -B build -S .`
   2. `make -C build`
   3. Transfer `build/pico_vac_gauge.uf2` to a Pico booted to `BOOTSEL` mode.

