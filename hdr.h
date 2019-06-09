#ifndef HDR_H

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define DEBUG
#include <assert.h>

#ifdef __linux__
  #include <dirent.h>
  #include <sys/stat.h>
  #include <sys/types.h>
  #include <unistd.h>
  #include <endian.h>
#endif

#ifdef _WIN32
  #define _WINSOCKAPI_
  #include <windows.h>
  #include <winsock2.h>
#endif

/* common error codes */
typedef enum{
  OK                  =  0,
  FILE_ERROR          = -1,
  FILE_WRITE_ERROR    = -2,
  FILE_READ_ERROR     = -3,
  FILE_NOT_FOUND      = -4,
  FILE_EMPTY          = -5,
  NO_IMAGE_DATA       = -6,
  NOT_ENOUGH_DATA     = -7,
  INVALID_BITS        = -8,
  INVALID_ENCODE_SIZE = -9,
  INVALID_DECODE_SIZE = -10,
  INVALID_ARGUMENT    = -11,
  INVALID_COMPRESSION = -12,
  INVALID_FILE_TYPE   = -13
}error_t;

/* header for encoding */
typedef struct{
  uint16_t version;  /* version of image hider (0x2019) */
  uint16_t id;       /* unique id for image */
  uint32_t seq;      /* sequence number of segment */
  uint32_t segments; /* total number of segments */
  uint32_t size;     /* size in bytes of the image encoded in segment */
  uint8_t  bits;     /* number of bits of encoding: 1, 2 of 4 */
}hdr;

/* */
error_t decode(uint8_t *decoded, 
               unsigned decoded_size, 
               unsigned bits, 
               uint8_t *bytes, 
               unsigned bytes_size);

/* */
error_t encode(uint8_t *encoded, 
               unsigned encoded_size, 
               unsigned bits, 
               uint8_t *bytes, 
               unsigned bytes_size);

#endif
