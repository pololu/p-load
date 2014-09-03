/* intel_hex.c:
 * Simple library for reading Intel hex (.ihx or .hex) files.
 */

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <errno.h>     // See /usr/include/asm-generic/errno.h
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <assert.h>
#include "intel_hex.h"

#ifdef __GNUC__
#define PRINTF_ATTR(s, f) __attribute__((format (printf, s, f)))
#else
#define PRINTF_ATTR(s, f)
#endif

void lineError(const char * fileName, unsigned int lineno, const char * format, ...) PRINTF_ATTR(3, 4);
void lineError(const char * fileName, unsigned int lineno, const char * format, ...)
{
    va_list ap;
    va_start(ap, format);
    fprintf(stderr, "%s:%d: ", fileName, lineno);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}


// This function has the same signature as fgets so they can be
// used interchangeably.  But this function is better than
// fgets because it handles all different line endings:
// \r , \n, or \r\n.
// The end of the line was reached, the returned string will always
// have a single \n at the end (it will NOT have \r\n at the end).
char * getLine(char * str, int num, FILE * stream)
{
    unsigned int length = 0;

    assert(num != 0);
    assert(str != NULL);

    while(true)
    {
        int c = fgetc(stream);
        if (c == EOF)
        {
            // Either reached the end of the file, or there was an error.
            // The person who called getLine should call ferror() or feof()
            // to figure out which.
            return length != 0 ? str : NULL;
        }

        if (c == '\r')
        {
            c = fgetc(stream);
            if (c != '\n' && c != EOF)
            {
                ungetc(c, stream);
            }
            c = '\n';
        }

        str[length++] = c;

        if (c == '\n' || length == num-1)
        {
            str[length-1] = 0;
            return str;
        }
    }
}

IHX_RESULT ihxRead(FILE * file, const char * fileName, uint32_t * lineNumber, ihxMemory * memories)
{
    // Assume the high 16 bits of the address are zero to start off with.
    uint16_t addressHigh = 0;

    assert(file != NULL);
    assert(fileName != NULL);
    assert(memories != NULL);

#ifdef _DEBUG
    for (ihxMemory * mem = memories; mem->image; mem++)
    {
        assert(mem->image != NULL);
        assert(mem->endAddress > mem->startAddress);
    }
#endif

    uint32_t internalLineNumber = 0;
    if (lineNumber == NULL)
    {
        lineNumber = &internalLineNumber;
    }

    while(1)
    {
        unsigned int i;
        int length;
        char line[1024];
        uint8_t data[sizeof(line)/2];
        uint8_t checksum;
        unsigned int byteCount, addressLow, recordType;

        (*lineNumber)++;
        if (!getLine(line, sizeof(line), file))
        {
            if (feof(file))
            {
                lineError(fileName, *lineNumber, "Unexpected EOF: expected hex file to contain an End of File record.");
                return IHX_FILE_EOF;
            }

            lineError(fileName, *lineNumber, "Unable to read hex file line.  Error code %d.", ferror(file));
            return IHX_FILE_READ_ERROR;
        }

        if (line[0] != ':')
        {
            lineError(fileName, *lineNumber, "Hex line does not start with colon (:).");
            return IHX_FILE_BAD_LINE;
        }

        // Trim off the white space at the end (typically will be a single \n).
        for(i = 0; line[i] && !isspace(line[i]); i++){}
        length = i;
        line[length] = 0;

        if ((length % 2) != 1)
        {
            lineError(fileName, *lineNumber, "Line should have an odd number of characters including the colon and not including whitespace at the end, but it has %d characters.", length);
            return IHX_FILE_BAD_LINE;
        }

        // Read the indentifying information of the line.
        if (3 != sscanf(line+1, "%02x%04x%02x", &byteCount, &addressLow, &recordType))
        {
            lineError(fileName, *lineNumber, "Invalid byte count, address, or record type field.");
            return IHX_FILE_BAD_LINE;
        }
        if (length != byteCount * 2 + 11)
        {
            lineError(fileName, *lineNumber, "Byte count field does not match actual line length.");
            return IHX_FILE_BAD_LINE;
        }

        // Read the data and checksum
        checksum = byteCount + (addressLow & 0xFF) + (addressLow >> 8) + recordType;
        for(i = 0; i < byteCount + 1; i++)
        {
            unsigned int data_byte;
            if (1 != sscanf(line + 9 + 2*i, "%02x", &data_byte))
            {
                lineError(fileName, *lineNumber, "Invalid characters encountered when reading data byte %d.", i);
                return IHX_FILE_BAD_LINE;
            }
            data[i] = data_byte;
            checksum += data_byte;
        }
        if (checksum != 0)
        {
            lineError(fileName, *lineNumber, "Incorrect checksum.");
            return IHX_FILE_BAD_LINE;
        }

        switch(recordType)
        {
        default:
            lineError(fileName, *lineNumber, "Unrecognized record type: %02x.", recordType);
            return IHX_FILE_BAD_LINE;
        case 4: // Extended Linear Address Record (sets high 16 bits)
            if (byteCount != 2)
            {
                lineError(fileName, *lineNumber, "Extended Linear Address record with %d bytes instead of 2.", byteCount);
                return IHX_FILE_BAD_LINE;
            }
            addressHigh = (data[0] << 8) + data[1];
            break;
        case 2: // Extended Segment Address Record (basically sets bits 4-20 of the address)
        case 5: // Start Linear Address Record (sets a 32-bit address)
            lineError(fileName, *lineNumber, "Unimplemented record type: %02x.", recordType);
            return IHX_FILE_BAD_LINE;
        case 0: // Data record
        {
            uint32_t address = addressLow + (addressHigh << 16);
            for (ihxMemory * mem = memories; mem->image; mem++)
            {
                if (address >= mem->startAddress && address <= mem->endAddress - byteCount)
                {
                    // The data is in the right region, so copy it to the image.
                    memcpy(&mem->image[address - mem->startAddress], data, byteCount);
                }
            }
            break;
        case 3: // Start Segment Address Record (specific to 80x86 processors)
            // Ignore this type.
            break;
        }
        case 1: // End of File record
            return IHX_SUCCESS;
        }
    }
}

IHX_RESULT ihxWrite(FILE * file, ihxMemory * memories)
{
    uint32_t lastAddress = 0;     // The last address written to.
    const uint32_t blockSize = 16;    // The maximum number of bytes of data we will put on a single line.

    for (ihxMemory * mem = memories; mem->image; mem++)
    {
        assert(mem->image != NULL);
        assert(mem->endAddress > mem->startAddress);

        uint32_t address = mem->startAddress;  // The address of the next byte that needs to be written to the file.
        while (address < mem->endAddress)      // Each iteration of this loop will write one data record.
        {
            uint8_t checksum;

            uint32_t dataBytesToWrite;                 // Decide the number of bytes to write in this line.
            if (address + blockSize <= mem->endAddress)
            {
                dataBytesToWrite = blockSize;              // We will write a full-size line.
            }
            else
            {
                dataBytesToWrite = mem->endAddress - address;   // We will write the last line.
            }

            if ((address >> 16) != (lastAddress >> 16))
            {
                // Emit an extended linear address record because the high 16 bits changed.
                fprintf(file, ":02000004%04X", address >> 16);
                checksum = 0;
                checksum += 2 + 4;
                checksum += (address >> 16) & 0xFF;
                checksum += (address >> 24) & 0xFF;
                fprintf(file, "%02X\n", (uint8_t)(256 - checksum));
            }

            // Emit the data record.
            fprintf(file, ":%02X%04X00", dataBytesToWrite, address & 0xFFFF);
            checksum = 0;
            checksum += dataBytesToWrite;
            checksum += (address >> 8) & 0xFF;
            checksum += address & 0xFF;

            while (dataBytesToWrite)
            {
                uint8_t data = mem->image[address - mem->startAddress];
                fprintf(file, "%02X", data);
                checksum += data;
                address++;
                dataBytesToWrite--;
            }
            fprintf(file, "%02X\n", (uint8_t)(256 - checksum));

            lastAddress = address;
        }
    }
    fprintf(file, ":00000001FF\n");

    return IHX_SUCCESS;
}
