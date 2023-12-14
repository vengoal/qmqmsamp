/* %Z% %W% %I% %E% %U% */
/********************************************************************/
/*                                                                  */
/* Program name: AMQSPECA                                           */
/*                                                                  */
/* Description: Sample C program to inquire channel names and send  */
/*              a STOP Channel request for each one (except         */
/*              Receiver, client-connecion and cluster-receiver     */
/*              channels)                                           */
/*                                                                  */
/*   <copyright                                                     */
/*   notice="lm-source-program"                                     */
/*   pids="5724-H72,"                                               */
/*   years="1993,2012"                                              */
/*   crc="922795601" >                                              */
/*   Licensed Materials - Property of IBM                           */
/*                                                                  */
/*   5724-H72,                                                      */
/*                                                                  */
/*   (C) Copyright IBM Corp. 1993, 2012 All Rights Reserved.        */
/*                                                                  */
/*   US Government Users Restricted Rights - Use, duplication or    */
/*   disclosure restricted by GSA ADP Schedule Contract with        */
/*   IBM Corp.                                                      */
/*   </copyright>                                                   */
/*                                                                  */
/********************************************************************/
/* Function:                                                        */
/*                                                                  */
/*                                                                  */
/* AMQSPECA uses PCF commands to communicate with a local Queue     */
/* Manager. It inquires the names of all channels and their type,   */
/* then for each channel which is not a Receiver,Client-connection  */
/* or Cluster-receiver  it sends a Stop Channel request             */
/*                                                                  */
/********************************************************************/
/*                                                                  */
/* Pre-requisites:                                                  */
/*                                                                  */
/* This program assumes the Admin queue name is                     */
/*        SYSTEM.ADMIN.COMMAND.QUEUE                                */
/*                                                                  */
/* It uses (and creates if necessary) 2 reply queues:               */
/*        SYSTEM.PCF.INQ.REPLY.QUEUE                                */
/*        SYSTEM.PCF.STOP.REPLY.QUEUE                               */
/*                                                                  */
/* Also the Command Processor must be running.                      */
/*                                                                  */
/********************************************************************/
/*                                                                  */
/* Parameters:                                                      */
/*     QMgr     Queue manager name (optional parameter)             */
/*                  *DFT is the default queue manager               */
/*              Default value is the default queue manager          */
/*                                                                  */
/*     Mode     Shutdown Mode (optional parameter)                  */
/*                  *CNTRLD                                         */
/*                  *IMMED                                          */
/*              Default value is *CNTRLD                            */
/*                                                                  */
/*     Debug    1 for debug print (optional parameter)              */
/*              If debug mode is selected, details of all calls     */
/*              are written to the file QGPL/PCFOUT                 */
/*                                                                  */
/********************************************************************/
/*                                                                  */
/* Usage:                                                           */
/*                                                                  */
/* Status = AMQSPECA(qmgr, mode, debug);                            */
/*                                                                  */
/* Status will be set to -1 on error.                               */
/* Status is set to the number of channels ended on successful      */
/* completion                                                       */
/*                                                                  */
/*                                                                  */
/* 1. CALL QMQM/AMQSPECA ('qmgrname' *CNTRLD)                       */
/*    or                                                            */
/*    CALL QMQM/AMQSPECA ('qmgrname' *IMMED)                        */
/*                                                                  */
/* 2. For a debug print,                                            */
/*    Create the debug file:                                        */
/*    CRTPF FILE(QGPL/PCFOUT) RCDLEN(132) FILETYPE(*SRC) MBR(*NONE) */
/*    Run the program                                               */
/*    CALL QMQM/AMQSPECA ('qmgrname' *CNTRLD '1')                   */
/*                                                                  */
/*    To examine the debug file:                                    */
/*    WRKOBJPDM QGPL PCFOUT                                         */
/*    Select option 12 (Work With)                                  */
/*    Select option 5  (Display)                                    */
/*                                                                  */
/********************************************************************/
/*                                                                  */
/* Exceptions Signalled :  none                                     */
/*                                                                  */
/* Exceptions Monitored :  none                                     */
/*                                                                  */
/* Note that this program does not wait for the channels to actually*/
/* end, it simply requests a stop channel with the appropriate      */
/* quiesce option.                                                  */
/*                                                                  */
/********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>

/** MQ Series Include files **/
#include <cmqc.h>
#include <cmqcfc.h>
#include <cmqxc.h>

/** Define the fixed names **/
#define ADMINQ  "SYSTEM.ADMIN.COMMAND.QUEUE"
#define REPLYQ  "SYSTEM.PCF.INQ.REPLY.QUEUE"
#define REPLYQ2 "SYSTEM.PCF.STOP.REPLY.QUEUE"

/** Define a few macros to save typing **/
#define SETUPHEADER \
    pPCFHeader->Type = MQCFT_COMMAND;\
    pPCFHeader->StrucLength = MQCFH_STRUC_LENGTH;\
    pPCFHeader->Version = MQCFH_VERSION_1;\
    pPCFHeader->MsgSeqNumber = MQCFC_LAST;\
    pPCFHeader->Control = MQCFC_LAST;

#define SETUPSTRING \
    pPCFString->Type = MQCFT_STRING;\
    pPCFString->StrucLength = (MQCFST_STRUC_LENGTH_FIXED +\
                                        StringSize+3)&0xffc;\
    pPCFString->CodedCharSetId = MQCCSI_DEFAULT; \
    pPCFString->StringLength = StringSize;\
    memset(pPCFString->String, ' ', StringSize);

#define SETUPINT \
    pPCFInteger->Type = MQCFT_INTEGER;\
    pPCFInteger->StrucLength = MQCFIN_STRUC_LENGTH;

#define SETUPINTL \
    pPCFIntL->Type = MQCFT_INTEGER_LIST;\
    pPCFIntL->StrucLength = MQCFIL_STRUC_LENGTH_FIXED + LongInt;

/** Define some global structure types **/
typedef struct ChlN
{
  MQCHAR48   ChannelName;
  MQLONG     ChlType;
} ChlName;


/** Define the procedure declarations **/
int    main(int argc, char *argv[]);
MQHOBJ OpenQ(char *QName, int Options);
MQHCONN Connect(char *QMgr, char *QMgrRealName);
int    CreateQ(char *QName, char *QMgr, char *QDesc);
void   CloseQ(MQHOBJ *HObj, char *QName);
void   Disconnect(MQHCONN *hconn, char *Qmgr);
void   ClearReplyQ(MQHOBJ hReplyQ);
int    GetMsgNoWait(MQHOBJ hMsgQ, MQBYTE *pAdminMsg, int AdminMsgLen);
int    GetMsg(MQHOBJ hMsgQ, MQBYTE *pAdminMsg, int AdminMsgLen,
              char *QName);
void   PutMsg(MQHOBJ hAdminQ, MQBYTE *pAdminMsg, MQLONG AdminMsgLen,
              char *ReplyQ);
void   HexDump(MQBYTE *ptr, MQLONG Size);
void   WaitForMessage(MQHOBJ hReplyQ, int Command, char *ReplyQ);
void   dprintf(char *fmt, ...);
char  *DateNTime(void);

int    CloseChannel(MQHOBJ hAdminQ, MQHOBJ hReplyQ, char *QMgr, char *Mode);
int    ProcessChannelReplies(MQHOBJ hAdminQ, MQHOBJ hReplyQ, char *QMgr,
                             char *Mode);
int    SendStop(char *ChannelName, int fQuiesce, MQHOBJ hAdminQ, char *QMgr);

/** Global Debug variables **/
int fDebug;

/** Global connection handle **/
MQHCONN hconn;

#define OUTFILE "QGPL/PCFOUT"
FILE *outfp;

/***********************************************************************/

int main(int argc, char *argv[])
{

  MQHOBJ  hAdminQ;
  MQHOBJ  hReplyQ;
  int     RetVal;
  MQCHAR48 QMgr;
  MQCHAR48 QMgrRealName = " ";

  char    Mode[48];
  int     Count;

  fDebug = 0;
  hconn = MQHC_UNUSABLE_HCONN;
  RetVal = -1;

  if(argc < 2)
  {
    /** No supplied arguments - assume *CNTRLD for shutdown mode **/
    /** Assume default queue manager                             **/
    strcpy(Mode, "*CNTRLD");
    strcpy(QMgr, "*DFT");
  }
  else
  {
    /** Copy the qmgr name **/
    strncpy(QMgr, argv[1], MQ_Q_MGR_NAME_LENGTH);
    strncpy(QMgrRealName, argv[1], MQ_Q_MGR_NAME_LENGTH);

    if(argc > 2)
    {
      /** Copy the shutdown mode **/
      strcpy(Mode, argv[2]);

      if(argc > 4)
      {
        /** Incorrect number of arguments **/
        return(-1);
      }
      else
        if(argc == 4)
      {
        fDebug = atoi(argv[3]);
      }
    }
  }

  if(fDebug)
  {
    outfp = fopen(OUTFILE, "w");
    if(outfp == NULL)
    {
      perror("Unable to open debug file");
      printf("Unable to open debug file %s. errno = %d\n",
             OUTFILE,errno);
      exit(-1);
    }
    dprintf("%sAMQSPECA Start\n", DateNTime() );
    dprintf("%sQueue manager : %s\n", DateNTime(), QMgr );
    dprintf("%sQuiesce mode  : %s\n", DateNTime(), Mode);
  }

  hconn = Connect(QMgr,QMgrRealName);
  if(hconn == MQHC_UNUSABLE_HCONN)
  {
    dprintf("%sFailed to connect to QMgr %s\n", DateNTime(), QMgr);
  }
  else
  {
    hAdminQ = OpenQ(ADMINQ, MQOO_OUTPUT);
    if(hAdminQ == (-1))
    {
      dprintf("%sFailed to open Admin Q %s\n", DateNTime(), ADMINQ);
    }

    hReplyQ = OpenQ(REPLYQ, MQOO_INPUT_EXCLUSIVE);
    if (hReplyQ == (-1))
    {
      RetVal = CreateQ(REPLYQ, QMgr, "MQSeries PCF Reply Queue for ENDCCTJOB(*YES)");
      if (RetVal == 0)
      {
        hReplyQ = OpenQ(REPLYQ, MQOO_INPUT_EXCLUSIVE);
        if (hReplyQ == (-1))
        {
          dprintf("%sFailed to Open Reply Q %s\n", DateNTime(), REPLYQ);
        }
      }
      else
      {
        dprintf("%sFailed to Create Reply Q %s\n", DateNTime(), REPLYQ);
        /* Force program to halt */
        hReplyQ = -1;
      }
    }

    if(hAdminQ != (-1) && hReplyQ != (-1))
    {
      RetVal = CloseChannel(hAdminQ, hReplyQ, QMgr, Mode);
    }

    CloseQ(&hAdminQ, ADMINQ);
    CloseQ(&hReplyQ, REPLYQ);
    Disconnect(&hconn, QMgr);
  }

  if(fDebug)
    fclose(outfp);

  return(RetVal);
}

/** End of procedure main **********************************************/



/***********************************************************************/
/* Connect to the queue manager.                                       */
/* If the default qmgr was requested get the name of it.               */
/***********************************************************************/
MQHCONN Connect(char *QMgr, char *QMgrRealName)
{
  MQLONG  CompCode;
  MQLONG  Reason;
  MQHCONN  localhconn;
  MQHOBJ  hobj = MQHO_UNUSABLE_HOBJ;
  MQLONG  selector = MQCA_Q_MGR_NAME;
  MQOD    objdesc = {MQOD_DEFAULT};

  dprintf("%sConnect %s\n", DateNTime(), QMgr);

  if(strcmp(QMgr,"*DFT") == 0)
  {
    strcpy(QMgr," ");
  }

  MQCONN(QMgr,
         &localhconn,
         &CompCode,
         &Reason);

  if(CompCode == MQCC_FAILED)
  {
    dprintf("%sConnect failed : CC/RC %d %d\n",
            DateNTime(), CompCode, Reason);
  }
  else if(QMgr[0] == ' ')
  {
    objdesc.ObjectType = MQOT_Q_MGR;

    MQOPEN(localhconn,
           &objdesc,
           MQOO_INQUIRE,
           &hobj,
           &CompCode,
           &Reason
          );

    if(CompCode != MQCC_FAILED)
    {
      MQINQ(localhconn,
            hobj,
            1,
            &selector,
            0,
            0,
            MQ_Q_MGR_NAME_LENGTH,
            QMgrRealName,
            &CompCode,
            &Reason
           );

      if(CompCode != MQCC_FAILED)
      {
        dprintf("%sQueue manager : %s\n", DateNTime(), QMgrRealName );
      }
      else
      {
        dprintf("%sMQINQ of qmgr object failed : CC/RC %d %d\n",
                DateNTime(), CompCode, Reason);
      }
    }
    else
    {
      dprintf("%sMQOPEN of qmgr object failed : CC/RC %d %d\n",
              DateNTime(), CompCode, Reason);
    }

    if(hobj != MQHO_UNUSABLE_HOBJ)
    {
      MQCLOSE(localhconn,
              &hobj,
              MQCO_NONE,
              &CompCode,
              &Reason
             );
    }
  }

  return(localhconn);
}

/** End of procedure Connect ******************************************/

void Disconnect(MQHCONN *hconn, char *QMgr)
{
  MQLONG  CompCode;
  MQLONG  Reason;

  if(*hconn != MQHC_UNUSABLE_HCONN)
  {
    dprintf("%sDisconnect %s\n", DateNTime(), QMgr);
    MQDISC(hconn,
           &CompCode,
           &Reason);
  }
}

/** End of procedure Disconnect ***************************************/

MQHOBJ OpenQ(char *QName, int Options)
{
  MQOD    od = {MQOD_DEFAULT};
  MQLONG  O_Options;
  MQLONG  CompCode;
  MQLONG  Reason;
  MQLONG  HObj;

  dprintf("%sOpenQ %s\n", DateNTime(), QName);

  strncpy(od.ObjectName, QName, 48);
  O_Options = Options | MQOO_FAIL_IF_QUIESCING;

  MQOPEN(hconn,
         &od,
         O_Options,
         &HObj,
         &CompCode,
         &Reason);
  if(CompCode == MQCC_FAILED)
  {
    dprintf("%sOpen failed : CC/RC %d %d\n",
            DateNTime(), CompCode, Reason);
    return(-1);
  }
  else
    return(HObj);
}

/** End of procedure OpenQ *********************************************/

int CreateQ(char *QName, char *QMgr, char *QDesc)
{
 int rc = 0;
 char CmdBuff[1024];
 char Except[7];

 dprintf("%sCreateQ %s\n", DateNTime(), REPLYQ);
 memset(CmdBuff,0,sizeof(CmdBuff));

 sprintf(CmdBuff,
        "CRTMQMQ QNAME(%s) QTYPE(*LCL) MQMNAME(%.48s) TEXT('%s')", QName, QMgr, QDesc);
 dprintf("%scmd %s\n", DateNTime(), CmdBuff);

 rc = system(CmdBuff);
 if (rc)
 {
   memcpy(Except, _EXCP_MSGID, 7);
   dprintf("%sError from CRTMQMQ command %7.7s\n", DateNTime(), Except);
 }

 return(rc);

}

/** End of procedure CreateQ *******************************************/

void CloseQ(MQHOBJ *HObj, char *QName)
{
  MQLONG  C_Options;
  MQLONG  CompCode;
  MQLONG  Reason;

  if(*HObj != (-1))
  {
    dprintf("%sCloseQ %s\n", DateNTime(), QName);

    C_Options = 0;

    MQCLOSE(hconn,
            HObj,
            C_Options,
            &CompCode,
            &Reason);
  }
}

/** End of procedure CloseQ ********************************************/

void ClearReplyQ(MQHOBJ hReplyQ)
{
  /** read all the messages on the Reply Q **/

  MQBYTE AdminMsg[55];
  MQLONG AdminMsgLen;
  int    fContinue;

  dprintf("%sClearReplyQ\n", DateNTime() );

  AdminMsgLen = 50;
  fContinue = 1;

  while(fContinue)
  {
    fContinue = GetMsgNoWait(hReplyQ, (MQBYTE *)AdminMsg,
                             AdminMsgLen);
  }
}

/** End of procedure ClearReplyQ ***************************************/

int GetMsgNoWait(MQHOBJ hMsgQ, MQBYTE *pAdminMsg, int AdminMsgLen)
{
  MQMD     md = {MQMD_DEFAULT};
  MQGMO    gmo = {MQGMO_DEFAULT};
  MQLONG   MsgLen;
  MQLONG   CompCode;
  MQLONG   Reason;

  dprintf("%sGetMsgNoWait\n", DateNTime() );

  gmo.Options = MQGMO_NO_WAIT
              | MQGMO_ACCEPT_TRUNCATED_MSG
              | MQGMO_FAIL_IF_QUIESCING
              | MQGMO_NO_SYNCPOINT;

  /** Clear the receive buffer **/
  memset(pAdminMsg, '\0', AdminMsgLen);

  MQGET(hconn,
        hMsgQ,
        &md,
        &gmo,
        AdminMsgLen,
        pAdminMsg,
        &MsgLen,
        &CompCode,
        &Reason);

  if(CompCode != MQCC_OK)
  {
    if(Reason != MQRC_TRUNCATED_MSG_ACCEPTED)
    {
      dprintf("%sMQGET Failed : CC/RC %d %d\n",
              DateNTime(), CompCode, Reason);
      return(0);
    }
    else
      return(1);
  }
  else
    return(1);
}

/** End of procedure GetMsgNoWait **************************************/

int GetMsg(MQHOBJ hMsgQ, MQBYTE *pAdminMsg, int AdminMsgLen, char *QName)
{
  MQMD     md = {MQMD_DEFAULT};
  MQGMO    gmo = {MQGMO_DEFAULT};
  MQLONG   MsgLen;
  MQLONG   CompCode;
  MQLONG   Reason;

  dprintf("%sGetMsg : QName %s\n", DateNTime(), QName);

  gmo.Options = MQGMO_WAIT
              | MQGMO_ACCEPT_TRUNCATED_MSG
              | MQGMO_FAIL_IF_QUIESCING
              | MQGMO_NO_SYNCPOINT;
  gmo.WaitInterval = 30000; /** 30 seconds wait **/

  /** Clear the receive buffer **/
  memset(pAdminMsg, '\0', AdminMsgLen);

  MQGET(hconn,
        hMsgQ,
        &md,
        &gmo,
        AdminMsgLen,
        pAdminMsg,
        &MsgLen,
        &CompCode,
        &Reason);

  HexDump(pAdminMsg, (MsgLen > AdminMsgLen) ? AdminMsgLen : MsgLen);

  if(CompCode != MQCC_OK)
  {
    if(Reason != MQRC_TRUNCATED_MSG_ACCEPTED)
    {
      dprintf("%sMQGET Failed : CC/RC %d %d\n",
              DateNTime(), CompCode, Reason);
      return(0);
    }
    return(MsgLen);
  }
  else
    return(1);
}

/** End of procedure GetMsg ********************************************/

void PutMsg(MQHOBJ hAdminQ, MQBYTE *pAdminMsg, MQLONG AdminMsgLen,
            char *ReplyQ)
{
  MQOD    od = {MQOD_DEFAULT};
  MQMD    md = {MQMD_DEFAULT};
  MQPMO  pmo = {MQPMO_DEFAULT};
  MQLONG CompCode;
  MQLONG Reason;

  dprintf("%sPutMsg : Reply Q %s\n", DateNTime(), ReplyQ);

  memcpy(md.ReplyToQ, ReplyQ, MQ_Q_NAME_LENGTH);
  md.MsgType = MQMT_REQUEST;
  memcpy(md.Format, MQFMT_ADMIN, MQ_FORMAT_LENGTH);

  pmo.Options = MQPMO_FAIL_IF_QUIESCING
              | MQPMO_NO_SYNCPOINT;

  dprintf("Message buffer:\n");
  HexDump((MQBYTE *)pAdminMsg, AdminMsgLen);

  MQPUT(hconn,
        hAdminQ,
        &md,
        &pmo,
        AdminMsgLen,
        pAdminMsg,
        &CompCode,
        &Reason);

  if(CompCode != MQCC_OK)
  {
    dprintf("%sMQPUT failed : CC/RC %d %d\n",
            DateNTime(), CompCode, Reason);
  }
}

/** End of procedure PutMsg ********************************************/

void HexDump(MQBYTE *Ptr, MQLONG Size)
{
  int Index1;
  int Index2;
  char buff[256];

  for(Index1 = 0; (Index1*16)<Size; Index1++)
  {
    sprintf(buff, "%04X ", Index1*16);
    for(Index2=0; Index2<16; Index2++)
    {
      if(((Index1*16)+Index2) < Size)
        sprintf(buff, "%s%02X", buff, Ptr[(Index1*16)+Index2]);
      else
        sprintf(buff, "%s  ", buff);

      if(Index2==7)
        sprintf(buff, "%s ", buff);
    }
    sprintf(buff, "%s  ", buff);
    for(Index2=0; Index2<16; Index2++)
    {
      if(((Index1*16)+Index2)<Size)
        if(isprint(Ptr[(Index1*16)+Index2]))
          sprintf(buff, "%s%c",buff,Ptr[(Index1*16)+Index2]);
        else
          sprintf(buff, "%s.", buff);
      else
        sprintf(buff, "%s ", buff);

      if(Index2==7)
        sprintf(buff, "%s ", buff);
    }
    dprintf("%s\n", buff);
  }
  dprintf("\n");
}

/** End of procedure HexDump *******************************************/

void WaitForMessage(MQHOBJ hReplyQ, int Command, char *ReplyQ)
{
  /** Wait for the last reply to the requested command **/
  /** Ignore any other response                        **/

  MQBYTE *pAdminMsg;
  MQCFH  *pPCFHeader;
  MQLONG  AdminMsgLen;
  int     fContinue;

  dprintf("%sWaitForMessage : Reply Q %s\n", DateNTime(), ReplyQ);

  /** Allocate a big buffer so we can see the whole message **/
  /** GetMsg will hexdump the message if required           **/
  AdminMsgLen = 0x200;
  pAdminMsg = (MQBYTE *)malloc(AdminMsgLen);
  pPCFHeader = (MQCFH *)pAdminMsg;

  fContinue = 1;

  while(fContinue)
  {
    GetMsg(hReplyQ, (MQBYTE *)pAdminMsg, AdminMsgLen, ReplyQ);
    if(pPCFHeader->Command == Command &&
       pPCFHeader->Control == MQCFC_LAST)
    {
      fContinue = 0;
    }
  }

  dprintf("%sGot message\n", DateNTime());

  free(pAdminMsg);    /** Release the buffer **/
}

/** End of procedure WaitForMessage ************************************/

int CloseChannel(MQHOBJ hAdminQ, MQHOBJ hReplyQ, char *QMgr, char *Mode)
{
  /** Make sure the ReplyQ is clear then request  **/
  /** details of all channels                     **/
  /** Send an ENDMQMCHL request for each channel  **/
  /** Ignore Receiver, client-connection and      **/
  /** Cluster-receiver channels                   **/

  MQBYTE  *pAdminMsg;
  MQCFH   *pPCFHeader;
  MQCFST  *pPCFString;

  MQLONG  LongInt = sizeof(LongInt);
  MQLONG  AdminMsgLen;
  int     NumChannels;
  int     StringSize;

  dprintf("%sCloseChannel\n", DateNTime());

  ClearReplyQ(hReplyQ);

  /** Emptied the queue, now ask for the channel details **/

  /** First set up the message pointers **/
  StringSize = MQ_CHANNEL_NAME_LENGTH;
  AdminMsgLen = MQCFH_STRUC_LENGTH +
                MQCFST_STRUC_LENGTH_FIXED +
                (int)((StringSize+3)&0xfffc);

  pAdminMsg   = (MQBYTE *)malloc(AdminMsgLen);
  pPCFHeader  = (MQCFH *)pAdminMsg;
  pPCFString  = (MQCFST *)(pAdminMsg + MQCFH_STRUC_LENGTH);

  /** Set up the request header **/
  SETUPHEADER;
  pPCFHeader->Command = MQCMD_INQUIRE_CHANNEL_STATUS;
  pPCFHeader->ParameterCount = 1;

  /** Set up parameter 1 : Channel Name **/
  SETUPSTRING;
  pPCFString->Parameter = MQCACH_CHANNEL_NAME;
  memcpy(pPCFString->String, "*", 1);    /** ALL channels **/

  /** Send the message **/
  PutMsg(hAdminQ, (MQBYTE *)pAdminMsg, AdminMsgLen, REPLYQ);

  /** Free up the memory we allocated **/
  free(pAdminMsg);

  /** Now we have to wait for the channel information to arrive. **/
  /** For each channel, send an ENDMQMCHL request                **/

  NumChannels = ProcessChannelReplies(hAdminQ, hReplyQ, QMgr, Mode);

  /** Return the number of channels **/
  return(NumChannels);
}

/** End of procedure CloseChannel **************************************/

int ProcessChannelReplies(MQHOBJ hAdminQ, MQHOBJ hReplyQ, char *QMgr, char *Mode)
{
  MQBYTE  pAdminMsg[1000];
  int     ChlCount;
  int     fContinue;
  int     RetVal;
  ChlName ChName;
  MQLONG  ChStatus;
  MQCFH   *pPCFHeader;
  MQCFST  *pPCFString;
  MQCFIN  *pPCFInteger;
  MQCFIL  *pPCFIntL;
  MQLONG  *pPCFType;
  int     Index;
  int     fQuiesce;
  int     StringSize;
  long    LongInt = sizeof(LongInt);

  dprintf("%sProcessChannelReplies\n", DateNTime());

  /** Set message count = 0                        **/
  /** Get a message with 30 sec timeout            **/
  /** while we've got a message                    **/
  /**     if channel type != Receiver, ClientConn  **/
  /**         or Cluster-receiver                  **/
  /**         inc message count                    **/
  /**         send ENDMQMCHL request               **/
  /**     endif                                    **/
  /**     get a message with 30 sec timeout        **/
  /** wend                                         **/
  /** return message count                         **/

  if(strncmp(Mode, "*c", 2) ==0 || strncmp(Mode, "*C", 2) ==0 )
    fQuiesce = MQQO_YES;
  else
    fQuiesce = MQQO_NO;

  ChlCount = 0;

  fContinue = GetMsg(hReplyQ, (MQBYTE *)pAdminMsg, sizeof(pAdminMsg), REPLYQ);
  while(fContinue)
  {
    /** Point to the header **/
    pPCFHeader = (MQCFH *)pAdminMsg;

    /** Point to the first parameter **/
    pPCFType = (MQLONG *)(pAdminMsg+MQCFH_STRUC_LENGTH);

    Index = 1;
    if(pPCFHeader->CompCode != 0)
    {
      if(pPCFHeader->Reason == MQRCCF_CHL_STATUS_NOT_FOUND)
      {
        dprintf("No current channels\n");
      }
      else
        dprintf("Status Failed %d\n", pPCFHeader->Reason);
    }
    else
    {
      /* We always receive the following information back...   */
      /* ChannelName, XmitQName, ConnectionName,               */
      /* ChannelInstanceType, ChannelType and ChannelStatus    */
      /* CHANNEL NAME */
      pPCFString = (MQCFST *)pPCFType;
      memcpy(ChName.ChannelName, pPCFString->String, MQ_CHANNEL_NAME_LENGTH);
      Index++;
      pPCFType = (MQLONG *)((MQBYTE *)pPCFType + pPCFString->StrucLength);
      /* XMITQ NAME */
      pPCFString = (MQCFST *)pPCFType;
      Index++;
      pPCFType = (MQLONG *)((MQBYTE *)pPCFType + pPCFString->StrucLength);
      /* CONNECTION NAME */
      pPCFString = (MQCFST *)pPCFType;
      Index++;
      pPCFType = (MQLONG *)((MQBYTE *)pPCFType + pPCFString->StrucLength);
      /* CHL INSTANCE TYPE */
      pPCFInteger = (MQCFIN *)pPCFType;
      Index++;
      pPCFType = (MQLONG *)((MQBYTE *)pPCFType + pPCFInteger->StrucLength);
      /* CHL TYPE */
      pPCFInteger = (MQCFIN *)pPCFType;
      ChName.ChlType = pPCFInteger->Value;
      Index++;
      pPCFType = (MQLONG *)((MQBYTE *)pPCFType + pPCFInteger->StrucLength);
      /* CHL STATUS */
      pPCFInteger = (MQCFIN *)pPCFType;
      ChStatus = pPCFInteger->Value;
      Index++;
      pPCFType = (MQLONG *)((MQBYTE *)pPCFType + pPCFInteger->StrucLength);

      if(ChName.ChlType != MQCHT_RECEIVER &&
         ChName.ChlType != MQCHT_CLNTCONN &&
         ChName.ChlType != MQCHT_CLUSRCVR)
      {
        if(ChStatus == MQCHS_STOPPED)
        {
          /* No point trying to stop a channel that is already stopped */
          dprintf("Channel %s already stopped\n", ChName.ChannelName);
        }
        else
        {
          /* We wont ever get inactive channels getting stopped as they */
          /* don't have current status...                               */
          ChlCount++;
          if(SendStop(ChName.ChannelName,fQuiesce,hAdminQ,QMgr) == (-1))
            return(-1);
        }
      }
      else
        dprintf("Channel Type %d\n", ChName.ChlType);
    }
    fContinue = GetMsg(hReplyQ, (MQBYTE *)pAdminMsg, sizeof(pAdminMsg), REPLYQ);
  }
  return(ChlCount);
}

/** End of procedure ProcessChannelReplies *****************************/

void dprintf(char *fmt, ...)
{
  char outbuff[256];
  va_list arg_ptr;

  va_start(arg_ptr, fmt);
  vsprintf(outbuff, fmt, arg_ptr);
  va_end(arg_ptr);

  if(fDebug)
  {
    printf("%s", outbuff);
    fputs(outbuff, outfp);
  }
}
/** End of procedure dprintf *******************************************/

char *DateNTime()
{
  static char DateString[50];
  time_t      ltime;

  time(&ltime);
  sprintf(DateString, "%s --- ", asctime(gmtime(&ltime)));

  return(DateString);
}

/** End of procedure DateNTime *****************************************/

int SendStop(char *ChannelName, int fQuiesce, MQHOBJ hAdminQ, char *QMgr)
{
  MQHOBJ  hReplyQ2;
  int     StopMsgLen;
  MQBYTE *pStopMsg;
  MQCFH   *pPCFHeader;
  MQCFST  *pPCFString;
  MQCFIN  *pPCFInteger;
  MQCFIL  *pPCFIntL;
  MQLONG  *pPCFType;
  int     Index, RetVal;
  int     StringSize;
  long    LongInt = sizeof(LongInt);

  dprintf("%sSendStop : %s\n", DateNTime(), ChannelName);

  hReplyQ2 = OpenQ(REPLYQ2, MQOO_INPUT_EXCLUSIVE);
  if (hReplyQ2 == (-1))
  {
    RetVal = CreateQ(REPLYQ2, QMgr, "MQSeries PCF Reply Queue 2 for ENDCCTJOB(*YES)");
    if (RetVal == 0)
    {
      hReplyQ2 = OpenQ(REPLYQ2, MQOO_INPUT_EXCLUSIVE);
      if (hReplyQ2 == (-1))
      {
        dprintf("%sFailed to Open ReplyQ2 %s\n", DateNTime(), REPLYQ2);
        return(-1);
      }
    }
    else
    {
      dprintf("%sFailed to Create ReplyQ2 %s\n", DateNTime(), REPLYQ2);
      return(-1);
    }
  }

  /** First set up the message pointers **/
  StringSize = MQ_CHANNEL_NAME_LENGTH;
  StopMsgLen = MQCFH_STRUC_LENGTH +
               MQCFST_STRUC_LENGTH_FIXED +
               (int)((StringSize+3)&0xfffc) +
               MQCFIN_STRUC_LENGTH;
  pStopMsg    = (MQBYTE *)malloc(StopMsgLen);
  pPCFHeader  = (MQCFH *)pStopMsg;
  pPCFString  = (MQCFST *)(pStopMsg + MQCFH_STRUC_LENGTH);
  pPCFInteger = (MQCFIN *)(pStopMsg +
                           MQCFH_STRUC_LENGTH +
                           MQCFST_STRUC_LENGTH_FIXED +
                           (int)((StringSize+3)&0xfffc) );

  /** Set up the request header **/
  SETUPHEADER;
  pPCFHeader->Command = MQCMD_STOP_CHANNEL;
  pPCFHeader->ParameterCount = 2;

  /** Set up parameter 1 : Channel Name **/
  SETUPSTRING;
  pPCFString->Parameter = MQCACH_CHANNEL_NAME;
  memcpy(pPCFString->String, ChannelName,
         MQ_CHANNEL_NAME_LENGTH);

  /** Set up parameter 2 : Quiesce Option **/
  SETUPINT;
  pPCFInteger->Parameter = MQIACF_QUIESCE;
  pPCFInteger->Value = fQuiesce;

  /** Send the message **/
  PutMsg(hAdminQ, (MQBYTE *)pStopMsg, StopMsgLen, REPLYQ2);

  /** Free up the memory we allocated **/
  free(pStopMsg);

  /** Wait for a reply to this message **/
  WaitForMessage(hReplyQ2, MQCMD_STOP_CHANNEL, REPLYQ2);

  CloseQ(&hReplyQ2, REPLYQ2);

  return(0);
}
