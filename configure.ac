# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([p-load], [1.0.0], [support@pololu.com])
AM_INIT_AUTOMAKE([1.11 -Wall -Werror foreign])
AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Tries to use C99 mode.
AC_PROG_CC_STDC

# Checks for the system USB library.
AC_SUBST(USB_SYSTEM_IMPL)
AC_CHECK_HEADERS([IOKit/usb/IOUSBLib.h],
                 [LIBS="-framework IOkit -framework CoreFoundation $LIBS"
                  USB_SYSTEM_IMPL=mac],
                 [AC_CHECK_LIB(usb-1.0, libusb_init,
                               [LIBS="-lusb-1.0 $LIBS"
                                USB_SYSTEM_IMPL=linux],
                               [AC_ERROR([No USB library found.])])])

AC_CHECK_HEADERS([stdint.h stdlib.h string.h sys/ioctl.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memset strerror])

AC_CONFIG_FILES([Makefile src/Makefile])

AC_OUTPUT
