#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "tif.h"

int main(int argc, char *argv[]){ 

  char *data = tif_get_data("image.tif");
  if(!data){
    printf("works\n");
  }
  
  return 0;
}






