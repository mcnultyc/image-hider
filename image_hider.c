#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "tif.h"



int main(int argc, char *argv[]){

  int decode = 0;
  int encode = 0;
  char *filename_img = NULL;
  char *filename = NULL;
  int i;
  for(i = 1; i < argc; i++){
    if(strcmp("-d", argv[i]) == 0){
      decode = 1;
    }
    else if(strcmp("-e", argv[i]) == 0){
      encode = 1;
    }
    else{
      if(!filename_img){
        filename_img = argv[i];
      }
      else if(!filename){
        filename = argv[i];
      }
    }
  }


  if((decode == 0 && encode == 0) ||
    !filename_img || !filename){
    fprintf(stderr, "invalid options\n");
    exit(EXIT_FAILURE); 
  }
  if(decode){
    printf("decoding...\n");
    printf("image: %s\n", filename_img);
    printf("output: %s\n", filename);
    tif_read_data(filename_img, filename, 4);
  }
  else if(encode){
    printf("encoding...\n");
    printf("image: %s\n", filename_img);
    printf("input: %s\n", filename);
    tif_write_data(filename, filename_img, 4);
    
  }



  return 0;
}






