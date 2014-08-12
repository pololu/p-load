Pololu USB Bootloader Utility (p-load)

The Pololu USB Bootloader Utility is a command-line program that
allows you to read and write from the memories of certain Pololu
products over USB using their USB bootloaders.

This utility currently supports the following products:

  * P-Star 25K50 Micro

To get a help screen showing the available command-line arguments,
just run "p-load" at a command prompt with no arguments.

The source code of the utility is available online at:

  https://github.com/pololu/p-load

Installers and source distributions are available for download from
the Pololu P-Star 25K50 Micro User's Guide:

  http://www.pololu.com/docs/0J62


== p-load on Windows ==

The recommended way to get p-load for Windows is to use our installer.

To build p-load from source in Windows, you will need to install
Microsoft Visual Studio Express 2013 for Windows Desktop or some other
version of Visual Studio 2013 that supports building C programs.  In
Visual Studio, open p-load.vcxproj and build it just like any other
project.


== p-load on Mac OS X ==

The recommended way to get p-load for Mac OS X is to use our installer.


== p-load on Linux ==

The recommended way to get p-load for Linux is to download and compile
the source distribution.

You will need to have libusb 1.0 installed before compiling or running
p-load.  In Ubuntu, you can install it by running

  sudo apt-get intall libusb-1.0-dev

After you have downloaded the p-load source distribution, run the
following sequence of commands or similar in order to extract the
package, build it, and install it:

  tar -xzvf p-load-*.tar.gz
  cd p-load-*
  ./configure && make
  sudo make install

If you downloaded this source code directly from github instead of
getting a source distribution, you will not have a "configure" script,
but you can create it yourself.  You will need to install autoconf and
automake, which are part of the GNU Build System (also known as
autotools), and then run "autoreconf --install".  The source
distribution can be created by running "make dist".
