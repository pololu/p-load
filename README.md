# Pololu USB Bootloader Utility (p-load)

Version: 2.3.1<br/>
Release date: 2017-12-17<br/>
[www.pololu.com](https://www.pololu.com/)

The Pololu USB Bootloader Utility is a command-line program that
allows you to read and write from the memories of certain Pololu
products over USB using their USB bootloaders.

This utility currently supports the following products:

  * P-Star 25K50 Micro
  * P-Star 45K50 Mini
  * Pololu USB AVR Programmer v2
  * Tic T825 Stepper Motor Controller
  * Tic T834 Stepper Motor Controller

To get a help screen showing the available command-line arguments,
run `p-load --help` at a command prompt.


## Installation

Installers for Windows and Mac OS X are available for download from the Pololu
P-Star 25K50 Micro User's Guide:

  http://www.pololu.com/docs/0J62

Linux users can download the source code and compile it.  See
[BUILDING.md](BUILDING.md) for instructions.

## EEPROM support

For bootloaders that support reading and writing from EEPROM, this utility can
read and write HEX files that contains data for both flash and EEPROM.  The
EEPROM data should be placed in the HEX file starting at address 0xF00000 (so
0xF00000 corresponds to the first byte of EEPROM).

## Credits

This repository contains a copy of [https://github.com/leethomason/tinyxml2](TinyXML-2)
by Lee Thomason (www.grinninglizard.com).

## Version history

* 2.3.1 (2017-12-17):
  * Fixed a bug that prevented p-load from reading HEX files with Windows line
    endings on Linux or macOS.
  * Fixed some issues with the console output.
* 2.3.0 (2017-11-10):
  * Added support for the P-Star 45K50 Bootloader.
* 2.2.0 (2017-11-02):
  * Added support for the Tic T834 Stepper Motor Controller.
  * Fixed some minor bugs.
* 2.1.0 (2017-07-13): Added support for the Tic T825 Stepper Motor Controller.
* 2.0.1 (2016-05-05): Fixed a problem with the Mac OS X release that prevented
  it from finding libusbp at run-time.
* 2.0.0 (2016-03-21):
    * Added support for the Pololu USB AVR Programmer v2.
    * Changed the included Windows driver so that devices show up in the
      "Universal Serial Bus devices" category instead of "Pololu USB Devices."
    * New library dependencies: libusbp-1 and libtinyxml2.
    * Mac OS X versions older than 10.11 are no longer supported, because
      libusbp-1 does not support them.
    * Added support for Pololu Firmware Image Archive Files (FMI files).
    * Added commas to the output of `--list` to make it easier to parse.
    * Added `--pause` and `--pause-on-error` options, which makes it easier
      to use p-load inside a temporary window.
    * Added `-h` and `--help`.
    * Output files are now only opened after all the data to be written is available.
    * Output files can now be written to the standard output.
    * Input files can now be read from the standard input.
    * Switched from C to C++ and rewrote most of the code.
    * Dropped support for autotools and Visual Studio in favor CMake,
      MSYS2, and mingw-w64.
* 1.0.0 (2014-08-15): Original release with support for the P-Star 25K50 Micro.
