#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tif.h"

typedef uint8_t  BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;

enum TAG_NAME{
  IMAGE_WIDTH     = 256,  /* image width in pixels */
  IMAGE_LENGTH    = 257,  /* image length in pixels */
  BITSPERSAMPLE   = 258,  /**/
  STRIP_OFFSETS   = 273,  /* offsets for strips */
  SAMPLESPERPIXEL = 277,  /**/
  ROWSPERSTRIP    = 278,  /* rows of contiguous data per strip*/
  STRIP_BYTES     = 279   /**/
};

/* TIFF header format */
typedef struct _tif_hdr{ 
  WORD id;          /* byte order identifier */
  WORD version;     /* TIFF version (0x2a) */
  DWORD ifd_offset;  /* offset of image file directory */
}TIFHDR;

/* TIFF tag format */
typedef struct _tif_tag{ 
  WORD  tag_id;      /* tag identifier*/
  WORD  type;        /* scalar type of data */
  DWORD data_count;  /* number of items in data */
  DWORD data_offset; /* byte offset of data */
}TIFTAG;

/* TIFF data information */
typedef struct _tif_data{ 
  DWORD  width;          /* image width in pixels */
  DWORD  length;         /* image length in pixels */
  DWORD  rows_per_strip; /* rows per strip of data */
  DWORD *strip_bytes;    /* size of each strip in bytes */
  DWORD *strip_offsets;  /* strip offsets for data */
  WORD   strip_count;    /* count for strips of data*/
  WORD   bits_per_sample;/* bit depth of the image */
}TIFDATA;



static void free_data(TIFDATA *dataptr){
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

static void read_hdr(char *filename, TIFHDR *hdrptr){ 
  FILE *fptr = NULL;
  if(!(fptr = fopen(filename, "rb"))){ 
    perror("error opening image");
    exit(EXIT_FAILURE);
  }
  if(fread((void*)hdrptr, sizeof(TIFHDR), 1, fptr) < 1){ 
    fprintf(stderr, "error reading image header\n");
    exit(EXIT_FAILURE);
  }
  fclose(fptr);
}

static void read_bits_per_sample(FILE *fptr, TIFDATA *dataptr, TIFTAG tag){
  if(!dataptr){
    fprintf(stderr, "invalid data argument\n");
    exit(EXIT_FAILURE);
  }
  if(fseek(fptr, tag.data_offset, SEEK_SET) < 0){
    fprintf(stderr, "error reading bits per sample\n");
    exit(EXIT_FAILURE);
  }
  if(fread((void*)&(dataptr->bits_per_sample), 
           sizeof(WORD), 1, fptr) < 1){
    fprintf(stderr, "error reading bits per sample\n");
    exit(EXIT_FAILURE);
  }
}

static void read_strip_bytes(FILE *fptr, TIFDATA *dataptr, TIFTAG tag){
  if(!dataptr){
    fprintf(stderr, "invalid data argument\n");
    exit(EXIT_FAILURE);
  }
  dataptr->strip_bytes = (DWORD*)malloc(sizeof(DWORD)*tag.data_count);
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
      memset((void*)&(dataptr->strip_bytes[i]), 0, sizeof(DWORD));
      if(fread((void*)&(dataptr->strip_bytes[i]), 
               bytes, 1, fptr) < 1){
        fprintf(stderr, "error reading strip offset bytes\n");
        exit(EXIT_FAILURE);
      }
    }
  }  
} 

static void read_strip_offsets(FILE *fptr, TIFDATA *dataptr, TIFTAG tag){
  if(!dataptr){
    fprintf(stderr, "invalid data argument\n");
    exit(EXIT_FAILURE);
  }
  dataptr->strip_count = tag.data_count;
  dataptr->strip_offsets = (DWORD*)malloc(sizeof(DWORD)*tag.data_count);
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
      memset((void*)&(dataptr->strip_offsets[i]), 0, sizeof(DWORD));
      if(fread((void*)&(dataptr->strip_offsets[i]), 
               bytes, 1, fptr) < 1){
        fprintf(stderr, "error reading strip offset\n");
        exit(EXIT_FAILURE);
      }
    }
  }
}

static void get_data(char *filename, TIFDATA *dataptr){
  TIFHDR hdr;
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
  WORD n_entries;
  if(fread((void*)&n_entries, sizeof(WORD), 1, fptr) < 1){
    fprintf(stderr, "error reading directory entries\n");
    exit(EXIT_FAILURE);
  }
  int i;
  for(i = 0; i < n_entries; i++){
    TIFTAG tag;
    if(fread((void*)&tag, sizeof(TIFTAG), 1, fptr) < 1){
      fprintf(stderr, "error reading tag\n");
      exit(EXIT_FAILURE);
    }
    long pos = ftell(fptr); 
    switch(tag.tag_id){
      case IMAGE_LENGTH:{
        dataptr->length = tag.data_offset;
      }
      break;
      case IMAGE_WIDTH:{
        dataptr->width = tag.data_offset;
      }
      break;
      case ROWSPERSTRIP:{
        dataptr->rows_per_strip = tag.data_offset; 
      }
      break;
      case STRIP_OFFSETS:{
        read_strip_offsets(fptr, dataptr, tag);
      }
      break;
      case STRIP_BYTES:{
        read_strip_bytes(fptr, dataptr, tag);
      }
      break;
      case BITSPERSAMPLE:{
        read_bits_per_sample(fptr, dataptr, tag);
      }
      break;
    }
    if(fseek(fptr, pos, SEEK_SET) < 0){
      fprintf(stderr, "error reading tag data\n");
      exit(EXIT_FAILURE);
    }
  }
  fclose(fptr);
}

static void decode(BYTE *decoded, unsigned decoded_size, unsigned bits, 
                   BYTE *bytes, unsigned bytes_size){
  if(bits == 0 || 8 % bits != 0){
    fprintf(stderr, "bits to decode must be powers of 2 <= 8\n");
    exit(EXIT_FAILURE);
  }
  unsigned bit_factor = 8/bits;
  if(bytes_size != decoded_size * bit_factor){
    fprintf(stderr, "invalid bytes for bit factor\n");
    exit(EXIT_FAILURE);
  }
  BYTE byte_mask = 0x80;
  BYTE decode_mask = 0x01;
  int b;
  for(b = 1; b < bits; b++){
    byte_mask |= byte_mask >> 1;
    decode_mask |= decode_mask << 1;
  }
  memset((void*)decoded, 0, decoded_size);
  int i, j;
  for(i = 0, j = 0; i < decoded_size && j < bytes_size; i++){
    BYTE mask = byte_mask;
    int b;
    for(b = 0; b < bit_factor; b++, j++){
      unsigned shift = (bit_factor - (b+1))*bits;
      BYTE byte = (bytes[j] & decode_mask) << shift;
      decoded[i] = decoded[i] | byte; 
      mask >>= bit_factor;
    } 
  }
}

static int read_strip(DWORD strip_offset, DWORD strip_bytes, 
                      DWORD size, DWORD byte_count, unsigned bits,
                      FILE *img, FILE *out){
  if(fseek(img, strip_offset, SEEK_SET) < 0){
    fprintf(stderr, "error seeking strip offset\n");
    exit(EXIT_FAILURE);
  }
  int bit_factor = 8/bits;
  BYTE *bytes = (BYTE*)malloc(bit_factor);
  memset((void*)bytes, 0, bit_factor);
  BYTE byte;
  int i;
  for(i = 0; i < strip_bytes && byte_count < size; 
                i += bit_factor, byte_count++){
    if(fread((void*)bytes, bit_factor, 1, img) < 1){
      fprintf(stderr, "error reading data\n");
      exit(EXIT_FAILURE);
    }
    decode((BYTE*)&byte, 1, bits, bytes, bit_factor); 
    if(fwrite((void*)&byte, sizeof(BYTE), 1, out) < 1){
      fprintf(stderr, "error updating image\n");
      exit(EXIT_FAILURE);
    }
  }
  free(bytes);
  return byte_count;
}

static void read_data(FILE *img, FILE *out, unsigned bits, TIFDATA data){ 
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
  BYTE *encoded = (BYTE*)malloc(encoded_size);
  if(fread((void*)encoded, encoded_size, 1, img) < 1){
    fprintf(stderr, "error reading data\n");
    exit(EXIT_FAILURE);
  }
  unsigned size;
  decode((BYTE*)&size, sizeof(size), bits, encoded, encoded_size);
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
}


static void encode(BYTE *encoded, unsigned encoded_size, unsigned bits, 
                   BYTE *bytes, unsigned bytes_size){
  if(bits == 0 || 8 % bits != 0){
    fprintf(stderr, "bits to encode must be powers of 2 <= 8\n");
    exit(EXIT_FAILURE);
  }
  unsigned bit_factor = 8/bits;
  if(encoded_size != bytes_size * bit_factor){// sizes for encoding data
    fprintf(stderr, "invalid bytes for bit factor\n");
    exit(EXIT_FAILURE);
  }
  BYTE byte_mask = 0x80;
  BYTE encode_mask = 0x01;
  int b;
  for(b = 1; b < bits; b++){
    byte_mask |= byte_mask >> 1;
    encode_mask |= encode_mask << 1;
  }
  int i, j;
  for(i = 0, j = 0; i < bytes_size && j < encoded_size; i++){
    BYTE mask = byte_mask;
    int b;
    for(b = 0; b < bit_factor; b++, j++){
      unsigned shift = (bit_factor - (b+1)) * bits;
      BYTE byte = (bytes[i] & mask) >> shift;
      encoded[j] = (encoded[j] & ~encode_mask) | byte; 
      mask >>= bits; 
    } 
  }
}

static int write_strip(DWORD strip_offset, DWORD strip_bytes, 
                       DWORD size, DWORD byte_count, unsigned bits,
                       FILE *in, FILE *img, FILE *out){
  if(fseek(img, strip_offset, SEEK_SET) < 0){
    fprintf(stderr, "error seeking strip offset\n");
    exit(EXIT_FAILURE);
  }
  int bit_factor = 8/bits;
  BYTE *bytes = (BYTE*)malloc(bit_factor);
  memset(bytes, 0, bit_factor);
  BYTE byte;
  int i;                                   
  for(i = 0; i < strip_bytes && byte_count < size; 
                            i += bit_factor, byte_count++){
    if(fread((void*)&byte, sizeof(BYTE), 1, in) < 1){// read byte
      fprintf(stderr, "error reading input data\n");
      exit(EXIT_FAILURE);
    }
    if(fread((void*)bytes, bit_factor, 1, img) < 1){// read bit_factor # bytes
      fprintf(stderr, "error reading image data\n");
      exit(EXIT_FAILURE);
    }
    encode(bytes, bit_factor, bits, (BYTE*)&byte, 1);
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

static void write_data(FILE *in, FILE *img, FILE *out, unsigned bits, TIFDATA data){
  if(data.strip_count == 0){
    fprintf(stderr, "image has no data\n");
    exit(EXIT_FAILURE);
  }
  if(fseek(img, data.strip_offsets[0], SEEK_SET) < 0){
    fprintf(stderr, "error reading image data\n");
    exit(EXIT_FAILURE);
  }  
  fseek(in, 0, SEEK_END);
  unsigned size = ftell(in); 
  fseek(in, 0, SEEK_SET); 
  int bit_factor = 8/bits;
  int encoded_size = sizeof(unsigned)*bit_factor;
  BYTE *encoded = (BYTE*)malloc(encoded_size);
  if(fread((void*)encoded, encoded_size, 1, img) < 1){
    fprintf(stderr, "error reading data\n");
    exit(EXIT_FAILURE);
  }
  encode(encoded, encoded_size, bits, 
        (BYTE*)&size, sizeof(unsigned));
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


void tif_read_data(char *filename_img, char *filename_out){
  FILE *img, *out;
  if(!(img = fopen(filename_img, "rb"))){ 
    perror("error opening input image");
    exit(EXIT_FAILURE);
  }
  if(!(out = fopen(filename_out, "wb"))){ 
    perror("error opening output file");
    exit(EXIT_FAILURE);
  }
  TIFDATA data;
  get_data(filename_img, &data);
  read_data(img, out, 1, data);
  free_data(&data);
  fclose(img);
  fclose(out);
}

void tif_write_data(char *filename_in, char *filename_img){
  TIFDATA data;
  get_data(filename_img, &data);
  FILE *in, *img;
  if(!(in = fopen(filename_in, "rb"))){
    fprintf(stderr, "error opening input file\n");
    exit(EXIT_FAILURE);
  }
  if(!(img = fopen(filename_img, "rb+"))){
    fprintf(stderr, "error opening image file\n");
    exit(EXIT_FAILURE);
  }
  write_data(in, img, img, 1, data);
  free_data(&data);
  fclose(in);
  fclose(img);
}
