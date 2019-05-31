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

static int read_strip(DWORD strip_offset, DWORD strip_bytes, 
                      DWORD size, DWORD byte_count,
                      FILE *in, FILE *out){
  if(fseek(in, strip_offset, SEEK_SET) < 0){
    fprintf(stderr, "error seeking strip offset\n");
    exit(EXIT_FAILURE);
  }
  BYTE bytes[2];
  memset(bytes, 0, sizeof(bytes));
  int i;
  for(i = 0; i < strip_bytes && byte_count < size; i += 2, byte_count++){
    if(fread((void*)bytes, sizeof(bytes), 1, in) < 1){
      fprintf(stderr, "error reading data\n");
      exit(EXIT_FAILURE);
    }
    BYTE byte = (bytes[0] & 0xF) << 4;
    byte |= (bytes[1] & 0xF);
    if(fwrite((void*)&byte, sizeof(BYTE), 1, out) < 1){
      fprintf(stderr, "error updating image\n");
      exit(EXIT_FAILURE);
    }
  }
  return byte_count;
}

static void read_data_(FILE *img, FILE *out, TIFDATA data){
  if(data.strip_count == 0){
    fprintf(stderr, "image has no data\n");
    exit(EXIT_FAILURE);
  }
  if(fseek(img, data.strip_offsets[0], SEEK_SET) < 0){
    fprintf(stderr, "error reading image data\n");
    exit(EXIT_FAILURE);
  } 
  BYTE encoded[8];
  if(fread((void*)encoded, sizeof(encoded), 1, img) < 1){
    fprintf(stderr, "error reading data\n");
    exit(EXIT_FAILURE);
  }
  //TODO call decode function  


  fseek(in, 0, SEEK_END);
  unsigned size = ftell(in); 
  fseek(in, 0, SEEK_SET); 
  BYTE encoded[8];
  if(fread((void*)encoded, sizeof(encoded), 1, img) < 1){
    fprintf(stderr, "error reading data\n");
    exit(EXIT_FAILURE);
  }
  encode(encoded, sizeof(encoded), 4, 
        (BYTE*)&size, sizeof(unsigned));
  if(img == out){
    if(fseek(out, -sizeof(encoded), SEEK_CUR) < 0){
      fprintf(stderr, "error resetting position\n");
      exit(EXIT_FAILURE);
    }
  }
  if(fwrite((void*)encoded, sizeof(encoded), 1, out) < 1){
    fprintf(stderr, "error encoding file size\n");
    exit(EXIT_FAILURE);
  }
  data.strip_bytes[0] -= 8;
  data.strip_offsets[0] += 8;
  printf("offset: %X\n", data.strip_offsets[0]);
  printf("size: %X, %d\n", size, size);
  printf("strip count: %d\n", data.strip_count);
  unsigned byte_count = 0;
  int i;
  for(i = 0; i < data.strip_count && byte_count < size; i++){
    byte_count = write_strip(data.strip_offsets[i], 
                  data.strip_bytes[i], 
                  size, byte_count,
                  in, img, out);
  }
  printf("bytes written: %u\n", byte_count);
}


static int write_strip(DWORD strip_offset, DWORD strip_bytes, 
                       DWORD size, DWORD byte_count,
                       FILE *in, FILE *img, FILE *out){
  printf("write strip: %d\n", size);
  if(fseek(img, strip_offset, SEEK_SET) < 0){
    fprintf(stderr, "error seeking strip offset\n");
    exit(EXIT_FAILURE);
  }
  BYTE bytes[2];// bit factor # bytes
  memset(bytes, 0, sizeof(bytes));
  BYTE byte;
  int i;                                        // i += bit_factor
  for(i = 0; i < strip_bytes && byte_count < size; i += 2, byte_count++){
    if(fread((void*)&byte, sizeof(BYTE), 1, in) < 1){// read byte
      fprintf(stderr, "error reading input data\n");
      exit(EXIT_FAILURE);
    }
    if(fread((void*)bytes, sizeof(bytes), 1, img) < 1){// read bit_factor # bytes
      fprintf(stderr, "error reading image data\n");
      exit(EXIT_FAILURE);
    }// call encode function
    bytes[0] = (bytes[0] & 0xF0) | ((byte & 0xF0) >> 4);
    bytes[1] = (bytes[1] & 0xF0) | (byte & 0xF);
    if(img == out){
      if(fseek(out, -sizeof(bytes), SEEK_CUR) < 0){
       fprintf(stderr, "error resetting position\n");
       exit(EXIT_FAILURE);
      }
    }
    if(fwrite((void*)bytes, sizeof(bytes), 1, out) < 1){
      fprintf(stderr, "error encoding image\n");
      exit(EXIT_FAILURE);
    }
  }
  printf("\tbytes encoded: %d\n", byte_count);
  return byte_count;
}

static void encode(BYTE *encoded, unsigned encoded_size, unsigned bits, 
                   BYTE *bytes, unsigned bytes_size){
  if(bits == 0 || 8 % bits != 0){
    fprintf(stderr, "bits to encode must be powers of 2 <= 8\n");
    exit(EXIT_FAILURE);
  }
  unsigned bit_factor = (8/bits);
  if(encoded_size != bytes_size * bit_factor){
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

static void write_data_(FILE *in, FILE *img, FILE *out, TIFDATA data){
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
  BYTE encoded[8];
  if(fread((void*)encoded, sizeof(encoded), 1, img) < 1){
    fprintf(stderr, "error reading data\n");
    exit(EXIT_FAILURE);
  }
  encode(encoded, sizeof(encoded), 4, 
        (BYTE*)&size, sizeof(unsigned));
  if(img == out){
    if(fseek(out, -sizeof(encoded), SEEK_CUR) < 0){
      fprintf(stderr, "error resetting position\n");
      exit(EXIT_FAILURE);
    }
  }
  if(fwrite((void*)encoded, sizeof(encoded), 1, out) < 1){
    fprintf(stderr, "error encoding file size\n");
    exit(EXIT_FAILURE);
  }
  data.strip_bytes[0] -= 8;
  data.strip_offsets[0] += 8;
  printf("offset: %X\n", data.strip_offsets[0]);
  printf("size: %X, %d\n", size, size);
  printf("strip count: %d\n", data.strip_count);
  unsigned byte_count = 0;
  int i;
  for(i = 0; i < data.strip_count && byte_count < size; i++){
    byte_count = write_strip(data.strip_offsets[i], 
                  data.strip_bytes[i], 
                  size, byte_count,
                  in, img, out);
  }
  printf("bytes written: %u\n", byte_count);
}





void tif_read_data(char *filename_in, char *filename_out){
 
  printf("reading data\n");
  FILE *finptr, *foutptr;
  if(!(finptr = fopen(filename_in, "rb"))){ 
    perror("error opening input image");
    exit(EXIT_FAILURE);
  }
  if(!(foutptr = fopen(filename_out, "wb"))){ 
    perror("error opening output file");
    exit(EXIT_FAILURE);
  }
  TIFDATA data;
  get_data(filename_in, &data);
  printf("length: %d\n", data.length);
  printf("width: %d\n", data.width);
  printf("bits per sample: %d\n", data.bits_per_sample);
  printf("strip count: %d\n", data.strip_count);
  printf("rows per strip: %d\n", data.rows_per_strip);
  printf("offsets:\n");
  int i;//int i;
  for(i = 0; i < data.strip_count; i++){
    printf("[%d]: %X\n", i, data.strip_offsets[i]);
  }
  printf("\nstrip bytes:\n");
  for(i = 0; i < data.strip_count; i++){
    printf("[%d]: %X\n", i, data.strip_bytes[i]);
  }
  printf("in read func\n");
  DWORD j, nibble_count, size; //, size
  for(i = 0, nibble_count = 0; i < data.strip_count 
      && nibble_count < size * 2; i++){
    DWORD strip_offset = data.strip_offsets[i];
    DWORD strip_bytes = data.strip_bytes[i];
    BYTE byte, decoded;
    if(i == 0){
      if(fseek(finptr, strip_offset, SEEK_SET) < 0){
       fprintf(stderr, "error reading data\n");
       exit(EXIT_FAILURE);
      }
    }
    if(i == 0){
      BYTE bytes[4];
      memset((void*)&bytes, 0, sizeof(bytes)); 
      printf("reading size\n");
      for(nibble_count = 0; nibble_count < sizeof(DWORD) * 2; nibble_count++){
        if(fread((void*)&byte, sizeof(BYTE), 1, finptr) < 1){
          fprintf(stderr, "error reading data\n");
          exit(EXIT_FAILURE);
        }
        if(nibble_count % 2 == 0){
          BYTE nibble = (byte & 0xF) << 4;
          bytes[nibble_count/2] |= nibble;
        }
        else{
          BYTE nibble = byte & 0xF;
          bytes[nibble_count/2] |= nibble;
        }
        printf("byte read: %X\n", byte);
      }
      nibble_count = 0;
      size = *((DWORD*)bytes);
      printf("bytes [0]: %X, [1]: %X,[2]: %X, [3]: %X\n", bytes[0], bytes[1], bytes[2], bytes[3]);
      printf("%u, %X\n", size, size);
    }
    for(j = 0; j < strip_bytes 
        && nibble_count < size * 2; j++, nibble_count++){
      if(fread((void*)&byte, sizeof(BYTE), 1, finptr) < 1){
        fprintf(stderr, "error reading data\n");
        exit(EXIT_FAILURE);
      }
      if(nibble_count % 2 == 0){
        decoded = 0;
        BYTE nibble = (byte & 0xF) << 4;
        decoded |= nibble;
      }
      else{
        BYTE nibble = byte & 0xF;
        decoded |= nibble;
        if(fwrite((void*)&decoded, sizeof(BYTE), 1, foutptr) < 1){
          fprintf(stderr, "error updating image\n");
          exit(EXIT_FAILURE);
        }
      }
    }     
  }
}

static void write_data(FILE *foutptr, FILE *finptr, TIFDATA data){

  write_data_(finptr, foutptr, foutptr, data);

  return;

  fseek(finptr, 0, SEEK_END); // seek to end of file
  DWORD size = ftell(finptr); // get current file pointer
  fseek(finptr, 0, SEEK_SET); // seek back to beginning of file
  printf("size: %X\n", size);
  DWORD i, j, nibble_count;
  for(i = 0, nibble_count = 0; i < data.strip_count 
      && nibble_count < size * 2; i++){
    DWORD strip_offset = data.strip_offsets[i];
    DWORD strip_bytes = data.strip_bytes[i];
    BYTE byte, encoded;
    if(i == 0){
      if(fseek(foutptr, strip_offset, SEEK_SET) < 0){
       fprintf(stderr, "error reading data\n");
       exit(EXIT_FAILURE);
      }
    }
    if(i == 0){
      BYTE bytes[4];
      bytes[0] =  size & 0xFF;
      bytes[1] = (size >> 8)  & 0xFF;
      bytes[2] = (size >> 16) & 0xFF;
      bytes[3] = (size >> 24) & 0xFF;
      for(nibble_count = 0; nibble_count < sizeof(DWORD) * 2; nibble_count++){
        byte = 0;
        if(fread((void*)&byte, sizeof(BYTE), 1, foutptr) < 1){
          fprintf(stderr, "error reading data\n");
          exit(EXIT_FAILURE);
        }
        if(nibble_count % 2 == 0){
          BYTE nibble = (bytes[nibble_count/2] & 0xF0) >> 4;
          byte = (byte & 0xF0) | nibble;
        }
        else{
          BYTE nibble = (bytes[nibble_count/2] & 0x0F);
          byte = (byte & 0xF0) | nibble;        
        }
        if(fseek(foutptr, -sizeof(BYTE), SEEK_CUR) < 0){
          fprintf(stderr, "error seeking\n");
          exit(EXIT_FAILURE);
        }
        if(fwrite((void*)&byte, sizeof(BYTE), 1, foutptr) < 1){
          fprintf(stderr, "error updating image\n");
          exit(EXIT_FAILURE);
        }
      }
      nibble_count = 0;
    }
    for(j = 0; j < strip_bytes 
        && nibble_count < size * 2; j++, nibble_count++){
      byte = 0;
      if(fread((void*)&byte, sizeof(BYTE), 1, foutptr) < 1){
        fprintf(stderr, "error reading data\n");
        exit(EXIT_FAILURE);
      }
      BYTE orig_byte = byte;
      if(nibble_count % 2 == 0){
        encoded = 0;
        if(fread((void*)&encoded, sizeof(BYTE), 1, finptr) < 1){
          fprintf(stderr, "error reading data\n");
          exit(EXIT_FAILURE);
        }
        DWORD pos = ftell(finptr);
        if(pos - 1 == 0x59FFC){
          printf("original byte read: %X\n", orig_byte);
          printf("encoded: %X\n", encoded);
          printf("image pos: %X\n", ftell(foutptr) - 1);
        }
        BYTE nibble = (encoded & 0xF0) >> 4;
        byte = (byte & 0xF0) | nibble;
      }
      else{
        DWORD pos = ftell(finptr);
        if(pos - 1 == 0x59FFC){
          printf("original byte read: %X\n", orig_byte);
          printf("encoded: %X\n", encoded);
          printf("image pos: %X\n", ftell(foutptr) - 1);
        }
        BYTE nibble = (encoded & 0x0F);
        byte = (byte & 0xF0) | nibble;        
      }
      if(fseek(foutptr, -sizeof(BYTE), SEEK_CUR) < 0){
        fprintf(stderr, "error seeking\n");
        exit(EXIT_FAILURE);
      }
      DWORD pos = ftell(finptr);
      if(pos - 1 == 0x59FFC){
       printf("fully encoded byte: %X\n", byte);
       printf("written to: %X\n", ftell(foutptr));
      }
      DWORD before_pos = ftell(foutptr);
      if(before_pos == 0xB4008){
        printf("writing byte: %X to address: %X\n", byte, before_pos);
      }
      if(fwrite((void*)&byte, sizeof(BYTE), 1, foutptr) < 1){
        fprintf(stderr, "error updating image\n");
        exit(EXIT_FAILURE);
      }
      DWORD after_pos = ftell(foutptr);
      if(before_pos == after_pos){
        printf("excuse me wtf\n");
      }
    }     
  }
}

char *tif_get_data(char *filename){
  /*
  TIFDATA data;
  get_data(filename, &data);
  printf("length: %d\n", data.length);
  printf("width: %d\n", data.width);
  printf("bits per sample: %d\n", data.bits_per_sample);
  printf("strip count: %d\n", data.strip_count);
  printf("rows per strip: %d\n", data.rows_per_strip);
  printf("offsets:\n");
  int i;
  for(i = 0; i < data.strip_count; i++){
    printf("[%d]: %X\n", i, data.strip_offsets[i]);
  }
  printf("\nstrip bytes:\n");
  for(i = 0; i < data.strip_count; i++){
    printf("[%d]: %X\n", i, data.strip_bytes[i]);
  }
  free_data(&data);
  */
  TIFHDR hdr;
  TIFDATA data;
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
        data.length = tag.data_offset;
      }
      break;
      case IMAGE_WIDTH:{
        data.width = tag.data_offset;
      }
      break;
      case ROWSPERSTRIP:{
        data.rows_per_strip = tag.data_offset; 
      }
      break;
      case STRIP_OFFSETS:{
        read_strip_offsets(fptr, &data, tag);
      }
      break;
      case STRIP_BYTES:{
        read_strip_bytes(fptr, &data, tag);
      }
      break;
      case BITSPERSAMPLE:{
        read_bits_per_sample(fptr, &data, tag);
        printf("bits per sample: %d\n", data.bits_per_sample);
      }
      break;
    }
    if(fseek(fptr, pos, SEEK_SET) < 0){
      fprintf(stderr, "error reading tag data\n");
      exit(EXIT_FAILURE);
    }
  }
  FILE *finptr = fopen("The-CodeBreakers.zip", "rb");
  if(!finptr){
    fprintf(stderr, "file not found\n");
    exit(EXIT_FAILURE);
  }
  write_data(fptr,finptr, data);
  free_data(&data);
  fclose(fptr);
  
  return NULL;
}
