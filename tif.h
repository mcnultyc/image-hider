#ifndef TIF_H
#define TIF_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "hdr.h"

error_t tif_write_data(char*, char*, int);
error_t tif_read_data(char*, char*, int); 
int  is_tif(char*);

#endif
