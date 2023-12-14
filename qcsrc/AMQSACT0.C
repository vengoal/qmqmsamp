/* %Z% %W% %I% %E% %U% */
/********************************************************************/
/*                                                                  */
/* Program name: amqsact                                            */
/*                                                                  */
/* Description: Sample C program that retrieves activity trace      */
/*              and writes the formatted data to standard output.   */
/*                                                                  */
/*   <copyright                                                     */
/*   notice="lm-source-program"                                     */
/*   pids="5724-H72"                                                */
/*   years="1994,2016"                                              */
/*   crc="1563970721" >                                             */
/*   Licensed Materials - Property of IBM                           */
/*                                                                  */
/*   5724-H72                                                       */
/*                                                                  */
/*   (C) Copyright IBM Corp. 1994, 2016 All Rights Reserved.        */
/*                                                                  */
/*   US Government Users Restricted Rights - Use, duplication or    */
/*   disclosure restricted by GSA ADP Schedule Contract with        */
/*   IBM Corp.                                                      */
/*   </copyright>                                                   */
/********************************************************************/
/*                                                                  */
/* Function:                                                        */
/*                                                                  */
/*                                                                  */
/*   amqsact is a sample C program to retrieve application          */
/*   activity trace messages and write the formatted contents of    */
/*   the message to the screen.                                     */
/*                                                                  */
/*   This sample can be used in either of two modes:                */
/*     a) To dynamically enable and retrieve activity trace based   */
/*        on an application name, channel name or connection Id for */
/*        the duration of running the sample.                       */
/*     b) To retrieve activity trace messages that are being        */
/*        generated independently, based on queue manager or        */
/*        application level configuration.                          */
/*   (Mode 'a' is only possible on MQ platforms where activity      */
/*   trace is published on the $SYS/MQ system topic strings.        */
/*   See the documentation for details on which platforms support   */
/*   this.)                                                         */
/*                                                                  */
/*   For mode 'a' no further configuration is required, other than  */
/*   the user of the sample being authorised to subscribe to the    */
/*   $SYS/MQ system topics for activity trace.                      */
/*   For mode 'b' see documentation on enabling application         */
/*   activity trace through the mqat.ini file and the ACTVTRC and   */
/*   ACTVCONO queue manager attributes.                             */
/*                                                                  */
/*   Mode a                                                         */
/*   ------                                                         */
/*                                                                  */
/*   To use mode 'a' use one of the -a, -c or -i attributes along   */
/*   with at least -m and -w.                                       */
/*                                                                  */
/*   Activity trace will be started automatically for any new or    */
/*   existing matching connections and continue until no new        */
/*   activity is observed for the -w period.                        */
/*                                                                  */
/*   To trace all activity on connections made by a specific        */
/*   application use -a and the name of the application as reported */
/*   by the ApplName of the connection (typically with any path     */
/*   removed, see the activity trace documentation for further      */
/*   details).                                                      */
/*   For example:                                                   */
/*     amqsact -m QMGR1 -w 30 -a amqsput.exe                        */
/*   This will trace all activity from connections made by the      */
/*   'amqsput.exe' application.                                     */
/*   '#' Can be specified to trace all applications or a '*' used   */
/*   to match a subset of application names.                        */
/*                                                                  */
/*   To trace all activity on a channel, either for client          */
/*   applications using a SVRCONN channel or for activity on either */
/*   end of a queue manager to queue manager channel use -c.        */
/*   For example:                                                   */
/*     amqsact -m QMGR1 -w 10 -c SYSTEM.DEF.SVRCONN                 */
/*   This will trace all activity on the SYSTEM.DEF.SVRCONN channel */
/*   '#' Can be specified to trace activity on all channels on this */
/*   queue manager or a '*' used to match a subset of channels.     */
/*                                                                  */
/*   To trace an existing specific MQ connection use -i and the 24  */
/*   byte unique connection identifier converted to 48 hexidecimal  */
/*   characters. This can be formed by concatenating the CONN value */
/*   to the end of the EXTCONN value returned by DISPLAY CONN in    */
/*   runmqsc.                                                       */
/*   For example:                                                   */
/*     amqsact -M QMGR1 -w 10                                       */
/*             -i 414D5143514D475231202020202020206B576B5420000701  */
/*   This will trace the existing connection that has a CONN value  */
/*   of '6B576B5420000701' and an EXTCONN value of                  */
/*   '414D5143514D47523120202020202020'.                            */
/*   '#' Can be specified to trace activity on all connections,     */
/*   this is equivalent to using -a with '#'.                       */
/*                                                                  */
/*   Parameters q, t, and b are not permitted when either a, c or   */
/*   i are specified.                                               */
/*                                                                  */
/*   Mode b                                                         */
/*   ------                                                         */
/*                                                                  */
/*   When in mode 'b', default behaviour is to retrieve and display */
/*   the activity trace messages that are being delivered to the    */
/*   SYSTEM.ADMIN.TRACE.ACTIVITY queue, based on how activity trace */
/*   has been configured.                                           */
/*                                                                  */
/*   The source of the activity trace messages can be changed from  */
/*   the default of SYSTEM.ADMIN.TRACE.ACTIVITY.QUEUE for example   */
/*   if the queue has been redefined as a QALIAS that targets       */
/*   another queue or topic. The -q and -t parameters give          */
/*   flexibility in where the messages are consumed from.           */
/*                                                                  */
/*   Additionally, if the MQ platform supports subscribing to       */
/*   system topic strings for application activity trace the -t     */
/*   parameter can be used to subscribe directly to one of those    */
/*   topic strings, or -q can be used to retrieve activity trace    */
/*   from an destination queue of a pre-existing subscription to    */
/*   one of the system topic strings.                               */
/*                                                                  */
/*      -- sample reads from SYSTEM.ADMIN.TRACE.ACTIVITY.QUEUE      */
/*      OR                                                          */
/*         reads from named queue (-q)                              */
/*      OR                                                          */
/*         creates managed non-durable subscription to a named      */
/*         topic (-t).                                              */
/*                                                                  */
/*   General                                                        */
/*   -------                                                        */
/*                                                                  */
/*      -- Details of any failures are written to the screen        */
/*                                                                  */
/*      -- Program ends, returning a return code of                 */
/*           zero if a message was processed                        */
/*             1   if no messages were available to process         */
/*            -1   if any other error occurred                      */
/*                                                                  */
/********************************************************************/
/*                                                                  */
/*   amqsat has the following parameters                            */
/*                                                                  */
/*   amqsact   [-m QMgrName]                                        */
/*             [-a ApplName]     # Name of application to trace     */
/*             [-c ChannelName]  # Name of channel to trace         */
/*             [-i ConnId]       # Unique connection id to trace    */
/*             [-q QName]        # Override default queue name      */
/*             [-t TopicString]  # Subscribe to event topic         */
/*             [-b]              # Only browse records              */
/*             [-v]              # Verbose output                   */
/*             [-d <depth>]      # Number of records to display     */
/*             [-w <timeout>]    # Time to wait (in seconds)        */
/*             [-s <startTime>]  # Start time of record to process  */
/*             [-e <endTime>]    # End time of record to process    */
/*                                                                  */
/********************************************************************/
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* includes for MQI  */
#include <cmqc.h>
#include <cmqcfc.h>
#include <cmqxc.h>

/********************************************************************/
/* Define constants used by this program                            */
/********************************************************************/
#if !defined(FALSE)
  #define FALSE 0
#endif

#if !defined(TRUE)
  #define TRUE (!FALSE)
#endif

MQLONG printHeader = FALSE;

/********************************************************************/
/* Private Function prototypes                                      */
/********************************************************************/
static int ParseOptions(int     argc,
                        char    *argv[],
                        char    qmgrName[MQ_Q_MGR_NAME_LENGTH],
                        char    **ppqname,
                        char    **pptopicstr,
                        MQLONG  *pbrowse,
                        MQLONG  *pVerbose,
                        MQLONG  *pmaximumRecords,
                        MQLONG  *pwaitTime,
                        time_t  *pstartTime,
                        time_t  *pendTime);

int getPCFDateTimeFromMsg(MQBYTE *buffer,
                          MQLONG parmCount,
                          MQLONG dateParameter,
                          MQLONG timeParameter,
                          MQCHAR dateString[MQ_DATE_LENGTH+1],
                          MQCHAR timeString[MQ_DATE_LENGTH+1]);

int printMonitoring(MQMD   *pmd,
                    MQBYTE *buffer,
                    MQLONG buflen,
                    MQLONG Verbose);

int printMonitoringRecord(MQLONG parameterCount,
                          MQLONG indentCount,
                          MQBYTE **ppbuffer,
                          MQLONG *pbuflen,
                          MQLONG Verbose);

int skipMonitoringRecord(MQLONG parameterCount,
                         MQBYTE **ppbuffer,
                         MQLONG *pbuflen);

char * getMonInteger(MQLONG _parm,
                     MQLONG _value,
                     MQCHAR *buffer,
                     MQLONG buflen);

int printTerseTraceLine(MQLONG parameterCount,
                        MQLONG indentCount,
                        MQBYTE **ppbuffer,
                        MQLONG *pbuflen);

/********************************************************************/
/* MAIN                                                             */
/********************************************************************/
int main(int argc, char **argv)
{

  /*   Declare MQI structures needed                                */
  MQCNO   cno = {MQCNO_DEFAULT};   /* Connection Options            */
  MQOD     od = {MQOD_DEFAULT};    /* Object Descriptor             */
  MQSD     sd = {MQSD_DEFAULT};    /* Subscription Descriptor       */
  MQMD     md = {MQMD_DEFAULT};    /* Message Descriptor            */
  MQGMO   gmo = {MQGMO_DEFAULT};   /* get message options           */
  /** note, sample uses defaults where it can **/

  MQHCONN  Hcon = MQHC_UNUSABLE_HCONN; /* connection handle         */
  MQHOBJ   Hobj = MQHO_NONE;       /* object handle                 */
  MQHOBJ   Hsub = MQHO_NONE;       /* subscription handle           */
  MQLONG   O_options;              /* MQOPEN options                */
  MQLONG   C_options;              /* MQCLOSE options               */
  MQLONG   CompCode;               /* completion code               */
  MQLONG   Reason;                 /* reason code                   */
  MQBYTE   *buffer;                /* message buffer                */
  MQLONG   buflen;                 /* buffer length                 */
  MQLONG   messlen;                /* message length received       */
  int      prc;                    /* return code from program      */
  int      frc;                    /* function return code          */
  MQLONG   exitLoop=FALSE;
  MQCFH    *pcfh;                  /* Pointer to header             */
  MQLONG   StartBrowse=TRUE;

  char     qmgrName[MQ_Q_MGR_NAME_LENGTH]="";
  char     * pQName = "SYSTEM.ADMIN.TRACE.ACTIVITY.QUEUE";
  char     subTopicStr[200]="";    /* For generating a system topic */
  char     * pTopicStr = &subTopicStr[0];
  MQLONG   waitInterval=0;
  time_t   intervalStartTime;
  time_t   intervalEndTime;
  MQLONG   browse = FALSE;
  MQLONG   subscribe = FALSE;
  MQLONG   maximumRecords = -1;
  MQLONG   RecordNum=0;
  MQLONG   Verbose=FALSE;

  /******************************************************************/
  /* Parse the options from the command line                        */
  /******************************************************************/
  prc = ParseOptions(argc,
                     argv,
                     qmgrName,
                     &pQName,
                     &pTopicStr,
                     &browse,
                     &Verbose,
                     &maximumRecords,
                     &waitInterval,
                     &intervalStartTime,
                     &intervalEndTime);

  if (prc != 0)
  {
    int pad=(int)strlen(argv[0]);
    fprintf(stderr,
            "Usage: %s [-m QMgrName]     # Queue manager to connect to\n"
            "       %*.s [-a ApplName]     # Name of application to trace\n"
            "       %*.s [-c ChannelName]  # Name of channel to trace\n"
            "       %*.s [-i ConnId]       # Unique connection id to trace\n"
            "       %*.s [-q QName]        # Override default queue name\n"
            "       %*.s [-t TopicString]  # Subscribe to event topic\n"
            "       %*.s [-b]              # Only browse records\n"
            "       %*.s [-v]              # Verbose output\n"
            "       %*.s [-d <depth>]      # Number of records to display\n"
            "       %*.s [-w <timeout>]    # Time to wait (in seconds)\n"
            "       %*.s [-s <startTime>]  # Start time of record to process <YYYY:MM:DD HH:MM:SS>\n"
            "       %*.s [-e <endTime>]    # End time of record to process <YYYY:MM:DD HH:MM:SS>\n"
            "Example:\n  amqsact -m QMGR1 -w 30 -a amqsput.exe\n",
            argv[0],   /* -m */
            pad, " ",  /* -a */
            pad, " ",  /* -c */
            pad, " ",  /* -i */
            pad, " ",  /* -q */
            pad, " ",  /* -t */
            pad, " ",  /* -b */
            pad, " ",  /* -v */
            pad, " ",  /* -d */
            pad, " ",  /* -w */
            pad, " ",  /* -s */
            pad, " "); /* -e */
    exit(-1);
  }

  /******************************************************************/
  /*                                                                */
  /*   The sample will not generate any activity trace              */
  /*                                                                */
  /******************************************************************/
  cno.Options |= MQCNO_ACTIVITY_TRACE_DISABLED;

  /******************************************************************/
  /*                                                                */
  /*   Connect to queue manager                                     */
  /*                                                                */
  /******************************************************************/
  MQCONNX(qmgrName,               /* queue manager                  */
          &cno,                   /* connection options             */
          &Hcon,                  /* connection handle              */
          &CompCode,              /* completion code                */
          &Reason);               /* reason code                    */

  /* report reason and stop if it failed     */
  if (CompCode == MQCC_FAILED)
  {
    fprintf(stderr, "MQCONN ended with reason code %d\n", (int)Reason);
    goto mod_exit;
  }

  if(pTopicStr[0] != 0)
  {
    subscribe = TRUE;

    /****************************************************************/
    /*                                                              */
    /* If a topic string has been provided or generated, subscribe  */
    /* to it using a managed non-durable subscription that is       */
    /* automatically removed on exit from the sample.               */
    /*                                                              */
    /****************************************************************/
    sd.ObjectString.VSPtr = pTopicStr;
    sd.ObjectString.VSLength = (MQLONG)strlen(pTopicStr);

    sd.Options = MQSO_CREATE
                 | MQSO_NON_DURABLE
                 | MQSO_FAIL_IF_QUIESCING
                 | MQSO_MANAGED;

    /****************************************************************/
    /* If the topic string contains a '*' character assume that     */
    /* this is for the purpose of a wildcard. The default wildcard  */
    /* is '#', so to use a '*' requires another subscription option.*/
    /****************************************************************/
    if(strchr(pTopicStr, '*'))
      sd.Options |= MQSO_WILDCARD_CHAR;

    MQSUB(Hcon,                     /* connection handle            */
          &sd,                      /* object descriptor for queue  */
          &Hobj,                    /* object handle                */
          &Hsub,                    /* subscription handle          */
          &CompCode,                /* completion code              */
          &Reason);                 /* reason code                  */

    /* report reason, if any; stop if failed      */
    if ((CompCode == MQCC_FAILED) &&
        (Reason != MQRC_NONE))
    {
      fprintf(stderr, "MQSUB ended with reason code %d\n", (int)Reason);
      goto mod_exit;
    }
  }
  else
  {
    /****************************************************************/
    /*                                                              */
    /*   Otherwise, open the named message queue for input;         */
    /*   exclusiveor shared use of the queue is controlled by the   */
    /*   queue definition here                                      */
    /*                                                              */
    /****************************************************************/
    strncpy(od.ObjectName, pQName, MQ_Q_NAME_LENGTH);

    O_options = MQOO_INPUT_AS_Q_DEF      /* open queue for input    */
                | MQOO_BROWSE            /* but not if MQM stopping */
                | MQOO_FAIL_IF_QUIESCING /* but not if MQM stopping */
                ;

    MQOPEN(Hcon,                    /* connection handle            */
           &od,                     /* object descriptor for queue  */
           O_options,               /* open options                 */
           &Hobj,                   /* object handle                */
           &CompCode,               /* completion code              */
           &Reason);                /* reason code                  */

    /* report reason, if any; stop if failed      */
    if ((CompCode == MQCC_FAILED) &&
        (Reason != MQRC_NONE))
    {
      fprintf(stderr, "MQOPEN ended with reason code %d\n", Reason);
      goto mod_exit;
    }
  }

  /******************************************************************/
  /* Initialise the buffer used for getting the message             */
  /******************************************************************/
  buflen = 1024 * 100; /* 100K */
  buffer=(MQBYTE *)malloc(buflen);
  if (buffer == NULL)
  {
    fprintf(stderr, "Failed to allocate memory(%d) for message\n",
            buflen);
    goto mod_exit;
  }

  /******************************************************************/
  /* Main processing loop                                           */
  /******************************************************************/
  do
  {
    gmo.Options = MQGMO_CONVERT;      /* convert if necessary       */

    /****************************************************************/
    /* When processing messages from a queue (rather than a         */
    /* subscription) browse the messages first to pick the ones     */
    /* we're interested in.                                         */
    /****************************************************************/
    if (!subscribe)
    {
      if (StartBrowse)
      {
        gmo.Options |= MQGMO_BROWSE_FIRST;
        StartBrowse=FALSE;
      }
      else
      {
        gmo.Options |= MQGMO_BROWSE_NEXT;
      }
    }

    if (waitInterval == 0)
    {
      gmo.Options |= MQGMO_NO_WAIT;
    }
    else
    {
      gmo.Options |= MQGMO_WAIT;
      gmo.WaitInterval = waitInterval * 1000;
    }

    do
    {
      memcpy(md.MsgId, MQMI_NONE, sizeof(md.MsgId));
      memcpy(md.CorrelId, MQCI_NONE, sizeof(md.CorrelId));

      /****************************************************************/
      /*                                                              */
      /*   MQGET sets Encoding and CodedCharSetId to the values in    */
      /*   the message returned, so these fields should be reset to   */
      /*   the default values before every call, as MQGMO_CONVERT is  */
      /*   specified.                                                 */
      /*                                                              */
      /****************************************************************/
      md.Encoding       = MQENC_NATIVE;
      md.CodedCharSetId = MQCCSI_Q_MGR;

      MQGET(Hcon,                /* connection handle                 */
            Hobj,                /* object handle                     */
            &md,                 /* message descriptor                */
            &gmo,                /* get message options               */
            buflen,              /* buffer length                     */
            buffer,              /* message buffer                    */
            &messlen,            /* message length                    */
            &CompCode,           /* completion code                   */
            &Reason);            /* reason code                       */

      /* report reason, if any     */
      if ((CompCode == MQCC_WARNING) &&
          (Reason == MQRC_TRUNCATED_MSG_FAILED))
      {
        buflen=messlen;
        buffer=(MQBYTE *)realloc(buffer, buflen);
        if (buffer == NULL)
        {
          fprintf(stderr,
                  "Failed to allocate memory(%d) for message\n", buflen);
          goto mod_exit;
        }
      }
    } while ((CompCode==MQCC_WARNING) &&
             (Reason == MQRC_TRUNCATED_MSG_FAILED));

    if (CompCode != MQCC_OK)
    {
      if (Reason == MQRC_NO_MSG_AVAILABLE)
      {
        exitLoop=TRUE;
      }
      else if (Reason == MQRC_Q_MGR_STOPPING)
      {
        fprintf(stderr, "Queue Manager Stopping.\n");
        exitLoop=TRUE;
      }
      else
      {
        fprintf(stderr,
                "Failed to retrieve message from queue [reason: %d]\n",
                Reason);
        exitLoop=TRUE;
      }
    }
    else
    {
      /**************************************************************/
      /* Check to see if the message matches what we are looking    */
      /* for.                                                       */
      /**************************************************************/
      pcfh=(MQCFH *)buffer;

      if (pcfh->Type != MQCFT_APP_ACTIVITY)
      {
        continue;
      }

      if (intervalStartTime != -1)
      {
        struct tm startTm = {0};
        time_t startTime;
        MQCHAR MsgStartDate[MQ_DATE_LENGTH+1];
        MQCHAR MsgStartTime[MQ_TIME_LENGTH+1];

        frc=getPCFDateTimeFromMsg(buffer + sizeof(MQCFH),
                                  pcfh->ParameterCount,
                                  MQCAMO_START_DATE,
                                  MQCAMO_START_TIME,
                                  MsgStartDate,
                                  MsgStartTime);
        if (frc == 0)
        {
          startTm.tm_year=1970;
          startTm.tm_mon=1;
          startTm.tm_mday=1;
          startTm.tm_hour=0;
          startTm.tm_min=0;
          startTm.tm_sec=0;

          sscanf(MsgStartDate, "%4u-%2u-%2u",
                 &(startTm.tm_year),
                 &(startTm.tm_mon),
                 &(startTm.tm_mday));
          sscanf(MsgStartTime, "%2u:%2u:%2u",
                 &(startTm.tm_hour),
                 &(startTm.tm_min),
                 &(startTm.tm_sec));

          if ((startTm.tm_year < 1900) ||
              (startTm.tm_mon < 1) ||
              (startTm.tm_mon > 12) ||
              (startTm.tm_mday < 1) ||
              (startTm.tm_mday > 31) ||
              (startTm.tm_hour > 24) ||
              (startTm.tm_min > 59) ||
              (startTm.tm_sec > 59))
          {
            continue;
          }

          startTm.tm_year-=1900;

          startTime=mktime(&startTm);

          if ((startTime == -1) ||
              (startTime < intervalStartTime))
          {
            continue;
          }
        }
      }

      if (intervalEndTime != -1)
      {
        struct tm endTm = {0};
        time_t endTime;
        MQCHAR MsgEndDate[MQ_DATE_LENGTH+1];
        MQCHAR MsgEndTime[MQ_TIME_LENGTH+1];

        frc=getPCFDateTimeFromMsg(buffer + sizeof(MQCFH),
                                  pcfh->ParameterCount,
                                  MQCAMO_END_DATE,
                                  MQCAMO_END_TIME,
                                  MsgEndDate,
                                  MsgEndTime);
        if (frc == 0)
        {
          endTm.tm_year=2999;
          endTm.tm_mon=12;
          endTm.tm_mday=31;
          endTm.tm_hour=23;
          endTm.tm_min=59;
          endTm.tm_sec=59;

          sscanf(MsgEndDate, "%4u-%2u-%2u",
                 &(endTm.tm_year),
                 &(endTm.tm_mon),
                 &(endTm.tm_mday));

          sscanf(MsgEndTime, "%2u:%2u:%2u",
                 &(endTm.tm_hour),
                 &(endTm.tm_min),
                 &(endTm.tm_sec));

          if ((endTm.tm_year < 1900) ||
              (endTm.tm_mon < 1) ||
              (endTm.tm_mon > 12) ||
              (endTm.tm_mday < 1) ||
              (endTm.tm_mday > 31) ||
              (endTm.tm_hour > 24) ||
              (endTm.tm_min > 59) ||
              (endTm.tm_sec > 59))
          {
            continue;
          }

          endTm.tm_year-=1900;

          endTime=mktime(&endTm);

          if ((endTime == -1) ||
              (endTime > intervalEndTime))
          {
            continue;
          }
        }
      }

      /**************************************************************/
      /* If we are not leaving the messages on the queue then       */
      /* reissue the get to remove them.                            */
      /**************************************************************/
      if (!subscribe && !browse)
      {
        gmo.Options = MQGMO_CONVERT |     /* convert if necessary   */
                      MQGMO_MSG_UNDER_CURSOR |
                      MQGMO_NO_WAIT;

        MQGET(Hcon,              /* connection handle               */
              Hobj,              /* object handle                   */
              &md,               /* message descriptor              */
              &gmo,              /* get message options             */
              buflen,            /* buffer length                   */
              buffer,            /* message buffer                  */
              &messlen,          /* message length                  */
              &CompCode,         /* completion code                 */
              &Reason);          /* reason code                     */

        if (CompCode != MQCC_OK)
        {
          /**********************************************************/
          /* If we fail to get the message then perhaps someone     */
          /* else has got there first so just continue on the next  */
          /* msg.                                                   */
          /**********************************************************/
          if (Reason != MQRC_NO_MSG_AVAILABLE)
            continue;
          else
          {
            printf("Failed to get message for reason %d.\n", Reason);
            exitLoop=TRUE;
          }
        }
      }

      /**************************************************************/
      /* We have found a matching message so process it.            */
      /**************************************************************/
      printHeader = TRUE;
      prc=printMonitoring(&md,
                          buffer,
                          messlen,
                          Verbose);
      if (prc == 0)
      {
        RecordNum++;
      }

      if ((maximumRecords != -1) && (RecordNum == maximumRecords))
      {
        exitLoop=TRUE;
      }
    }
  } while (!exitLoop);

  printf("%d Records Processed.\n", RecordNum);

mod_exit:

  /******************************************************************/
  /*                                                                */
  /*   Close the subscription (if it was made)                      */
  /*                                                                */
  /******************************************************************/
  if(Hsub != MQHO_NONE)
  {
    C_options = MQCO_NONE;             /* no close options          */

    MQCLOSE(Hcon,                      /* connection handle         */
            &Hsub,                     /* subscription handle       */
            C_options,
            &CompCode,                 /* completion code           */
            &Reason);                  /* reason code               */

    if (Reason != MQRC_NONE)
    {
      if ((CompCode != MQRC_CONNECTION_BROKEN) &&
          (CompCode != MQRC_Q_MGR_STOPPING))
      {
        fprintf(stderr, "MQCLOSE of subscription ended with reason code %d\n",
                Reason);
      }
    }
  }

  /******************************************************************/
  /*                                                                */
  /*   Close the source queue (if it was opened)                    */
  /*                                                                */
  /******************************************************************/
  if(Hobj != MQHO_NONE)
  {
    C_options = MQCO_NONE;             /* no close options          */

    MQCLOSE(Hcon,                      /* connection handle         */
            &Hobj,                     /* object handle             */
            C_options,
            &CompCode,                 /* completion code           */
            &Reason);                  /* reason code               */

    if (Reason != MQRC_NONE)
    {
      if ((CompCode != MQRC_CONNECTION_BROKEN) &&
          (CompCode != MQRC_Q_MGR_STOPPING))
      {
        fprintf(stderr, "MQCLOSE of queue ended with reason code %d\n",
                Reason);
      }
    }
  }

  /******************************************************************/
  /*                                                                */
  /*   Disconnect from MQM                                          */
  /*                                                                */
  /******************************************************************/
  if(Hcon != MQHC_UNUSABLE_HCONN)
  {
    MQDISC(&Hcon,                       /* connection handle        */
           &CompCode,                   /* completion code          */
           &Reason);                    /* reason code              */

    if (Reason != MQRC_NONE)
    {
      if ((CompCode != MQRC_CONNECTION_BROKEN) &&
          (CompCode != MQRC_Q_MGR_STOPPING))
      {
        fprintf(stderr, "MQDISC ended with reason code %d\n", Reason);
      }
    }
  }

  /******************************************************************/
  /*                                                                */
  /* END OF AMQSMON0                                                */
  /*                                                                */
  /******************************************************************/
  return((RecordNum >0)?0:1);
}

/********************************************************************/
/*                                                                  */
/* Function: parseOptions                                           */
/*                                                                  */
/*                                                                  */
/*   This function parses the options supplied by the user on the   */
/*   command line and returns the options selected as a set of      */
/*   flags back to the caller.                                      */
/*                                                                  */
/********************************************************************/
static int ParseOptions(int     argc,
                        char    *argv[],
                        char    qmgrName[MQ_Q_MGR_NAME_LENGTH],
                        char    **ppqname,
                        char    **pptopicstr,
                        MQLONG  *pbrowse,
                        MQLONG  *pVerbose,
                        MQLONG  *pmaximumRecords,
                        MQLONG  *pwaitTime,
                        time_t  *pstartTime,
                        time_t  *pendTime)
{
  int counter;              /* Argument counter                    */
  int InvalidArgument=0;    /* Number of invalid argument          */
  int error=FALSE;          /* Number of invalid argument          */
  int i;                    /* Simple counter                      */
  char *p;                  /* Simple character pointer            */
  char *parg;               /* pointer to current argument         */
  char *pval;               /* pointer to current value            */
  char *pApplName = NULL;   /* application name to trace           */
  char *pChannelName = NULL;/* channel name to trace               */
  char *pConnId = NULL;     /* connection id to trace              */

  struct tm startTime={0};
  struct tm endTime={0};

  int SubTypeSpecified     = FALSE;      /* -a, -c or -i specified */
  int qnameSpecified       = FALSE;
  int topicstrSpecified    = FALSE;
  int browseSpecified      = FALSE;
  int verboseSpecified     = FALSE;
  int depthSpecified       = FALSE;
  int qmgrSpecified        = FALSE;
  int waitSpecified        = FALSE;
  int startTimeSpecified   = FALSE;
  int endTimeSpecified     = FALSE;

  memset(qmgrName, 0, MQ_Q_MGR_NAME_LENGTH);
  *pbrowse=FALSE;
  *pVerbose =FALSE;
  *pmaximumRecords=-1;
  *pwaitTime=0;
  *pstartTime=-1;
  *pendTime=-1;

  for (counter=1; (counter < argc) && !InvalidArgument; counter++)
  {
    parg=argv[counter];

    if (parg[0] != '-')
    {
      InvalidArgument=counter;
      continue;
    }

    switch (parg[1])
    {
    case 'm': /* Queue manager Name */
      if (qmgrSpecified)
      {
        InvalidArgument=counter;
      }
      else
      {
        qmgrSpecified=TRUE;
        memset(qmgrName, 0, MQ_Q_MGR_NAME_LENGTH);
        for (p=&(parg[2]); *p==' '; p++) ; /* Strip off spaces        */

        if ((*p == '\0') && ((counter+1) < argc))
        {
          pval=argv[++counter];
        }
        else
        {
          pval=p;
        }

        for (i=0; (*pval != '\0') && (i <MQ_Q_MGR_NAME_LENGTH); i++)
        {
          qmgrName[i]=*pval++;
        }
      }
      break;
    case 'a': /* Application name */
      /* If tracing an application by name, a queue or topic cannot */
      /* also be specified and browse does not make sense. */
      if (qnameSpecified | topicstrSpecified | browseSpecified | SubTypeSpecified)
      {
        InvalidArgument=counter;
      }
      else
      {
        SubTypeSpecified=TRUE;

        for (p=&(parg[2]); *p==' '; p++) ; /* Strip off spaces      */

        if ((*p == '\0') && ((counter+1) < argc))
        {
          pval=argv[++counter];
        }
        else
        {
          pval=p;
        }
        pApplName = pval;
      }
      break;
    case 'c': /* Channel name */
      /* If tracing a channel by name, a queue or topic cannot also */
      /* be specified and browse does not make sense. */
      if (qnameSpecified | topicstrSpecified | browseSpecified | SubTypeSpecified)
      {
        InvalidArgument=counter;
      }
      else
      {
        SubTypeSpecified=TRUE;

        for (p=&(parg[2]); *p==' '; p++) ; /* Strip off spaces      */

        if ((*p == '\0') && ((counter+1) < argc))
        {
          pval=argv[++counter];
        }
        else
        {
          pval=p;
        }
        pChannelName = pval;
      }
      break;
    case 'i': /* Connection Id */
      /* If tracing a connection id, a queue or topic cannot also   */
      /* be specified and browse does not make sense. */
      if (qnameSpecified | topicstrSpecified | browseSpecified | SubTypeSpecified)
      {
        InvalidArgument=counter;
      }
      else
      {
        SubTypeSpecified=TRUE;

        for (p=&(parg[2]); *p==' '; p++) ; /* Strip off spaces      */

        if ((*p == '\0') && ((counter+1) < argc))
        {
          pval=argv[++counter];
        }
        else
        {
          pval=p;
        }
        pConnId = pval;
      }
      break;
    case 'q': /* Queue Name */
      /* If subscribing to a system topic has been specified a      */
      /* queue name cannot also be specified. */
      if (qnameSpecified | topicstrSpecified | SubTypeSpecified)
      {
        InvalidArgument=counter;
      }
      else
      {
        qnameSpecified=TRUE;

        for (p=&(parg[2]); *p==' '; p++) ; /* Strip off spaces        */

        if ((*p == '\0') && ((counter+1) < argc))
        {
          pval=argv[++counter];
        }
        else
        {
          pval=p;
        }
        *ppqname = pval;
      }
      break;
    case 't': /* Topic String */
      /* If subscribing to a system topic has been specified a      */
      /* queue name cannot also be specified. */
      if (topicstrSpecified | qnameSpecified | SubTypeSpecified)
      {
        InvalidArgument=counter;
      }
      else
      {
        topicstrSpecified=TRUE;

        for (p=&(parg[2]); *p==' '; p++) ; /* Strip off spaces        */

        if ((*p == '\0') && ((counter+1) < argc))
        {
          pval=argv[++counter];
        }
        else
        {
          pval=p;
        }
        *pptopicstr = pval;
      }
      break;
    case 'b': /* Browse messages only */
      /* If subscribing to a system topic has been specified browse */
      /* is not valid. */
      if (browseSpecified | SubTypeSpecified)
      {
        InvalidArgument=counter;
      }
      else
      {
        browseSpecified=TRUE;
        *pbrowse=TRUE;
      }
      break;
    case 'v': /* Verbose output */
      if (verboseSpecified)
      {
        InvalidArgument=counter;
      }
      else
      {
        verboseSpecified=TRUE;
        *pVerbose=TRUE;
      }
      break;
    case 'd': /* Maximum number of messages to display */
      if (depthSpecified)
      {
        InvalidArgument=counter;
      }
      else
      {
        depthSpecified=TRUE;
        for (p=&(parg[2]); *p==' '; p++) ; /* Strip off spaces        */
        if (*p == '\0')
        {
          if ((counter+1 < argc) && argv[counter+1][0] != '-')
          {
            counter++;
            pval=argv[counter];
          }
          else
          {
            pval=NULL;
          }
        }
        else
        {
          pval=p;
        }

        if ((pval == NULL) || (*pval == '\0'))
        {
          *pmaximumRecords=1;
        }
        else
        {
          *pmaximumRecords=atoi(pval);
          if (*pmaximumRecords <= 0)
          {
            InvalidArgument=counter;
          }
        }
      }
      break;
    case 'w': /* The maximum time to wait for a message  */
      if (waitSpecified)
      {
        InvalidArgument=counter;
      }
      else
      {
        waitSpecified=TRUE;
        for (p=&(parg[2]); *p==' '; p++) ; /* Strip off spaces        */
        if (*p == '\0')
        {
          if ((counter+1 < argc) && argv[counter+1][0] != '-')
          {
            counter++;
            pval=argv[counter];
          }
          else
          {
            pval=NULL;
          }
        }
        else
        {
          pval=p;
        }

        if ((pval == NULL) || (*pval == '\0'))
        {
          *pwaitTime=0;
        }
        else
        {
          *pwaitTime=atoi(pval);
          if (*pwaitTime < 0)
          {
            *pwaitTime=0;
          }
        }
      }
      break;
    case 's': /* The start time of records to process */
      memset(&startTime, 0, sizeof(startTime));
      startTime.tm_year=1970;
      startTime.tm_mon=1;
      startTime.tm_mday=1;
      startTime.tm_hour=0;
      startTime.tm_min=0;
      startTime.tm_sec=0;

      if (startTimeSpecified)
      {
        InvalidArgument=counter;
      }
      else
      {
        startTimeSpecified=TRUE;
        for (p=&(parg[2]); *p==' '; p++) ; /* Strip off spaces        */
        if (*p == '\0')
        {
          if ((counter+1 < argc) && (argv[counter+1][0] != '-'))
          {
            counter++;
            pval=argv[counter];
          }
          else
          {
            pval=NULL;
          }
        }
        else
        {
          pval=p;
        }
        if ((pval == NULL) || (*pval == '\0'))
        {
          InvalidArgument=counter;
        }
        else
        {
          sscanf(pval, "%4u-%2u-%2u %2u:%2u:%2u",
                 &(startTime.tm_year),
                 &(startTime.tm_mon),
                 &(startTime.tm_mday),
                 &(startTime.tm_hour),
                 &(startTime.tm_min),
                 &(startTime.tm_sec));

          if ((startTime.tm_year < 1900) ||
              (startTime.tm_mon < 1) ||
              (startTime.tm_mon > 12) ||
              (startTime.tm_mday < 1) ||
              (startTime.tm_mday > 31) ||
              (startTime.tm_hour > 24) ||
              (startTime.tm_min > 59) ||
              (startTime.tm_sec > 59))
          {
            InvalidArgument=counter;
          }
          else
          {
            startTime.tm_year-=1900;

            *pstartTime=mktime(&startTime);

            if (*pstartTime == -1)
            {
              InvalidArgument=counter;
            }
          }
        }
      }
      break;
    case 'e': /* The end time of records to process */
      memset(&endTime, 0, sizeof(endTime));
      endTime.tm_year=2999;
      endTime.tm_mon=12;
      endTime.tm_mday=31;
      endTime.tm_hour=23;
      endTime.tm_min=59;
      endTime.tm_sec=59;

      if (endTimeSpecified)
      {
        InvalidArgument=counter;
      }
      else
      {
        endTimeSpecified=TRUE;
        for (p=&(parg[2]); *p==' '; p++) ; /* Strip off spaces        */
        if (*p == '\0')
        {
          if ((counter+1 < argc) && (argv[counter+1][0] != '-'))
          {
            counter++;
            pval=argv[counter];
          }
          else
          {
            pval=NULL;
          }
        }
        else
        {
          pval=p;
        }
        if ((pval == NULL) || (*pval == '\0'))
        {
          InvalidArgument=counter;
        }
        else
        {
            sscanf(pval, "%4u-%2u-%2u %2u:%2u:%2u",
                 &(endTime.tm_year),
                 &(endTime.tm_mon),
                 &(endTime.tm_mday),
                 &(endTime.tm_hour),
                 &(endTime.tm_min),
                 &(endTime.tm_sec));

          if ((endTime.tm_year < 1900) ||
              (endTime.tm_mon < 1) ||
              (endTime.tm_mon > 12) ||
              (endTime.tm_mday < 1) ||
              (endTime.tm_mday > 31) ||
              (endTime.tm_hour > 24) ||
              (endTime.tm_min > 59) ||
              (endTime.tm_sec > 59))
          {
            InvalidArgument=counter;
          }
          else
          {
            endTime.tm_year-=1900;

            *pendTime=mktime(&endTime);

            if (*pendTime == -1)
            {
              InvalidArgument=counter;
            }
          }
        }
      }
      break;
    default:
      InvalidArgument=counter;
      break;
    }
  }

  if (InvalidArgument != 0)
  {
    fprintf(stderr, "Argument %d has an error\n",
            InvalidArgument);
    error=TRUE;
  }
  /******************************************************************/
  /*                                                                */
  /* If an application name, channel name or connection id has been */
  /* specified then this sample will subscribe directly to the      */
  /* matching activity trace system topic string. No queue manager  */
  /* configuration is required for this other than to ensure that   */
  /* the user has authority to subscribe to the appropriate         */
  /* $SYS/MQ topic string using a suitable topic object.            */
  /*                                                                */
  /******************************************************************/
  else if (SubTypeSpecified)
  {
    /* A queue manager name must be specified */
    if (!qmgrSpecified || !waitSpecified)
    {
      fprintf(stderr, "A queue manager name and a wait time are required when using -a, -c or -i\n");
      error=TRUE;
    }
    else
    {
      char    qmgrNameEscape[MQ_Q_MGR_NAME_LENGTH+1]="";

      /* When subscribing directly to activity trace, '/' characters*/
      /* in a queue manager name are replaced with '&'. */
      for(i=0; qmgrName[i] != '\0'; i++)
      {
        if (qmgrName[i] == '/')
          qmgrNameEscape[i] = '&';
        else
          qmgrNameEscape[i] = qmgrName[i];
      }

      /* Build the activity trace topic string to subscribe to */
      if (pApplName != NULL) /* Use the application name */
        sprintf(*pptopicstr, "$SYS/MQ/INFO/QMGR/%s/ActivityTrace/ApplName/%s",
                qmgrNameEscape, pApplName);
      else if (pChannelName != NULL) /* Use the channel name */
      {
        /* '/' characters in a channel name are replaced with '&' */
        for(i=0; pChannelName[i] != '\0'; i++)
        {
          if (pChannelName[i] == '/')
            pChannelName[i] = '&';
        }

        sprintf(*pptopicstr, "$SYS/MQ/INFO/QMGR/%s/ActivityTrace/ChannelName/%s",
                qmgrNameEscape, pChannelName);
      }
      else /* Use the connection id */
        sprintf(*pptopicstr, "$SYS/MQ/INFO/QMGR/%s/ActivityTrace/ConnectionId/%s",
                qmgrNameEscape, pConnId);

      fprintf(stdout, "Subscribing to the activity trace topic:\n  '%s'\n\n",
              *pptopicstr);
    }
  }

  return error;
}
/********************************************************************/
/*                                                                  */
/* Keywords and Constants                                           */
/*                                                                  */
/*                                                                  */
/*   The following Keywords and Constants are used for formatting   */
/*   the PCF application trace data                        */
/*                                                                  */
/********************************************************************/
/********************************************************************/

/********************************************************************/
/* Structure: ConstDefinitions                                      */
/*                                                                  */
/*   This structure is used to define test definitions of MQ        */
/*   constants into text identifiers.                               */
/*                                                                  */
/********************************************************************/
struct ConstDefinitions
{
  MQLONG Value;
  char *Identifier;
};


/* For MQIACF_API_ENVIRONMENT */
static struct ConstDefinitions  ExitEnvironment[] =
{
  {MQXE_OTHER,          "MQXE_OTHER"},
  {MQXE_MCA,            "MQXE_MCA"},
  {MQXE_MCA_SVRCONN,    "MQXE_MCA_SVRCONN"},
  {MQXE_COMMAND_SERVER, "MQXE_COMMAND_SERVER"},
  {MQXE_MQSC,           "MQXE_MQSC"},
  {-1, ""}
};

/* For MQIA_PLATFORM */
static struct ConstDefinitions  Platform[] =
{
  {MQPL_ZOS,        "MQPL_ZOS"},
  {MQPL_OS2,        "MQPL_OS2"},
  {MQPL_UNIX,       "MQPL_UNIX"},
  {MQPL_OS400,      "MQPL_OS400"},
  {MQPL_WINDOWS,    "MQPL_WINDOWS"},
  {MQPL_WINDOWS_NT, "MQPL_WINDOWS_NT"},
  {MQPL_VMS,        "MQPL_VMS"},
  {MQPL_NSS,        "MQPL_NSS"},
  {MQPL_OPEN_TP1,   "MQPL_OPEN_TP1"},
  {MQPL_VM,         "MQPL_VM"},
  {MQPL_TPF,        "MQPL_TPF"},
  {MQPL_VSE,        "MQPL_VSE"},
  {-1, ""}
};

/* For MQIA_APPL_TYPE */
static struct ConstDefinitions  AppType[] =
{
  {MQAT_NO_CONTEXT,       "MQAT_NO_CONTEXT"},
  {MQAT_CICS,             "MQAT_CICS"},
  {MQAT_MVS,              "MQAT_MVS"},
  {MQAT_OS390,            "MQAT_OS390"},
  {MQAT_ZOS,              "MQAT_ZOS"},
  {MQAT_IMS,              "MQAT_IMS"},
  {MQAT_OS2,              "MQAT_OS2"},
  {MQAT_DOS,              "MQAT_DOS"},
  {MQAT_UNIX,             "MQAT_UNIX"},
  {MQAT_QMGR,             "MQAT_QMGR"},
  {MQAT_OS400,            "MQAT_OS400"},
  {MQAT_WINDOWS,          "MQAT_WINDOWS"},
  {MQAT_CICS_VSE,         "MQAT_CICS_VSE"},
  {MQAT_WINDOWS_NT,       "MQAT_WINDOWS_NT"},
  {MQAT_VMS,              "MQAT_VMS"},
  {MQAT_NSK,              "MQAT_NSK"},
  {MQAT_VOS,              "MQAT_VOS"},
  {MQAT_OPEN_TP1,         "MQAT_OPEN_TP1"},
  {MQAT_VM,               "MQAT_VM"},
  {MQAT_IMS_BRIDGE,       "MQAT_IMS_BRIDGE"},
  {MQAT_XCF,              "MQAT_XCF"},
  {MQAT_CICS_BRIDGE,      "MQAT_CICS_BRIDGE"},
  {MQAT_NOTES_AGENT,      "MQAT_NOTES_AGENT"},
  {MQAT_TPF,              "MQAT_TPF"},
  {MQAT_USER,             "MQAT_USER"},
  {MQAT_BROKER,           "MQAT_BROKER"},
  {MQAT_JAVA,             "MQAT_JAVA"},
  {MQAT_DQM,              "MQAT_DQM"},
  {MQAT_CHANNEL_INITIATOR,"MQAT_CHANNEL_INITIATOR"},
  {MQAT_WLM,              "MQAT_WLM"},
  {MQAT_BATCH,            "MQAT_BATCH"},
  {MQAT_RRS_BATCH,        "MQAT_RRS_BATCH"},
  {MQAT_SIB,              "MQAT_SIB"},
  {MQAT_SYSTEM_EXTENSION, "MQAT_SYSTEM_EXTENSION"},
  {MQAT_USER_FIRST,       "MQAT_USER_FIRST"},
  {MQAT_USER_LAST,        "MQAT_USER_LAST"},
  {-1, ""}
};


/* For MQIACF_API_CALLER_TYPE */
static struct ConstDefinitions  ExitAPICaller[] =
{
  {MQXACT_EXTERNAL,   "MQXACT_EXTERNAL"},
  {MQXACT_INTERNAL,   "MQXACT_INTERNAL"},
  {-1, ""}
};

/* For MQIACF_COMP_CODE */
static struct ConstDefinitions  CompCode[] =
{
  {MQCC_OK,           "MQCC_OK"},
  {MQCC_WARNING,      "MQCC_WARNING"},
  {MQCC_FAILED,       "MQCC_FAILED"},
  {MQCC_UNKNOWN,      "MQCC_UNKNOWN"},
  {-1, ""}
};
/* For MQIACF_CTL_OPERATION */
static struct ConstDefinitions  CtlOp[] =
{
  {MQOP_DEREGISTER,    "MQOP_DEREGISTER"},
  {MQOP_REGISTER,      "MQOP_REGISTER"},
  {MQOP_SUSPEND,       "MQOP_SUSPEND"},
  {MQOP_RESUME,        "MQOP_RESUME"},
  {MQOP_START,         "MQOP_START"},
  {MQOP_START_WAIT,    "MQOP_START_WAIT"},
  {MQOP_STOP,          "MQOP_STOP"},
  {-1, ""}
};
/* For MQIACF_MQCB_OPERATION */
static struct ConstDefinitions  CallbackOp[] =
{
  {MQOP_DEREGISTER,    "MQOP_DEREGISTER"},
  {MQOP_REGISTER,      "MQOP_REGISTER"},
  {MQOP_SUSPEND,       "MQOP_SUSPEND"},
  {MQOP_RESUME,        "MQOP_RESUME"},
  {-1, ""}
};
/* For MQIACF_MQCB_TYPE */
static struct ConstDefinitions  CallbackType[] =
{
  {MQCBT_MESSAGE_CONSUMER, "MQCBT_MESSAGE_CONSUMER"},
  {MQCBT_EVENT_HANDLER,    "MQCBT_EVENT_HANDLER"},
  {-1, ""}
};
/* For MQIACF_CALL_TYPE */
static struct ConstDefinitions  CallType[] =
{
  {MQCBCT_START_CALL,      "MQCBCT_START_CALL"},
  {MQCBCT_STOP_CALL,       "MQCBCT_STOP_CALL"},
  {MQCBCT_REGISTER_CALL,   "MQCBCT_REGISTER_CALL"},
  {MQCBCT_DEREGISTER_CALL, "MQCBCT_DEREGISTER_CALL"},
  {MQCBCT_EVENT_CALL,      "MQCBCT_EVENT_CALL"},
  {MQCBCT_MSG_REMOVED,     "MQCBCT_MSG_REMOVED"},
  {MQCBCT_MSG_NOT_REMOVED, "MQCBCT_MSG_NOT_REMOVED"},
  {-1, ""}
};

/* For MQIACF_OPERATION_ID */
static struct ConstDefinitions  OperationId[] =
{
  {MQXF_CONN,            "MQXF_CONN"},
  {MQXF_CONNX,           "MQXF_CONNX"},
  {MQXF_DISC,            "MQXF_DISC"},
  {MQXF_OPEN,            "MQXF_OPEN"},
  {MQXF_CLOSE,           "MQXF_CLOSE"},
  {MQXF_PUT1,            "MQXF_PUT1"},
  {MQXF_PUT,             "MQXF_PUT"},
  {MQXF_GET,             "MQXF_GET"},
  {MQXF_INQ,             "MQXF_INQ"},
  {MQXF_SET,             "MQXF_SET"},
  {MQXF_BEGIN,           "MQXF_BEGIN"},
  {MQXF_CMIT,            "MQXF_CMIT"},
  {MQXF_BACK,            "MQXF_BACK"},
  {MQXF_STAT,            "MQXF_STAT"},
  {MQXF_CB,              "MQXF_CB"},
  {MQXF_CTL,             "MQXF_CTL"},
  {MQXF_CALLBACK,        "MQXF_CALLBACK"},
  {MQXF_SUB,             "MQXF_SUB"},
  {MQXF_SUBRQ,           "MQXF_SUBRQ"},
  {MQXF_XACLOSE,         "MQXF_XACLOSE"},
  {MQXF_XACOMMIT,        "MQXF_XACOMMIT"},
  {MQXF_XACOMPLETE,      "MQXF_XACOMPLETE"},
  {MQXF_XAEND,           "MQXF_XAEND"},
  {MQXF_XAFORGET,        "MQXF_XAFORGET"},
  {MQXF_XAOPEN,          "MQXF_XAOPEN"},
  {MQXF_XAPREPARE,       "MQXF_XAPREPARE"},
  {MQXF_XARECOVER,       "MQXF_XARECOVER"},
  {MQXF_XAROLLBACK,      "MQXF_XAROLLBACK"},
  {MQXF_XASTART,         "MQXF_XASTART"},
  {-1, ""}
};

/* Object types */
static struct ConstDefinitions ObjTypes[] =
{
  {MQOT_Q_MGR                 ,"MQOT_Q_MGR"},
  {MQOT_Q                     ,"MQOT_Q"},
  {MQOT_NAMELIST              ,"MQOT_NAMELIST"},
  {MQOT_PROCESS               ,"MQOT_PROCESS"},
  {MQOT_STORAGE_CLASS         ,"MQOT_STORAGE_CLASS"},
  {MQOT_CHANNEL               ,"MQOT_CHANNEL"},
  {MQOT_AUTH_INFO             ,"MQOT_AUTH_INFO"},
  {MQOT_CF_STRUC              ,"MQOT_CF_STRUC"},
  {MQOT_LISTENER              ,"MQOT_LISTENER"},
  {MQOT_SERVICE               ,"MQOT_SERVICE"},
  {MQOT_RESERVED_1            ,"MQOT_RESERVED_1"},
  {MQOT_ALL                   ,"MQOT_ALL"},
  {MQOT_ALIAS_Q               ,"MQOT_ALIAS_Q"},
  {MQOT_MODEL_Q               ,"MQOT_MODEL_Q"},
  {MQOT_LOCAL_Q               ,"MQOT_LOCAL_Q"},
  {MQOT_REMOTE_Q              ,"MQOT_REMOTE_Q"},
  {MQOT_SENDER_CHANNEL        ,"MQOT_SENDER_CHANNEL"},
  {MQOT_SERVER_CHANNEL        ,"MQOT_SERVER_CHANNEL"},
  {MQOT_REQUESTER_CHANNEL     ,"MQOT_REQUESTER_CHANNEL"},
  {MQOT_RECEIVER_CHANNEL      ,"MQOT_RECEIVER_CHANNEL"},
  {MQOT_CURRENT_CHANNEL       ,"MQOT_CURRENT_CHANNEL"},
  {MQOT_SAVED_CHANNEL         ,"MQOT_SAVED_CHANNEL"},
  {MQOT_SVRCONN_CHANNEL       ,"MQOT_SVRCONN_CHANNEL"},
  {MQOT_CLNTCONN_CHANNEL      ,"MQOT_CLNTCONN_CHANNEL"},
  {MQOT_REMOTE_Q_MGR_NAME     ,"MQOT_REMOTE_Q_MGR_NAME"},
  {-1, ""}
};

/* Channel types */
static struct ConstDefinitions ChlTypes[] =
{
  {MQCHT_SENDER                 ,"MQCHT_SENDER"},
  {MQCHT_SERVER                 ,"MQCHT_SERVER"},
  {MQCHT_RECEIVER               ,"MQCHT_RECEIVER"},
  {MQCHT_REQUESTER              ,"MQCHT_REQUESTER"},
  {MQCHT_ALL                    ,"MQCHT_ALL"},
  {MQCHT_CLNTCONN               ,"MQCHT_CLNTCONN"},
  {MQCHT_SVRCONN                ,"MQCHT_SVRCONN"},
  {MQCHT_CLUSRCVR               ,"MQCHT_CLUSRCVR"},
  {MQCHT_CLUSSDR                ,"MQCHT_CLUSSDR"},
  {-1, ""}
};


/* For MQIA_PLATFORM */
static struct ConstDefinitions  FunTypes[] =
{
  {MQFUN_TYPE_UNKNOWN        ,"MQFUN_TYPE_UNKNOWN"},
  {MQFUN_TYPE_JVM            ,"MQFUN_TYPE_JVM"},
  {MQFUN_TYPE_PROGRAM        ,"MQFUN_TYPE_PROGRAM"},
  {MQFUN_TYPE_PROCEDURE      ,"MQFUN_TYPE_PROCEDURE"},
  {MQFUN_TYPE_USERDEF        ,"MQFUN_TYPE_USERDEF"},
  {MQFUN_TYPE_COMMAND        ,"MQFUN_TYPE_COMMAND"},
  {-1, ""}
};

/* For MQIACH_XMIT_PROTOCOL_TYPE */
static struct ConstDefinitions XpTypes[] =
{
  {MQXPT_ALL                 ,"MQXPT_ALL"},
  {MQXPT_DECNET              ,"MQXPT_DECNET"},
  {MQXPT_LOCAL               ,"MQXPT_LOCAL"},
  {MQXPT_LU62                ,"MQXPT_LU62"},
  {MQXPT_NETBIOS             ,"MQXPT_NETBIOS"},
  {MQXPT_SPX                 ,"MQXPT_SPX"},
  {MQXPT_TCP                 ,"MQXPT_TCP"},
  {MQXPT_UDP                 ,"MQXPT_UDP"},
  {-1, ""}
};

/* For MQIACF_MSG_TYPE */
static struct ConstDefinitions MsgTypes[] =
{
  {MQMT_REQUEST              ,"MQMT_REQUEST"},
  {MQMT_REPLY                ,"MQMT_REPLY"},
  {MQMT_DATAGRAM             ,"MQMT_DATAGRAM"},
  {MQMT_REPORT               ,"MQMT_REPORT"},
  {MQMT_MQE_FIELDS_FROM_MQE  ,"MQMT_MQE_FIELDS_FROM_MQE"},
  {MQMT_MQE_FIELDS           ,"MQMT_MQE_FIELDS"},
  {-1,""}
};

/********************************************************************/
/* Structure: MonDefinitions                                        */
/*                                                                  */
/*   This structure is used to build tables used to convert PCF     */
/*   constants into text identifiers.                               */
/*                                                                  */
/********************************************************************/
struct MonDefinitions
{
  MQLONG Parameter;
  char *Identifier;
  struct ConstDefinitions *NamedValues;
};

struct MonDefinitions MonitoringByteStringFields[] =
{
  {MQBACF_CONNECTION_ID,     "ConnectionId", NULL},
  {MQBACF_MESSAGE_DATA,      "Message Data", NULL},
  {MQBACF_MSG_ID,            "Msg_id", NULL},
  {MQBACF_CORREL_ID,         "Correl_id", NULL},
  {MQBACF_MQMD_STRUCT,       "MQMD Structure", NULL},
  {MQBACF_MQGMO_STRUCT,      "MQGMO Structure", NULL},
  {MQBACF_MQPMO_STRUCT,      "MQPMO Structure", NULL},
  {MQBACF_MQCB_FUNCTION,     "Callback Function", NULL},
  {MQBACF_SUB_CORREL_ID,     "SUB_CORREL_ID"},
  {MQBACF_MQSTS_STRUCT ,     "MQSTS Structure"},
  {MQBACF_MQSD_STRUCT  ,     "MQSD Structure"},
  {MQBACF_XA_XID       ,     "XA_XID"},
  {MQBACF_ALTERNATE_SECURITYID,   "Alternate_security id", NULL},
  {MQBACF_MQCBC_STRUCT,      "MQCBC Structure", NULL},
  {MQBACF_MQCBD_STRUCT,      "MQCBD Structure", NULL},
  {MQBACF_MQCD_STRUCT,       "MQCD Structure", NULL},
  {MQBACF_MQCNO_STRUCT,      "MQCNO Structure", NULL},
  {MQBACF_GROUP_ID,          "GroupId", NULL},
  {MQBACF_ACCOUNTING_TOKEN,  "AccountingToken", NULL},
  {MQBACF_MQBO_STRUCT,       "MQBO Structure", NULL},
  {MQBACF_XQH_MSG_ID,        "Transmit Header Msg_id", NULL},
  {MQBACF_XQH_CORREL_ID,     "Transmit Header Correl_id", NULL}
};

struct MonDefinitions MonitoringIntegerFields[] =
{
  {MQIA_COMMAND_LEVEL,        "CommandLevel", NULL},
  {MQIACF_PROCESS_ID,         "ApplicationPid", NULL},
  {MQIACF_SEQUENCE_NUMBER,    "SeqNumber", NULL},
  {MQIACF_THREAD_ID,          "ApplicationTid", NULL},
  {MQIACF_HOBJ,               "Hobj", NULL},
  {MQIACF_CLOSE_OPTIONS,      "Close Options", NULL},
  {MQIACF_CONNECT_OPTIONS,    "Connect Options", NULL},
  {MQIACH_XMIT_PROTOCOL_TYPE, "Transport Type", XpTypes},
  {MQIACF_CTL_OPERATION,      "Control Operation", CtlOp},
  {MQIACF_OPERATION_ID,       "Operation Id", OperationId},
  {MQIACF_API_CALLER_TYPE,    "API Caller Type", ExitAPICaller},
  {MQIACF_API_ENVIRONMENT,    "API Environment", ExitEnvironment},
  {MQIA_APPL_TYPE,            "Application Type", AppType},
  {MQIA_PLATFORM,             "Platform", Platform},
  {MQIACF_COMP_CODE,          "Completion Code", CompCode},
  {MQIACF_MQCB_OPERATION,     "Callback Operation", CallbackOp},
  {MQIACF_MQCB_TYPE,          "Callback type", CallbackType},
  {MQIACF_CALL_TYPE,          "Call type", CallType},
  {MQIACF_MQCB_OPTIONS,       "Callback options",NULL},
  {MQIACF_SELECTOR_COUNT,     "Selector Count",NULL},
  {MQIACF_SELECTORS,          "Selectors",NULL},
  {MQIACF_INTATTR_COUNT,      "Int Attr Count",NULL},
  {MQIACF_INT_ATTRS,          "Int Attrs",NULL},
  {MQIACF_SUBRQ_ACTION,       "Sub Request Action",NULL},
  {MQIACF_NUM_PUBS,           "MQIACF_NUM_PUBS"},
  {MQIACF_HSUB,               "MQIACF_HSUB"},
  {MQIACF_SUBRQ_OPTIONS,      "MQIACF_SUBRQ_OPTIONS"},
  {MQIACF_SUB_OPTIONS,        "MQIACF_SUB_OPTIONS"},
  {MQIACF_NUM_PUBS,           "Num Pubs",NULL},
  {MQIACF_REASON_CODE,        "Reason Code", NULL},
  {MQIACF_TRACE_DETAIL,       "Trace Detail Level", NULL},
  {MQIACF_BUFFER_LENGTH,      "Buffer Length", NULL},
  {MQIACF_GET_OPTIONS,        "Get Options", NULL},
  {MQIACF_MSG_LENGTH,         "Msg length", NULL},
  {MQIACF_REPORT,             "Report Options", NULL},
  {MQIACF_MSG_TYPE,           "Msg_type", MsgTypes},
  {MQIACF_EXPIRY,             "Expiry", NULL},
  {MQIACF_PRIORITY,           "Priority", NULL},
  {MQIACF_PERSISTENCE,        "Persistence", NULL},
  {MQIA_CODED_CHAR_SET_ID,    "Coded_char_set_id", NULL},
  {MQIACF_ENCODING,           "Encoding", NULL},
  {MQIACF_OBJECT_TYPE,        "Object_type", ObjTypes},
  {MQIACF_OPEN_OPTIONS,       "Open_options", NULL},
  {MQIACF_RECS_PRESENT,       "Recs_present", NULL},
  {MQIACF_KNOWN_DEST_COUNT,   "Known_dest_count", NULL},
  {MQIACF_UNKNOWN_DEST_COUNT, "Unknown_dest_count", NULL},
  {MQIACF_INVALID_DEST_COUNT, "Invalid_dest_count", NULL},
  {MQIACF_RESOLVED_TYPE,      "Resolved_type", ObjTypes},
  {MQIACF_PUT_OPTIONS,        "Put Options", NULL},
  {MQIACH_MAX_MSG_LENGTH,     "Max message length", NULL},
  {MQIACF_XA_RMID,            "MQIACF_XA_RMID", NULL},
  {MQIACF_XA_FLAGS,           "MQIACF_XA_FLAGS", NULL},
  {MQIACF_XA_RETCODE,         "MQIACF_XA_RETCODE", NULL},
  {MQIACF_XA_HANDLE,          "MQIACF_XA_HANDLE"},
  {MQIACF_XA_RETVAL,          "MQIACF_XA_RETVAL"},
  {MQIACF_STATUS_TYPE,        "MQIACF_STATUS_TYPE"},
  {MQIACF_XA_COUNT,           "MQIACF_XA_COUNT"},
  {MQIACH_XMIT_PROTOCOL_TYPE, "Xmit Protocol", XpTypes},
  {MQIACF_TRACE_DATA_LENGTH,  "Trace Data Length", NULL},
  {MQIACF_FEEDBACK,           "Feedback", NULL},
  {MQIACF_POINTER_SIZE,       "Pointer size", NULL},
  {MQIACF_APPL_FUNCTION_TYPE, "Appl Function Type", FunTypes},
  {MQIACH_CHANNEL_TYPE,       "Channel Type", ChlTypes}
};

struct MonDefinitions MonitoringInteger64Fields[] =
{
  {MQIAMO64_HIGHRES_TIME,     "High Res Time", NULL},
  {MQIAMO64_QMGR_OP_DURATION, "QMgr Operation Duration", NULL}
};

struct MonDefinitions MonitoringStringFields[] =
{
  {MQCA_REMOTE_Q_MGR_NAME,        "RemoteQMgr", NULL},
  {MQCACF_APPL_NAME,              "ApplicationName", NULL},
  {MQCACF_USER_IDENTIFIER,        "UserId", NULL},
  {MQCACH_CONNECTION_NAME,        "ConnName", NULL},
  {MQCACH_CHANNEL_NAME,           "Channel Name", NULL},
  {MQCAMO_END_DATE,               "IntervalEndDate", NULL},
  {MQCAMO_END_TIME,               "IntervalEndTime", NULL},
  {MQCACF_OPERATION_DATE,         "OperationDate", NULL},
  {MQCACF_OPERATION_TIME,         "OperationTime", NULL},
  {MQCAMO_START_DATE,             "IntervalStartDate", NULL},
  {MQCAMO_START_TIME,             "IntervalStartTime", NULL},
  {MQCA_Q_MGR_NAME,               "QueueManager", NULL},
  {MQCACF_RESOLVED_Q_NAME,        "Resolved_Q_Name", NULL},
  {MQCACH_FORMAT_NAME,            "Format_name", NULL},
  {MQCACF_REPLY_TO_Q,             "Reply_to_Q ", NULL},
  {MQCACF_REPLY_TO_Q_MGR,         "Reply_to_Q_Mgr", NULL},
  {MQCACF_PUT_DATE,               "Put_date", NULL},
  {MQCACF_PUT_TIME,               "Put_time", NULL},
  {MQCACF_OBJECT_NAME,            "Object_name", NULL},
  {MQCACF_OBJECT_Q_MGR_NAME,      "Object_Q_mgr_name", NULL},
  {MQCACF_DYNAMIC_Q_NAME,         "Dynamic_Q_name", NULL},
  {MQCACF_RESOLVED_LOCAL_Q_NAME,  "Resolved_local_Q_name", NULL},
  {MQCACF_RESOLVED_LOCAL_Q_MGR,   "Resolved_local_Q_mgr", NULL},
  {MQCACF_RESOLVED_Q_MGR,         "Resolved_Q_mgr", NULL},
  {MQCACF_OBJECT_STRING,          "Object_string", NULL},
  {MQCACF_SELECTION_STRING,       "Selection_string", NULL},
  {MQCACF_ALTERNATE_USERID,       "Alternate_userid", NULL},
  {MQCACF_RESOLVED_OBJECT_STRING, "Resolved_object_string", NULL},
  {MQCACF_MQCB_NAME,              "Callback name", NULL},
  {MQCACF_CHAR_ATTRS,             "Character Attributes", NULL},
  {MQCACF_XA_INFO,                "XA Info", NULL},
  {MQCACF_SUB_NAME,               "Subscription Name", NULL},
  {MQCACF_SUB_USER_DATA,          "Subscription User Data", NULL},
  {MQCA_Q_NAME,                   "QueueName", NULL},
  {MQCA_POLICY_NAME,              "Protection Policy", NULL},
  {MQCACF_HOST_NAME,              "Host Name", NULL},
  {MQCACF_APPL_FUNCTION,          "Application Function", NULL},
  {MQCACF_XQH_REMOTE_Q_NAME,      "Transmit Header RemoteQName", NULL},
  {MQCACF_XQH_REMOTE_Q_MGR,       "Transmit Header RemoteQMgr", NULL},
  {MQCACF_XQH_PUT_DATE,           "Transmit Header Put_date", NULL},
  {MQCACF_XQH_PUT_TIME,           "Transmit Header Put_time", NULL}
};

struct MonDefinitions MonitoringGroupFields[] =
{
  {MQGACF_ACTIVITY_TRACE,  "MQI Operation"},
  {MQGACF_APP_DIST_LIST,   "Distribution List"}
};

void printMonInteger(char * _indent, MQLONG _parm, MQINT32 _value)
{
  int i;
  int tagId=-1;
  struct ConstDefinitions *pDef;
  for (i=0; (i < (sizeof(MonitoringIntegerFields) /
       sizeof(struct MonDefinitions))) && (tagId == -1); i++)
  {
    if (MonitoringIntegerFields[i].Parameter==_parm)
    {
      tagId=i;
    }
  }
  if (tagId != -1)
  {
    if (MonitoringIntegerFields[tagId].NamedValues == NULL)
    {
      printf("%s%s: %d\n",
             _indent,
             MonitoringIntegerFields[tagId].Identifier,
             _value);
    }
    else
    {
      pDef=MonitoringIntegerFields[tagId].NamedValues;
      for (;(pDef->Value != _value) && (pDef->Value != -1); pDef++)
        ;
      if (pDef->Value == _value)
      {
        printf("%s%s: %s\n",
               _indent,
               MonitoringIntegerFields[tagId].Identifier,
               pDef->Identifier);
      }
      else
      {
        printf("%s%s: %d\n",
               _indent,
               MonitoringIntegerFields[tagId].Identifier,
               _value);
      }
    }
  }
  else
  {
    printf("%s%d: %d\n",
           _indent,
           _parm,
           _value);
  }
  return;
}

void printMonIntegerList(char * _indent, MQLONG _parm, int _count, MQINT32 * _values)
{
  int i, j;
  int tagId=-1;
  for (i=0; (i < (sizeof(MonitoringIntegerFields) /
       sizeof(struct MonDefinitions))) && (tagId == -1); i++)
  {
    if (MonitoringIntegerFields[i].Parameter==_parm)
    {
      tagId=i;
    }
  }
  if (tagId != -1)
  {
    printf("%s%s: [",
           _indent,
           MonitoringIntegerFields[tagId].Identifier);
  }
  else
  {
    printf("%s%d: [",
           _indent,
           _parm);
  }
  for (j=0; j < _count; j++)
  {
    if (j == (_count -1))
    {
      printf("%d]\n", _values[j]);
    }
    else
    {
      printf("%d, ", _values[j]);
    }
  }
}

void printMonInteger64(char * _indent, MQLONG _parm, MQINT64 _value)
{
  int i;
  int tagId=-1;
  for (i=0; (i < (sizeof(MonitoringInteger64Fields) /
       sizeof(struct MonDefinitions))) && (tagId == -1); i++)
  {
    if (MonitoringInteger64Fields[i].Parameter==_parm)
    {
      tagId=i;
    }
  }
  if (tagId != -1)
  {
    printf("%s%s: %lld\n",
           _indent,
           MonitoringInteger64Fields[tagId].Identifier,
           _value);
  }
  else
  {
    printf("%s%d: %lld\n",
           _indent,
           _parm,
           _value);
  }
  return;
}

void printMonInteger64List(char * _indent, MQLONG _parm, int _count, MQINT64 * _values)
{
  int i, j;
  int tagId = -1;
  for (i=0; (i < (sizeof(MonitoringInteger64Fields) /
       sizeof(struct MonDefinitions))) && (tagId == -1); i++)
  {
    if (MonitoringInteger64Fields[i].Parameter==_parm)
    {
      tagId=i;
    }
  }
  if (tagId != -1)
  {
    printf("%s%s: [",
           _indent,
           MonitoringInteger64Fields[tagId].Identifier);
  }
  else
  {
    printf("%s%d: [",
           _indent,
           _parm);
  }
  for (j=0; j < _count; j++)
  {
    if (j == (_count -1))
    {
      printf("%lld]\n", _values[j]);
    }
    else
    {
      printf("%lld, ", _values[j]);
    }
  }
  return;
}

void printMonString(char * _indent, MQLONG _parm, int _len, char * _string)
{
  int i;
  int tagId = -1;
  int length = _len;
  for (i=0; (i < (sizeof(MonitoringStringFields) /
       sizeof(struct MonDefinitions))) && (tagId == -1); i++)
  {
    if (MonitoringStringFields[i].Parameter==_parm)
    {
      tagId=i;
    }
  }
  for (length=_len-1;
       _string[length] == '\0' || _string[length] == ' ';
       length--);
  if (tagId != -1)
  {
    printf("%s%s: '%.*s'\n",
           _indent,
           MonitoringStringFields[tagId].Identifier,
           length+1,
           _string);
  }
  else
  {
    printf("%s%d: '%.*s'\n",
           _indent,
           _parm,
           length+1,
           _string);
  }
  return;
}

void printMonStringList(char * _indent, MQLONG _parm, int _count, int _len, char * _strings)
{
  int i, j;
  int tagId = -1;
  for (i=0; (i < (sizeof(MonitoringStringFields) /
       sizeof(struct MonDefinitions))) && (tagId == -1); i++)
  {
    if (MonitoringStringFields[i].Parameter==_parm)
    {
      tagId=i;
    }
  }
  if (tagId != -1)
  {
    printf("%s%s: [",
           _indent,
           MonitoringStringFields[tagId].Identifier);
  }
  else
  {
    printf("%s%d: [",
           _indent,
           _parm);
  }
  for (j=0; j < _count; j++)
  {
    if (j == (_count -1))
    {
      printf("'%.*s']\n", _len, &(_strings[j * _len]));
    }
    else
    {
      printf("'%.*s', ", _len, &(_strings[j * _len]));
    }
  }
  printf("]\n");
  return;
}

void printMonByteString(char * _indent, MQLONG _parm, int _len, MQBYTE * _array)
{
  int i, j, k;
  int tagId=-1;
  MQCHAR hexline[80];
  for (i=0; (i < (sizeof(MonitoringByteStringFields) /
       sizeof(struct MonDefinitions))) && (tagId == -1); i++)
  {
    if (MonitoringByteStringFields[i].Parameter==_parm)
    {
      tagId=i;
    }
  }
  if (tagId != -1)
  {
    printf("%s%s:",
           _indent,
           MonitoringByteStringFields[tagId].Identifier);
  }
  else
  {
    printf("%s%d:",
           _indent,
           _parm);
  }
  i=0;
  do
  {
    j=0;
    printf("\n%s%08X: ",_indent, i);
    while ( (j < 16) && (i < _len) )
    {
      if (j % 2 == 0)
        printf(" ");
      k = (_array[i] & 0x000000FF);
      printf("%02X", k);
      hexline[j] = isprint(k) ? k : '.';
      j++;
      i++;
    }
    if (j < 16)
    {
      for ( ;j < 16; j++)
      {
        if (j % 2 == 0) printf(" ");
          printf("  ");
        hexline[j] = ' ';
      }
    }
    hexline[j] = '\0';
    printf("  '%s'",hexline);
  } while(i<_len);
  printf("\n");
}

void printMonGroup(char * _indent, MQLONG _parm, int _count)
{
  int i;
  int tagId = -1;
  for (i=0; (i < (sizeof(MonitoringIntegerFields) /
       sizeof(struct MonDefinitions))) && (tagId == -1); i++)
  {
    if (MonitoringGroupFields[i].Parameter==_parm)
    {
      tagId=i;
    }
  }
  if (tagId != -1)
  {
    printf("%s%s: %d\n",
           _indent,
           MonitoringGroupFields[tagId].Identifier,
           _count);
  }
  else
  {
    printf("%sGROUP(%d): %d\n",
           _indent,
           _parm,
           _count);
  }
  return;
}

/********************************************************************/
/*                                                                  */
/* Function: printMonitoring                                        */
/*                                                                  */
/*                                                                  */
/*   This function translates a message buffer containing an        */
/*   activity trace message as written by MQ and writes the         */
/*   formatted data to standard output.                             */
/*                                                                  */
/********************************************************************/
int printMonitoring(MQMD *pmd,
                    MQBYTE *buffer,
                    MQLONG buflen,
                    MQLONG Verbose)
{
  int frc=0;
  PMQCFH pcfh;
  char underline[78];                    /* Underline string        */

  /******************************************************************/
  /* First check the format of the message to ensure it is of a     */
  /* valid type for us to start processing.                         */
  /******************************************************************/
  if (memcmp(pmd->Format, MQFMT_ADMIN, sizeof(pmd->Format)) != 0)
  {
    fprintf(stderr, "Invalid monitoring record 'md.Format' = %8.8s.\n",
            pmd->Format);
    return -1;
  }

  /******************************************************************/
  /* Gain access to the PCF header and check the definitions to     */
  /* ensure this is indeed an monitoring PCF record.                */
  /******************************************************************/
  pcfh=(PMQCFH)buffer;

  if ((pcfh->Type != MQCFT_APP_ACTIVITY))
  {
    fprintf(stderr, "Invalid monitoring record 'cfh.Type' = %d.\n",
            (int)pcfh->Type);
    return -1;
  }

  buffer+=sizeof(MQCFH);
  switch (pcfh->Command)
  {
  case MQCMD_ACTIVITY_TRACE:
    printf("MonitoringType: MQI Activity Trace\n");
    /* Print message correlID*/
    printMonByteString("",
                       MQBACF_CORREL_ID,
                       MQ_CORREL_ID_LENGTH,
                       pmd->CorrelId);
    frc=printMonitoringRecord(pcfh->ParameterCount,
                              0,
                              &buffer,
                              &buflen,
                              Verbose);
    break;
  default:
    fprintf(stderr, "Invalid monitoring record 'cfh.Command' = %d.\n",
            (int)pcfh->Command);
    break;
  }

  memset(underline, '=', sizeof(underline)-1);
  underline[sizeof(underline)-1]='\0';
  printf("%s\n", underline);
  printf("\n");

  return frc;
}

/********************************************************************/


/********************************************************************/
/*                                                                  */
/* Function: getMonInteger                                          */
/*                                                                  */
/*   This function works out the string value of an integer parm.   */
/*                                                                  */
/********************************************************************/
char * getMonInteger(MQLONG _parm,
                     MQLONG _value,
                     MQCHAR *buffer,
                     MQLONG buflen)
{
  int i;
  int tagId=-1;
  struct ConstDefinitions *pDef;

  memset(buffer,0,buflen);
  for (i=0; (i < (sizeof(MonitoringIntegerFields) /
                  sizeof(struct MonDefinitions))) && (tagId == -1); i++)
  {
    if (MonitoringIntegerFields[i].Parameter==_parm)
    {
      tagId=i;
    }
  }
  if (tagId != -1)
  {
    if (MonitoringIntegerFields[tagId].NamedValues == NULL)
    {
      sprintf(buffer,"%s:%02d",
              MonitoringIntegerFields[tagId].Identifier, _value);
    }
    else
    {
      pDef=MonitoringIntegerFields[tagId].NamedValues;
      for (;(pDef->Value != _value) && (pDef->Value != -1); pDef++)
        ;
      if (pDef->Value == _value)
      {
        sprintf(buffer,"%s",
                pDef->Identifier);
      }
      else
      {
        sprintf(buffer,"%s:%0d",
                MonitoringIntegerFields[tagId].Identifier, _value);
      }
    }
  }
  else
  {
    sprintf(buffer,"%d:%02d",
            _parm,
            _value);
  }
  return buffer;
}

/********************************************************************/
/*                                                                  */
/* Function: printMonitoringRecord                                  */
/*                                                                  */
/*                                                                  */
/*   This function prints out the contents of a message buffer      */
/*   containing MQI activity trace records.                         */
/*                                                                  */
/********************************************************************/
int printMonitoringRecord(MQLONG parameterCount,
                          MQLONG indentCount,
                          MQBYTE **ppbuffer,
                          MQLONG *pbuflen,
                          MQLONG Verbose)
{
  int frc = 0;                        /* Function return code       */
  int rc = 0;                         /* Internal return code       */
  int groupCount=0;                   /* Group counter              */
  char indent[21];                    /* indentString               */
  MQBYTE *ptr = NULL;                 /* Current buffer pointer     */
  MQLONG bytesLeft = 0;               /* Number of remaining bytes  */
  MQLONG item_count = 0;              /* Parameter counter          */
  MQLONG Type = 0;                    /* Type of next structure     */
  MQLONG Length = 0;                  /* Length of next structure   */
  MQLONG Parameter = 0 ;              /* Type of next parameter     */
  MQLONG StringLength = 0 ;           /* Type of next parameter     */
  MQLONG Count = 0 ;                  /* Type of next parameter     */
  MQINT64 Value64 = 0;                /* 64 bit value               */
  MQLONG  Value32 = 0;                /* 32 bit value               */
  MQCHAR * StringData = NULL;         /* String value               */
  MQLONG * Values32 = NULL;           /* 32 bit integer values      */
  MQLONG ParameterCount = 0 ;         /* Parameter count in group   */

  if (indentCount <= (sizeof(indent)-1))
  {
    memset(indent, ' ', indentCount);
    indent[indentCount]='\0';
  }
  else
  {
    memset(indent, ' ', (sizeof(indent)-1));
    indent[sizeof(indent)-1]='\0';
  }

  bytesLeft=*pbuflen;
  ptr=*ppbuffer;
  for (item_count=0; (frc == 0) && (item_count < parameterCount); item_count++)
  {
    /****************************************************************/
    /* The first 12 bytes of every PCF structure contain its type,  */
    /* length and parameter ID.                                     */
    /****************************************************************/
    if (bytesLeft < (sizeof(MQLONG) * 3))
    {
      fprintf(stderr,
              "Premature end of buffer before processing complete.\n");
      frc=-1;
      break;
    }

    memcpy(&Type, (ptr + offsetof(MQCFIN, Type)), sizeof(MQLONG));
    memcpy(&Length, (ptr + offsetof(MQCFIN, StrucLength)), sizeof(MQLONG));
    memcpy(&Parameter, (ptr + offsetof(MQCFIN, Parameter)), sizeof(MQLONG));

    if (bytesLeft < Length)
    {
      fprintf(stderr,
              "Premature end of buffer before processing complete.\n");
      frc=-1;
      break;
    }

    switch (Type)
    {
    case MQCFT_INTEGER:
      memcpy(&Value32, (ptr + offsetof(MQCFIN, Value)), sizeof(MQLONG));

      printMonInteger(indent,
                      Parameter,
                      Value32);

      bytesLeft-=Length;
      ptr+=Length;
      break;

    case MQCFT_INTEGER64:
      memcpy(&Value64, (ptr + offsetof(MQCFIN64, Value)), sizeof(MQINT64));

      printMonInteger64(indent,
                        Parameter,
                        Value64);

      bytesLeft-=Length;
      ptr+=Length;
      break;

    case MQCFT_STRING:
      memcpy(&StringLength, (ptr + offsetof(MQCFST, StringLength)), sizeof(MQLONG));
      StringData = (MQCHAR *)(ptr + offsetof(MQCFST, String));

      printMonString(indent,
                     Parameter,
                     StringLength,
                     StringData);

      bytesLeft-=Length;
      ptr+=Length;
      break;

    case MQCFT_INTEGER_LIST:
      memcpy(&Count, (ptr + offsetof(MQCFIL, Count)), sizeof(MQLONG));
      Values32 = (MQLONG *)malloc(sizeof(MQLONG) * Count);
      if(Values32)
      {
        memcpy(Values32, (ptr + offsetof(MQCFIL, Values)), sizeof(MQLONG) * Count);

        printMonIntegerList(indent,
                            Parameter,
                            Count,
                            Values32);
        free(Values32);
        Values32 = NULL;
      }
      else
      {
        fprintf(stderr, "Unable to allocate storage for integer list (%d/%d).\n",
                Count, Parameter);
        frc=-1;
      }
      bytesLeft-=Length;
      ptr+=Length;
      break;

    case MQCFT_STRING_LIST:
      memcpy(&Count, (ptr + offsetof(MQCFSL, Count)), sizeof(MQLONG));
      memcpy(&StringLength, (ptr + offsetof(MQCFSL, StringLength)), sizeof(MQLONG));
      StringData = (MQCHAR *)(ptr + offsetof(MQCFSL, Strings));
      printMonStringList(indent,
                         Parameter,
                         Count,
                         StringLength,
                         StringData);

      bytesLeft-=Length;
      ptr+=Length;
      break;

    case MQCFT_BYTE_STRING:
      memcpy(&StringLength, (ptr + offsetof(MQCFBS, StringLength)), sizeof(MQLONG));
      StringData = (MQCHAR *)(ptr + offsetof(MQCFBS, String));

      printMonByteString(indent,
                         Parameter,
                         StringLength,
                         StringData);

      bytesLeft-=Length;
      ptr+=Length;
      break;

    case MQCFT_GROUP:
      rc = 0;
      memcpy(&ParameterCount, (ptr + offsetof(MQCFGR, ParameterCount)), sizeof(MQLONG));
      if (Verbose)
      {
        printMonGroup(indent, Parameter, groupCount);
      }
      groupCount++;
      bytesLeft-=Length;
      ptr+=Length;
      if (Verbose)
      {
        printMonitoringRecord(ParameterCount,
                              indentCount+2,
                              &ptr,
                              &bytesLeft,
                              Verbose);
      }
      else
      {
        printTerseTraceLine(ParameterCount,
                            indentCount+2,
                            &ptr,
                            &bytesLeft);

      }
      break;

    default:
      fprintf(stderr, "Error processing unknown PCF structure (%d)\n",
              (int)Type);
      break;
    }
  }

  *ppbuffer=ptr;
  *pbuflen=bytesLeft;

  return 0;
}

/********************************************************************/
/*                                                                  */
/* Function: printTerseTraceLine                                    */
/*                                                                  */
/*                                                                  */
/*   This function prints out a single line of trace for an MQI     */
/*   operation                                                      */
/*                                                                  */
/********************************************************************/
int printTerseTraceLine(MQLONG parameterCount,
                        MQLONG indentCount,
                        MQBYTE **ppbuffer,
                        MQLONG *pbuflen)
{
  int frc = 0;                        /* Function return code       */
  char indent[21];                    /* indentString               */
  MQBYTE *ptr = NULL;                 /* Current buffer pointer     */
  MQLONG bytesLeft = 0;               /* Number of remaining bytes  */
  MQLONG count = 0;                   /* Parameter counter          */
  MQLONG Type = 0;                    /* Type of next structure     */
  MQLONG Length = 0;                  /* Length of next structure   */
  MQLONG Parameter = 0;               /* Type of next parameter     */
  MQLONG ParameterCount = 0 ;         /* Parameter count in group   */
  MQLONG StringLength = 0 ;           /* Type of next parameter     */
  MQLONG  Value32 = 0;                /* 32 bit value               */
  MQCHAR operation[128];
  MQLONG tid = 0;
  MQCHAR operationDate[MQ_DATE_LENGTH+1];
  MQCHAR operationTime[MQ_TIME_LENGTH+1];
  MQCHAR CompCode[128];                /* Completion Code            */
  MQLONG ReasonCode = 0;               /* Reason Code                */
  MQLONG HObj = MQHO_UNUSABLE_HOBJ;    /* Object Handle              */
  MQCHAR ObjName[MQ_OBJECT_NAME_LENGTH+1]; /* Object Name            */
  MQCHAR buffer[1024];
  char *titleString = "%s%3s %-10s %-8s  %-15s %-13s %-4s  %-48s\n";
  char *formatString= "%s%03d %-10s %-8s  %-15s %-13s %04d";
  char underline[78];                    /* Underline string         */

  memset( ObjName, '\0', sizeof(ObjName));

  memset(underline, '=', sizeof(underline)-1);
  underline[sizeof(underline)-1]='\0';

  if (indentCount <= (sizeof(indent)-1))
  {
    memset(indent, ' ', indentCount);
    indent[indentCount]='\0';
  }
  else
  {
    memset(indent, ' ', (sizeof(indent)-1));
    indent[sizeof(indent)-1]='\0';
  }

  bytesLeft=*pbuflen;
  ptr=*ppbuffer;
  memset(operationDate,0,MQ_DATE_LENGTH+1);
  memset(operationTime,0,MQ_TIME_LENGTH+1);
  memset(operation,0,128);
  memset(CompCode,0,128);
  for (count=0; (frc == 0) && (count < parameterCount); count++)
  {
    /****************************************************************/
    /* The first 12 bytes of every PCF structure contain its type,  */
    /* length and parameter id... check that this looks valid       */
    /****************************************************************/
    if (bytesLeft < (sizeof(MQLONG) * 3))
    {
      fprintf(stderr,
              "Premature end of buffer before processing complete.\n");
      frc=-1;
      break;
    }

    memcpy(&Type, (ptr + offsetof(MQCFIN, Type)), sizeof(MQLONG));
    memcpy(&Length, (ptr + offsetof(MQCFIN, StrucLength)), sizeof(MQLONG));
    memcpy(&Parameter, (ptr + offsetof(MQCFIN, Parameter)), sizeof(MQLONG));

    if (bytesLeft < Length)
    {
      fprintf(stderr,
              "Premature end of buffer before processing complete.\n");
      frc=-1;
      break;
    }
    if (Type == MQCFT_GROUP)
    {
      memcpy(&ParameterCount, (ptr + offsetof(MQCFGR, ParameterCount)), sizeof(MQLONG));
      ptr+=Length;
      bytesLeft-=Length;
      frc=skipMonitoringRecord(ParameterCount,
                               &ptr,
                               &bytesLeft);
    }
    else
    {
      memset(buffer, 0, sizeof(buffer));
      switch (Parameter)
      {
      case MQIACF_OPERATION_ID:
        memcpy(&Value32, (ptr + offsetof(MQCFIN, Value)), sizeof(MQLONG));
        strcpy(operation, getMonInteger(Parameter,
                                        Value32, buffer, sizeof(buffer)));
        bytesLeft-=Length;
        ptr+=Length;
        break;

      case MQIACF_THREAD_ID:
        memcpy(&tid, (ptr + offsetof(MQCFIN, Value)), sizeof(MQLONG));
        bytesLeft-=Length;
        ptr+=Length;
        break;

      case MQCACF_OPERATION_DATE:
        memcpy(&StringLength, (ptr + offsetof(MQCFST, StringLength)), sizeof(MQLONG));
        memcpy(operationDate, (ptr + offsetof(MQCFST, String)), StringLength );
        bytesLeft-=Length;
        ptr+=Length;
        break;

      case MQCACF_OPERATION_TIME:
        memcpy(&StringLength, (ptr + offsetof(MQCFST, StringLength)), sizeof(MQLONG));
        memcpy(operationTime, (ptr + offsetof(MQCFST, String)), StringLength );
        bytesLeft-=Length;
        ptr+=Length;
        break;

      case MQIACF_COMP_CODE:
        memcpy(&Value32, (ptr + offsetof(MQCFIN, Value)), sizeof(MQLONG));
        strcpy(CompCode, getMonInteger(Parameter,
                                       Value32, buffer, sizeof(buffer)));
        bytesLeft-=Length;
        ptr+=Length;
        break;

      case MQIACF_REASON_CODE:
        memcpy(&Value32, (ptr + offsetof(MQCFIN, Value)), sizeof(MQLONG));
        ReasonCode = Value32;
        bytesLeft-=Length;
        ptr+=Length;
        break;

      case MQIACF_HOBJ:
        memcpy(&Value32, (ptr + offsetof(MQCFIN, Value)), sizeof(MQLONG));
        HObj = Value32;
        bytesLeft-=Length;
        ptr+=Length;
        break;

      case MQCACF_OBJECT_NAME:
        memcpy(&StringLength, (ptr + offsetof(MQCFST, StringLength)), sizeof(MQLONG));
        strncpy(ObjName, (MQCHAR *)(ptr + offsetof(MQCFST, String)),
                ( 48 > StringLength ? 48 : StringLength ) );
        bytesLeft-=Length;
        ptr+=Length;
        break;

      default:  /* Just ignore additional single parameters */
        bytesLeft-=Length;
        ptr+=Length;
        break;
      }
    }

  }
  /****************************************************************/
  /* Print the header if required                                 */
  /****************************************************************/
  if (printHeader == TRUE)
  {
    printf("%s\n", underline);
    printf(  titleString, indent,
             "Tid",  "Date", "Time","Operation",
             "CompCode","MQRC", "HObj (ObjName)");
    printHeader = FALSE;
  }
  /****************************************************************/
  /* Print the line of trace                                      */
  /****************************************************************/
  printf(  formatString, indent,
           tid,  operationDate, operationTime,operation,
           CompCode, ReasonCode);

  if ( HObj != MQHO_UNUSABLE_HOBJ)
  {
    printf(  "  %d", HObj);
    if ( ObjName[0] != '\0')
    {
      printf(  " (%s)", ObjName);
    }
  }
  else
  {
    printf(  "  -");
  }
  printf(  "\n");

  *ppbuffer=ptr;
  *pbuflen=bytesLeft;

  return 0;
}
/********************************************************************/
/*                                                                  */
/* Function: skipMonitoringRecord                                   */
/*                                                                  */
/*   This function accepts parses through a PCF message skipping    */
/*   the number of structures identified in the paramterCount       */
/*   parameter.                                                     */
/*                                                                  */
/********************************************************************/
int skipMonitoringRecord(MQLONG parameterCount,
                         MQBYTE **ppbuffer,
                         MQLONG *pbuflen)
{
  int      frc = 0;                /* Function return code          */
  MQBYTE   *ptr;                   /* Current buffer pointer        */
  MQLONG   bytesLeft;              /* Number of remaining bytes     */
  MQLONG   item_count;             /* Parameter counter             */
  MQLONG   Type = 0;               /* Type of next structure        */
  MQLONG   Length = 0;             /* Length of next structure      */
  MQLONG   Parameter = 0;          /* PCF param of next structure   */
  MQLONG   StringLength = 0 ;      /* Length of next parameter      */
  MQLONG   ParameterCount = 0 ;    /* Parameter count in group      */

  bytesLeft=*pbuflen;
  ptr=*ppbuffer;
  for (item_count=0; (frc == 0) && (item_count < parameterCount); item_count++)
  {
    /****************************************************************/
    /* The first 12 bytes of every PCF structure contain its type,  */
    /* length and parameter id so check that this looks valid...    */
    /****************************************************************/
    if (bytesLeft < (sizeof(MQLONG) * 3))
    {
      fprintf(stderr,
              "Premature end of buffer before processing complete.\n");
      frc=-1;
      break;
    }
    memcpy(&Type, (ptr + offsetof(MQCFIN, Type)), sizeof(MQLONG));
    memcpy(&Length, (ptr + offsetof(MQCFIN, StrucLength)), sizeof(MQLONG));
    memcpy(&Parameter, (ptr + offsetof(MQCFIN, Parameter)), sizeof(MQLONG));

    if (bytesLeft < Length)
    {
      fprintf(stderr,
              "Premature end of buffer before processing complete.\n");
      frc=-1;
      break;
    }

    switch (Type)
    {
    case MQCFT_INTEGER:
    case MQCFT_INTEGER64:
    case MQCFT_STRING:
    case MQCFT_INTEGER_LIST:
    case MQCFT_INTEGER64_LIST:
    case MQCFT_STRING_LIST:
    case MQCFT_BYTE_STRING:
      bytesLeft-=Length;
      ptr+=Length;
      break;

    case MQCFT_GROUP:
      memcpy(&ParameterCount, (ptr + offsetof(MQCFGR, ParameterCount)), sizeof(MQLONG));
      ptr+=Length;
      bytesLeft-=Length;
      frc=skipMonitoringRecord(ParameterCount,
                               &ptr,
                               &bytesLeft);

      break;

    default:
      fprintf(stderr, "Error processing unknown PCF structure (%d)\n",
              (int)Type);
      break;
    }
  }

  *ppbuffer=ptr;
  *pbuflen=bytesLeft;

  return  frc;
}

/********************************************************************/
/*                                                                  */
/* Function: getPCFDateTimeFromMsg                                  */
/*                                                                  */
/*                                                                  */
/*   This function accepts a PCF message and searches through       */
/*   the PCF structures for the identified Date Time strings.       */
/*                                                                  */
/********************************************************************/
int getPCFDateTimeFromMsg(MQBYTE *buffer,
                          MQLONG parmCount,
                          MQLONG dateParameter,
                          MQLONG timeParameter,
                          MQCHAR dateString[MQ_DATE_LENGTH+1],
                          MQCHAR timeString[MQ_DATE_LENGTH+1])
{
  MQBYTE   *bufptr;                /* Temporary buffer pointer      */
  int      item_count;             /* Loop counter                  */
  int      dateFound=FALSE;        /* Date parameter found          */
  int      timeFound=FALSE;        /* Time parameter found          */
  MQLONG   Type = 0;               /* Type of next structure        */
  MQLONG   Length = 0;             /* Length of next structure      */
  MQLONG   Parameter = 0;          /* PCF param of next structure   */
  MQLONG   StringLength = 0 ;      /* Length of next parameter      */
  MQLONG   ParameterCount = 0 ;    /* Parameter count in group      */

  bufptr=buffer;

  dateString[0]='\0';
  timeString[0]='\0';

  for (item_count=0; !(dateFound && timeFound) && (item_count < parmCount); item_count++)
  {
    memcpy(&Type, (bufptr + offsetof(MQCFIN, Type)), sizeof(MQLONG));
    memcpy(&Length, (bufptr + offsetof(MQCFIN, StrucLength)), sizeof(MQLONG));
    memcpy(&Parameter, (bufptr + offsetof(MQCFIN, Parameter)), sizeof(MQLONG));

    if (Type == MQCFT_STRING)
    {
      if (!dateFound && (Parameter == dateParameter))
      {
        memcpy(&StringLength, (bufptr + offsetof(MQCFST, StringLength)), sizeof(MQLONG));
        strncpy(dateString, (MQCHAR *)(bufptr + offsetof(MQCFST, String)), StringLength);
        dateString[MQ_DATE_LENGTH]='\0';
        dateFound=TRUE;
      }
      else if (!timeFound && (Parameter == timeParameter))
      {
        memcpy(&StringLength, (bufptr + offsetof(MQCFST, StringLength)), sizeof(MQLONG));
        strncpy(timeString, (MQCHAR *)(bufptr + offsetof(MQCFST, String)), StringLength);
        timeString[MQ_TIME_LENGTH]='\0';
        timeFound=TRUE;
      }
    }
    else if (Type == MQCFT_GROUP)
    {
      memcpy(&ParameterCount, (bufptr + offsetof(MQCFGR, ParameterCount)), sizeof(MQLONG));
      parmCount+=ParameterCount;
    }

    bufptr+=Length;
  }

  return(dateFound && timeFound)?0:1;
}

