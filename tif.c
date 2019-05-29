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

static void write_data(FILE *foutptr, FILE *finptr, TIFDATA data){
  fseek(finptr, 0, SEEK_END); // seek to end of file
  unsigned long size = ftell(finptr); // get current file pointer
  fseek(finptr, 0, SEEK_SET); // seek back to beginning of file
  BYTE mask = 0x0F;
  DWORD i, j, nibble_count;
  for(i = 0, nibble_count = 0; i < data.strip_count 
      && nibble_count < size * 2; i++){
    DWORD strip_offset = data.strip_offsets[i];
    DWORD strip_bytes = data.strip_bytes[i];
    if(fseek(foutptr, strip_offset, SEEK_SET) < 0){
      fprintf(stderr, "error reading data\n");
      exit(EXIT_FAILURE);
    }
    BYTE encoded;
    for(j = 0; j < strip_bytes 
        && nibble_count < size * 2; j++, nibble_count++){
      BYTE byte;
      if(fread((void*)&byte, sizeof(BYTE), 1, foutptr) < 1){
        fprintf(stderr, "error reading data\n");
        exit(EXIT_FAILURE);
      }
      if(nibble_count % 2 == 0){
        if(fread((void*)&encoded, sizeof(BYTE), 1, finptr) < 1){
          fprintf(stderr, "error reading data\n");
          exit(EXIT_FAILURE);
        }
        BYTE nibble = (encoded & 0xF0) >> 4;
        byte = (byte & 0xF0) | nibble;
      }
      else{
        BYTE nibble = (encoded & 0x0F);
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
  }
}

char *tif_get_data(char *filename){
  TIFHDR hdr;
  TIFDATA data;
  read_hdr(filename, &hdr);
  FILE *fptr = NULL;
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
  FILE *finptr = fopen("war_and_peace.zip", "rb");
  if(!finptr){
    fprintf(stderr, "file not found\n");
    exit(EXIT_FAILURE);
  }
  write_data(fptr,finptr, data);
  free_data(&data);
  fclose(fptr);
  return NULL;
}
