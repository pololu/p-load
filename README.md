# Pololu USB Bootloader Utility (p-load)

Version: 2.0.0<br/>
Release date: 2016 Mar 21<br/>
[www.pololu.com](https://www.pololu.com/)

The Pololu USB Bootloader Utility is a command-line program that
allows you to read and write from the memories of certain Pololu
products over USB using their USB bootloaders.

This utility currently supports the following products:

  * P-Star 25K50 Micro
  * Pololu USB AVR Programmer v2

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

* 2.0.0 (2016 Mar 21):
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
* 1.0.0 (2014 Aug 15): Original release.
