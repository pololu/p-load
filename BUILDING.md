# Building from source

If you want to build p-load from its source code, you can follow these
instructions.


## Building from source on Linux with nixcrpkgs

This software can be cross-compiled on a Linux machine using
[nixcrpkgs](https://github.com/pololu/nixcrpkgs), a collection of tools for
cross-compiling.  This is how we build our official installers for all
platforms.

To get started, you should first install [Nix, the purely functional
package manager](http://nixos.org/nix/), on a Linux machine by following the
instructions on the Nix website.

Next, run the comamnds below to download the latest version of
[nixcrpkgs](https://github.com/pololu/nixcrpkgs), and add nixcrpkgs to
your `NIX_PATH` environment variable.  You
will need to have [git](https://git-scm.com/) installed.  (If you don't want to
install git, you can download Nixcrpkgs as a ZIP file from GitHub instead.)

    git clone https://github.com/pololu/nixcrpkgs
    export NIX_PATH=$NIX_PATH:nixcrpkgs=$(pwd)/nixcrpkgs

Now run these commands to download this software and build it:

    git clone https://github.com/pololu/p-load
    cd p-load
    nix-build -A linux-x86

The name `linux-x86` in the command above is the name of an attribute in the set
defined in `default.nix`, which specifies that we want to build the software
for a Linux machine running on an i686 processor.  You can specify `win32`
instead to get a version for Windows, and you can look in `default.nix` to see
the names of other supported platforms.

Note: Building for macOS requires additional setup.  See the `README.md` file
in nixcrpkgs for details.

The first time you build the software with this method, the build will take a
lot of time, memory, and disk space, because it involves building a
cross-compiler from source.  Subsequent builds will usually be faster
because the the previously-built compilers and libraries can be reused if you
have not changed the recipes for building them.

Once the build is completed, there should be a symbolic link in the current
directory named `result` that points to the compiled software.


## Building from source on Linux (for Linux)

Another way to build for Linux is to use the development tools provided by
your Linux distribution.  Note that if you use this method, the
executables you build will most likely depend on shared libraries
from your Linux distribution.

You will need to install git, gcc, make, CMake, and libudev.  Most Linux
distributions come with a package manager that you can use to install
these dependencies.

On Ubuntu, Raspbian, and other Debian-based distributions, you can install the
dependencies by running:

    sudo apt-get install build-essential git cmake libudev-dev

On Arch Linux, libudev should already be installed, and you can install the
other dependencies by running:

    pacman -Sy base-devel git cmake

For other Linux distributions, consult the documentation of your distribution
for information about how to install these dependencies.

This software depends on the Pololu USB Library version 1.x.x (libusbp-1).  Run
the commands below to download, compile, and install the latest version of that
library on your computer.

    git clone https://github.com/pololu/libusbp -b v1-latest
    cd libusbp
    mkdir build
    cd build
    cmake ..
    make
    sudo make install
    cd ../..

You can test to see if libusbp-1 was installed correctly by running
`pkg-config --cflags libusbp-1`,
which should output something like
`-I/usr/local/include/libusbp-1`.
If it says that libusbp-1 was not found in the pkg-config search path,
retry the instructions above.

Run these commands to build this software and install it

    git clone https://github.com/pololu/p-load
    cd p-load
    mkdir build
    cd build
    cmake ..
    make
    sudo make install
    cd ../..

You will need to install a udev rule to give non-root users permission
to access Pololu USB devices. Run this command from the p-load directory:

    sudo cp udev-rules/99-pololu.rules /etc/udev/rules.d/

You should now be able to run `p-load` in your shell.

If you get an error about libusbp failing to load (for example,
"cannot open shared object file: No such file or directory"), then
run `sudo ldconfig` and try again.  If that does not work, it is likely that
your system does not search for libraries in `/usr/local/lib`
by default.  To fix this issue for your current shell session, run:

    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib

A more permanent solution, which will affect all users of the computer, is to
run:

    sudo sh -c 'echo /usr/local/lib > /etc/ld.so.conf.d/local.conf'
    sudo ldconfig


## Building from source on Windows with MSYS2

Another way to build this software for Windows is to use
[MSYS2](http://msys2.github.io/).  Note that if you use this method, the
executables you build will depend on DLLs from the MSYS2 environment.

If you have not done so already, follow the
instructions on the MSYS2 website to download, install, and update your MSYS2
environment.  In particular, be sure to update your installed packages.

Next, start a shell by selecting "MSYS2 MinGW 32-bit" from your Start menu or
by running `mingw32.exe`.  This is the right environment to use if you want to
build 32-bit software that works on 32-bit or 64-bit Windows.  (If you want to
build 64-bit software that only works on 64-bit versions of Windows, select
"MSYS2 MinGW 64-bit" or `mingw64.exe`.)

Run this command to install the required development tools:

    pacman -S base-devel git $MINGW_PACKAGE_PREFIX-{toolchain,cmake}

If pacman prompts you to enter a selection of packages to install, just press
enter to install all of the packages.

This software depends on the Pololu USB Library version 1.x.x (libusbp-1).  Run
the commands below to download, compile, and install the latest version of that
library into your MSYS2 environment.

    git clone https://github.com/pololu/libusbp -b v1-latest
    cd libusbp
    mkdir build
    cd build
    MSYS2_ARG_CONV_EXCL=- cmake .. -G"MSYS Makefiles" -DCMAKE_INSTALL_PREFIX=$MINGW_PREFIX
    make install DESTDIR=/
    cd ../..

You can test to see if libusbp-1 was installed correctly by running
`pkg-config --cflags libusbp-1`,
which should output something like
`-IC:/msys64/mingw32/include/libusbp-1`.
If it says that libusbp-1 was not found in the pkg-config search path,
retry the instructions above.

Run these commands to build this software and install it:

    git clone https://github.com/pololu/p-load
    cd p-load
    mkdir build
    cd build
    MSYS2_ARG_CONV_EXCL=- cmake .. -G"MSYS Makefiles" -DCMAKE_INSTALL_PREFIX=$MINGW_PREFIX
    make install DESTDIR=/
    cd ../..

You should now be able to run `p-load` in your shell.


## Building from source on macOS with Homebrew

Another way to build this software for macOS is to use
[Homebrew](http://brew.sh/).  Note that if you use this method, the executables
you build will depend on shared libraries from Homebrew.

First, install [Homebrew](http://brew.sh/).

Then use brew to install the dependencies:

    brew install pkg-config cmake

This software depends on the Pololu USB Library version 1.x.x (libusbp-1).  Run
the commands below to download, compile, and install the latest version of that
library on your computer.

    git clone https://github.com/pololu/libusbp -b v1-latest
    cd libusbp
    mkdir build
    cd build
    cmake ..
    make
    sudo make install
    cd ../..

You can test to see if libusbp-1 was installed correctly by running
`pkg-config --cflags libusbp-1`,
which should output something like
`-I/usr/local/include/libusbp-1`.
If it says that libusbp-1 was not found in the pkg-config search path,
retry the instructions above.

Run these commands to build this software and install it:

    git clone https://github.com/pololu/p-load
    cd p-load
    mkdir build
    cd build
    cmake ..
    make
    sudo make install
    cd ../..

You should now be able to run `p-load` in your shell.

