#ifndef _INTEL_HEX_H
#define _INTEL_HEX_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// Functions that return IHX_RESULT will either return one of the result codes
// below or one of the standard error codes defined in errno.h.
typedef int IHX_RESULT;
#define IHX_SUCCESS 0
#define IHX_FILE_READ_ERROR 300
#define IHX_FILE_BAD_LINE 301
#define IHX_FILE_EOF 302

/* Represents a memory space that can either be read from a HEX file or written to it.
 * You might have one of these to represent flash memory, another to represent EEPROM,
 * and (for reading only) another one that represents an alternate address region for flash.
 * (On the STM32, sometimes each part of flash actually has two addresses.)
 */
typedef struct ihxMemory
{
    // A pointer to a memory region of size endAddress - startAddress.
    uint8_t * image;

    // The address in HEX-file address space that corresponds to image[0].
    uint32_t startAddress;

    // The address in HEX-file address space that corresponds to the byte that
    // is one byte after the end of the image buffer.  Think of it as the exclusive end address
    // of the region in the hex file that you are interested in reading.
    uint32_t endAddress;
} ihxMemory;

/* Reads the lines of an Intel Hex file.  Each line can be thought of a change to the image.
 * The changes are applied to the supplied image buffer in the order they appear.
 * file: A handle to a file to read the hex lines from.
 * fileName: The name of the file.  Used to generate good error messages.
 * lineNumber: A pointer to the number of the last line in the file that was processed.  For
 *   example, if other code has already processed line 1 and line 2, lineNumber should equal 2.
 *   Used to generate good error messages.
 * memories: Pointer to a null-terminated array that specifies the memory regions to read.
 *   The array should have a dummy entry at the end where image == NULL.
 */
IHX_RESULT ihxRead(FILE * file, const char * fileName, uint32_t * lineNumber, ihxMemory * memories);

/* ihxWriteFile: Writes an Intel hex file to the disk.
 *   file: the file stream we are writing to
 *   image: a pointer to a buffer the holds the image.  The buffer must be big enough:
 *      ihxWriteFile will read all bytes from image[0] to image[endAddress - startAddress - 1].
 *   startAddress: This is the hex file address that corresponds to image[0]
 *   endAddress: This is the hex file address that corresponds to image[endAddress - startAddress].
 *   memories: Pointer to a null-terminated array that specifies the memory regions to write.
 *     The array should have a dummy entry at the end where image == NULL.
 */
IHX_RESULT ihxWrite(FILE * file, ihxMemory * memories);

#endif
