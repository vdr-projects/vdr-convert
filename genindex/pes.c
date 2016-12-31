/*
 * (C) 2003-2006 Stefan Huelswitt <s.huelswitt@gmx.de>, (C) RF 2016
 * pes.c:
 *
 * See the main source file 'genindex.c' for copyright information and
 * how to reach the author.
 *
 *-------------------------------------------------------------------------------
 *  Revision History
 *  $Log: pes.c,v $
 *  Revision 1.4  2016/12/31 16:11:24  richard
 *  Support for 5.1 AC3 or DTS surround sound
 *
 *  Revision 1.3  2016/10/01 13:32:55  richard
 *  Make header error non fatal
 *
 *  Revision 1.2  2016/09/01 15:37:42  richard
 *  Extensively updated to V0.2 for EN 300 743 compliant subtitles
 *  added   -b  and  -n flags
 *
 *-------------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#define __STDC_FORMAT_MACROS // Required for format specifiers
#include <inttypes.h>
#include "pes.h"
#include "ringbuffer.h"
#include "tools.h"


//#define DEBUG(x...) printf(x)
#define DEBUG(x...)

//#define PD(x...) printf(x)
#define PD(x...)

#define PAGE_COMPOSITION_SEGMENT    0x10
#define REGION_COMPOSITION_SEGMENT  0x11
#define CLUT_DEFINITION_SEGMENT     0x12
#define OBJECT_DATA_SEGMENT         0x13
#define DISPLAY_DEFINITION_SEGMENT  0x14
#define DISPARITY_SIGNALING_SEGMENT 0x15 // DVB BlueBook A156
#define END_OF_DISPLAY_SET_SEGMENT  0x80
#define STUFFING_SEGMENT            0xFF

#include "fifo.h"

// from VDR remux.c

void PesSetPts(uchar *p, int64_t Pts)
{
  p[ 9] = ((Pts >> 29) & 0x0E) | (p[9] & 0xF1);
  p[10] =   Pts >> 22;
  p[11] = ((Pts >> 14) & 0xFE) | 0x01;
  p[12] =   Pts >>  7;
  p[13] = ((Pts <<  1) & 0xFE) | 0x01;
}

void PesSetDts(uchar *p, int64_t Dts)
{
  p[14] = ((Dts >> 29) & 0x0E) | (p[14] & 0xF1);
  p[15] =   Dts >> 22;
  p[16] = ((Dts >> 14) & 0xFE) | 0x01;
  p[17] =   Dts >>  7;
  p[18] = ((Dts <<  1) & 0xFE) | 0x01;
}

int64_t PtsDiff(int64_t Pts1, int64_t Pts2)
{
  int64_t d = Pts2 - Pts1;
  if (d > MAX33BIT / 2)
     return d - (MAX33BIT + 1);
  if (d < -MAX33BIT / 2)
     return d + (MAX33BIT + 1);
  return d;
}

// from VDR dvbsubtitle.c to assemble subs - DIH.

// --- cDvbSubtitleAssembler -------------------------------------------------

class cDvbSubtitleAssembler {
private:
  uchar *data;
  int length;
  int pos;
  int size;
  bool Realloc(int Size);
public:
  cDvbSubtitleAssembler(void);
  virtual ~cDvbSubtitleAssembler();
  void Reset(void);
  unsigned char *Get(int &Length);
  void Put(const uchar *Data, int Length);

};

cDvbSubtitleAssembler::cDvbSubtitleAssembler(void)
{
  data = NULL;
  size = 0;
  Reset();
}

cDvbSubtitleAssembler::~cDvbSubtitleAssembler()
{
  free(data);
}

void cDvbSubtitleAssembler::Reset(void)
{
  length = 0;
  pos = 0;
}

bool cDvbSubtitleAssembler::Realloc(int Size)
{
  if (Size > size) {
     Size = max(Size, 2048);
     if (uchar *NewBuffer = (uchar *)realloc(data, Size)) {
        size = Size;
        data = NewBuffer;
     } else {
        DEBUG("ERROR: can't allocate memory for subtitle assembler\n");  
        length = 0;
        size = 0;
        free(data);
        data = NULL;
        return false;
     }
   }
   return true;
}

unsigned char *cDvbSubtitleAssembler::Get(int &Length)
{
  if (length > pos + 5) {
     Length = (data[pos + 4] << 8) + data[pos + 5] + 6;
     if (length >= pos + Length) {
        unsigned char *result = data + pos;
        pos += Length;
        return result;
     }
  }
  return NULL;
}

void cDvbSubtitleAssembler::Put(const uchar *Data, int Length)
{
  if (Length && Realloc(length + Length)) {
     memcpy(data + length, Data, Length);
     length += Length;
  }
}

// --- cPES --------------------------------------------------------------------

cPES::cPES(eRule ru)
{
  rb=new cRingBufferFrame(KILOBYTE(50));
  defaultRule=ru;
  Reset();
  dvbSubtitleAssembler = new cDvbSubtitleAssembler; 
  fifo_init(&fifo, pktbuf, sizeof(pktbuf));
}

cPES::~cPES()
{
  delete rb;
  //??
  delete dvbSubtitleAssembler;
}

void cPES::Reset(void)
{
  for(int i=0 ; i<NUM_RULESETS ; i++) SetDefaultRule(defaultRule,i);
  UseRuleset(0);
  ClearSeen();
  Clear();
}

void cPES::Clear(void)
{
  Lock();
  mode=pmNewSync; frame=0; mpegType=2;
  rb->Clear();
  Unlock();
}

bool cPES::ValidRuleset(const int num)
{
  if(num>=0 && num<NUM_RULESETS) return true;
  DEBUG("PES: illegal ruleset %d\n",num);
  return false;
}

void cPES::UseRuleset(const int num)
{
  if(ValidRuleset(num)) {
    currRules=rules[num];
    currNum=num;
  }
}

int cPES::CurrentRuleset(void)
{
  return currNum;
}

void cPES::SetDefaultRule(eRule ru, const int num)
{
  if(ValidRuleset(num)) {
    for(int i=0 ; i<NUM_RULES ; i++) {
      rules[num][i]=ru;
    }
  }  
}

void cPES::SetRule(uchar type, eRule ru, const int num)
{
  if(ValidRuleset(num)) {
    rules[num][type]=ru;
  }
}

void cPES::SetRuleR(uchar ltype, uchar htype, eRule ru, const int num)
{
  if(ValidRuleset(num)) {
    if(ltype<htype) {
      for(; ltype<=htype ; ltype++) {
        rules[num][ltype]=ru;
      }
    } else {
      DEBUG("PES: bad range %x-%x\n",ltype,htype);
    }
  }
}

unsigned int cPES::Seen(uchar type) const
{
  return seen[type];
}

void cPES::ClearSeen(void)
{
  memset(seen,0,sizeof(seen));
  totalBytes=totalSkipped=totalZeros=0;
  skipped=zeros=0;
}

void cPES::Skip(uchar *data, int count)
{
  if(data) {
    Skipped(data,count);
    while(count>0) {
      skipped++;
      if(!*data++) {
        zeros++;
      }
      count--;
    }
  } else if(skipped) {
    totalSkipped+=skipped;
    if(skipped==zeros) {
      totalZeros+=zeros;
    } else {
      DEBUG("PES: skipped %d bytes\n",skipped);
    }
    skipped=zeros=0;
  }
}

void cPES::Statistics(bool build)
{
  if(totalBytes) {
    printf("PES: Stats %ld kbytes total, %ld kbytes skipped, %ld zero-gaps\n",
          ROUNDUP(totalBytes,1024),ROUNDUP(totalSkipped,1024),totalZeros);
	if (build) {
    	printf("PES: Stats %ld kbytes subtitle data output: %d subs packets (%d short packets)\n", totsubsize/1024, subspkts, errpkts);
    }
    for(int type=0 ; type<=0xFF ; type++) {
      if(seen[type]) {
        printf("PES: Stats %02X: %d packets PTS-first: %jd PTS-last: %jd\n",type,seen[type],ptsfirst[type], ptslast[type]);
      }
    }
  }
}

void cPES::ModifyPaketSize(int mod)
{
  if(SOP) {
    int size=header[4]*256 + header[5] + mod;
    header[4]=(size>>8)&0xFF;
    header[5]=(size   )&0xFF;
  } else {
    DEBUG("PES: modify paket size called in middle of packet\n");
  }
}

void cPES::Redirect(eRule ru)
{
  if(SOP) {
    currRule=ru;
    redirect=true;
  } else {
    DEBUG("PES: redirect called in middle of packet\n");
  }
}

int cPES::HeaderSize(uchar *head, int len)
{
  if(len<PES_MIN_SIZE) return -PES_MIN_SIZE;
  switch(head[3]) {
    default:
    // Program end
    case 0xB9:
    // Programm stream map
    case 0xBC:
    // video stream start codes
    case 0x00 ... 0xB8:
    // reserved
    case 0xF0 ... 0xFF:
      return PES_MIN_SIZE;

    // Pack header
    case 0xBA:
      if(len<5) return -5;
      switch(head[4]&0xC0) {
        default:
          DEBUG("PES: unknown mpegType in pack header (0x%02x)\n",head[4]);
          // fall through
        case 0x00:
          mpegType=1;
          return 12;
        case 0x40:
          mpegType=2;
          if(len<14) return -14;
          return 14+(head[13]&0x07); // add stuffing bytes
        }

    // System header
    case 0xBB:
      if(len<6) return -6;
      return 6+head[4]*256+head[5]; //XXX+ is there a difference between mpeg1 & mpeg2??

    // Padding stream
    case 0xBE:
    // Private stream2 (navigation data)
    case 0xBF:
      return 6; //XXX+ is there a difference between mpeg1 & mpeg2??

    // all the rest (the real packets)
    // Private stream1
    case 0xBD:
    case 0xC0 ... 0xCF:
    case 0xD0 ... 0xDF:
    //video
    case 0xE0 ... 0xEF:
      if(len<7) {
         return -7;
      }
      int index=6;

      while((head[index]&0xC0)==0xC0) { // skip stuffing bytes
        index++; 
        if (index >= len) {
            return -(index+1);
        }
      }

      if((head[index]&0xC0)==0x80) { // mpeg2
        mpegType=2;
        index+=2;
		if(index>=len) {
            return -(index+1);
        }
        return index+1+head[index]; // mpeg2 header data bytes
      }
      mpegType=1;
      if((head[index]&0xC0)==0x40) { // mpeg1 buff size
        index+=2; 
        if(index>=len) {
           return -(index+1);
        }
      }

      switch(head[index]&0x30) {
        case 0x30: 
          index+=9; 
          break; // mpeg1 pts&dts          
        case 0x20: 
          index+=4; 
          break; // mpeg1 pts
        case 0x10: 
          DEBUG("PES: bad pts/dts flags in MPEG1 header (0x%02x)\n",head[index]); 
          break;
      }
      return index+1;
   }
}

int cPES::PacketSize(uchar *head, int len)
{
  switch(head[3]) {
    default:
    // video stream start codes
    case 0x00 ... 0xB8:
    // Program end
    case 0xB9:
    // Pack header
    case 0xBA:
    // System header
    case 0xBB:
    // Programm stream map
    case 0xBC:
    // reserved
    case 0xF0 ... 0xFF:
      return len; // packet size = header size

    // Private stream1
    case 0xBD:
    // Padding stream
    case 0xBE:
    // Private stream2 (navigation data)
    case 0xBF:
    // all the rest (the real packets)
    case 0xC0 ... 0xCF:
    case 0xD0 ... 0xDF:
    case 0xE0 ... 0xEF:
      return 6 + head[4]*256 + head[5];
    }
}

int cPES::Return(int used, int len)
{
    PD("PES: return used=%d len=%d mode=%d\n",used,len,mode);
    if(SOP && unsavedHeader && used>=len) {
      // if we are about to finish the current data packet and we have
      // an unsaved header inside, we must save the header to the buffer
      memcpy(hbuff,header,headerSize);
      header=hbuff; 
      unsavedHeader=false;
      PD("PES: header saved\n");      // RF don't see this
    }
    if(used>len) {
      DEBUG("PES: BUG! used %d > len %d\n",used,len);
      used=len;
    }
    if(used>0) {
      totalBytes+=used;
    }
    
    Unlock(); // release lock from Process()
    return used;
}

// RF Note that this is called with file chunks - do not assume ends on whole packet boundary!
int cPES::Process(const uchar *data, int len)
{
  Lock(); // lock is released in Return()
  PD("PES: enter data=%p len=%d mode=%d have=%d need=%d old=%d\n",
      data,len,mode,have,need,old);
  int used=0;
  static int size;
  static uchar headerstore[64];
  static int savedheadersize;
  static int overrun;
  static bool subsstarted;

  while(used<len) {
    uchar *c=(uchar *)data+used;
    int rest=len-used, n;
    switch(mode) {
      case pmNewSync:
        PD("PES: new sync from %d, rest %d\n",used,rest);
        have=old=need=0;
        unsavedHeader=false; 
        outputHeader=true; 
        SOP=true; 
        redirect=false;
        mode=pmFastSync;
        // fall through

      case pmFastSync:
        // a short cut for the most common case
        // if matched here, header isn't copied around
        if(rest>=PES_MIN_SIZE) {
          PD("PES: fastsync try used=%d: %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                used,c[0],c[1],c[2],c[3],c[4],c[5],c[6],c[7],c[8]);
          if(c[0]==0x00 && c[1]==0x00 && c[2]==0x01) {
            headerSize=HeaderSize(c,rest);
            if(headerSize>0 && rest>=headerSize) {
              // found a packet start :-)
              PD("PES: fastsync hit used=%d headerSize=%d rest=%d\n",used,headerSize,rest);
              header=c; 
              unsavedHeader=true;
              used+=headerSize; 
              mode=pmHeaderOk; 
              continue;
            }
          }
          else if(c[2]!=0x00) { 
             used+=3; 
             Skip(c,3); 
             continue; 
          }
          else { 
             used++; 
             Skip(c); 
             continue; 
          }
        }
        // copy remaining bytes to buffer
        memcpy(hbuff,c,rest);
        have=old=rest; 
        used+=rest;
        mode=pmSync;
        PD("PES: buffering started old=%d\n",old);
        break;

      case pmSync:
        PD("PES: slowsync have=%d old=%d\n",have,old);
        if(have<PES_MIN_SIZE && rest>=1) { 
           hbuff[have++]=c[0]; 
           used++; 
           continue; 
        }
        if(have>=PES_MIN_SIZE) {
          PD("PES: slowsync try used=%d: %02x %02x %02x %02x\n",
                used,hbuff[0],hbuff[1],hbuff[2],hbuff[3]);
          if(hbuff[0]==0x00 && hbuff[1]==0x00 && hbuff[2]==0x01) {
            need=abs(HeaderSize(hbuff,have));
            mode=pmGetHeader; 
            continue;
          }
          // no sync found, move buffer one position ahead
          have--; 
          Skip(hbuff);
          memmove(hbuff,hbuff+1,have);
          // if all bytes from previous data block used up, switch to FastSync
          if(!--old) {
            used=0; mode=pmFastSync;
            PD("PES: buffering ended\n");
          }
          continue;
        }
        break;

      case pmGetHeader:
        if(have<need) {
          n=min(need-have,rest);
          memcpy(hbuff+have,c,n);
          have+=n; 
          used+=n;
          PD("PES: get header n=%d need=%d have=%d used=%d\n",n,need,have,used);
          continue;
        }
        if(have>=need) {
          need=abs(HeaderSize(hbuff,have));
          if(have<need) continue;
          // header data complete
          PD("PES: slowsync hit used=%d\n",used);
          if(have>need) DEBUG("PES: bug, buffered too much. have=%d need=%d\n",have,need);
          if(have>(int)sizeof(hbuff)) DEBUG("PES: bug, header buffer overflow. have=%d size=%d\n",have,(int)sizeof(hbuff));
          headerSize=need;
          header=hbuff;
          mode=pmHeaderOk;
        }
        break;

      case pmHeaderOk:
        type=header[3]; 
        seen[type]++; 
        if (PesHasPts(header) && len > 6) {
          ptslast[type]=PesGetPts(header);
          if (!ptsfirst[type]) {
            ptsfirst[type]=ptslast[type];
          }
        }
        Skip(0);
        if(type<=0xB8) {
          // packet types 0x00-0xb8 are video stream start codes
          DEBUG("PES: invalid packet type 0x%02x, skipping\n",type);
          mode=pmNewSync;
          break;
        }
        payloadSize=PacketSize(header,headerSize)-headerSize;
        if(payloadSize<0) {
          DEBUG("PES: invalid payloadsize %d, skipping\n",payloadSize);
          mode=pmNewSync;
          break;
        }
        PD("PES: found sync at offset %d, type %02x, length %d, next expected %d\n",
              used-headerSize,type,headerSize+payloadSize,used+payloadSize);
        PD("PES: header type=%02x mpeg=%d header=%d payload=%d:",
              type,mpegType,headerSize,payloadSize);
        for(int i=0 ; i<headerSize ; i++) {
          PD(" %02x",header[i]);
        }
        PD("\n");
        currRule=currRules[type];
        have=need=0;
        mode=pmPayload;
        // fall through

      case pmPayload:
        n=min(payloadSize-have,rest);
        if(!n) {
          if(payloadSize==0) {
            n=1;
          } else {
            break;
          }
        }
        // RF we crash into this 2nd time round, if have header (usual). So subhead set
        PD("PES: payload have=%d n=%d SOP=%d\n",have,n,SOP);
        switch(currRule) {
          default:
            DEBUG("PES: bug, unknown rule %d, assuming pass\n",currRule);
            // fall through
          case prPass: n=n; break;
          case prSkip: n=-n; break;
          case prAct1: n=Action1(type,c,n); break;
          case prAct2: n=Action2(type,c,n); break;
//          case prAct3: n=Action3(type,c,n); break;
          case prAct3: 
            // first deal with AC3 / DTS
            n=Action1(type,c,n); // don't scan output
            int PayloadOffset;
            PayloadOffset = headerSize;
            //PES hdr extension, from dvbsubtitle.c, Compatibility mode for old subtitles plugin (doesn't match vanilla 1.X recordings)
            if ((header[7] & 0x01) && (header[PayloadOffset - 3] & 0x81) == 0x01 &&  header[PayloadOffset - 2] == 0x81) {
                PayloadOffset--;
            }
            if (header[PayloadOffset] != 0x20) {   // assume AC3 or DTS
              // VDR has the PES "DVD" substream header like 80 01 00 01 to identify the stream
              // this confuses ffmpeg in a .ts, so strip it at SOP. sse http://stnsoft.com/DVD/ass-hdr.html
              if (SOP) {
                used+=4;
                n-=4;
                payloadSize-=4;         //dummy it up so we don't overstep
                ModifyPaketSize(-4);    //actually truncate rewritten stream also
                Skip(c,4);
              }
              break;   // passthrough       
            } else {
              /*  Subtitles stream
                  Because we have to assemble AN ENTIRE PES packet of possibly several existing 2k PES packets, 
                  and usually several subtitle segments, then as far as the long main loop is concerned, 
                  we drop all the subs packets. 

                  VDR1.x packets can contain fragments of segments too

                  We have no idea how long we have to wait - need END_OF_DISPLAY_SET_SEGMENT to complete a PES
                  Then write out the assembled PES independently, hoping it's PTS doesn't 
                  upset something downstream. Should in any case be indpendent, but ffmpeg is touchy
              */
              n = -n; //drop all content for main loop
              if (-n) {
                PayloadOffset = headerSize;
                bool ResetSubtitleAssembler = 0;
                //VDR 1.x subs, skip special 4 byte "substream header" (VDR always strips all packets)
                int SubstreamHeaderLength = (have == 0 ? 4 : 0);  // Full packet, not a continuation chunk from file
                // c points to the payload only now             
                if (SOP && SubstreamHeaderLength) {        // only if we have it
                  ResetSubtitleAssembler = c[3] == 0x00;
                  //PES hdr extension, from dvbsubtitle.c, Compatibility mode for old subtitles plugin (doesn't match vanilla 1.X recordings)
                  if ((header[7] & 0x01) && (header[PayloadOffset - 3] & 0x81) == 0x01 &&  header[PayloadOffset - 2] == 0x81) {
                    PayloadOffset--;
                    SubstreamHeaderLength = 1;
                    ResetSubtitleAssembler = (header[8] >= 5);    // i.e. there's a header extension
                  }
                  if (PesHasPts(header)) {
                    int64_t oldpts = ptsnow[type];
                    ptsnow[type] = PesGetPts(header);
                    if (ptsnow[type] < oldpts) {
                      // Non-monotonic subs send ffmpeg loopy
                      printf("PES: OOPs Non-monotonic PTS at %jd (previous %jd!) - packet dropped\n",ptsnow[type],oldpts);
                      break;
                    }
                    int64_t ptsdiff = (PtsDiff(ptsnow[0xc0],ptsnow[type]))/90;   //millisecs
                    DEBUG("PES: subtitle packet pts %jd, latest main audio pts %jd, subs ahead %jdms\n",ptsnow[type],ptsnow[0xc0],ptsdiff);
                    if (ptsnow[0xc0] && abs((int)ptsdiff) > 5000) {
                      printf("genindex: OOPs large subs timing offset! %jdms\n",ptsdiff);
                    }
                    if (headerSize +2 > (int)sizeof (headerstore)) {
                      printf("PES: OOPs huge header! %d at %jd - packet dropped\n",headerSize,ptsnow[type]);
                      break;
                    }
                    if (overrun) {    // continuation packets don't have a PTS AFAIK
                      DEBUG("PES: Suspect  %d bytes left at start of new packet!\n",overrun);
                    }                    

                    memcpy (headerstore,header,headerSize);
                    headerstore[6] |= 0x01;                     // Set "Original" - per trouble-free .ts FWiW

  //                if (audiopts) PesSetPts(headerstore,audiopts);    // use existing if no audiopts
                    
  //                Working on theory that ffmpeg wants a DTS for some reason, copy the PTS
  //                - it turns out that ffmpeg does this internally.
  //                  PesSetDtsbit(headerstore);
  //                  PesSetDts(headerstore,ptsnow[type]-18000);         // ~200ms earlier similar to other streams
  //                  headerstore[8]+=5;                        // 5 for DTS (we know it has PES ext:this is safe)
                  
                    savedheadersize = headerSize+2+0;         // 5 for DTS
                    memset (headerstore+headerSize+0,0x20,1); // subs data_identifier which is stripped below
                    memset (headerstore+headerSize+1+0,0x00,1); // stream_id
                  }
                }

                if (-n > SubstreamHeaderLength) {
                  const uchar *data = c + SubstreamHeaderLength; // skip substream header
                  int length = -n - SubstreamHeaderLength; // skip substream header
                  if (ResetSubtitleAssembler) {
                    dvbSubtitleAssembler->Reset();
                  }
                  if (length > 3) {
                    int Count, offset;
                    int substotalthispacket=0; 
                    if (data[0] == 0x20 && data[1] == 0x00 && data[2] == 0x0F) {
                      offset = 2;
                    } else {
                      offset = 0;
                    }
                    int subslength = length - offset;
                    DEBUG("PES: Put payload length %d into dvbSubtitleAssembler\n",subslength);
                    dvbSubtitleAssembler->Put(data + offset, subslength);
                    while (true) {
                      unsigned char *b = dvbSubtitleAssembler->Get(Count);    // Count = seg size *req'd*, not actual available!
                      if (b && b[0] == 0x0F && Count > 5) {
                        // a complete segement
                        uchar segmentType = b[1];
                        if (segmentType == STUFFING_SEGMENT) {
                          continue;
                        }
                        if (fifo_write(&fifo, b, Count) != Count) {
                          DEBUG("PES: FIFO error\n");
                          Return(used,len);                        
                        }
                        substotalthispacket+=Count-overrun;
                        size+=Count;
                        DEBUG("PES: subtitle complete segment length %d type %02X, subs in fifo %d (used %d this segment)\n",Count, segmentType, size, Count-overrun);
                        if (Count-overrun < 0) {
                          DEBUG("PES: Suspect overrun of %d bytes!\n",overrun);
                        }
                        overrun=0;    // must have used up all available data at start of a new segment
  
                        if (segmentType == END_OF_DISPLAY_SET_SEGMENT) {
                          uchar outbuf[KILOBYTE(64)];
                          if (fifo_read(&fifo, outbuf, size) != size) {
                            DEBUG("PES: FIFO error\n");
                            Return(used,len);
                          }
                          // skip the small 10...80 subs periodic "cleardown" packets at beginning of file only.
                          // These currently prevent reliable ffmpeg subs detection
                          int err = dvbsub_probe(outbuf,size,ptsnow[type]);
                          if (err == 1)  subsstarted = 1;                          
                          if (subsstarted) {
                            if (err < 0 )  errpkts++;     // mark it, but pass all the same as should be validly assembled
                            outbuf[size++] = 0xff;  // end_of_PES_data_field_marker
                            totsubsize+=size;       // Actual subs segment data = should match ffmpeg report
                            subspkts+=1;
                            int pkt = savedheadersize -6 + size;  //6 for 00.00.01.bd.HH.hh, 1 for end_of_PES_data_field_marker
                            headerstore[4]=(pkt>>8)&0xFF;
                            headerstore[5]=(pkt   )&0xFF;
                            // Now can output ALL re-assembled subs in a SINGLE PES pkt
                            DEBUG("PES: *** subtitle complete PES packet with subs of %d bytes @pts %jd ***\n",size, ptsnow[type]);
                            PD("PES: output buffered subtitle header=%p count=%d\n",headerstore,savedheadersize);
                            int r=Output(headerstore,savedheadersize);
                            if(r<0) return Return(-1,len);
                            PD("PES: output buffered subtitle count=%d\n",size);
                            r=Output(outbuf,size);
                            if(r<0) return Return(-1,len);
                          } else {
                            DEBUG("PES: Skipping inital short subs packet for ffmpeg compatibility!\n");
                          }
                          size = 0; 
                        }
                      } else {
                        if (Count > subslength - substotalthispacket) {
  //                        if (c[SubstreamHeaderLength+offset+substotalthispacket] == 0xff) {
                          if (subslength - substotalthispacket < 6) {    //cruft or stuffing
                            break; 
                          }
                          // overrun the PES packet. Keep building packet with next PES segement.
                          overrun += subslength - substotalthispacket;
                          DEBUG("PES: subtitle incomplete segment, requires %d vs. %d available (needs %d)\n",Count, overrun, Count-overrun);
                          if (overrun < 0) {
                            printf("PES: BUG! Available %d bytes, @pts %jd ***\n",overrun, ptsnow[type]);
                            exit (1);
                          }
                        }
                        break;
                      }
                    }
                    // We have a complete disassembly of the sub(s)
                  }
                }
              }
            }
            break;           
          case prAct4: n=Action4(type,c,n); break;
          case prAct5: n=Action5(type,c,n); break;
          case prAct6: n=Action6(type,c,n); break;
          case prAct7: n=Action7(type,c,n); break;
          case prAct8: n=Action8(type,c,n); break;
        }
        if(n==0) {
          if(redirect) {
              redirect=false; 
              continue; 
          }
          return Return(used,len);
        }
        need=n; 
        SOP=false;
        mode=pmRingGet;
        // fall through

      case pmRingGet:
        frame=rb->Get();
        if(frame) {     // RF doesn't happen
          outCount=frame->Count();
          outData=(uchar *)frame->Data();
          PD("PES: ringbuffer got frame %p count=%d\n",frame,outCount);
          nextMode=pmRingDrop; 
          mode=pmOutput; 
          break;
        }
        mode=pmDataPut;
        // fall through

      case pmDataPut:
        if(need<0) {    //for skipped
          need=-need; 
          outputHeader=false;
          mode=pmDataReady; 
          continue;
        }
        if(outputHeader) {
          outData=header; 
          outCount=headerSize; 
          outputHeader=false;
          nextMode=pmDataPut;
        }
        else if(payloadSize) {
          outData=c; 
          outCount=need;
          nextMode=pmDataReady;
        } else {
          mode=pmDataReady;
          continue;
        }
        mode=pmOutput;
        // fall through

      case pmOutput:
        for(;;) {
          PD("PES: output data=%p count=%d -> ",outData,outCount);
          n=Output(outData,outCount);
          PD("n=%d\n",n);
          if(n<0) return Return(-1,len);
          if(n==0) return Return(used,len);
          outCount-=n; 
          outData+=n;
          if(outCount<=0) {
            mode=nextMode; 
            break;
          }
        }
        break;

      case pmDataReady:
        if(payloadSize) {
           used+=need; 
           have+=need; 
        }
        PD("PES: data ready need=%d have=%d paySize=%d used=%d\n",
            need,have,payloadSize,used);
        if(have>=payloadSize) {
          PD("PES: packet finished\n");
          if(have>payloadSize) DEBUG("PES: payload exceeded, size=%d have=%d\n",payloadSize,have);
          mode=pmNewSync;
        } else {
           mode=pmPayload;
        }
        break;

      case pmRingDrop:
        PD("PES: ringbuffer drop %p\n",frame);
        rb->Drop(frame); frame=0;
        mode=pmRingGet;
        break;

      default:
        DEBUG("PES: bug, bad mode %d\n",mode);
        return Return(-1,len);
      }
    }
  PD("PES: leave\n");
  return Return(used,len);
}


// From ffmpeg - This is the code used to score the dvbsub packets
// (If there aren't a complete set of segments 0x10 - 13 & 5 segments total, not useful pkt)
// ***NOTE*** This is modified from ffmpeg which appears to have logical bugs

int cPES::dvbsub_probe(uchar *p, int size, int64_t pts)
{
    int i, j, k;
    const uint8_t *end = p + size;
    int type, len;
    int max_score = 0;
    const uint8_t *ptr = p;

    for(i=0; i<size; i++) {
        if (p[i] == 0x0f) {
            ptr = p + i;
            uint8_t histogram[6] = {0};
            int min = 255;
            for(j=0; 5 < end - ptr; j++) {  // was 6. Relevant to this environment as tested pkt doesn't have 0xff tail
                if (*ptr != 0x0f) {
                  if (*ptr == 0xff) {
                    ptr++;          // similar to dvbsub_decode, continue if (allowable) stuffing  
                    continue;
                  } else {
                    break;
                  }
                }
                type    = ptr[1];
//                page_id = AV_RB16(ptr + 2);
//                len     = AV_RB16(ptr + 4);
                len = (*(ptr+4) << 8) + *(ptr+5);
                if (type == 0x80) {
                    ;
                } else if (type >= 0x10 && type <= 0x14) {
                    histogram[type - 0x10] ++;
                } else
                    break;
                if (6 + len > end - ptr)
                    break;
                ptr += 6 + len;             
            }
            for (k=0; k < 4; k++) {
                min = FFMIN(min, histogram[k]);
            }
            if (min && j > max_score)
                max_score = j;
        }
        if (max_score >= 5) {     // was > 5. Test was in outer loop, not clear why, can invalidate good scores
          return 1;
        } else {
          // if (end - ptr > 6) {
          //  printf("PES: dvbsub probe warning hex offset %04X in packet pts %jd : %02X %02X %02X %02X %02X %02X %02X %02X\n",int(ptr-p),pts,p[i],p[i+1],p[i+2],p[i+3],p[i+4],p[i+5],p[i+6],p[i+7]);
          //  return -1;
          // }

          // if it wasn't one of the expected short clear-down packets + margin, flag it as suspect
          return -1;
          // If i incremented after scoring, stomps though FROM START + i looking for anything like 0f.
          // So don't stomp once started it's not helpful, be happy with what you have
        }
    }
    return 0;
}
