/*
 */

#include "tif.h"

/* */
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
  uint16_t id;          /* byte order identifier */
  uint16_t version;     /* TIFF version (0x2a) */
  uint32_t ifd_offset;  /* offset of image file directory */
}tif_hdr;

/* TIFF tag format */
typedef struct{ 
  uint16_t  tag_id;      /* tag identifier */
  uint16_t  type;        /* scalar type of data */
  uint32_t  data_count;  /* number of items in data */
  uint32_t  data_offset; /* byte offset of data */
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

/* */
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

/* */
static error_t read_hdr(FILE *img, tif_hdr *hdrptr){
  assert(img != NULL);
  assert(hdrptr != NULL); 
  if(fread((void*)hdrptr, sizeof(tif_hdr), 1, img) < 1){ 
   return FILE_READ_ERROR; 
  }
  return OK;
}

/* */
static error_t read_bits_per_sample(FILE *img, tif_data *dataptr, tif_tag tag){
  assert(img != NULL);
  assert(dataptr != NULL);
  if(fseek(img, tag.data_offset, SEEK_SET) < 0){
    return FILE_ERROR;
  }
  if(fread((void*)&(dataptr->bits_per_sample), 
           sizeof(uint16_t), 1, img) < 1){
    return FILE_READ_ERROR;
  }
  return OK;
}

/* */
static error_t read_strip_bytes(FILE *img, tif_data *dataptr, tif_tag tag){
  assert(img != NULL);
  assert(dataptr != NULL);
  dataptr->strip_bytes = (uint32_t*)malloc(sizeof(uint32_t)*tag.data_count);
  dataptr->strip_count = tag.data_count;
  if(tag.data_count == 1){
    *(dataptr->strip_bytes) = tag.data_offset;
  }
  else{
    if(fseek(img, tag.data_offset, SEEK_SET) < 0){
      return FILE_ERROR;
    }
    short bytes = (tag.type == 3)? 2 : 4;
    int i;
    for(i = 0; i < tag.data_count; i++){
      memset((void*)&(dataptr->strip_bytes[i]), 0, sizeof(uint32_t));
      if(fread((void*)&(dataptr->strip_bytes[i]), 
               bytes, 1, img) < 1){
        return FILE_READ_ERROR;
      }
    }
  } 
  return OK;
} 

/* */
static error_t read_strip_offsets(FILE *img, tif_data *dataptr, tif_tag tag){
  assert(img != NULL);
  assert(dataptr != NULL);
  dataptr->strip_count = tag.data_count;
  dataptr->strip_offsets = (uint32_t*)malloc(sizeof(uint32_t)*tag.data_count);
  if(tag.data_count == 1){
    *(dataptr->strip_offsets) = tag.data_offset;
  }
  else{
    if(fseek(img, tag.data_offset, SEEK_SET) < 0){
      return FILE_ERROR;
    }
    short bytes = (tag.type == 3)? 2 : 4;
    int i;
    for(i = 0; i < tag.data_count; i++){
      memset((void*)&(dataptr->strip_offsets[i]), 0, sizeof(uint32_t));
      if(fread((void*)&(dataptr->strip_offsets[i]), 
               bytes, 1, img) < 1){
        return FILE_READ_ERROR;
      }
    }
  }
  return OK;
}

/* */
static error_t get_data(FILE *img, tif_data *dataptr){
  assert(img != NULL);
  assert(dataptr != NULL);
  tif_hdr hdr;
  read_hdr(img, &hdr);
  if(fseek(img, hdr.ifd_offset, SEEK_SET) < 0){
    return FILE_ERROR;
  }
  memset((void*)dataptr, 0, sizeof(tif_data)); 
  uint16_t n_entries;
  if(fread((void*)&n_entries, sizeof(uint16_t), 1, img) < 1){
    return FILE_READ_ERROR;
  }
  int i;
  for(i = 0; i < n_entries; i++){
    tif_tag tag;
    if(fread((void*)&tag, sizeof(tif_tag), 1, img) < 1){
      return FILE_READ_ERROR;
    }
    long pos = ftell(img); 
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
      case STRIP_OFFSETS:{
        error_t ret = read_strip_offsets(img, dataptr, tag);
        if(ret != OK) return ret; // propagate error
      }
      break;
      case STRIP_BYTES:{
        error_t ret = read_strip_bytes(img, dataptr, tag);
        if(ret != OK) return ret;
      }
      break;
      case BITSPERSAMPLE:{
        error_t ret = read_bits_per_sample(img, dataptr, tag);
        if(ret != OK) return ret;
      }
      break;
      case COMPRESSION:
        dataptr->compression = tag.data_offset;
      break;
    }
    if(fseek(img, pos, SEEK_SET) < 0){
      return FILE_ERROR;
    }
  }
  if(dataptr->compression != 1){
    return INVALID_COMPRESSION;
  } 
  return OK;
}

/* */
static error_t read_strip(uint32_t strip_offset, uint32_t strip_bytes, 
                          uint32_t size, uint32_t byte_count, unsigned bits,
                          FILE *img, FILE *out){
  assert(img != NULL);
  assert(out != NULL);
  if(fseek(img, strip_offset, SEEK_SET) < 0){
    return FILE_ERROR;
  }
  int bit_factor = 8/bits;
  uint8_t *bytes = (uint8_t*)malloc(bit_factor);
  memset((void*)bytes, 0, bit_factor);
  uint8_t byte;
  int i;
  for(i = 0; i < strip_bytes && byte_count < size; 
                i += bit_factor, byte_count++){
    if(fread((void*)bytes, bit_factor, 1, img) < 1){
      return FILE_READ_ERROR;
    }
    decode((uint8_t*)&byte, 1, bits, bytes, bit_factor); 
    if(fwrite((void*)&byte, sizeof(uint8_t), 1, out) < 1){
      return FILE_WRITE_ERROR;
    }
  }
  free(bytes);
  return byte_count;
}

/* */
static error_t read_data(FILE *img, FILE *out, unsigned bits, tif_data data){ 
  assert(img != NULL);
  assert(out != NULL);
  if(data.strip_count == 0){
    return NO_IMAGE_DATA;
  }
  if(fseek(img, data.strip_offsets[0], SEEK_SET) < 0){
    return FILE_ERROR;
  } 
  int bit_factor = 8/bits;
  int encoded_size = sizeof(int)*bit_factor;
  uint8_t *encoded = (uint8_t*)malloc(encoded_size);
  if(fread((void*)encoded, encoded_size, 1, img) < 1){
    return FILE_READ_ERROR;
  }
  unsigned size;
  decode((uint8_t*)&size, sizeof(unsigned), bits, encoded, encoded_size);
  data.strip_bytes[0] -= encoded_size;
  data.strip_offsets[0] += encoded_size;
  unsigned byte_count = 0;
  int i;
  for(i = 0; i < data.strip_count && byte_count < size; i++){
    byte_count = read_strip(data.strip_offsets[i], 
                  data.strip_bytes[i], 
                  size, byte_count, bits,
                  img, out);
    if(byte_count < 0){
      return byte_count; // propagate error
    }
  }
  free(encoded);
  return OK;
}

/* */
static error_t write_strip(uint32_t strip_offset, uint32_t strip_bytes, 
                           uint32_t size, uint32_t byte_count, unsigned bits,
                           FILE *in, FILE *img, FILE *out){
  assert(in != NULL);
  assert(img != NULL);
  assert(out != NULL);
  if(fseek(img, strip_offset, SEEK_SET) < 0){
    return FILE_ERROR;
  }
  int bit_factor = 8/bits;
  uint8_t *bytes = (uint8_t*)malloc(bit_factor);
  memset(bytes, 0, bit_factor);
  uint8_t byte;
  int i;                                   
  for(i = 0; i < strip_bytes && byte_count < size; 
                            i += bit_factor, byte_count++){
    if(fread((void*)&byte, sizeof(uint8_t), 1, in) < 1){ 
      return FILE_READ_ERROR;
    }
    if(fread((void*)bytes, bit_factor, 1, img) < 1){
      return FILE_READ_ERROR;
    }
    encode(bytes, bit_factor, bits, (uint8_t*)&byte, 1);
    if(img == out){
      if(fseek(out, -bit_factor, SEEK_CUR) < 0){
        return FILE_ERROR;
      }
    }
    if(fwrite((void*)bytes, bit_factor, 1, out) < 1){
      return FILE_WRITE_ERROR;
    }
  }
  free(bytes);
  return byte_count;
}

static error_t write_data(FILE *in, FILE *img, FILE *out, unsigned bits, tif_data data){
  assert(in != NULL);
  assert(img != NULL);
  assert(out != NULL);
  if(data.strip_count == 0){
    return NO_IMAGE_DATA;
  }
  printf("strip count: %d\n", data.strip_count);
  printf("strip offset[0]: %X\n", data.strip_offsets[0]);
  printf("strip bytes[0]: %X\n", data.strip_bytes[0]);
  printf("strip offset[1]: %X\n", data.strip_offsets[1]);
  printf("strip bytes[1]: %X\n", data.strip_bytes[1]);
  exit(1);
  if(fseek(img, data.strip_offsets[0], SEEK_SET) < 0){
    return FILE_ERROR;
  }  
  fseek(in, 0, SEEK_END);
  unsigned size = ftell(in); 
  fseek(in, 0, SEEK_SET); 
  int bit_factor = 8/bits;
  int encoded_size = sizeof(int)*bit_factor;
  uint8_t *encoded = (uint8_t*)malloc(encoded_size);
  if(fread((void*)encoded, encoded_size, 1, img) < 1){
    return FILE_READ_ERROR;
  }
  encode(encoded, encoded_size, bits, 
        (uint8_t*)&size, sizeof(unsigned));
  if(img == out){
    if(fseek(out, -encoded_size, SEEK_CUR) < 0){
      return FILE_ERROR;
    }
  }
  if(fwrite((void*)encoded, encoded_size, 1, out) < 1){
    return FILE_WRITE_ERROR;
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
    if(byte_count < 0){
      return byte_count; // propagate error
    }
  }
  free(encoded);
  return OK;
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

error_t tif_read_data(char *filename_img, char *filename_out, int bits){
  assert(filename_img != NULL);
  assert(filename_out != NULL);
  FILE *img, *out;
  if(!(img = fopen(filename_img, "rb"))){ 
    return FILE_ERROR;
  }
  if(!(out = fopen(filename_out, "wb"))){ 
    return FILE_ERROR;
  }
  tif_data data;
  int ret = get_data(img, &data);
  if(ret != OK){ return ret; }// propagate error
  ret = read_data(img, out, 1, data);
  if(ret != OK){ return ret; }
  free_data(&data);
  fclose(img);
  fclose(out);
}

error_t tif_write_data(char *filename_in, char *filename_img, int bits){
  assert(filename_in != NULL);
  assert(filename_img != NULL);
  tif_data data;
<<<<<<< HEAD
  get_data(filename_img, &data);
  if(data.compression != 1){
    fprintf(stderr, "only uncompressed images supported\n");
    //exit(EXIT_FAILURE);
  }
=======
>>>>>>> errors
  FILE *in, *img;
  if(!(in = fopen(filename_in, "rb"))){
    return FILE_ERROR;
  }
  if(!(img = fopen(filename_img, "rb+"))){
    return FILE_ERROR;
  }
  int ret = get_data(img, &data);
  if(ret != OK){ return ret; }
  fseek(in, 0, SEEK_END);
  unsigned size = ftell(in); 
  fseek(in, 0, SEEK_SET); 
  if(size == 0){ // empty file
    return FILE_EMPTY; 
  }
  int useable_size = get_useable_size(data, bits);
  if(size > useable_size){ // not enough image data for bit encoding
    return NOT_ENOUGH_DATA;
  }
<<<<<<< HEAD
  write_data(in, img, img, bits, data);
=======
  ret = write_data(in, img, img, 1, data);
  if(ret != OK){ return ret; }
>>>>>>> errors
  free_data(&data);
  fclose(in);
  fclose(img);
}
