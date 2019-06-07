/*
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tif.h"


typedef enum{
  OK                = 0,
  FILE_ERROR        = 1,
  FILE_WRITE_ERROR  = 2,
  FILE_READ_ERROR   = 3,
  FILE_NOT_FOUND    = 4,
  NO_IMAGE_DATA     = 5,
  INVALID_FILE_TYPE = 4
}error_t;

typedef enum{
  IMAGE_WIDTH     = 256,  /* image width in pixels */
  IMAGE_LENGTH    = 257,  /* image length in pixels */
  BITSPERSAMPLE   = 258,  /**/
  COMPRESSION     = 259,  /* compression scheme used */
  STRIP_OFFSETS   = 273,  /* offsets for strips */
  SAMPLESPERPIXEL = 277,  /**/
  ROWSPERSTRIP    = 278,  /* rows of contiguous data per strip*/
  STRIP_BYTES     = 279   /**/
}tag_t;

/* TIFF header format */
typedef struct{ 
  uint16_t id;          /* uint8_t order identifier */
  uint16_t version;     /* TIFF version (0x2a) */
  uint32_t ifd_offset;  /* offset of image file directory */
}tif_hdr;

/* TIFF tag format */
typedef struct{ 
  uint16_t  tag_id;      /* tag identifier*/
  uint16_t  type;        /* scalar type of data */
  uint32_t  data_count;  /* number of items in data */
  uint32_t  data_offset; /* uint8_t offset of data */
}tif_tag;

/* TIFF data information */
typedef struct{ 
  uint32_t   width;          /* image width in pixels */
  uint32_t   length;         /* image length in pixels */
  uint32_t   rows_per_strip; /* rows per strip of data */
  uint32_t  *strip_bytes;    /* size of each strip in bytes */
  uint32_t  *strip_offsets;  /* strip offsets for data */
  uint16_t   strip_count;    /* count for strips of data*/
  uint16_t   bits_per_sample;/* bit depth of the image */
  uint8_t    compression;    /* compression scheme used by image */
}tif_data;

/* header for encoding */
typedef struct{
  uint16_t version;  /* version of image hider (0x2019) */
  uint16_t seq;      /* sequence number of segment */
  uint32_t segments; /* total number of segments */
  uint32_t size;     /* size in bytes of the image encoded in segment */
  uint8_t  bits;     /* number of bits of encoding: 1, 2 of 4 */
}hdr;

static void free_data(tif_data *dataptr){
  dataptr->width = 0;
  dataptr->length = 0;
  dataptr->rows_per_strip = 0;
  free(dataptr->strip_bytes);
  free(dataptr->strip_offsets);
  dataptr->strip_bytes = NULL;
  dataptr->strip_offsets = NULL;
  dataptr->strip_count = 0;
  dataptr->bits_per_sample = 0;
}

static error_t read_hdr(char *filename, tif_hdr *hdrptr){ 
  FILE *fptr = NULL;
  if(!(fptr = fopen(filename, "rb"))){ 
    exit(EXIT_FAILURE);
  }
  if(fread((void*)hdrptr, sizeof(tif_hdr), 1, fptr) < 1){ 
    fprintf(stderr, "error reading image header\n");
    exit(EXIT_FAILURE);
  }
  fclose(fptr);
  return OK;
}

static void read_bits_per_sample(FILE *fptr, tif_data *dataptr, tif_tag tag){
  if(!dataptr){
    fprintf(stderr, "invalid data argument\n");
    exit(EXIT_FAILURE);
  }
  if(fseek(fptr, tag.data_offset, SEEK_SET) < 0){
    fprintf(stderr, "error reading bits per sample\n");
    exit(EXIT_FAILURE);
  }
  if(fread((void*)&(dataptr->bits_per_sample), 
           sizeof(uint16_t), 1, fptr) < 1){
    fprintf(stderr, "error reading bits per sample\n");
    exit(EXIT_FAILURE);
  }
}

static void read_strip_bytes(FILE *fptr, tif_data *dataptr, tif_tag tag){
  if(!dataptr){
    fprintf(stderr, "invalid data argument\n");
    exit(EXIT_FAILURE);
  }
  dataptr->strip_bytes = (uint32_t*)malloc(sizeof(uint32_t)*tag.data_count);
  dataptr->strip_count = tag.data_count;
  if(tag.data_count == 1){
    *(dataptr->strip_bytes) = tag.data_offset;
  }
  else{
    if(fseek(fptr, tag.data_offset, SEEK_SET) < 0){
      fprintf(stderr, "error reading strip offset bytes\n");
      exit(EXIT_FAILURE);
    }
    short bytes = (tag.type == 3)? 2 : 4;
    int i;
    for(i = 0; i < tag.data_count; i++){
      memset((void*)&(dataptr->strip_bytes[i]), 0, sizeof(uint32_t));
      if(fread((void*)&(dataptr->strip_bytes[i]), 
               bytes, 1, fptr) < 1){
        fprintf(stderr, "error reading strip offset bytes\n");
        exit(EXIT_FAILURE);
      }
    }
  }  
} 

static void read_strip_offsets(FILE *fptr, tif_data *dataptr, tif_tag tag){
  if(!dataptr){
    fprintf(stderr, "invalid data argument\n");
    exit(EXIT_FAILURE);
  }
  dataptr->strip_count = tag.data_count;
  dataptr->strip_offsets = (uint32_t*)malloc(sizeof(uint32_t)*tag.data_count);
  if(tag.data_count == 1){
    *(dataptr->strip_offsets) = tag.data_offset;
  }
  else{
    if(fseek(fptr, tag.data_offset, SEEK_SET) < 0){
      fprintf(stderr, "error reading strip offsets\n");
      exit(EXIT_FAILURE);
    }
    short bytes = (tag.type == 3)? 2 : 4;
    int i;
    for(i = 0; i < tag.data_count; i++){
      memset((void*)&(dataptr->strip_offsets[i]), 0, sizeof(uint32_t));
      if(fread((void*)&(dataptr->strip_offsets[i]), 
               bytes, 1, fptr) < 1){
        fprintf(stderr, "error reading strip offset\n");
        exit(EXIT_FAILURE);
      }
    }
  }
}

static void get_data(char *filename, tif_data *dataptr){
  tif_hdr hdr;
  read_hdr(filename, &hdr);
  FILE *fptr;
  if(!(fptr = fopen(filename, "rb+"))){ 
    perror("error opening image");
    exit(EXIT_FAILURE);
  } 
  if(fseek(fptr, hdr.ifd_offset, SEEK_SET) < 0){
    perror("error seeking directory offset");
    exit(EXIT_FAILURE);
  }
  uint16_t n_entries;
  if(fread((void*)&n_entries, sizeof(uint16_t), 1, fptr) < 1){
    fprintf(stderr, "error reading directory entries\n");
    exit(EXIT_FAILURE);
  }
  int i;
  for(i = 0; i < n_entries; i++){
    tif_tag tag;
    if(fread((void*)&tag, sizeof(tif_tag), 1, fptr) < 1){
      fprintf(stderr, "error reading tag\n");
      exit(EXIT_FAILURE);
    }
    long pos = ftell(fptr); 
    switch(tag.tag_id){
      case IMAGE_LENGTH:
        dataptr->length = tag.data_offset;
      break;
      case IMAGE_WIDTH:
        dataptr->width = tag.data_offset;
      break;
      case ROWSPERSTRIP:
        dataptr->rows_per_strip = tag.data_offset; 
      break;
      case STRIP_OFFSETS:
        read_strip_offsets(fptr, dataptr, tag);
      break;
      case STRIP_BYTES:
        read_strip_bytes(fptr, dataptr, tag);
      break;
      case BITSPERSAMPLE:
        read_bits_per_sample(fptr, dataptr, tag);
      break;
      case COMPRESSION:
        dataptr->compression = tag.data_offset;
      break;
    }
    if(fseek(fptr, pos, SEEK_SET) < 0){
      fprintf(stderr, "error reading tag data\n");
      exit(EXIT_FAILURE);
    }
  }
  fclose(fptr);
}

static void decode(uint8_t *decoded, unsigned decoded_size, unsigned bits, 
                   uint8_t *bytes, unsigned bytes_size){
  if(bits == 0 || 8 % bits != 0){
    fprintf(stderr, "bits to decode must be powers of 2 <= 8\n");
    exit(EXIT_FAILURE);
  }
  unsigned bit_factor = 8/bits;
  if(bytes_size != decoded_size * bit_factor){
    fprintf(stderr, "invalid bytes for bit factor\n");
    exit(EXIT_FAILURE);
  }
  uint8_t byte_mask = 0x80;
  uint8_t decode_mask = 0x01;
  int b;
  for(b = 1; b < bits; b++){
    byte_mask |= byte_mask >> 1;
    decode_mask |= decode_mask << 1;
  }
  memset((void*)decoded, 0, decoded_size);
  int i, j;
  for(i = 0, j = 0; i < decoded_size && j < bytes_size; i++){
    uint8_t mask = byte_mask;
    int b;
    for(b = 0; b < bit_factor; b++, j++){
      unsigned shift = (bit_factor - (b+1))*bits;
      uint8_t uint8_t = (bytes[j] & decode_mask) << shift;
      decoded[i] = decoded[i] | uint8_t; 
      mask >>= bit_factor;
    } 
  }
}

static int read_strip(uint32_t strip_offset, uint32_t strip_bytes, 
                      uint32_t size, uint32_t byte_count, unsigned bits,
                      FILE *img, FILE *out){
  if(fseek(img, strip_offset, SEEK_SET) < 0){
    fprintf(stderr, "error seeking strip offset\n");
    exit(EXIT_FAILURE);
  }
  int bit_factor = 8/bits;
  uint8_t *bytes = (uint8_t*)malloc(bit_factor);
  memset((void*)bytes, 0, bit_factor);
  uint8_t byte;
  int i;
  for(i = 0; i < strip_bytes && byte_count < size; 
                i += bit_factor, byte_count++){
    if(fread((void*)bytes, bit_factor, 1, img) < 1){
      fprintf(stderr, "error reading data\n");
      exit(EXIT_FAILURE);
    }
    decode((uint8_t*)&byte, 1, bits, bytes, bit_factor); 
    if(fwrite((void*)&byte, sizeof(uint8_t), 1, out) < 1){
      fprintf(stderr, "error updating image\n");
      exit(EXIT_FAILURE);
    }
  }
  free(bytes);
  return byte_count;
}

static void read_data(FILE *img, FILE *out, unsigned bits, tif_data data){ 
  if(data.strip_count == 0){
    fprintf(stderr, "image has no data\n");
    exit(EXIT_FAILURE);
  }
  if(fseek(img, data.strip_offsets[0], SEEK_SET) < 0){
    fprintf(stderr, "error reading image data\n");
    exit(EXIT_FAILURE);
  } 
  int bit_factor = 8/bits;
  int encoded_size = sizeof(unsigned)*bit_factor;
  uint8_t *encoded = (uint8_t*)malloc(encoded_size);
  if(fread((void*)encoded, encoded_size, 1, img) < 1){
    fprintf(stderr, "error reading data\n");
    exit(EXIT_FAILURE);
  }
  unsigned size;
  decode((uint8_t*)&size, sizeof(size), bits, encoded, encoded_size);
  data.strip_bytes[0] -= encoded_size;
  data.strip_offsets[0] += encoded_size;
  unsigned byte_count = 0;
  int i;
  for(i = 0; i < data.strip_count && byte_count < size; i++){
    byte_count = read_strip(data.strip_offsets[i], 
                  data.strip_bytes[i], 
                  size, byte_count, bits,
                  img, out);
  }
  free(encoded);
}


static void encode(uint8_t *encoded, unsigned encoded_size, unsigned bits, 
                   uint8_t *bytes, unsigned bytes_size){
  if(bits == 0 || 8 % bits != 0){
    fprintf(stderr, "bits to encode must be powers of 2 <= 8\n");
    exit(EXIT_FAILURE);
  }
  unsigned bit_factor = 8/bits;
  if(encoded_size != bytes_size * bit_factor){// sizes for encoding data
    fprintf(stderr, "invalid bytes for bit factor\n");
    exit(EXIT_FAILURE);
  }
  uint8_t byte_mask = 0x80;
  uint8_t encode_mask = 0x01;
  int b;
  for(b = 1; b < bits; b++){
    byte_mask |= byte_mask >> 1;
    encode_mask |= encode_mask << 1;
  }
  int i, j;
  for(i = 0, j = 0; i < bytes_size && j < encoded_size; i++){
    uint8_t mask = byte_mask;
    int b;
    for(b = 0; b < bit_factor; b++, j++){
      unsigned shift = (bit_factor - (b+1)) * bits;
      uint8_t uint8_t = (bytes[i] & mask) >> shift;
      encoded[j] = (encoded[j] & ~encode_mask) | uint8_t; 
      mask >>= bits; 
    } 
  }
}

static int write_strip(uint32_t strip_offset, uint32_t strip_bytes, 
                       uint32_t size, uint32_t byte_count, unsigned bits,
                       FILE *in, FILE *img, FILE *out){
  if(fseek(img, strip_offset, SEEK_SET) < 0){
    fprintf(stderr, "error seeking strip offset\n");
    exit(EXIT_FAILURE);
  }
  int bit_factor = 8/bits;
  uint8_t *bytes = (uint8_t*)malloc(bit_factor);
  memset(bytes, 0, bit_factor);
  uint8_t byte;
  int i;                                   
  for(i = 0; i < strip_bytes && byte_count < size; 
                            i += bit_factor, byte_count++){
    if(fread((void*)&byte, sizeof(uint8_t), 1, in) < 1){// read uint8_t
      fprintf(stderr, "error reading input data\n");
      exit(EXIT_FAILURE);
    }
    if(fread((void*)bytes, bit_factor, 1, img) < 1){// read bit_factor # bytes
      fprintf(stderr, "error reading image data\n");
      exit(EXIT_FAILURE);
    }
    encode(bytes, bit_factor, bits, (uint8_t*)&byte, 1);
    if(img == out){
      if(fseek(out, -bit_factor, SEEK_CUR) < 0){
       fprintf(stderr, "error resetting position\n");
       exit(EXIT_FAILURE);
      }
    }
    if(fwrite((void*)bytes, bit_factor, 1, out) < 1){
      fprintf(stderr, "error encoding image\n");
      exit(EXIT_FAILURE);
    }
  }
  free(bytes);
  return byte_count;
}

static void write_data(FILE *in, FILE *img, FILE *out, unsigned bits, tif_data data){
  if(data.strip_count == 0){
    fprintf(stderr, "image has no data\n");
    exit(EXIT_FAILURE);
  }
  printf("strip count: %d\n", data.strip_count);
  printf("strip offset[0]: %X\n", data.strip_offsets[0]);
  printf("strip bytes[0]: %X\n", data.strip_bytes[0]);
  printf("strip offset[1]: %X\n", data.strip_offsets[1]);
  printf("strip bytes[1]: %X\n", data.strip_bytes[1]);
  exit(1);
  if(fseek(img, data.strip_offsets[0], SEEK_SET) < 0){
    fprintf(stderr, "error reading image data\n");
    exit(EXIT_FAILURE);
  }  
  fseek(in, 0, SEEK_END);
  unsigned size = ftell(in); 
  fseek(in, 0, SEEK_SET); 
  int bit_factor = 8/bits;
  int encoded_size = sizeof(unsigned)*bit_factor;
  uint8_t *encoded = (uint8_t*)malloc(encoded_size);
  if(fread((void*)encoded, encoded_size, 1, img) < 1){
    fprintf(stderr, "error reading data\n");
    exit(EXIT_FAILURE);
  }
  encode(encoded, encoded_size, bits, 
        (uint8_t*)&size, sizeof(unsigned));
  if(img == out){
    if(fseek(out, -encoded_size, SEEK_CUR) < 0){
      fprintf(stderr, "error resetting position\n");
      exit(EXIT_FAILURE);
    }
  }
  if(fwrite((void*)encoded, encoded_size, 1, out) < 1){
    fprintf(stderr, "error encoding file size\n");
    exit(EXIT_FAILURE);
  }
  data.strip_bytes[0] -= encoded_size;
  data.strip_offsets[0] += encoded_size;
  unsigned byte_count = 0;
  int i;
  for(i = 0; i < data.strip_count && byte_count < size; i++){
    byte_count = write_strip(data.strip_offsets[i], 
                  data.strip_bytes[i], 
                  size, byte_count, bits,
                  in, img, out);
  }
  free(encoded);
}


static int get_useable_size(tif_data data, int bits){
  int bit_factor = 8/bits;
  int useable_size = 0;
  int i;
  for(i = 0; i < data.strip_count; i++){
    useable_size += data.strip_bytes[i]/bit_factor;
  }
  return useable_size; 
}

void tif_read_data(char *filename_img, char *filename_out, int bits){
  FILE *img, *out;
  if(!(img = fopen(filename_img, "rb"))){ 
    perror("error opening input image");
    exit(EXIT_FAILURE);
  }
  if(!(out = fopen(filename_out, "wb"))){ 
    perror("error opening output file");
    exit(EXIT_FAILURE);
  }
  tif_data data;
  get_data(filename_img, &data);
  if(data.compression != 1){
    fprintf(stderr, "only uncompressed images supported\n");
    exit(EXIT_FAILURE);
  }
  read_data(img, out, 1, data);
  free_data(&data);
  fclose(img);
  fclose(out);
}

void tif_write_data(char *filename_in, char *filename_img, int bits){
  tif_data data;
  get_data(filename_img, &data);
  if(data.compression != 1){
    fprintf(stderr, "only uncompressed images supported\n");
    //exit(EXIT_FAILURE);
  }
  FILE *in, *img;
  if(!(in = fopen(filename_in, "rb"))){
    fprintf(stderr, "error opening input file\n");
    exit(EXIT_FAILURE);
  }
  if(!(img = fopen(filename_img, "rb+"))){
    fprintf(stderr, "error opening image file\n");
    exit(EXIT_FAILURE);
  }
  fseek(in, 0, SEEK_END);
  unsigned size = ftell(in); 
  fseek(in, 0, SEEK_SET); 
  if(size == 0){
    fprintf(stderr, "%s is empty\n", filename_in);
    exit(EXIT_FAILURE);
  }
  int useable_size = get_useable_size(data, bits);
  if(size > useable_size){
    fprintf(stderr, 
        "%s has only %u useable bytes for a %u bit encoding. %u are required.\n", 
        filename_img, useable_size, bits, size);
    exit(EXIT_FAILURE);
  }
  write_data(in, img, img, bits, data);
  free_data(&data);
  fclose(in);
  fclose(img);
}
