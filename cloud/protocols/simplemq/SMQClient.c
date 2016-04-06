/**
 *     ____             _________                __                _
 *    / __ \___  ____ _/ /_  __(_)___ ___  ___  / /   ____  ____ _(_)____
 *   / /_/ / _ \/ __ `/ / / / / / __ `__ \/ _ \/ /   / __ \/ __ `/ / ___/
 *  / _, _/  __/ /_/ / / / / / / / / / / /  __/ /___/ /_/ / /_/ / / /__
 * /_/ |_|\___/\__,_/_/ /_/ /_/_/ /_/ /_/\___/_____/\____/\__, /_/\___/
 *                                                       /____/
 *
 ****************************************************************************
 *   PROGRAM MODULE
 *
 *   $Id: SMQClient.c 3638 2015-01-05 18:21:33Z wini $
 *
 *   COPYRIGHT:  Real Time Logic LLC, 2014
 *
 *   This software is copyrighted by and is the sole property of Real
 *   Time Logic LLC.  All rights, title, ownership, or other interests in
 *   the software remain the property of Real Time Logic LLC.  This
 *   software may only be used in accordance with the terms and
 *   conditions stipulated in the corresponding license agreement under
 *   which the software has been supplied.  Any unauthorized use,
 *   duplication, transmission, distribution, or disclosure of this
 *   software is expressly forbidden.
 *                                                                        
 *   This Copyright notice may not be removed or modified without prior
 *   written consent of Real Time Logic LLC.
 *                                                                         
 *   Real Time Logic LLC. reserves the right to modify this software
 *   without notice.
 *
 *               https://realtimelogic.com
 ****************************************************************************  
 */

#include <ctype.h>
#include "SMQClient.h"


#define MSG_INIT         1
#define MSG_CONNECT      2
#define MSG_CONNACK      3
#define MSG_SUBSCRIBE    4
#define MSG_SUBACK       5
#define MSG_CREATE       6
#define MSG_CREATEACK    7
#define MSG_PUBLISH      8
#define MSG_UNSUBSCRIBE  9
#define MSG_DISCONNECT   11
#define MSG_PING         12
#define MSG_PONG         13
#define MSG_OBSERVE      14
#define MSG_UNOBSERVE    15
#define MSG_CHANGE       16
#define MSG_CREATESUB    17
#define MSG_CREATESUBACK 18
#define MSG_PUBFRAG      19



#define SMQ_VERSION 1

#if defined(B_LITTLE_ENDIAN)
static void
netConvU16(U8* out, const U8* in)
{
   out[0] = in[1];
   out[1] = in[0];
}
static void
netConvU32(U8* out, const U8* in)
{
   out[0] = in[3];
   out[1] = in[2];
   out[2] = in[1];
   out[3] = in[0];
}
#elif defined(B_BIG_ENDIAN)
#define netConvU16(out, in) memcpy(out,in,2)
#define netConvU32(out, in) memcpy(out,in,4)
#else
#error ENDIAN_NEEDED_Define_one_of_B_BIG_ENDIAN_or_B_LITTLE_ENDIAN
#endif

#define SMQ_resetb(o) (o)->bufIx=0

static int
SMQ_recv(SMQ* o, U8* buf, int len)
{
   o->inRecv=TRUE;
   len = se_recv(&o->sock, buf, len, o->timeout);
   o->inRecv=FALSE;
   return len;
}


/* Reads and stores the 3 frame header bytes (len:2 & msg:1) in the buffer.
   Returns zero on success and a value (error code) less than zero on error
 */
static int
SMQ_readFrameHeader(SMQ* o)
{
   int x;
   SMQ_resetb(o);
   do
   {
      x=SMQ_recv(o, o->buf, 3 - o->bufIx);
      o->bufIx += (U16)x; /* assume it's OK */
   } while(x > 0 && o->bufIx < 3);
   if(x > 0)
   {
      netConvU16((U8*)&o->frameLen, o->buf);
      return 0;
   }
   o->status = x == 0 ? SMQE_TIMEOUT : x;
   return o->status;
}



/* Reads a complete frame.
   Designed to be used by control frames.

   Returns Frame Len (FL) on success and a value (error code) less
   than zero on error.
*/
static int
SMQ_readFrame(SMQ* o, int hasFH)
{
   int x;
   if(!hasFH && SMQ_readFrameHeader(o)) return o->status;
   if(o->frameLen > o->bufLen || o->frameLen < 3)
      return o->status = SMQE_BUF_OVERFLOW;
   do
   {
      x=SMQ_recv(o, o->buf+o->bufIx, o->frameLen - o->bufIx);
      o->bufIx += (U16)x; /* assume it's OK */
   } while(x > 0 && o->bufIx < o->frameLen);
   SMQ_resetb(o);
   if(x > 0) return 0;
   o->status = x == 0 ? SMQE_TIMEOUT : x;
   return o->status;
}


static int
SMQ_readData(SMQ* o, U16 size)
{
   int x;
   do {
      x=SMQ_recv(o, o->buf+o->bufIx, size-o->bufIx);
      o->bufIx += (U16)x; /* assume it's OK */
   } while(x > 0 && o->bufIx < size);
   if(x > 0) return o->bufIx;
   o->status = x == 0 ? SMQE_TIMEOUT : x;
   return o->status;
}



static int
SMQ_flushb(SMQ* o)
{
   if(o->bufIx)
   {
      int x = se_send(&o->sock, o->buf, o->bufIx);
      SMQ_resetb(o);
      if(x < 0)
      {
         o->status = x;
         return x;
      }
   }
   return 0;
}



static int
SMQ_putb(SMQ* o, const void* data, int len)
{
   if(len < 0)
      len = strlen((char*)data);
   if(o->bufLen <= (o->bufIx + len))
      return -1;
   memcpy(o->buf + o->bufIx, data, len);
   o->bufIx += (U16)len;
   return 0;
}



static int
SMQ_writeb(SMQ* o, const void* data, int len)
{
   if(len < 0)
      len = strlen((char*)data);
   if(o->bufLen <= (o->bufIx + len))
   {
      if(SMQ_flushb(o)) return o->status;
      if((len+20) >= o->bufLen)
      {
         len=se_send(&o->sock, data, len);
         if(len < 0)
         {
            o->status = len;
            return len;
         }
         return 0;
      }
   }
   memcpy(o->buf + o->bufIx, data, len);
   o->bufIx += (U16)len;
   return 0;
}


void
SMQ_constructor(SMQ* o, U8* buf, U16 bufLen)
{
   memset(o, 0, sizeof(SMQ));
   o->buf = buf;
   o->bufLen = bufLen;
   o->timeout = 60 * 1000;
   o->pingTmo = 20 * 60 * 1000;
}


int
SMQ_init(SMQ* o, const char* url, U32* rnd)
{
   int x;
   const char* path;
   const char* eohn; /* End Of Hostname */
   U16 portNo=0;
   if(strncmp("http://",url,7))
      return SMQE_INVALID_URL;
   url+=7;
   path=strchr((char*)url, '/');
   if(!path)
      return o->status = SMQE_INVALID_URL;

   if(path > url && isdigit((unsigned char)*(path-1)))
   {
      for(eohn = path-2 ; ; eohn--)
      {
         if(path > url)
         {
            if( ! isdigit((unsigned char)*eohn) )
            {
               const char* ptr = eohn;
               if(*ptr != ':')
                  goto L_defPorts;
               while(++ptr < path)
                  portNo = 10 * portNo + (*ptr-'0');
               break;
            }
         }
         else
            return o->status = SMQE_INVALID_URL;
      }
   }
   else
   {
L_defPorts:
      portNo=80;
      eohn=path; /* end of eohn */
   }
   /* Write hostname */

   SMQ_resetb(o);
   o->bufIx = (U16)(eohn-url); /* save hostname len */
   if((o->bufIx+1) >= o->bufLen)
      return o->status = SMQE_BUF_OVERFLOW;
   memcpy(o->buf, url, o->bufIx); 
   o->buf[o->bufIx]=0;

   /* connect to 'hostname' */
   if( (x = se_connect(&o->sock, (char*)o->buf, portNo)) != 0 )
      return o->status = x;

   /* Send HTTP header. Host is included for multihomed servers */
   SMQ_resetb(o);
   if(SMQ_writeb(o, SMQSTR("GET ")) ||
      SMQ_writeb(o, path, -1) ||
      SMQ_writeb(o,SMQSTR(" HTTP/1.0\r\nHost: ")) ||
      SMQ_writeb(o, url, eohn-url) ||
      SMQ_writeb(o, SMQSTR("\r\nSimpleMQ: 1\r\n")) ||
      SMQ_writeb(o, SMQSTR("User-Agent: SimpleMQ/1\r\n\r\n")) ||
      SMQ_flushb(o))
   {
      return o->status;
   }

   /* Get the Init message */
   if(SMQ_readFrame(o, FALSE)) return o->status;
   if(o->frameLen < 14 || o->buf[2] != MSG_INIT || o->buf[3] != SMQ_VERSION)
      return SMQE_PROTOCOL_ERROR;
   if(rnd)
      netConvU32((U8*)rnd,o->buf+4);
   memmove(o->buf, o->buf+8, o->frameLen-8);
   o->buf[o->frameLen-8]=0;
   return 0;
}



int
SMQ_connect(SMQ* o, const char* uid, int uidLen, const char* credentials,
            U8 credLen, const char* info, int infoLen)
{
   if(o->bufLen < 5+uidLen+credLen+infoLen) return SMQE_BUF_OVERFLOW;
   o->bufIx = 2;
   o->buf[o->bufIx++] = MSG_CONNECT;
   o->buf[o->bufIx++] = SMQ_VERSION;
   o->buf[o->bufIx++] = (U8)uidLen;
   SMQ_putb(o,uid,uidLen);
   o->buf[o->bufIx++] = credLen;
   if(credLen)
      SMQ_putb(o,credentials,credLen);
   if(info)
      SMQ_putb(o,info,infoLen);
   netConvU16(o->buf, (U8*)&o->bufIx); /* Frame Len */
   if(SMQ_flushb(o)) return o->status;

   /* Get the response message Connack */
   if(SMQ_readFrame(o, FALSE)) return o->status;
   if(o->frameLen < 8 || o->buf[2] != MSG_CONNACK)
      return SMQE_PROTOCOL_ERROR;
   netConvU32((U8*)&o->clientTid, o->buf+4);
   if(o->buf[3] != 0)
   {
      memmove(o->buf, o->buf+8, o->frameLen-8);
      o->buf[o->frameLen-8]=0;
   }
   else
      o->buf[0]=0; /* No error message */
   o->status = (int)o->buf[3]; /* OK or error code */
   return o->status;
}


void
SMQ_disconnect(SMQ* o)
{
   o->bufIx = 3;
   netConvU16(o->buf, (U8*)&o->bufIx); /* Frame Len */
   o->buf[2] = MSG_DISCONNECT;
   SMQ_flushb(o);
}


void
SMQ_destructor(SMQ* o)
{
   se_close(&o->sock);
}

/* Send MSG_SUBSCRIBE, MSG_CREATE, or MSG_CREATESUB */
static int
SMQ_subOrCreate(SMQ* o,const char* topic,int msg)
{
   int len = strlen(topic);
   if( ! len ) return SMQE_PROTOCOL_ERROR;
   if((3+len) > o->bufLen) return SMQE_BUF_OVERFLOW;
   o->bufIx = 2;
   o->buf[o->bufIx++] = (U8)msg;
   SMQ_putb(o,topic,len);
   netConvU16(o->buf, (U8*)&o->bufIx); /* Frame Len */
   return SMQ_flushb(o);
}


int
SMQ_subscribe(SMQ* o, const char* topic)
{
   return SMQ_subOrCreate(o,topic, MSG_SUBSCRIBE);
}


int
SMQ_create(SMQ* o, const char* topic)
{
   return SMQ_subOrCreate(o,topic,MSG_CREATE);
}


int
SMQ_createsub(SMQ* o, const char* topic)
{
   return SMQ_subOrCreate(o,topic,MSG_CREATESUB);
}


static int
SMQ_sendMsgWithTid(SMQ* o, int msgType, U32 tid)
{
   o->bufIx=7;
   netConvU16(o->buf, (U8*)&o->bufIx); /* Frame Len */
   o->buf[2] = (U8)msgType;
   netConvU32(o->buf+3, (U8*)&tid);
   return SMQ_flushb(o) ? o->status : 0;
}


int
SMQ_unsubscribe(SMQ* o, U32 tid)
{
   return SMQ_sendMsgWithTid(o, MSG_UNSUBSCRIBE, tid);
}


int
SMQ_publish(SMQ* o, const void* data, int len, U32 tid, U32 subtid)
{
   U16 tlen=(U16)len+15;
   if(tlen <= o->bufLen && ! o->inRecv)
   {
      netConvU16(o->buf, (U8*)&tlen); /* Frame Len */
      o->buf[2] = MSG_PUBLISH;
      netConvU32(o->buf+3, (U8*)&tid);
      netConvU32(o->buf+7,(U8*)&o->clientTid);
      netConvU32(o->buf+11,(U8*)&subtid);
      o->bufIx = 15;
      if(SMQ_writeb(o, data, len) || SMQ_flushb(o)) return o->status;
   }
   else
   {
      U8 buf[15];
      netConvU16(buf, (U8*)&tlen); /* Frame Len */
      buf[2] = MSG_PUBLISH;
      netConvU32(buf+3, (U8*)&tid);
      netConvU32(buf+7,(U8*)&o->clientTid);
      netConvU32(buf+11,(U8*)&subtid);
      o->status=se_send(&o->sock, buf, 15);
      if(o->status < 0) return o->status;
      o->status=se_send(&o->sock, data, len);
      if(o->status < 0) return o->status;
      o->status=0;
   }
   return 0;
}


int
SMQ_wrtstr(SMQ* o, const char* data)
{
   return SMQ_write(o,data,strlen(data));
}


int
SMQ_write(SMQ* o,  const void* data, int len)
{
   U8* ptr = (U8*)data;
   if(o->inRecv)
      return SMQE_PROTOCOL_ERROR;
   while(len > 0)
   {
      int chunk,left;
      if(!o->bufIx)
         o->bufIx = 15;
      left = o->bufLen - o->bufIx;
      chunk = len > left ? left : len;
      memcpy(o->buf+o->bufIx, ptr, chunk);
      ptr += chunk;
      len -= chunk;
      o->bufIx += (U16)chunk;
      if(o->bufIx >= o->bufLen && SMQ_pubflush(o, 0, 0))
         return o->status;
   }
   return 0;
}


int
SMQ_pubflush(SMQ* o, U32 tid, U32 subtid)
{
   if(!o->bufIx)
      o->bufIx = 15;
   netConvU16(o->buf, (U8*)&o->bufIx); /* Frame Len */
   o->buf[2] = MSG_PUBFRAG;
   netConvU32(o->buf+3, (U8*)&tid);
   netConvU32(o->buf+7,(U8*)&o->clientTid);
   netConvU32(o->buf+11,(U8*)&subtid);
   o->status=se_send(&o->sock, o->buf, o->bufIx);
   SMQ_resetb(o);
   if(o->status < 0) return o->status;
   o->status=0;
   return 0;
}



int
SMQ_observe(SMQ* o, U32 tid)
{
   return SMQ_sendMsgWithTid(o, MSG_OBSERVE, tid);
}


int
SMQ_unobserve(SMQ* o, U32 tid)
{
   return SMQ_sendMsgWithTid(o, MSG_UNOBSERVE, tid);
}


int
SMQ_getMessage(SMQ* o, U8** msg)
{
   int x;

   if(o->bytesRead)
   {
      if(o->bytesRead < o->frameLen)
      {
         U16 size = o->frameLen - o->bytesRead;
         *msg = o->buf;
         x=SMQ_readData(o, size <= o->bufLen ? size : o->bufLen);
         SMQ_resetb(o);
         if(x > 0) o->bytesRead += (U16)x;
         else o->bytesRead = 0;
         return x;
      }
      o->bytesRead = 0;
   }

  L_readMore:
   if(SMQ_readFrameHeader(o))
   {
      /* Timeout is not an error in between frames */
      if(o->status == SMQE_TIMEOUT)
      {
         if(o->pingTmoCounter >= 0)
         {
            o->pingTmoCounter += o->timeout;
            if(o->pingTmoCounter >= o->pingTmo)
            {
               o->pingTmoCounter = -10000; /* PONG tmo hard coded to 10 sec */
               o->bufIx=3;
               netConvU16(o->buf, (U8*)&o->bufIx); /* Frame Len */
               o->buf[2] = MSG_PING;
               o->status=se_send(&o->sock, o->buf, o->bufIx);
               SMQ_resetb(o);
               if(o->status < 0) return o->status;
            }
         }
         else
         {
            o->pingTmoCounter += o->timeout;
            if(o->pingTmoCounter >= 0)
               return SMQE_PONGTIMEOUT;
         }
         return 0;
      }
      return o->status;
   }
   o->pingTmoCounter=0;
   switch(o->buf[2])
   {
      case MSG_DISCONNECT:
         if(SMQ_readFrame(o, TRUE))
            o->buf[0]=0;
         else
         {
            memmove(o->buf, o->buf+3, o->frameLen-3);
            o->buf[o->frameLen-3]=0;
         }
         return SMQE_DISCONNECT;

      case MSG_CREATEACK:
      case MSG_CREATESUBACK:
      case MSG_SUBACK:
         if(SMQ_readFrame(o, TRUE)) return o->status;
         if(o->frameLen < 9) return SMQE_PROTOCOL_ERROR;
         if(msg) *msg = o->buf; /* topic name */
         if(o->buf[3]) /* Denied */
         {
            o->ptid=0;
            o->status = -1;
         }
         else
         {
            netConvU32((U8*)&o->ptid, o->buf+4);
            o->status = 0;
         }
         switch(o->buf[2])
         {
            case MSG_CREATEACK:    x = SMQ_CREATEACK;    break;
            case MSG_CREATESUBACK: x = SMQ_CREATESUBACK; break;
            default: x = SMQ_SUBACK;
         }
         memmove(o->buf, o->buf+8, o->frameLen-8);
         o->buf[o->frameLen-8]=0;
         return x;

      case MSG_PUBLISH:
         if(o->frameLen < 15) return SMQE_PROTOCOL_ERROR;
         o->bytesRead = o->frameLen <= o->bufLen ? o->frameLen : o->bufLen;
         x=SMQ_readData(o, o->bytesRead);
         SMQ_resetb(o);
         if(x > 0)
         {
            netConvU32((U8*)&o->tid, o->buf+3);
            netConvU32((U8*)&o->ptid, o->buf+7);
            netConvU32((U8*)&o->subtid, o->buf+11);
            *msg = o->buf + 15;
            return o->bytesRead - 15;
         }
         o->bytesRead=0;
         return x < 0 ? x : -1;

      case MSG_PING:
      case MSG_PONG:
         if(o->frameLen != 3) return SMQE_PROTOCOL_ERROR;
         if(o->buf[2] == MSG_PING)
         {
            o->buf[2] = MSG_PONG;
            if(SMQ_flushb(o)) return o->status;
         }
         goto L_readMore;

      case MSG_CHANGE:
         if(o->frameLen != 11) return SMQE_PROTOCOL_ERROR;
         if(SMQ_readFrame(o, TRUE)) return o->status;
            netConvU32((U8*)&o->ptid, o->buf+7);
            o->status = (int)o->ptid;
            netConvU32((U8*)&o->ptid, o->buf+3);
            return SMQ_SUBCHANGE;

      default:
         return SMQE_PROTOCOL_ERROR;
   }
}
