/*
 * pes.h:
 *
 * See the main source file 'genindex.c' for copyright information and
 * how to reach the author.
 *
 *-------------------------------------------------------------------------------
 *  Revision History
 *  $Log: pes.h,v $
 *  Revision 1.2  2016/09/01 15:37:42  richard
 *  Extensively updated to V0.2 for EN 300 743 compliant subtitles
 *  added   -b  and  -n flags
 *
 *-------------------------------------------------------------------------------
 *
 */

#ifndef __PES_H
#define __PES_H

#include "thread.h"
#include "tools.h"
#include "fifo.h"

#define PES_MIN_SIZE 4    // min. number of bytes to identify a packet
#define PES_HDR_SIZE 6    // length of PES header
#define PES_EXT_SIZE 3    // length of PES extension

#define NUM_RULESETS 4    // number of rule sets
#define NUM_RULES    256  // how many rules in every set

class cFrame;
class cRingBufferFrame;
//RF
class cDvbSubtitleAssembler; // for legacy PES recordings

class cPES : public cMutex {
protected:
  enum eRule { prPass, prSkip, prAct1, prAct2, prAct3, prAct4, prAct5, prAct6, prAct7, prAct8 };
private:
  eRule rules[NUM_RULESETS][NUM_RULES], *currRules, currRule, defaultRule;
  int currNum;
  // Statistics
  unsigned int seen[256];
  int skipped, zeros;
  int64_t totalBytes, totalSkipped, totalZeros;
  //
  enum eMode { pmNewSync, pmFastSync, pmSync, pmGetHeader, pmHeaderOk, pmPayload,
               pmRingGet, pmRingDrop, pmDataPut, pmDataReady, pmOutput };
  eMode mode, nextMode;
  uchar hbuff[PES_HDR_SIZE+PES_EXT_SIZE+256];
  uchar type;
  int have, need, old;
  bool unsavedHeader, outputHeader, redirect;
  //
  cFrame *frame;
  const uchar *outData;
  int outCount;
  int saveddata;
  //
  bool ValidRuleset(const int num);
  void Skip(uchar *data, int count=1);
  int Return(int used, int len);
  int HeaderSize(uchar *head, int len);
  int PacketSize(uchar *head, int len);
//RF
  cDvbSubtitleAssembler *dvbSubtitleAssembler;
  int dvbsub_probe(uchar *p, int size, int64_t pts);
  uchar pktbuf[KILOBYTE(64)];
  fifo_t fifo;
  int64_t ptsnow[256];
  int64_t ptsfirst[256];
  int64_t ptslast[256];
  int64_t totsubsize;
  int subspkts;
  int errpkts;

protected:
  bool SOP;        // true if we process the start of packet
  int headerSize;  // size of the header including additional header data
  uchar *header;   // the actual header
  int mpegType;    // gives type of packet 1=mpeg1 2=mpeg2
  int payloadSize; // number of data bytes in the packet
  //
  cRingBufferFrame *rb;
  //
  // Rule Management
  void UseRuleset(int num);
  int CurrentRuleset(void);
  void SetDefaultRule(eRule ru, const int num=0);
  void SetRule(uchar type, eRule ru, const int num=0);
  void SetRuleR(uchar ltype, uchar htype, eRule ru, const int num=0);
  void Reset(void);
  // Misc
  unsigned int Seen(uchar type) const;
  void ClearSeen(void);
  void Statistics(bool);
  void ModifyPaketSize(int mod);
  // Data Processing
  int Process(const uchar *data, int len);
  void Redirect(eRule ru);
  void Clear(void);
  virtual int Output(const uchar *data, int len) { return len; }
  virtual int Action1(uchar type, uchar *data, int len) { return len; }
  virtual int Action2(uchar type, uchar *data, int len) { return len; }
  virtual int Action3(uchar type, uchar *data, int len) { return len; }
  virtual int Action4(uchar type, uchar *data, int len) { return len; }
  virtual int Action5(uchar type, uchar *data, int len) { return len; }
  virtual int Action6(uchar type, uchar *data, int len) { return len; }
  virtual int Action7(uchar type, uchar *data, int len) { return len; }
  virtual int Action8(uchar type, uchar *data, int len) { return len; }
  virtual void Skipped(uchar *data, int len) {}
public:

  cPES(eRule ru=prPass);
  virtual ~cPES();
  };

inline bool PesHasPts(const uchar *p)
{
  return (p[7] & 0x80) && p[8] >= 5;
}

inline bool PesHasDts(const uchar *p)
{
  return (p[7] & 0x40) && p[8] >= 10;
}

inline void PesSetDtsbit(uchar *p)
{
  p[7] |= 0x40;
}

inline void PesSetPtsbit(uchar *p)
{
  p[7] |= 0x80;
}

inline int64_t PesGetPts(const uchar *p)
{
  return ((((int64_t)p[ 9]) & 0x0E) << 29) |
         (( (int64_t)p[10])         << 22) |
         ((((int64_t)p[11]) & 0xFE) << 14) |
         (( (int64_t)p[12])         <<  7) |
         ((((int64_t)p[13]) & 0xFE) >>  1);
}

inline int64_t PesGetDts(const uchar *p)
{
  return ((((int64_t)p[14]) & 0x0E) << 29) |
         (( (int64_t)p[15])         << 22) |
         ((((int64_t)p[16]) & 0xFE) << 14) |
         (( (int64_t)p[17])         <<  7) |
         ((((int64_t)p[18]) & 0xFE) >>  1);
}


#define MAX33BIT  0x00000001FFFFFFFFLL // max. possible value with 33 bit
#define ROUNDUP(N, S) ((N + (S>>1)) / S)

//ffmpeg 
#define FFMIN(a,b) ((a) > (b) ? (b) : (a))

#endif

// --------- $Id: pes.h,v 1.2 2016/09/01 15:37:42 richard Exp $ ---------- END