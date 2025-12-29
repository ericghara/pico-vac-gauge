### CLion Raspberry Pi pico template project

Project template for pico board. It uses picoprobe and openocd for debugging.

### Getting started
`cd pico-sdk`

`git submodule add https://github.com/raspberrypi/pico-sdk.git`

`git submodule update --init`

Use the build button run of course will have an error.  The `uf2` file will be located in `cmake-build-*/some_project.uf2`

Take a look at clion section of [Getting Started with Pi Pico](https://pip-assets.raspberrypi.com/categories/610-raspberry-pi-pico/documents/RP-008276-DS-1-getting-started-with-pico.pdf?disposition=inline).

#### Debugging
![openocd](docs/debug.png)
OpenOCD plugin, only accepts one single file. This means we can't add separate files for `board` and `interface` like what you see in the pico documentation.
The solution to this is to create a custom file and then source all required files. `openocd_pico.cfg` file serves as our config.

This repository contains the svd file. You can use this file to see the current value of registers:
![svd](docs/svd.png)


To get serial console, you need to first install "Serial Monitor" plugin and then create a new connection:
![serial config](docs/serial.png)
output:
![serial console](docs/serial-console.png)

