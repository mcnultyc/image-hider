#include "hdr.h"

int is_tif(char *filename){
  assert(filename != NULL);
  FILE *in = fopen(filename, "rb");
  char buffer[4];
  if(fread(buffer, 4, 1, in) < 1){
    return -1;
  }
  if(buffer[0] == 0x49 && buffer[1] == 0x49 &&
     buffer[2] == 0x2A && buffer[3] == 0x00){
    return 1;
  }
  //TODO support big-endian 0x4D4D 
  fclose(in);
  return 0;
}

#ifdef __linux__
error_t get_images(char *path, image_dir **head){
  assert(path != NULL);
  assert(head != NULL && *head != NULL);
  DIR *dir = opendir(path);
  struct dirent *dirp;
  if(dir == NULL){
    return FILE_ERROR;
  } 
  while((dirp = readdir(dir)) != NULL){
    struct stat info;
    char filename[100];
    sprintf(filename, "%s/%s", path, dirp->d_name);             
    if(stat(filename, &info) >= 0){
      if((info.st_mode & S_IFMT) == S_IFREG){
        if(is_tif(filename) > 0){
          //TODO 
        } 
      }
    }
  } 
  return OK; 
}
#endif


#ifdef _WIN32
image_dir *get_images(){
  //TODO
  return NULL;
}
#endif

image_dir* insert_image(image_dir *head, FILE *img){
  assert(img != NULL);
  image_dir *node = (image_dir*)malloc(sizeof(image_dir));
  node->img = img;
  node->next = head;
  return node; 
}


void free_dir(image_dir *head){
  //TODO
}

error_t encode(uint8_t *encoded, unsigned encoded_size, unsigned bits, 
               uint8_t *bytes,   unsigned bytes_size){
  assert(encoded != NULL);
  assert(bytes != NULL);
  if(bits != 1 && bits != 2 && bits != 4){
    return INVALID_BITS;
  }
  unsigned bit_factor = 8/bits;
  if(encoded_size != bytes_size * bit_factor){
    return INVALID_ENCODE_SIZE;
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
      uint8_t byte = (bytes[i] & mask) >> shift;
      encoded[j] = (encoded[j] & ~encode_mask) | byte; 
      mask >>= bits; 
    } 
  }
  return OK;
}

error_t decode(uint8_t *decoded, unsigned decoded_size, unsigned bits, 
               uint8_t *bytes,   unsigned bytes_size){
  assert(decoded != NULL);
  assert(bytes != NULL);
  if(bits != 1 && bits != 2 && bits != 4){
    return INVALID_BITS;
  }
  unsigned bit_factor = 8/bits;
  if(bytes_size != decoded_size * bit_factor){
    return INVALID_DECODE_SIZE;
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
      uint8_t byte = (bytes[j] & decode_mask) << shift;
      decoded[i] = decoded[i] | byte; 
      mask >>= bit_factor;
    } 
  }
  return OK;
}
