//fifo.h
// Simple fifo
// http://stratifylabs.co/embedded%20design%20tips/2013/10/02/Tips-A-FIFO-Buffer-Implementation/

#include <stdio.h>
#include <stdlib.h>
#define __STDC_FORMAT_MACROS // Required for format specifiers
#include <inttypes.h>
#include "fifo.h"

//This initializes the FIFO structure with the given buffer and size
void fifo_init(fifo_t * f, uchar * buf, int size) {
     f->head = 0;
     f->tail = 0;
     f->size = size;
     f->buf = buf;
}
 
//This reads nbytes bytes from the FIFO
//The number of bytes read is returned
int fifo_read(fifo_t * f, uchar * buf, int nbytes) {
     int i;
     uchar * p;
     p = buf;
     for(i=0; i < nbytes; i++){
          if( f->tail != f->head ){ //see if any data is available
               *p++ = f->buf[f->tail];  //grab a byte from the buffer
               f->tail++;  //increment the tail
               if( f->tail == f->size ){  //check for wrap-around
                    f->tail = 0;
               }
          } else {
               return i; //number of bytes read 
          }
     }
     return nbytes;
}
 
//This writes up to nbytes bytes to the FIFO
//If the head runs in to the tail, not all bytes are written
//The number of bytes written is returned
int fifo_write(fifo_t * f, const uchar * buf, int nbytes) {
     int i;
     const uchar * p;
     p = buf;
     for(i=0; i < nbytes; i++) {
           //first check to see if there is space in the buffer
           if ( (f->head + 1 == f->tail) ||
                ( (f->head + 1 == f->size) && (f->tail == 0) )) {
                 return i; //no more room
           } else {
               f->buf[f->head] = *p++;
               f->head++;  //increment the head
               if( f->head == f->size ){  //check for wrap-around
                    f->head = 0;
               }
           }
     }
     return nbytes;
}
// --------- $Id: vdr-convert,v 1.3 2016/09/01 13:01:48 richard Exp $ ---------- END
