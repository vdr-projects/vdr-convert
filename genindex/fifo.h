//fifo.h
/*-------------------------------------------------------------------------------
 *  Revision History
 *  $Log: xmltv2vdr.pl,v $
 *-------------------------------------------------------------------------------
*/

#ifndef __FIFO_H
#define __FIFO_H

#include <stdio.h>
#include "tools.h"

typedef struct {
     uchar * buf;
     int head;
     int tail;
     int size;
} fifo_t;

void fifo_init(fifo_t * f, uchar * buf, int size);
int fifo_read(fifo_t * f, uchar * buf, int nbytes);
int fifo_write(fifo_t * f, const uchar * buf, int nbytes);
 
#endif // __FIFO_H

// --------- $Id: vdr-convert,v 1.3 2016/09/01 13:01:48 richard Exp $ ---------- END