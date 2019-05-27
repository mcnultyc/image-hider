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
  WORD ifd_offset;  /* offset of image file directory */
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
}TIFDATA;


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

char *tif_get_data(char *filename){
  TIFHDR hdr;
  TIFDATA data;
  read_hdr(filename, &hdr);
  FILE *fptr = NULL;
  if(!(fptr = fopen(filename, "rb"))){ 
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
        printf("length: %d\n", data.length);
      }
      break;
      case IMAGE_WIDTH:{
        data.width = tag.data_offset;
        printf("width: %d\n", data.width);
      }
      break;
      case ROWSPERSTRIP:{
        data.rows_per_strip = tag.data_offset;
        printf("rows: %u\n", data.rows_per_strip); 
      }
      break;
      case STRIP_OFFSETS:{
        read_strip_offsets(fptr, &data, tag);
        printf("strip offsets:\n");
        int i;
        for(i = 0; i < data.strip_count; i++){
          printf("[%d]: %X\n",i, data.strip_offsets[i]);
        }
      }
      break;
      case STRIP_BYTES:{
        read_strip_bytes(fptr, &data, tag);
        printf("strip bytes:\n");
        int i;
        for(i =0; i < data.strip_count; i++){
          printf("[%d]: %d\n",i, data.strip_bytes[i]);
        }
      }
      break;
    }
    printf("id: %d\n", tag.tag_id);
    printf("scalar type: %d\n", tag.type);
    printf("data count: %d\n", tag.data_count);
    printf("data offset: %X\n", tag.data_offset);
    printf("-----------------------\n");
    if(fseek(fptr, pos, SEEK_SET) < 0){
      fprintf(stderr, "error reading tag data\n");
      exit(EXIT_FAILURE);
    }
  }
  return NULL;
}
