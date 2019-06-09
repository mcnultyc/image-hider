#include "hdr.h"

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
