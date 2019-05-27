#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "tif.h"

#define WORD  uint16_t
#define DWORD uint32_t

/* TIFF header format */
typedef struct _tif_hdr{ 
  WORD id;          /* byte order identifier*/
  WORD version;     /* TIFF version (0x2a) */
  WORD ifd_offset;  /* offset of image file directory*/
}TIFHDR;

/* TIFF tag format */
typedef struct _tif_tag{ 
  WORD  tag_id;     /* tag identifier*/
  WORD  type;       /* scalar type of data */
  DWORD data_count; /* number of items in data */
  DWORD data_offset;/* byte offset of data */
}TIFTAG;

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

char *tif_get_data(char *filename){
  TIFHDR hdr;
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
    printf("tag id: %d\n", tag.tag_id);
    printf("type: %d\n", tag.type);
    printf("data count: %d\n", tag.data_count);
    printf("data offset: %X\n", tag.data_offset);
  }
  return NULL;
}
