/* %Z% %W% %I% %E% %U% */
/********************************************************************/
/*                                                                  */
/* Program name: AMQSSTOP                                           */
/*                                                                  */
/* Description: Sample C program that issues a STOP CONN            */
/*              request to the command server for all connections   */
/*              for the specified process.                          */
/*   <copyright                                                     */
/*   notice="lm-source-program"                                     */
/*   pids="5724-H72,"                                               */
/*   years="1994,2012"                                              */
/*   crc="2142022968" >                                             */
/*   Licensed Materials - Property of IBM                           */
/*                                                                  */
/*   5724-H72,                                                      */
/*                                                                  */
/*   (C) Copyright IBM Corp. 1994, 2012 All Rights Reserved.        */
/*                                                                  */
/*   US Government Users Restricted Rights - Use, duplication or    */
/*   disclosure restricted by GSA ADP Schedule Contract with        */
/*   IBM Corp.                                                      */
/*   </copyright>                                                   */
/********************************************************************/
/*                                                                  */
/* Function:                                                        */
/*                                                                  */
/*   AMQSSTP0 is a sample C program to send a series of requests    */
/*   to the command server to inquire on all of the connections     */
/*   for a selected process and then request the connections to     */
/*   be stopped.                                                    */
/*                                                                  */
/*   Upon successful completion, further requests to MQ by the      */
/*   process using any existing MQ connections will fail with       */
/*   with the Reason code MQRC_CONNECTION_BROKEN.                   */
/*                                                                  */
/*   Usage:                                                         */
/*       amqsstop -p <pid> -m <qmgr>                                */
/*                                                                  */
/*    Program logic:                                                */
/*       Main                                                       */
/*         MQOPEN SYSTEM.DEFAULT.MODEL.QUEUE for INPUT              */
/*         MQOPEN SYSTEM.ADMIN.COMMAND.QUEUE for OUTPUT             */
/*         MQPUT 'INQUIRE CONN where PID = pid' pcf command         */
/*         while not last messgae                                   */
/*         .  MQGET reply                                           */
/*         .  StopConnection(reply.ConnId)                          */
/*         MQCLOSE reply queue                                      */
/*         MQCLOSE request queue                                    */
/*                                                                  */
/*       StopConnection()                                           */
/*         MQPUT 'STOP CONN(ConnId)' pcf command                    */
/*         MQGET reply                                              */
/*                                                                  */
/********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
   /* includes for MQI */
#include <cmqc.h>
#include <cmqcfc.h>

#define BUFFER_LENGTH 2048

static MQMD     DefaultMD  = {MQMD_DEFAULT};  /* Default MD structure    */
static MQOD     DefaultOD  = {MQOD_DEFAULT};  /* Default MD structure    */
static MQGMO    DefaultGMO = {MQGMO_DEFAULT}; /* Default GMO structure   */
static MQPMO    DefaultPMO = {MQPMO_DEFAULT}; /* Default PMO structure   */
static MQCFH    DefaultCFH = {MQCFH_DEFAULT}; /* Default CFH structure   */

/********************************************************************/
/* Stop connection function prototype                               */
/********************************************************************/
int StopConnection(MQHCONN Hcon,
                   MQCHAR *pCmdQName,
                   MQHOBJ HCmdQ,
                   MQCHAR *pReplyQName,
                   MQHOBJ HReplyQ,
                   MQBYTE ConnectionId[MQ_CONNECTION_ID_LENGTH]);

/********************************************************************/
/* Main module                                                      */
/********************************************************************/
int main(int argc, char **argv)
{

  MQCHAR   qmgrName[MQ_Q_MGR_NAME_LENGTH+1]=""; /* Qmgr name        */
  MQCHAR   ReplyQueue[]="SYSTEM.DEFAULT.MODEL.QUEUE";
                                   /* Reply queue                   */
  MQCHAR   CmdQueue[]="SYSTEM.ADMIN.COMMAND.QUEUE";
  MQCHAR48 ResolvedReplyQ;         /* Resolved reply Q name         */

  MQHCONN  Hcon;                   /* connection handle             */
  MQHOBJ   HReplyQ;                /* Reply queue handle            */
  MQHOBJ   HCmdQ;                  /* Request queue handle          */
  MQLONG   C_options = MQCO_NONE;  /* MQCLOSE options               */
  MQBYTE   RequestMsgId[MQ_MSG_ID_LENGTH]; /* Request msg id        */
  MQGMO    gmo;                    /* Get message options           */
  MQPMO    pmo;                    /* Put message options           */
  MQOD     od;                     /* Object Descriptor             */
  MQMD     md;                     /* Message Descriptor            */
  MQLONG   O_options;              /* MQOPEN options                */
  MQLONG   CompCode;               /* completion code               */
  MQLONG   Reason;                 /* reason code                   */
  MQLONG   ExitReason=0;           /* reason code from Stop Request */

  MQCHAR   buffer[BUFFER_LENGTH];  /* Message buffer                */
  MQLONG   messlen;                /* message length                */
  MQLONG   buflen;                 /* buffer length                 */
  MQLONG   offset;                 /* Offset within message buffer  */
  MQLONG   count;                  /* Loop counter                  */
  char     *parg;                  /* Pointer to argument           */
  int      ProcessId=0;            /* Pid of target process         */
  MQLONG   moreResponses;          /* Loop control variable         */
  MQLONG   inqSelectors[1];        /* MQINQ selections              */
  MQLONG   openInputCount;         /* Number of open handles on Q   */

  MQCFH    *pCfh;                  /* Pointer to MQCFH header       */
  MQCFBS   *pCfbs;                 /* Pointer to CFBS parameter     */
  MQCFBS   *pConnId;               /* Pointer to ConnectionId struct*/
  MQCFIF   *pFilter;               /* Pointer to Filter structure   */
  MQCFIL   *pFieldList;            /* Pointer to Field List struct  */

  /******************************************************************/
  /*   Parse arguments                                              */
  /******************************************************************/
  for (count=1; count < argc; count++)
  {
    if (strncmp("-m", argv[count], 2) == 0)
    {
      if ((*(parg=&(argv[count][2])) == '\0') && (count+1 < argc))
      {
        parg=argv[++count];
      }

      strncpy(qmgrName, parg, MQ_Q_MGR_NAME_LENGTH);
    }
    else if (strncmp("-p", argv[count], 2) == 0)
    {
      if ((*(parg=&(argv[count][2])) == '\0') && (count+1 < argc))
      {
        parg=argv[++count];
      }

      ProcessId=atoi(parg);
    }
  }

  if (ProcessId == 0)
  {
    printf("Usage: amqsstop [-m <QMgrName>] -p <ProcessId>\n");
    exit(99);
  }

  /******************************************************************/
  /*   Connect to queue manager                                     */
  /******************************************************************/
  MQCONN(qmgrName,                /* queue manager                  */
         &Hcon,                   /* connection handle              */
         &CompCode,               /* completion code                */
         &Reason);                /* reason code                    */

  /* report reason and stop if it failed     */
  if (CompCode == MQCC_FAILED)
  {
    printf("MQCONN ended with reason code %d\n", Reason);
    exit( (int)Reason );
  }

  /******************************************************************/
  /*   Open the reply queue                                         */
  /******************************************************************/
  memcpy(&od, &DefaultOD, sizeof(MQOD));
  strcpy(od.ObjectName, ReplyQueue);
  O_options = MQOO_INPUT_EXCLUSIVE | MQOO_FAIL_IF_QUIESCING;

  MQOPEN(Hcon,                      /* connection handle            */
         &od,                       /* object descriptor for queue  */
         O_options,                 /* open options                 */
         &HReplyQ,                  /* object handle                */
         &CompCode,                 /* MQOPEN completion code       */
         &Reason);                  /* reason code                  */

  /* report reason, if any; stop if failed      */
  if (CompCode == MQCC_FAILED)
  {
    printf("MQOPEN(%s) ended with reason code %d\n", od.ObjectName, Reason);
    exit( (int)Reason );
  }
  strncpy(ResolvedReplyQ, od.ObjectName, MQ_Q_NAME_LENGTH);

  /******************************************************************/
  /*   Open the command queue                                       */
  /******************************************************************/
  memcpy(&od, &DefaultOD, sizeof(MQOD));
  strcpy(od.ObjectName, CmdQueue);
  O_options = MQOO_OUTPUT | MQOO_INQUIRE | MQOO_FAIL_IF_QUIESCING;

  MQOPEN(Hcon,                      /* connection handle            */
         &od,                       /* object descriptor for queue  */
         O_options,                 /* open options                 */
         &HCmdQ,                    /* object handle                */
         &CompCode,                 /* MQOPEN completion code       */
         &Reason);                  /* reason code                  */

  /* report reason, if any; stop if failed      */
  if (CompCode == MQCC_FAILED)
  {
    printf("MQOPEN(%s) ended with reason code %d\n", od.ObjectName, Reason);
    exit( (int)Reason );
  }

  /******************************************************************/
  /* Prepare the request message                                    */
  /******************************************************************/
  pCfh=(MQCFH *)buffer;
  *pCfh=DefaultCFH;
  pCfh->Command=MQCMD_INQUIRE_CONNECTION;
  offset=sizeof(MQCFH);

  pConnId=(MQCFBS *)(buffer+offset);
  pConnId->Type=MQCFT_BYTE_STRING;
  pConnId->StrucLength=MQCFBS_STRUC_LENGTH_FIXED;
  pConnId->Parameter=MQBACF_GENERIC_CONNECTION_ID;
  pConnId->StringLength=0;
  pCfh->ParameterCount++;

  offset+=MQCFBS_STRUC_LENGTH_FIXED;

  pFilter=(MQCFIF *)(buffer+offset);
  pFilter->Type=MQCFT_INTEGER_FILTER;
  pFilter->StrucLength=MQCFIF_STRUC_LENGTH;
  pFilter->Parameter=MQIACF_PROCESS_ID;
  pFilter->Operator=MQCFOP_EQUAL;
  pFilter->FilterValue=ProcessId;
  pCfh->ParameterCount++;

  offset+=MQCFIF_STRUC_LENGTH;

  pFieldList=(MQCFIL *)(buffer+offset);
  pFieldList->Type=MQCFT_INTEGER_LIST;
  pFieldList->StrucLength=MQCFIL_STRUC_LENGTH_FIXED + (sizeof(MQLONG) * 2);
  pFieldList->Parameter=MQIACF_CONNECTION_ATTRS;
  pFieldList->Count=2;
  pFieldList->Values[0]=MQIACF_PROCESS_ID;
  pFieldList->Values[1]=MQBACF_CONNECTION_ID;
  pCfh->ParameterCount++;

  offset+=MQCFIL_STRUC_LENGTH_FIXED + (sizeof(MQLONG) * 2);

  /******************************************************************/
  /* Attempt to verify that the command server is running by seeing */
  /* how many processes have the COMMAND Queue open for input. It   */
  /* should be at least one if the command server is running.       */
  /******************************************************************/
  inqSelectors[0]=MQIA_OPEN_INPUT_COUNT;

  MQINQ(Hcon,
        HCmdQ,
        1,
        inqSelectors,
        1,
        &openInputCount,
        0,
        NULL,
        &CompCode,
        &Reason);

  if (CompCode == MQCC_FAILED)
  {
    printf("MQINQ(%s) ended with reason code %d\n", CmdQueue, Reason);
    exit( (int)Reason );
  }

  if (openInputCount == 0)
  {
    printf("Cannot exexute command. Command server is not running\n");
    exit((int)MQRC_INQUIRY_COMMAND_ERROR);
  }

  /******************************************************************/
  /* Put the command to the COMMAND.QUEUE                           */
  /******************************************************************/
  memcpy(&md, &DefaultMD, sizeof(MQMD));
  memcpy(md.Format, MQFMT_ADMIN, (size_t)MQ_FORMAT_LENGTH);
  strncpy(md.ReplyToQ, ResolvedReplyQ, MQ_Q_NAME_LENGTH);
  md.MsgType = MQMT_REQUEST;
  md.Report = MQRO_COPY_MSG_ID_TO_CORREL_ID;

  memcpy(&pmo, &DefaultPMO, sizeof(MQPMO));
  pmo.Options |= MQPMO_NO_SYNCPOINT;
  pmo.Options |= MQPMO_NEW_MSG_ID;
  pmo.Options |= MQPMO_NEW_CORREL_ID;

  MQPUT(Hcon,                /* connection handle               */
        HCmdQ,               /* object handle                   */
        &md,                 /* message descriptor              */
        &pmo,                /* default options (datagram)      */
        offset,              /* message length                  */
        buffer,              /* message buffer                  */
        &CompCode,           /* completion code                 */
        &Reason);            /* reason code                     */

  /* report reason, if any */
  if (CompCode == MQCC_FAILED)
  {
    printf("MQOPEN(%s) ended with reason code %d\n", CmdQueue, Reason);
    exit( (int)Reason );
  }
  memcpy(RequestMsgId, md.MsgId, MQ_MSG_ID_LENGTH);

  /******************************************************************/
  /* Retrieve the replies                                           */
  /* We will wait up to 15 seconds for each response from the       */
  /* command server.                                                */
  /******************************************************************/
  do
  {
    moreResponses=0;
    buflen = sizeof(buffer) - 1; /* buffer size available for GET   */

    memcpy(&md, &DefaultMD, sizeof(MQMD));
    memcpy(md.MsgId, MQMI_NONE, MQ_MSG_ID_LENGTH);
    memcpy(md.CorrelId, RequestMsgId, MQ_MSG_ID_LENGTH);

    md.Encoding       = MQENC_NATIVE;
    md.CodedCharSetId = MQCCSI_Q_MGR;

    memcpy(&gmo, &DefaultGMO, sizeof(MQGMO));/* Initialize GMO      */
    gmo.Version = MQGMO_VERSION_2;
    gmo.MatchOptions = MQMO_MATCH_CORREL_ID;
    gmo.Options = MQGMO_WAIT       /* wait for new messages         */
                | MQGMO_CONVERT    /* convert if necessary          */
                | MQGMO_NO_SYNCPOINT; /* No Syncpoint               */
    gmo.WaitInterval = 15000;      /* 15 second limit for waiting   */

    MQGET(Hcon,                /* connection handle                 */
          HReplyQ,             /* object handle                     */
          &md,                 /* message descriptor                */
          &gmo,                /* get message options               */
          buflen,              /* buffer length                     */
          buffer,              /* message buffer                    */
          &messlen,            /* message length                    */
          &CompCode,           /* completion code                   */
          &Reason);            /* reason code                       */

    if (Reason == MQRC_NONE)
    {
      /**************************************************************/
      /* We have a response from the command server so issue the    */
      /* stop connection request.                                   */
      /**************************************************************/
      pCfh=(MQCFH *)buffer;

      if (pCfh->CompCode != MQCC_OK)
      {
        switch(pCfh->Reason)
        {
          case MQRC_Q_MGR_STOPPING:
            printf("Command failed to complete. Queue Manager stopping\n");
            break;
          case MQRCCF_NONE_FOUND:
            printf("No active connections for process(%d)\n", ProcessId);
            break;
          case MQRCCF_COMMAND_FAILED:
          default:
            if (ExitReason == MQRC_NONE)
            {
              printf("Failed to inquire connections for process (Reason=%d)\n",
                pCfh->Reason);
            }
            break;
        }
        if (ExitReason == MQRC_NONE)
        {
          ExitReason=pCfh->Reason;
        }
      }
      else
      {
        MQLONG counter;
        MQLONG processed;

        offset=sizeof(MQCFH);
        for (counter=0, processed=0;
             (counter < pCfh->ParameterCount) && !processed;
             counter++)
        {
          MQLONG Type;
          MQLONG StrucLength;

          Type=*(MQLONG *)&(buffer[offset]);
          StrucLength=*(MQLONG *)&(buffer[offset+sizeof(MQLONG)]);

          /**********************************************************/
          /* Precautionary check that we are not going past the end */
          /* of the buffer                                          */
          /**********************************************************/
          if ((offset + StrucLength) > messlen)
            break;

          switch (Type)
          {
            case MQCFT_BYTE_STRING:
              pCfbs=(MQCFBS *)&(buffer[offset]);

              if (pCfbs->Parameter == MQBACF_CONNECTION_ID)
              {
                Reason=StopConnection(Hcon,
                                      CmdQueue,
                                      HCmdQ,
                                      ResolvedReplyQ,
                                      HReplyQ,
                                      pCfbs->String);
                if ((Reason != 0) && (ExitReason == 0))
                {
                  ExitReason=Reason;
                }
                processed=1;
              }
              break;
            default:
              /* Ignore any other structure types as we are not interested */
              break;
          }

          offset+=StrucLength;
        }
      }

      if (pCfh->Control != MQCFC_LAST)
      {
        moreResponses=1;
      }
    }
    else if (CompCode == MQRC_NO_MSG_AVAILABLE)
    {
      /**************************************************************/
      /* We have timed-out waiting for a response from the command  */
      /* server.                                                    */
      /**************************************************************/
      printf("Timed-out out waiting for response from command server\n");
    }
    else
    {
      /**************************************************************/
      /* Some other error has occurred.                             */
      /**************************************************************/
      printf("MQGET(%s) ended with reason code %d\n", ReplyQueue, Reason);
    }
  } while (moreResponses);

  /******************************************************************/
  /* Close the reply queue                                          */
  /******************************************************************/
  MQCLOSE(Hcon,                     /* connection handle            */
          &HReplyQ,                 /* object handle                */
          C_options,
          &CompCode,                /* completion code              */
          &Reason);                 /* reason code                  */

  if (Reason != MQRC_NONE)
  {
    printf("MQCLOSE(%s) ended with reason code %d\n", ReplyQueue, Reason);
  }

  /******************************************************************/
  /* Close the command queue                                        */
  /******************************************************************/
  MQCLOSE(Hcon,                     /* connection handle            */
          &HCmdQ,                   /* object handle                */
          C_options,
          &CompCode,                /* completion code              */
          &Reason);                 /* reason code                  */

  if (Reason != MQRC_NONE)
  {
    printf("MQCLOSE(%s) ended with reason code %d\n", CmdQueue, Reason);
  }

  /******************************************************************/
  /* Disconnect from Queue Manager                                  */
  /******************************************************************/
  MQDISC(&Hcon,                   /* connection handle            */
         &CompCode,               /* completion code              */
         &Reason);                /* reason code                  */

  if (Reason != MQRC_NONE)
  {
    printf("MQDISC ended with reason code %d\n", Reason);
  }

  exit((int)ExitReason);
}

int StopConnection(MQHCONN Hcon,
                   MQCHAR *pCmdQName,
                   MQHOBJ HCmdQ,
                   MQCHAR *pReplyQName,
                   MQHOBJ HReplyQ,
                   MQBYTE ConnectionId[MQ_CONNECTION_ID_LENGTH])
{
  MQGMO    gmo;                    /* Get message options           */
  MQPMO    pmo;                    /* Put message options           */
  MQMD     md;                     /* Message Descriptor            */
  MQLONG   CompCode;               /* completion code               */
  MQLONG   Reason;                 /* reason code                   */

  MQCHAR   buffer[BUFFER_LENGTH];  /* Message buffer                */
  MQLONG   messlen;                /* message length                */
  MQLONG   offset;                 /* Offset within message buffer  */
  MQCFH    *pCfh;                  /* Pointer to MQCFH header       */

  MQBYTE   StopMsgId[MQ_MSG_ID_LENGTH]; /* Stop Request msg id      */
  MQCFBS   *pConnId;               /* Pointer to ConnectionId struct*/
  MQLONG   buflen;                 /* buffer length                 */

  char     FormattedConnectionId[(MQ_CONNECTION_ID_LENGTH *2) +1];
  MQLONG   Counter;                /* Loop counter                  */
  int      rc=0;                   /* Function return code          */

  /******************************************************************/
  /* Make printable form of connection id                           */
  /******************************************************************/
  for (Counter=0; Counter < MQ_CONNECTION_ID_LENGTH; Counter++)
  {
    sprintf(&(FormattedConnectionId[Counter*2]), "%02X",
      ConnectionId[Counter]);
  }

  /******************************************************************/
  /* Prepare the request message                                    */
  /******************************************************************/
  pCfh=(MQCFH *)buffer;
  memcpy(pCfh, &DefaultCFH, sizeof(MQCFH));
  pCfh->Command=MQCMD_STOP_CONNECTION;
  offset=sizeof(MQCFH);

  pConnId=(MQCFBS *)(buffer+offset);
  pConnId->Type=MQCFT_BYTE_STRING;
  pConnId->StrucLength=MQCFBS_STRUC_LENGTH_FIXED + MQ_CONNECTION_ID_LENGTH;
  pConnId->Parameter=MQBACF_CONNECTION_ID;
  pConnId->StringLength=MQ_CONNECTION_ID_LENGTH;
  memcpy(pConnId->String, ConnectionId, MQ_CONNECTION_ID_LENGTH);
  pCfh->ParameterCount++;

  offset+=pConnId->StrucLength;

  /******************************************************************/
  /* Put the command to the COMMAND.QUEUE                           */
  /*                                                                */
  /******************************************************************/
  memcpy(&md, &DefaultMD, sizeof(MQMD));
  memcpy(md.Format, MQFMT_ADMIN, (size_t)MQ_FORMAT_LENGTH);
  strncpy(md.ReplyToQ, pReplyQName, MQ_Q_NAME_LENGTH);
  md.MsgType = MQMT_REQUEST;
  md.Report = MQRO_COPY_MSG_ID_TO_CORREL_ID;

  memcpy(&pmo, &DefaultPMO, sizeof(MQPMO));
  pmo.Options |= MQPMO_NO_SYNCPOINT;
  pmo.Options |= MQPMO_NEW_MSG_ID;
  pmo.Options |= MQPMO_NEW_CORREL_ID;

  MQPUT(Hcon,                /* connection handle               */
        HCmdQ,               /* object handle                   */
        &md,                 /* message descriptor              */
        &pmo,                /* default options (datagram)      */
        offset,              /* message length                  */
        buffer,              /* message buffer                  */
        &CompCode,           /* completion code                 */
        &Reason);            /* reason code                     */

  /* report reason, if any */
  if (CompCode == MQCC_FAILED)
  {
    printf("MQPUT(%s) ended with reason code %d\n",
      pCmdQName, Reason);
    rc=1;
  }
  else
  {
    memcpy(StopMsgId, md.MsgId, MQ_MSG_ID_LENGTH);

    /******************************************************************/
    /* Retrieve the replies                                           */
    /* We will wait up to 15 seconds for a response                   */
    /******************************************************************/
    buflen = sizeof(buffer) - 1;   /* buffer size available for GET   */

    memcpy(&md, &DefaultMD, sizeof(MQMD));
    memcpy(md.MsgId, MQMI_NONE, MQ_MSG_ID_LENGTH);
    memcpy(md.CorrelId, StopMsgId, MQ_MSG_ID_LENGTH);

    md.Encoding       = MQENC_NATIVE;
    md.CodedCharSetId = MQCCSI_Q_MGR;

    memcpy(&gmo, &DefaultGMO, sizeof(MQGMO));  /* Initialize GMO      */
    gmo.Version = MQGMO_VERSION_2;
    gmo.MatchOptions = MQMO_MATCH_CORREL_ID;
    gmo.Options = MQGMO_WAIT         /* wait for new messages         */
                | MQGMO_CONVERT      /* convert if necessary          */
                | MQGMO_NO_SYNCPOINT;  /* No Syncpoint                */
    gmo.WaitInterval = 15000;        /* 15 second limit for waiting   */

    MQGET(Hcon,                  /* connection handle                 */
          HReplyQ,               /* object handle                     */
          &md,                   /* message descriptor                */
          &gmo,                  /* get message options               */
          buflen,                /* buffer length                     */
          buffer,                /* message buffer                    */
          &messlen,              /* message length                    */
          &CompCode,             /* completion code                   */
          &Reason);              /* reason code                       */

    if (CompCode == MQCC_OK)
    {
      pCfh=(MQCFH *)buffer;

      if (pCfh->CompCode == MQCC_OK)
      {
        printf("Stopped connection 0x%s\n", FormattedConnectionId);
      }
      else
      {
        printf("Failed to stop connection 0x%s with reason %d\n",
          FormattedConnectionId, pCfh->Reason);
        rc=pCfh->Reason;
      }
    }
    else
    {
      printf("Failed to put Stop Request to queue(%s) Reason(%d)\n",
             pCmdQName, Reason);
      rc=Reason;
    }
  }

  return rc;
}
