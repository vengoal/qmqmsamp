/* %Z% %W% %I% %E% %U% */
/********************************************************************/
/*                                                                  */
/* Program name: AMQSMON0                                           */
/*                                                                  */
/* Description: Sample C program that gets an accounting message    */
/*              from the specified queue and writes the formatted   */
/*              data to standard output.                            */
/*   <copyright                                                     */
/*   notice="lm-source-program"                                     */
/*   pids="5724-H72"                                                */
/*   years="1994,2016"                                              */
/*   crc="1962249324" >                                             */
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
/*   AMQSMON0 is a sample C program to retrieve either statistics   */
/*   or accounting messages and write the formatted contents of     */
/*   the message to the screen.                                     */
/*                                                                  */
/*      -- sample reads from selected message queue                 */
/*                                                                  */
/*      -- displays the contents of the message to the screen       */
/*         formatted as name value pairs.                           */
/*                                                                  */
/*      -- Details of any failures are written to the screen        */
/*                                                                  */
/*      -- Program ends, returning a return code of                 */
/*           zero if a message was processed                        */
/*             1   if no messages were available to process         */
/*            -1   if any other error occurred                      */
/*                                                                  */
/*   Program logic:                                                 */
/*      Take name of input queue from the parameter                 */
/*      MQOPEN queue for INPUT                                      */
/*      MQGET message, remove from queue                            */
/*      .  If messages was available print formatted output         */
/*      MQCLOSE the subject queue                                   */
/*                                                                  */
/*                                                                  */
/********************************************************************/
/*                                                                  */
/*   AMQSMON0 has the following parameters                          */
/*                                                                  */
/*   amqsmon0 -t statistics | accounting                            */
/*            [-m qmgr]                  # Qmgr Name                */
/*            [-b]                       # Browse messages          */
/*            [ -a                |      # filter QMGR records      */
/*              -q <queueName>    |      # filter on queue          */
/*              -c <ChannelName>  |      # filter on channel        */
/*              -i <connectionId> ]      # filter on connectionId   */
/*            [ -d <maxMsgCount> ]       # Maximum records count    */
/*            [ -w <interval> ]          # No msg available timeout */
/*                                                                  */
/*                                                                  */
/*                                                                  */
/*                                                                  */
/*                                                                  */
/*                                                                  */
/*                                                                  */
/*                                                                  */
/********************************************************************/
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

#define MAX_FIELDLIST 128            /* Number of individual fields */
                                     /* which can be selected       */

#define FILTER_MQI_AND_QMGR     1
#define FILTER_QUEUE            2
#define FILTER_CHANNEL          3
#define FILTER_CONNECTION       4

/********************************************************************/
/* Private Function prototypes                                      */
/********************************************************************/
static int ParseOptions(int     argc,
                        char    *argv[],
                        MQLONG  *pmonitoringType,
                        char    qmgrName[MQ_Q_MGR_NAME_LENGTH],
                        MQLONG  *pbrowse,
                        MQLONG  *pmaximumRecords,
                        MQLONG  *pfilterType,
                        MQLONG  *pfilterObject,
                        MQLONG  *pwaitTime,
                        time_t  *pstartTime,
                        time_t  *pendTime,
                        MQCHAR  filterName[MQ_OBJECT_NAME_LENGTH],
                        MQLONG  fieldList[MAX_FIELDLIST]);

int checkPCFStringInMsg(MQBYTE *buffer,
                        MQLONG parmCount,
                        MQLONG parameter,
                        MQLONG valueLength,
                        MQCHAR *value);

int checkPCFByteStringInMsg(MQBYTE *buffer,
                            MQLONG parmCount,
                            MQLONG parameter,
                            MQLONG valueLength,
                            MQBYTE *value);

int getPCFDateTimeFromMsg(MQBYTE *buffer,
                          MQLONG parmCount,
                          MQLONG dateParameter,
                          MQLONG timeParameter,
                          MQCHAR dateString[MQ_DATE_LENGTH+1],
                          MQCHAR timeString[MQ_DATE_LENGTH+1]);

int printMonitoring(MQMD   *pmd,
                    MQBYTE *buffer,
                    MQLONG buflen,
                    MQLONG filterObject,
                    MQCHAR filterName[MQ_OBJECT_NAME_LENGTH],
                    MQLONG *fieldList);

int printMonitoringRecord(MQLONG parameterCount,
                          MQLONG indentCount,
                          MQBYTE **ppbuffer,
                          MQLONG *pbuflen,
                          MQLONG filterObject,
                          MQCHAR filterName[MQ_OBJECT_NAME_LENGTH],
                          MQLONG *fieldList);

int skipMonitoringRecord(MQLONG parameterCount,
                         MQBYTE **ppbuffer,
                         MQLONG *pbuflen);

int checkFieldInList(MQLONG value,
                     MQLONG *valueList);

/********************************************************************/
/* MAIN                                                             */
/********************************************************************/
int main(int argc, char **argv)
{

  /*   Declare MQI structures needed                                */
  MQOD     od = {MQOD_DEFAULT};    /* Object Descriptor             */
  MQMD     md = {MQMD_DEFAULT};    /* Message Descriptor            */
  MQGMO   gmo = {MQGMO_DEFAULT};   /* get message options           */
     /** note, sample uses defaults where it can **/

  MQHCONN  Hcon;                   /* connection handle             */
  MQHOBJ   Hobj;                   /* object handle                 */
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
  MQLONG   monitoringType;
  MQLONG   filterType;
  MQLONG   filterObject;
  MQLONG   waitInterval=0;
  time_t   intervalStartTime;
  time_t   intervalEndTime;
  MQCHAR   filterName[MQ_OBJECT_NAME_LENGTH];
  MQLONG   browse = FALSE;
  MQLONG   maximumRecords = -1;
  MQLONG   RecordNum=0;
  MQLONG   fieldList[MAX_FIELDLIST];

  /******************************************************************/
  /* Parse the options from the command line                        */
  /******************************************************************/
  prc = ParseOptions(argc,
                     argv,
                     &monitoringType,
                     qmgrName,
                     &browse,
                     &maximumRecords,
                     &filterType,
                     &filterObject,
                     &waitInterval,
                     &intervalStartTime,
                     &intervalEndTime,
                     filterName,
                     fieldList);

  if (prc != 0)
  {
    int pad=(int)strlen(argv[0]);
    fprintf(stderr,
      "Usage: %s [-m QMgrName]\n"
      "       %*.s [-t statistics]          # Process statistics queue\n"
      "       %*.s    [-a]                  # Show only QMGR statistics\n"
      "       %*.s    [-q [queue_name]]     # Show only Queue statistics\n"
      "       %*.s    [-c [channel_name]]   # Show only Channel statistics\n"
      "       %*.s [-t accounting]          # Process accounting queue\n"
      "       %*.s    [-a]                  # Process only MQI accounting\n"
      "       %*.s    [-q] [queue_name]]    # Process only Queue accounting\n"
      "       %*.s    [-i <connectionId>]   # Process only matching connection\n"
      "       %*.s [-b]                     # Only browse records\n"
      "       %*.s [-d <depth>]             # Number of records to display\n"
      "       %*.s [-w <timeout>]           # Time to wait (in seconds)\n"
      "       %*.s [-l <fieldlist,...>]     # Display only these fields\n"
      "       %*.s [-s <startTime>]         # Start time of record to process\n"
      "       %*.s [-e <endTime>]           # End time of record to process\n",
      argv[0],   /* -m */
      pad, " ",  /* -t */
      pad, " ",  /* -a */
      pad, " ",  /* -q */
      pad, " ",  /* -c */
      pad, " ",  /* -t */
      pad, " ",  /* -a */
      pad, " ",  /* -q */
      pad, " ",  /* -i */
      pad, " ",  /* -b */
      pad, " ",  /* -d */
      pad, " ",  /* -w */
      pad, " ",  /* -l */
      pad, " ",  /* -s */
      pad, " "); /* -e */
    exit(-1);
  }

  /******************************************************************/
  /*                                                                */
  /*   Create object descriptor for subject queue                   */
  /*                                                                */
  /******************************************************************/
  switch(monitoringType)
  {
    case MQCFT_ACCOUNTING:
      strncpy(od.ObjectName, "SYSTEM.ADMIN.ACCOUNTING.QUEUE", MQ_Q_NAME_LENGTH);
      break;
    case MQCFT_STATISTICS:
      strncpy(od.ObjectName, "SYSTEM.ADMIN.STATISTICS.QUEUE", MQ_Q_NAME_LENGTH);
      break;
    default:
      fprintf(stderr, "Error: Unknown monitoring Type\n");
      exit(-1);
      break;
  }

  /******************************************************************/
  /*                                                                */
  /*   Connect to queue manager                                     */
  /*                                                                */
  /******************************************************************/
  MQCONN(qmgrName,                /* queue manager                  */
         &Hcon,                   /* connection handle              */
         &CompCode,               /* completion code                */
         &Reason);                /* reason code                    */

  /* report reason and stop if it failed     */
  if (CompCode == MQCC_FAILED)
  {
    fprintf(stderr, "MQCONN ended with reason code %d\n", Reason);
    exit(-1);
  }

  /******************************************************************/
  /*                                                                */
  /*   Open the named message queue for input; exclusive or shared  */
  /*   use of the queue is controlled by the queue definition here  */
  /*                                                                */
  /******************************************************************/

  O_options = MQOO_INPUT_AS_Q_DEF    /* open queue for input      */
            | MQOO_BROWSE            /* but not if MQM stopping   */
            | MQOO_FAIL_IF_QUIESCING /* but not if MQM stopping   */
            ;

  MQOPEN(Hcon,                      /* connection handle            */
         &od,                       /* object descriptor for queue  */
         O_options,                 /* open options                 */
         &Hobj,                     /* object handle                */
         &CompCode,                 /* completion code              */
         &Reason);                  /* reason code                  */

  /* report reason, if any; stop if failed      */
  if ((CompCode == MQCC_FAILED) &&
      (Reason != MQRC_NONE))
  {
    fprintf(stderr, "MQOPEN ended with reason code %d\n", Reason);
    exit(-1);
  }

  /******************************************************************/
  /* Initialise the buffer used for getting the message             */
  /******************************************************************/
  buflen = 1024 * 100; /* 100K */
  buffer=(MQBYTE *)malloc(buflen);
  if (buffer == NULL)
  {
    fprintf(stderr, "Failed to allocate memory(%d) for message\n", buflen);
    exit(-1);
  }

  /******************************************************************/
  /* Main processing loop                                           */
  /******************************************************************/
  do
  {
    gmo.Options = MQGMO_CONVERT;      /* convert if necessary         */

    if (StartBrowse)
    {
      gmo.Options |= MQGMO_BROWSE_FIRST;
      StartBrowse=FALSE;
    }
    else
    {
      gmo.Options |= MQGMO_BROWSE_NEXT;
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
      if ((CompCode == MQCC_WARNING) && (Reason == MQRC_TRUNCATED_MSG_FAILED))
      {
        buflen=messlen;
        buffer=(MQBYTE *)realloc(buffer, buflen);
        if (buffer == NULL)
        {
          fprintf(stderr,
            "Failed to allocate memory(%d) for message\n", buflen);
          exit(-1);
        }
      }
    } while ((CompCode==MQCC_WARNING) && (Reason == MQRC_TRUNCATED_MSG_FAILED));

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
        fprintf(stderr, "Failed to retrieve message from queue [reason: %d]\n",
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

      if ((pcfh->Type != MQCFT_STATISTICS) &&
          (pcfh->Type != MQCFT_ACCOUNTING))
      {
        continue;
      }

      if ((filterType != 0) && (filterType != pcfh->Command))
      {
        continue;
      }

      if ((filterObject == FILTER_QUEUE) && (filterName[0] != '\0') &&
          (checkPCFStringInMsg(buffer + sizeof(MQCFH),
                               pcfh->ParameterCount,
                               MQCA_Q_NAME,
                               MQ_Q_NAME_LENGTH,
                               filterName) != 0))
      {
        continue;
      }
      else if ((filterObject == FILTER_CONNECTION) && (filterName[0] != '\0') &&
          (checkPCFByteStringInMsg(buffer + sizeof(MQCFH),
                                   pcfh->ParameterCount,
                                   MQBACF_CONNECTION_ID,
                                   MQ_CONNECTION_ID_LENGTH,
                                   (MQBYTE *)filterName) != 0))
      {
        continue;
      }
      else if ((filterObject == FILTER_CHANNEL) && (filterName[0] != '\0') &&
          (checkPCFStringInMsg(buffer + sizeof(MQCFH),
                               pcfh->ParameterCount,
                               MQCACH_CHANNEL_NAME,
                               MQ_CHANNEL_NAME_LENGTH,
                               filterName) != 0))
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
          sscanf(MsgStartTime, "%2u.%2u.%2u",
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

          startTm.tm_mon-=1;
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
          sscanf(MsgEndTime, "%2u.%2u.%2u",
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

          endTm.tm_mon-=1;
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
      /* If we are not browsing the queue then reissue the get.     */
      /**************************************************************/
      if (!browse)
      {
        gmo.Options = MQGMO_CONVERT |     /* convert if necessary         */
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
          continue;
        }
      }

      /**************************************************************/
      /* We have found a matching message so process it.            */
      /**************************************************************/
      prc=printMonitoring(&md,
                          buffer,
                          messlen,
                          filterObject,
                          filterName,
                          (fieldList[0] != 0)?fieldList:NULL);
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

  /******************************************************************/
  /*                                                                */
  /*   Close the source queue (if it was opened)                    */
  /*                                                                */
  /******************************************************************/
  C_options = MQCO_NONE;             /* no close options            */

  MQCLOSE(Hcon,                      /* connection handle           */
          &Hobj,                     /* object handle               */
          C_options,
          &CompCode,                 /* completion code             */
          &Reason);                  /* reason code                 */

  if (Reason != MQRC_NONE)
  {
    if ((CompCode != MQRC_CONNECTION_BROKEN) &&
        (CompCode != MQRC_Q_MGR_STOPPING))
    {
      fprintf(stderr, "MQCLOSE ended with reason code %d\n", Reason);
    }
  }

  /******************************************************************/
  /*                                                                */
  /*   Disconnect from MQM                                          */
  /*                                                                */
  /******************************************************************/
  MQDISC(&Hcon,                       /* connection handle          */
           &CompCode,                 /* completion code            */
           &Reason);                  /* reason code                */

  if (Reason != MQRC_NONE)
  {
    if ((CompCode != MQRC_CONNECTION_BROKEN) &&
        (CompCode != MQRC_Q_MGR_STOPPING))
    {
      fprintf(stderr, "MQDISC ended with reason code %d\n", Reason);
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
                        MQLONG  *pmonitoringType,
                        char    qmgrName[MQ_Q_MGR_NAME_LENGTH],
                        MQLONG  *pbrowse,
                        MQLONG  *pmaximumRecords,
                        MQLONG  *pfilterType,
                        MQLONG  *pfilterObject,
                        MQLONG  *pwaitTime,
                        time_t  *pstartTime,
                        time_t  *pendTime,
                        MQCHAR  filterName[MQ_OBJECT_NAME_LENGTH],
                        MQLONG  fieldList[MAX_FIELDLIST])
{
  int counter;              /* Argument counter                    */
  int InvalidArgument=0;    /* Number of invalid argument          */
  int error=FALSE;          /* Number of invalid argument          */
  int i;                    /* Simple counter                      */
  char *p;                  /* Simple character pointer            */
  char *parg;               /* pointer to current argument         */
  char *pval;               /* pointer to current value            */
  char accountingString[]="accounting";
  char statisticsString[]="statistics";

  struct tm startTime={0};
  struct tm endTime={0};

  int typeSpecified = FALSE;
  int filterSpecified = FALSE;
  int browseSpecified = FALSE;
  int depthSpecified = FALSE;
  int qmgrSpecified=FALSE;
  int waitSpecified=FALSE;
  int listSpecified=FALSE;
  int startTimeSpecified=FALSE;
  int endTimeSpecified=FALSE;

  memset(qmgrName, 0, MQ_Q_MGR_NAME_LENGTH);
  *pbrowse=FALSE;
  *pmaximumRecords=-1;
  *pfilterType=0;
  *pfilterObject=0;
  *pwaitTime=0;
  *pstartTime=-1;
  *pendTime=-1;
  memset(filterName, 0, MQ_OBJECT_NAME_LENGTH);
  memset(fieldList, 0, (sizeof(MQLONG) * MAX_FIELDLIST));

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
      case 't': /* Statistics || Accounting */
        if (typeSpecified)
        {
          InvalidArgument=counter;
        }
        else
        {
          typeSpecified=TRUE;
          for (p=&(parg[2]); *p==' '; p++) ; /* Strip off spaces        */
          if ((*p == '\0') && ((counter+1) < argc))
          {
            pval=argv[++counter];
          }
          else
          {
            pval=p;
          }

          /* Check is accounting specified */
          for (i=0; (pval[i] != '\0') &&
                    (toupper(pval[i]) == toupper(accountingString[i])); i++)
            ;
          if ((accountingString[i] == '\0') && (pval[i] == '\0'))
          {
            *pmonitoringType=MQCFT_ACCOUNTING;
          }
          else
          {
            /* Check is accounting specified */
            for (i=0; (pval[i] != '\0') &&
                      (toupper(pval[i]) == toupper(statisticsString[i])); i++)
              ;
            if ((statisticsString[i] == '\0') && (pval[i] == '\0'))
            {
              *pmonitoringType=MQCFT_STATISTICS;
            }
            else
            {
              InvalidArgument=counter;
            }
          }
        }
        break;
      case 'a': /* Show only MQI Accounting or QMGR statistics */
        if (filterSpecified)
        {
          InvalidArgument=counter;
        }
        else
        {
          filterSpecified=TRUE;
          *pfilterObject=FILTER_MQI_AND_QMGR;
        }
        break;
      case 'q': /* Show only Queue Accounting or statistics    */
        if (filterSpecified)
        {
          InvalidArgument=counter;
        }
        else
        {
          filterSpecified=TRUE;
          *pfilterObject=FILTER_QUEUE;
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

          if ((pval != NULL) && (*pval != '\0'))
          {
            for (i=0; (i < MQ_OBJECT_NAME_LENGTH) && (pval[i] != '\0'); i++)
            {
              filterName[i]=pval[i];
            }
          }
        }
        break;
      case 'c': /* Show only Channel statistics    */
        if (filterSpecified)
        {
          InvalidArgument=counter;
        }
        else
        {
          filterSpecified=TRUE;
          *pfilterObject=FILTER_CHANNEL;
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

          if ((pval != NULL) && (*pval != '\0'))
          {
            for (i=0; (i < MQ_OBJECT_NAME_LENGTH) && (pval[i] != '\0'); i++)
            {
              filterName[i]=pval[i];
            }
          }
        }
        break;
      case 'i': /* Show only Connection accounting    */
        if (filterSpecified)
        {
          InvalidArgument=counter;
        }
        else
        {
          filterSpecified=TRUE;
          *pfilterObject=FILTER_CONNECTION;
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

          if ((pval != NULL) && (*pval != '\0'))
          {
            if (strlen(pval) == 48)
            {
              char convHex[3];

              convHex[2]='\0';

              for (i=0; (i < 24); i++)
              {
                convHex[0]=pval[i*2];
                convHex[1]=pval[(i*2)+1];
                filterName[i]=(MQCHAR)strtol(convHex, NULL, 16);
              }
            }
            else
            {
              InvalidArgument=counter;
            }
          }
        }
        break;
      case 'b': /* Browse messages only */
        if (browseSpecified)
        {
          InvalidArgument=counter;
        }
        else
        {
          browseSpecified=TRUE;
          *pbrowse=TRUE;
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
      case 'l': /* The subset of fields to display  */
        if (listSpecified)
        {
          InvalidArgument=counter;
        }
        else
        {
          listSpecified=TRUE;
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
            char intBuffer[11];
            int fieldCount=0;
            int i=0;
            int value=0;
            int endLoop=FALSE;

            for (; *pval != '\0'; pval++)
            do
            {
              if (isdigit(*pval) && (i < (sizeof(intBuffer)-1)))
              {
                intBuffer[i++]=*pval;
                pval++;
              }
              else
              {
                if (fieldCount < MAX_FIELDLIST)
                {
                  intBuffer[i]='\0';
                  value=atoi(intBuffer);
                  if (value != 0)
                  {
                    fieldList[fieldCount++]=value;
                  }
                  i=0;
                }
                else
                {
                  InvalidArgument=i;
                }
                if (*pval == '\0')
                {
                  endLoop=TRUE;
                }
                else
                {
                  pval++;
                }
              }
            } while (!endLoop);
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
            sscanf(pval, "%4u-%2u-%2u %2u.%2u.%2u",
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
              startTime.tm_mon-=1;
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
            sscanf(pval, "%4u-%2u-%2u %2u.%2u.%2u",
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
              endTime.tm_mon-=1;
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
  /* We done the tokenisation now do the lexical analysis           */
  /******************************************************************/
  if (!error)
  {
    if (!typeSpecified)
    {
      fprintf(stderr, "You must select either the accounting or statistics queue\n");
      error=TRUE;
    }
  }

  if ((!error) && (filterSpecified))
  {
    if (*pmonitoringType == MQCFT_STATISTICS)
    {
      if (*pfilterObject == FILTER_MQI_AND_QMGR)
      {
        *pfilterType = MQCMD_STATISTICS_MQI;
      }
      else if (*pfilterObject == FILTER_QUEUE)
      {
        *pfilterType = MQCMD_STATISTICS_Q;
      }
      else if (*pfilterObject == FILTER_CHANNEL)
      {
        *pfilterType = MQCMD_STATISTICS_CHANNEL;
      }
      else
      {
        fprintf(stderr,
          "Invalid filter option specified for statistics reporting\n");
        error=TRUE;
      }
    }
    else
    {
      if (*pfilterObject == FILTER_MQI_AND_QMGR)
      {
        *pfilterType = MQCMD_ACCOUNTING_MQI;
      }
      else if (*pfilterObject == FILTER_QUEUE)
      {
        *pfilterType = MQCMD_ACCOUNTING_Q;
      }
      else if (*pfilterObject == FILTER_CONNECTION)
      {
        *pfilterType = 0;
      }
      else
      {
        fprintf(stderr,
          "Invalid filter option specified for accounting reporting\n");
        error=TRUE;
      }
    }
  }

  return error;
}

/********************************************************************/
/*                                                                  */
/* Function: printMonitoring                                        */
/*                                                                  */
/*                                                                  */
/*   This function translates a message buffer containing an        */
/*   accounting or statics message as written by MQ and writes      */
/*   the formatted data to standard output.                         */
/*                                                                  */
/********************************************************************/
int printMonitoring(MQMD *pmd,
                    MQBYTE *buffer,
                    MQLONG buflen,
                    MQLONG filterObject,
                    MQCHAR filterName[MQ_OBJECT_NAME_LENGTH],
                    MQLONG *fieldList)
{
  int frc=0;
  PMQCFH pcfh;

  /******************************************************************/
  /* First check the formated of the message to ensure it is of a   */
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

  if ((pcfh->Type != MQCFT_ACCOUNTING) &&
      (pcfh->Type != MQCFT_STATISTICS))
  {
    fprintf(stderr, "Invalid monitoring record 'cfh.Type' = %d.\n",
            pcfh->Type);
    return -1;
  }

  buffer+=sizeof(MQCFH);
  switch(pcfh->Command)
  {
    case MQCMD_ACCOUNTING_MQI:
      printf("MonitoringType: MQIAccounting\n");
      frc=printMonitoringRecord(pcfh->ParameterCount,
                                0,
                                &buffer,
                                &buflen,
                                filterObject,
                                filterName,
                                fieldList);
      break;
    case MQCMD_ACCOUNTING_Q:
      printf("MonitoringType: QueueAccounting\n");
      frc=printMonitoringRecord(pcfh->ParameterCount,
                                0,
                                &buffer,
                                &buflen,
                                filterObject,
                                filterName,
                                fieldList);
      break;
    case MQCMD_STATISTICS_MQI:
      printf("MonitoringType: MQIStatistics\n");
      frc=printMonitoringRecord(pcfh->ParameterCount,
                                0,
                                &buffer,
                                &buflen,
                                filterObject,
                                filterName,
                                fieldList);
      break;
    case MQCMD_STATISTICS_CHANNEL:
      printf("MonitoringType: ChannelStatistics\n");
      frc=printMonitoringRecord(pcfh->ParameterCount,
                                0,
                                &buffer,
                                &buflen,
                                filterObject,
                                filterName,
                                fieldList);
      break;
    case MQCMD_STATISTICS_Q:
      printf("MonitoringType: QueueStatistics\n");
      frc=printMonitoringRecord(pcfh->ParameterCount,
                                0,
                                &buffer,
                                &buflen,
                                filterObject,
                                filterName,
                                fieldList);
      break;
    default:
      fprintf(stderr, "Invalid monitoring record 'cfh.Command' = %d.\n",
            pcfh->Command);
      break;
  }

  printf("\n");

  return frc;
}

/********************************************************************/
/********************************************************************/
/*                                                                  */
/* Keywords and Constants                                           */
/*                                                                  */
/*                                                                  */
/*   The following Keywords and Constants are used for formatting   */
/*   the PCF statistics and accounting Data.                        */
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

static struct ConstDefinitions QueueTypes[] =
{
  {MQQT_LOCAL, "Local"},
  {MQQT_MODEL, "Model"},
  {MQQT_ALIAS, "Alias"},
  {MQQT_REMOTE, "Remote"},
  {MQQT_CLUSTER, "Cluster"},
  {-1, ""}
};

static struct ConstDefinitions QueueDefTypes[] =
{
  {MQQDT_PREDEFINED, "Predefined"},
  {MQQDT_PERMANENT_DYNAMIC, "PermanentDynamic"},
  {MQQDT_TEMPORARY_DYNAMIC, "TemporaryDynamic"},
  {MQQDT_SHARED_DYNAMIC, "SharedDynamic"},
  {-1, ""}
};

static struct ConstDefinitions ChannelTypes[] =
{
  {MQCHT_SENDER, "Sender"},
  {MQCHT_SERVER, "Server"},
  {MQCHT_RECEIVER, "Receiver"},
  {MQCHT_REQUESTER, "Requester"},
  {MQCHT_SVRCONN, "SvrConn"},
  {MQCHT_CLUSRCVR, "ClusRcvr"},
  {MQCHT_CLUSSDR, "ClusSdr"},
  {-1, ""}
};

static struct ConstDefinitions DisconnectTypes[] =
{
  {MQDISCONNECT_NORMAL, "Normal"},
  {MQDISCONNECT_IMPLICIT, "Implicit"},
  {MQDISCONNECT_Q_MGR, "QMgr"},
  {-1, ""}
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
  {MQBACF_CONNECTION_ID,      "ConnectionId", NULL}
};

struct MonDefinitions MonitoringIntegerFields[] =
{
  {MQIA_COMMAND_LEVEL,        "CommandLevel"},
  {MQIA_DEFINITION_TYPE,      "QueueDefinitionType", QueueDefTypes},
  {MQIA_Q_TYPE,               "QueueType", QueueTypes},
  {MQIACF_PROCESS_ID,         "ApplicationPid", NULL},
  {MQIACF_SEQUENCE_NUMBER,    "SeqNumber", NULL},
  {MQIACF_THREAD_ID,          "ApplicationTid", NULL},
  {MQIACH_CHANNEL_TYPE,       "ChannelType", ChannelTypes},
  {MQIAMO_AVG_BATCH_SIZE,     "AverageBatchSize", NULL},
  {MQIAMO_BACKOUTS,           "BackCount", NULL},
  {MQIAMO_BROWSES_FAILED,     "BrowseFailCount", NULL},
  {MQIAMO_BROWSES,            "BrowseCount", NULL},
  {MQIAMO_BROWSE_MAX_BYTES,   "BrowseMaxBytes", NULL},
  {MQIAMO_BROWSE_MIN_BYTES,   "BrowseMinBytes", NULL},
  {MQIAMO_CBS,                "CbCount", NULL},
  {MQIAMO_CBS_FAILED,         "CbFailCount", NULL},
  {MQIAMO_CLOSES,             "CloseCount", NULL},
  {MQIAMO_CLOSES_FAILED,      "CloseFailCount", NULL},
  {MQIAMO_COMMITS,            "CommitCount", NULL},
  {MQIAMO_COMMITS_FAILED,     "CommitFailCount", NULL},
  {MQIAMO_CONNS,              "ConnCount", NULL},
  {MQIAMO_CONNS_FAILED,       "ConnFailCount", NULL},
  {MQIAMO_CONNS_MAX,          "ConnHighwater", NULL},
  {MQIAMO_CTLS,               "CtlCount", NULL},
  {MQIAMO_CTLS_FAILED,        "CtlFailCount", NULL},
  {MQIAMO_DISC_TYPE,          "DiscType", DisconnectTypes},
  {MQIAMO_DISCS,              "DiscCount", NULL},
  {MQIAMO_EXIT_TIME_AVG,      "ExitMinAverage", NULL},
  {MQIAMO_EXIT_TIME_MAX,      "ExitMaxTime", NULL},
  {MQIAMO_EXIT_TIME_MIN,      "ExitMinTime", NULL},
  {MQIAMO_FULL_BATCHES,       "FullBatchCount", NULL},
  {MQIAMO_GENERATED_MSGS,     "GeneratedMsgCount", NULL},
  {MQIAMO_GETS_FAILED,        "GetFailCount", NULL},
  {MQIAMO_GETS,               "GetCount", NULL},
  {MQIAMO_GET_MAX_BYTES,      "GetMaxBytes", NULL},
  {MQIAMO_GET_MIN_BYTES,      "GetMinBytes", NULL},
  {MQIAMO_INCOMPLETE_BATCHES, "IncmplBatchCount", NULL},
  {MQIAMO_INQS,               "InqCount", NULL},
  {MQIAMO_INQS_FAILED,        "InqFailCount", NULL},
  {MQIAMO_MSGS,               "MsgCount", NULL},
  {MQIAMO_MSGS_EXPIRED,       "ExpiredMsgCount", NULL},
  {MQIAMO_MSGS_NOT_QUEUED,    "NonQueuedMsgCount", NULL},
  {MQIAMO_MSGS_PURGED,        "PurgeCount", NULL},
  {MQIAMO_NET_TIME_AVG,       "NetMinAverage", NULL},
  {MQIAMO_NET_TIME_MAX,       "NetMaxTime", NULL},
  {MQIAMO_NET_TIME_MIN,       "NetMinTime", NULL},
  {MQIAMO_OBJECT_COUNT,       "ObjectCount", NULL},
  {MQIAMO_OPENS,              "OpenCount", NULL},
  {MQIAMO_OPENS_FAILED,       "OpenFailCount", NULL},
  {MQIAMO_PUBLISH_MSG_COUNT,  "PublishMsgCount", NULL},
  {MQIAMO_PUT_RETRIES,        "PutRetryCount", NULL},
  {MQIAMO_PUT1S,              "Put1Count", NULL},
  {MQIAMO_PUT1S_FAILED,       "Put1FailCount", NULL},
  {MQIAMO_PUTS,               "PutCount", NULL},
  {MQIAMO_PUTS_FAILED,        "PutFailCount", NULL},
  {MQIAMO_PUT_MAX_BYTES,      "PutMaxBytes", NULL},
  {MQIAMO_PUT_MIN_BYTES,      "PutMinBytes", NULL},
  {MQIAMO_Q_MIN_DEPTH,        "QMinDepth", NULL},
  {MQIAMO_Q_MAX_DEPTH,        "QMaxDepth", NULL},
  {MQIAMO_SETS,               "SetCount", NULL},
  {MQIAMO_SETS_FAILED,        "SetFailCount", NULL},
  {MQIAMO_STATS,              "StatCount", NULL},
  {MQIAMO_STATS_FAILED,       "StatFailCount", NULL},
  {MQIAMO_SUB_DUR_HIGHWATER,  "DurableSubscriptionHighWater", NULL},
  {MQIAMO_SUB_DUR_LOWWATER,   "DurableSubscriptionLowWater", NULL},
  {MQIAMO_SUB_NDUR_HIGHWATER, "NonDurableSubscriptionHighWater", NULL},
  {MQIAMO_SUB_NDUR_LOWWATER,  "NonDurableSubscriptionLowWater", NULL},
  {MQIAMO_SUBRQS,             "SubRqCount", NULL},
  {MQIAMO_SUBRQS_FAILED,      "SubRqFailCount", NULL},
  {MQIAMO_SUBS_DUR,           "DurableSubscribeCount", NULL},
  {MQIAMO_SUBS_FAILED,        "SubscribeFailCount", NULL},
  {MQIAMO_SUBS_NDUR,          "NonDurableSubscribeCount", NULL},
  {MQIAMO_TOPIC_PUTS,         "PutTopicCount", NULL},
  {MQIAMO_TOPIC_PUTS_FAILED,  "PutTopicFailCount", NULL},
  {MQIAMO_TOPIC_PUT1S,        "Put1TopicCount", NULL},
  {MQIAMO_TOPIC_PUT1S_FAILED, "Put1TopicFailCount", NULL},
  {MQIAMO_UNSUBS_DUR,         "DurableUnsubscribeCount", NULL},
  {MQIAMO_UNSUBS_FAILED,      "UnsubscribeFailCount", NULL},
  {MQIAMO_UNSUBS_NDUR,        "NonDurableUnsubscribeCount", NULL}
};

struct MonDefinitions MonitoringInteger64Fields[] =
{
  {MQIAMO64_BROWSE_BYTES,     "BrowseBytes", NULL},
  {MQIAMO64_BYTES,            "TotalBytes", NULL},
  {MQIAMO64_GET_BYTES,        "GetBytes", NULL},
  {MQIAMO64_PUT_BYTES,        "PutBytes", NULL},
  {MQIAMO64_PUBLISH_MSG_BYTES,"PublishMsgBytes", NULL},
  {MQIAMO64_Q_TIME_AVG,       "TimeOnQAvg", NULL},
  {MQIAMO64_Q_TIME_MAX,       "TimeOnQMax", NULL},
  {MQIAMO64_Q_TIME_MIN,       "TimeOnQMin", NULL},
  {MQIAMO64_AVG_Q_TIME,       "AverageQueueTime", NULL},
  {MQIAMO64_TOPIC_PUT_BYTES,  "PutTopicBytes", NULL}
};

struct MonDefinitions MonitoringStringFields[] =
{
  {MQCA_REMOTE_Q_MGR_NAME,    "RemoteQMgr", NULL},
  {MQCACF_APPL_NAME,          "ApplicationName", NULL},
  {MQCACF_USER_IDENTIFIER,    "UserId", NULL},
  {MQCACH_CHANNEL_NAME,       "ChannelName", NULL},
  {MQCACH_CONNECTION_NAME,    "ConnName", NULL},
  {MQCAMO_CLOSE_DATE,         "CloseDate", NULL},
  {MQCAMO_CLOSE_TIME,         "CloseTime", NULL},
  {MQCAMO_CONN_DATE,          "ConnDate", NULL},
  {MQCAMO_CONN_TIME,          "ConnTime", NULL},
  {MQCAMO_DISC_DATE,          "DiscDate", NULL},
  {MQCAMO_DISC_TIME,          "DiscTime", NULL},
  {MQCAMO_END_DATE,           "IntervalEndDate", NULL},
  {MQCAMO_END_TIME,           "IntervalEndTime", NULL},
  {MQCAMO_OPEN_DATE,          "OpenDate", NULL},
  {MQCAMO_OPEN_TIME,          "OpenTime", NULL},
  {MQCAMO_START_DATE,         "IntervalStartDate", NULL},
  {MQCAMO_START_TIME,         "IntervalStartTime", NULL},
  {MQCA_CREATION_DATE,        "CreateDate", NULL},
  {MQCA_CREATION_TIME,        "CreateTime", NULL},
  {MQCA_Q_MGR_NAME,           "QueueManager", NULL},
  {MQCA_Q_NAME,               "QueueName", NULL}
};

struct MonDefinitions MonitoringGroupFields[] =
{
  {MQGACF_Q_ACCOUNTING_DATA,  "QueueAccounting"},
  {MQGACF_Q_STATISTICS_DATA,  "QueueStatistics"},
  {MQGACF_CHL_STATISTICS_DATA,"ChannelStatistics"}
};

#define printMonInteger(_indent, _parm, _value)                     \
{                                                                   \
  MQLONG i;                                                         \
  MQLONG tagId=-1;                                                  \
  struct ConstDefinitions *pDef;                                    \
  for (i=0; (i < (sizeof(MonitoringIntegerFields) /                 \
       sizeof(struct MonDefinitions))) && (tagId == -1); i++)       \
  {                                                                 \
    if (MonitoringIntegerFields[i].Parameter==(_parm))              \
    {                                                               \
      tagId=i;                                                      \
    }                                                               \
  }                                                                 \
  if (tagId != -1)                                                  \
  {                                                                 \
    if (MonitoringIntegerFields[tagId].NamedValues == NULL)         \
    {                                                               \
      printf("%s%s: %d\n",                                          \
             _indent,                                               \
             MonitoringIntegerFields[tagId].Identifier,             \
             _value);                                               \
    }                                                               \
    else                                                            \
    {                                                               \
      pDef=MonitoringIntegerFields[tagId].NamedValues;              \
      for (;(pDef->Value != _value) && (pDef->Value != -1); pDef++) \
        ;                                                           \
      if (pDef->Value == _value)                                    \
      {                                                             \
        printf("%s%s: %s\n",                                        \
               _indent,                                             \
               MonitoringIntegerFields[tagId].Identifier,           \
               pDef->Identifier);                                   \
      }                                                             \
      else                                                          \
      {                                                             \
        printf("%s%s: %s\n",                                        \
               _indent,                                             \
               MonitoringIntegerFields[tagId].Identifier,           \
               _value);                                             \
      }                                                             \
    }                                                               \
  }                                                                 \
  else                                                              \
  {                                                                 \
    printf("%s%d: %d\n",                                            \
           _indent,                                                 \
           _parm,                                                   \
           _value);                                                 \
  }                                                                 \
}

#define printMonIntegerList(_indent, _parm, _count, _values)        \
{                                                                   \
  MQLONG i, j;                                                      \
  MQLONG tagId=-1;                                                  \
  for (i=0; (i < (sizeof(MonitoringIntegerFields) /                 \
       sizeof(struct MonDefinitions))) && (tagId == -1); i++)       \
  {                                                                 \
    if (MonitoringIntegerFields[i].Parameter==(_parm))              \
    {                                                               \
      tagId=i;                                                      \
    }                                                               \
  }                                                                 \
  if (tagId != -1)                                                  \
  {                                                                 \
    printf("%s%s: [",                                               \
           _indent,                                                 \
           MonitoringIntegerFields[tagId].Identifier);              \
  }                                                                 \
  else                                                              \
  {                                                                 \
    printf("%s%d: [",                                               \
           _indent,                                                 \
           _parm);                                                  \
  }                                                                 \
  for (j=0; j < (_count); j++)                                      \
  {                                                                 \
    if (j == ((_count) -1))                                         \
    {                                                               \
      printf("%d]\n", _values[j]);                                  \
    }                                                               \
    else                                                            \
    {                                                               \
      printf("%d, ", _values[j]);                                   \
    }                                                               \
  }                                                                 \
}

#define printMonInteger64(_indent, _parm, _value)                   \
{                                                                   \
  MQLONG i;                                                         \
  MQLONG tagId=-1;                                                  \
  for (i=0; (i < (sizeof(MonitoringInteger64Fields) /               \
       sizeof(struct MonDefinitions))) && (tagId == -1); i++)       \
  {                                                                 \
    if (MonitoringInteger64Fields[i].Parameter==(_parm))            \
    {                                                               \
      tagId=i;                                                      \
    }                                                               \
  }                                                                 \
  if (tagId != -1)                                                  \
  {                                                                 \
    printf("%s%s: %lld\n",                                           \
           _indent,                                                 \
           MonitoringInteger64Fields[tagId].Identifier,             \
           _value);                                                 \
  }                                                                 \
  else                                                              \
  {                                                                 \
    printf("%s%d: %lld\n",                                           \
           _indent,                                                 \
           _parm,                                                   \
           _value);                                                 \
  }                                                                 \
}

#define printMonInteger64List(_indent, _parm, _count, _values)      \
{                                                                   \
  MQLONG i, j;                                                      \
  MQLONG tagId=-1;                                                  \
  for (i=0; (i < (sizeof(MonitoringInteger64Fields) /               \
       sizeof(struct MonDefinitions))) && (tagId == -1); i++)       \
  {                                                                 \
    if (MonitoringInteger64Fields[i].Parameter==(_parm))            \
    {                                                               \
      tagId=i;                                                      \
    }                                                               \
  }                                                                 \
  if (tagId != -1)                                                  \
  {                                                                 \
    printf("%s%s: [",                                               \
           _indent,                                                 \
           MonitoringInteger64Fields[tagId].Identifier);            \
  }                                                                 \
  else                                                              \
  {                                                                 \
    printf("%s%d: [",                                               \
           _indent,                                                 \
           _parm);                                                  \
  }                                                                 \
  for (j=0; j < (_count); j++)                                      \
  {                                                                 \
    if (j == ((_count) -1))                                         \
    {                                                               \
      printf("%lld]\n", _values[j]);                                \
    }                                                               \
    else                                                            \
    {                                                               \
      printf("%lld, ", _values[j]);                                 \
    }                                                               \
  }                                                                 \
}


#define printMonString(_indent, _parm, _len, _string)               \
{                                                                   \
  MQLONG i;                                                         \
  MQLONG tagId=-1;                                                  \
  MQLONG length=_len;                                               \
  for (i=0; (i < (sizeof(MonitoringStringFields) /                  \
       sizeof(struct MonDefinitions))) && (tagId == -1); i++)       \
  {                                                                 \
    if (MonitoringStringFields[i].Parameter==(_parm))               \
    {                                                               \
      tagId=i;                                                      \
    }                                                               \
  }                                                                 \
  for (length=_len-1;                                               \
       _string[length] == '\0' || _string[length] == ' ';           \
       length--);                                                   \
  if (tagId != -1)                                                  \
  {                                                                 \
    printf("%s%s: '%.*s'\n",                                        \
           _indent,                                                 \
           MonitoringStringFields[tagId].Identifier,                \
           length+1,                                                \
           _string);                                                \
  }                                                                 \
  else                                                              \
  {                                                                 \
    printf("%s%d: '%.*s'\n",                                        \
           _indent,                                                 \
           _parm,                                                   \
           length+1,                                                \
           _string);                                                \
  }                                                                 \
}

#define printMonStringList(_indent, _parm, _count, _len, _strings)  \
{                                                                   \
  MQLONG i, j;                                                      \
  MQLONG tagId=-1;                                                  \
  for (i=0; (i < (sizeof(MonitoringStringFields) /                  \
       sizeof(struct MonDefinitions))) && (tagId == -1); i++)       \
  {                                                                 \
    if (MonitoringStringFields[i].Parameter==(_parm))               \
    {                                                               \
      tagId=i;                                                      \
    }                                                               \
  }                                                                 \
  if (tagId != -1)                                                  \
  {                                                                 \
    printf("%s%s: [",                                               \
           _indent,                                                 \
           MonitoringStringFields[tagId].Identifier);               \
  }                                                                 \
  else                                                              \
  {                                                                 \
    printf("%s%d: [",                                               \
           _indent,                                                 \
           _parm);                                                  \
  }                                                                 \
  for (j=0; j < (_count); j++)                                      \
  {                                                                 \
    if (j == ((_count) -1))                                         \
    {                                                               \
      printf("'%.*s']\n", _len, &(_strings[j * (_len)]));           \
    }                                                               \
    else                                                            \
    {                                                               \
      printf("'%.*s', ", _len, &(_strings[j * (_len)]));            \
    }                                                               \
  }                                                                 \
  printf("]\n");                                                    \
}

#define printMonByteString(_indent, _parm, _len, _array)            \
{                                                                   \
  MQLONG i, j;                                                      \
  MQLONG tagId=-1;                                                  \
  for (i=0; (i < (sizeof(MonitoringByteStringFields) /              \
       sizeof(struct MonDefinitions))) && (tagId == -1); i++)       \
  {                                                                 \
    if (MonitoringByteStringFields[i].Parameter==(_parm))           \
    {                                                               \
      tagId=i;                                                      \
    }                                                               \
  }                                                                 \
  if (tagId != -1)                                                  \
  {                                                                 \
    printf("%s%s: x'",                                              \
           _indent,                                                 \
           MonitoringByteStringFields[tagId].Identifier);           \
  }                                                                 \
  else                                                              \
  {                                                                 \
    printf("%s%d: x'",                                              \
           _indent,                                                 \
           _parm);                                                  \
  }                                                                 \
  for (j=0; j < _len; j++)                                          \
  {                                                                 \
    printf("%02x", (unsigned char)_array[j]);                       \
  }                                                                 \
  printf("'\n");                                                    \
}

#define printMonGroup(_indent, _parm, _count)                       \
{                                                                   \
  MQLONG i;                                                         \
  MQLONG tagId=-1;                                                  \
  for (i=0; (i < (sizeof(MonitoringIntegerFields) /                 \
       sizeof(struct MonDefinitions))) && (tagId == -1); i++)       \
  {                                                                 \
    if (MonitoringGroupFields[i].Parameter==(_parm))                \
    {                                                               \
      tagId=i;                                                      \
    }                                                               \
  }                                                                 \
  if (tagId != -1)                                                  \
  {                                                                 \
    printf("%s%s: %d\n",                                            \
           _indent,                                                 \
           MonitoringGroupFields[tagId].Identifier,                 \
           _count);                                                 \
  }                                                                 \
  else                                                              \
  {                                                                 \
    printf("%sGROUP(%d): %d\n",                                     \
           _indent,                                                 \
           _parm,                                                   \
           _count);                                                 \
  }                                                                 \
}

/********************************************************************/
/*                                                                  */
/* Function: printMonitoringRecord                                  */
/*                                                                  */
/*                                                                  */
/*   This function prints out the contents of a message buffer      */
/*   containing an MQI accounting record.                           */
/*                                                                  */
/********************************************************************/
int printMonitoringRecord(MQLONG parameterCount,
                          MQLONG indentCount,
                          MQBYTE **ppbuffer,
                          MQLONG *pbuflen,
                          MQLONG filterObject,
                          MQCHAR filterName[MQ_OBJECT_NAME_LENGTH],
                          MQLONG *fieldList)
{
  int frc = 0;                        /* Function return code       */
  int rc = 0;                         /* Internal return code       */
  int groupCount=0;                   /* Group counter              */
  char indent[21];                    /* indentString               */
  MQBYTE *ptr;                        /* Current buffer pointer     */
  MQLONG bytesLeft;                   /* Number of remaining bytes  */
  MQLONG count;                       /* Parameter counter          */
  MQLONG Type;                        /* Type of next structure     */
  MQLONG Length;                      /* Length of next structure   */
  PMQCFIN pcfin;                      /* Integer parameter          */
  PMQCFIN64 pcfin64;                  /* Integer 64 parameter       */
  PMQCFIL pcfil;                      /* Integer list parameter     */
  PMQCFIL64 pcfil64;                  /* Integer 64 list parameter  */
  PMQCFST pcfst;                      /* String parameter           */
  PMQCFSL pcfsl;                      /* String list parameter      */
  PMQCFBS pcfbs;                      /* Byte String parameter      */
  PMQCFGR pcfgr;                      /* Group parameter            */
  MQINT64 value64;                    /* Aligned 64 bit value       */
  MQINT64 *values64;                  /* Aligned 64 bit value list  */

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
  for (count=0; (frc == 0) && (count < parameterCount); count++)
  {
    /****************************************************************/
    /* The first 4 bytes of every PCF structure contain its type so */
    /* check that this looks valid                                  */
    /****************************************************************/
    if (bytesLeft < (sizeof(MQLONG) + sizeof(MQLONG)))
    {
      fprintf(stderr, "Premature end of buffer before processing complete.\n");
      frc=-1;
      break;
    }
    Type=*((MQLONG *)ptr);
    Length=*((MQLONG *)(ptr+sizeof(MQLONG)));

    if (bytesLeft < Length)
    {
      fprintf(stderr, "Premature end of buffer before processing complete.\n");
      frc=-1;
      break;
    }

    switch (Type)
    {
      case MQCFT_INTEGER:
        pcfin=(PMQCFIN)ptr;
        if (checkFieldInList(pcfin->Parameter, fieldList))
        {
          printMonInteger(indent,
                          pcfin->Parameter,
                          pcfin->Value);
        }
        bytesLeft-=Length;
        ptr+=Length;
        break;
      case MQCFT_INTEGER64:
        pcfin64=(PMQCFIN64)ptr;
        if (checkFieldInList(pcfin64->Parameter, fieldList))
        {
          memcpy(&value64, &(pcfin64->Value), sizeof(MQINT64));
          printMonInteger64(indent,
                            pcfin64->Parameter,
                            value64);
        }
        bytesLeft-=Length;
        ptr+=Length;
        break;
      case MQCFT_STRING:
        pcfst=(PMQCFST)ptr;
        if (checkFieldInList(pcfst->Parameter, fieldList))
        {
          printMonString(indent,
                         pcfst->Parameter,
                         pcfst->StringLength,
                         pcfst->String);
        }
        bytesLeft-=Length;
        ptr+=Length;
        break;
      case MQCFT_INTEGER_LIST:
        pcfil=(PMQCFIL)ptr;
        if (checkFieldInList(pcfil->Parameter, fieldList))
        {
          printMonIntegerList(indent,
                              pcfil->Parameter,
                              pcfil->Count,
                              pcfil->Values);
        }
        bytesLeft-=Length;
        ptr+=Length;
        break;
      case MQCFT_INTEGER64_LIST:
        pcfil64=(PMQCFIL64)ptr;
        if (checkFieldInList(pcfil64->Parameter, fieldList))
        {
          values64=(MQINT64 *)malloc(sizeof(MQINT64) * pcfil64->Count);
          if (values64 == NULL)
          {
            fprintf(stderr, "Unable to allocate %d bytes for MQCFIN64 list\n",
                    sizeof(MQINT64) * pcfil64->Count);
            frc=-1;
          }
          else
          {
            memcpy(values64, pcfil64->Values, sizeof(MQINT64) * pcfil64->Count);

            printMonInteger64List(indent,
                                  pcfil64->Parameter,
                                  pcfil64->Count,
                                  values64);

            free(values64);
          }
        }
        bytesLeft-=Length;
        ptr+=Length;
        break;
      case MQCFT_STRING_LIST:
        pcfsl=(PMQCFSL)ptr;
        if (checkFieldInList(pcfsl->Parameter, fieldList))
        {
          printMonStringList(indent,
                             pcfsl->Parameter,
                             pcfsl->Count,
                             pcfsl->StringLength,
                             pcfsl->Strings);
        }
        bytesLeft-=Length;
        ptr+=Length;
        break;
      case MQCFT_BYTE_STRING:
        pcfbs=(PMQCFBS)ptr;
        if (checkFieldInList(pcfbs->Parameter, fieldList))
        {
          printMonByteString(indent,
                             pcfbs->Parameter,
                             pcfbs->StringLength,
                             pcfbs->String);
        }
        bytesLeft-=Length;
        ptr+=Length;
        break;
      case MQCFT_GROUP:
        /*
         * If we are filtering only certain records establish if
         * we are to print this record.
         */
        rc = 0;
        pcfgr=(PMQCFGR)ptr;
        if ((filterObject == FILTER_QUEUE)  && (filterName[0] != '\0'))
        {
          rc=checkPCFStringInMsg(ptr + sizeof(MQCFGR),
                                 pcfgr->ParameterCount,
                                 MQCA_Q_NAME,
                                 MQ_Q_NAME_LENGTH,
                                 filterName);

        }
        else if ((filterObject == FILTER_CHANNEL) && (filterName[0] != '\0'))
        {
          rc=checkPCFStringInMsg(ptr + sizeof(MQCFGR),
                                 pcfgr->ParameterCount,
                                 MQCACH_CHANNEL_NAME,
                                 MQ_CHANNEL_NAME_LENGTH,
                                 filterName);
        }

        if (rc == 0)
        {
          pcfgr=(PMQCFGR)ptr;
          printMonGroup(indent, pcfgr->Parameter, groupCount);
          groupCount++;
          bytesLeft-=pcfgr->StrucLength;
          ptr+=pcfgr->StrucLength;
          printMonitoringRecord(pcfgr->ParameterCount,
                                indentCount+2,
                                &ptr,
                                &bytesLeft,
                                0,
                                NULL,
                                fieldList);
        }
        else
        {
          bytesLeft-=pcfgr->StrucLength;
          ptr+=pcfgr->StrucLength;
          skipMonitoringRecord(pcfgr->ParameterCount,
                               &ptr,
                               &bytesLeft);
        }
        break;
      default:
        fprintf(stderr, "Error processing unknown PCF structure (%d)\n", Type);
        break;
    }
  }

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
  int frc = 0;                        /* Function return code       */
  MQBYTE *ptr;                        /* Current buffer pointer     */
  MQLONG bytesLeft;                   /* Number of remaining bytes  */
  MQLONG count;                       /* Parameter counter          */
  MQLONG Type;                        /* Type of next structure     */
  MQLONG Length;                      /* Length of next structure   */
  PMQCFGR pcfgr;                      /* Group parameter            */

  bytesLeft=*pbuflen;
  ptr=*ppbuffer;
  for (count=0; (frc == 0) && (count < parameterCount); count++)
  {
    /****************************************************************/
    /* The first 4 bytes of every PCF structure contain its type so */
    /* check that this looks valid                                  */
    /****************************************************************/
    if (bytesLeft < (sizeof(MQLONG) + sizeof(MQLONG)))
    {
      fprintf(stderr, "Premature end of buffer before processing complete.\n");
      frc=-1;
      break;
    }
    Type=*((MQLONG *)ptr);
    Length=*((MQLONG *)(ptr+sizeof(MQLONG)));

    if (bytesLeft < Length)
    {
      fprintf(stderr, "Premature end of buffer before processing complete.\n");
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
        pcfgr=(MQCFGR *)ptr;
        ptr+=Length;
        bytesLeft-=Length;
        frc=skipMonitoringRecord(pcfgr->ParameterCount,
                                 &ptr,
                                 &bytesLeft);

        break;
      default:
        fprintf(stderr, "Error processing unknown PCF structure (%d)\n", Type);
        break;
    }
  }

  *ppbuffer=ptr;
  *pbuflen=bytesLeft;

  return  frc;
}

/********************************************************************/
/*                                                                  */
/* Function: checkPCFStringInMsg                                    */
/*                                                                  */
/*                                                                  */
/*   This function accepts a PCF message and a the details of a     */
/*   structure to search for within the message. If a matching      */
/*   structure is found then 0 (zero) is returned.                  */
/*                                                                  */
/*   We don't use strcmp for the comparison to allow us to match    */
/*   NULL terminated strings with blank padded strings.             */
/*                                                                  */
/********************************************************************/
int checkPCFStringInMsg(MQBYTE *buffer,
                        MQLONG parmCount,
                        MQLONG parameter,
                        MQLONG valueLength,
                        MQCHAR *value)
{
  int rc=1;                        /* Initialise rc with not found  */

  MQCFST   *pcfst;                 /* Pointer to string structure   */
  MQLONG   *pType;                 /* Pointer to structure type     */
  MQLONG   *pLength;               /* Pointer to structure length   */
  MQBYTE   *bufptr;                /* Temporary buffer pointer      */
  MQLONG   i, j;                   /* Loop counters                 */
  MQLONG   count;                  /* Loop counter                  */
  int      endLoop=FALSE;          /* Loop control variable         */
  int      endOfValue=FALSE;       /* Loop control variable         */
  int      endOfString=FALSE;      /* Loop control variable         */

  bufptr=buffer;

  for (count=0; (rc == 1) && (count < parmCount); count++)
  {
    pType=(MQLONG *)bufptr;
    pLength=(MQLONG *)(bufptr + sizeof(MQLONG));

    if (*pType == MQCFT_STRING)
    {
      pcfst=(MQCFST *)bufptr;
      if (pcfst->Parameter == parameter)
      {
        rc=0; /* Assume a match */
        endLoop=FALSE;
        i=0; j=0;
        do
        {
          if ((value[i] != pcfst->String[j]) &&
              !((value[i] == ' ') && (pcfst->String[j] == '\0')) &&
              !((value[i] == '\0') && (pcfst->String[j] == ' ')))
          {
            /* invalid match */
            rc=1;
            endLoop=TRUE;
          }
          if ((i < valueLength) && (value[i] != '\0'))
          {
            i++;
          }
          else
          {
            endOfValue=TRUE;
          }
          if ((j < pcfst->StringLength) && (pcfst->String[j] != '\0'))
          {
            j++;
          }
          else
          {
            endOfString=TRUE;
          }
        } while (!endOfValue && ! endOfString && !endLoop);
      }
    }
    else if (*pType == MQCFT_GROUP)
    {
      MQCFGR *pcfgr=(MQCFGR *)bufptr;

      parmCount+=pcfgr->ParameterCount;
    }

    bufptr+=*pLength;
  }

  return rc;
}

/********************************************************************/
/*                                                                  */
/* Function: checkPCFByteStringInMsg                                */
/*                                                                  */
/*                                                                  */
/*   This function accepts a PCF message and a the details of a     */
/*   structure to search for within the message. If a matching      */
/*   structure is found then 0 (zero) is returned.                  */
/*                                                                  */
/********************************************************************/
int checkPCFByteStringInMsg(MQBYTE *buffer,
                            MQLONG parmCount,
                            MQLONG parameter,
                            MQLONG valueLength,
                            MQBYTE *value)
{
  int rc=1;                        /* Initialise rc with not found  */

  MQCFBS   *pcfbs;                 /* Pointer to string structure   */
  MQLONG   *pType;                 /* Pointer to structure type     */
  MQLONG   *pLength;               /* Pointer to structure length   */
  MQBYTE   *bufptr;                /* Temporary buffer pointer      */
  MQLONG   count;                  /* Loop counter                  */

  bufptr=buffer;

  for (count=0; (rc == 1) && (count < parmCount); count++)
  {
    pType=(MQLONG *)bufptr;
    pLength=(MQLONG *)(bufptr + sizeof(MQLONG));

    if (*pType == MQCFT_BYTE_STRING)
    {
      pcfbs=(MQCFBS *)bufptr;
      if ((pcfbs->Parameter == parameter) &&
          (pcfbs->StringLength ==  valueLength) &&
          (memcmp(pcfbs->String, value, valueLength) == 0))
      {
        rc=0;
      }
    }
    else if (*pType == MQCFT_GROUP)
    {
      MQCFGR *pcfgr=(MQCFGR *)bufptr;

      parmCount+=pcfgr->ParameterCount;
    }

    bufptr+=*pLength;
  }

  return rc;
}

/********************************************************************/
/*                                                                  */
/* Function: checkFieldInList                                       */
/*                                                                  */
/*                                                                  */
/*   This function scans a list of integers for a matching          */
/*   integer, returning TRUE is the matching integer is found.      */
/*                                                                  */
/********************************************************************/
int checkFieldInList(MQLONG value,
                     MQLONG *valueList)
{
  int rc=FALSE;                    /* Initialise rc with not found  */
  int count;                       /* Loop counter                  */

  if (valueList == NULL)
  {
    return TRUE;            /* If no list provided then return TRUE */
  }

  for (count=0; (rc == FALSE) && (count < MAX_FIELDLIST); count++)
  {
    if (valueList[count] == value)
    {
      rc=TRUE;
    }
  }

  return rc;
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
  MQCFST   *pcfst;                 /* Pointer to string structure   */
  MQCFGR   *pcfgr;                 /* Pointer to group structure    */
  MQLONG   *pType;                 /* Pointer to structure type     */
  MQLONG   *pLength;               /* Pointer to structure length   */
  MQBYTE   *bufptr;                /* Temporary buffer pointer      */
  int      count;                  /* Loop counter                  */
  int      dateFound=FALSE;        /* Date parameter found          */
  int      timeFound=FALSE;        /* Time parameter found          */

  bufptr=buffer;

  dateString[0]='\0';
  timeString[0]='\0';

  for (count=0; !(dateFound && timeFound) && (count < parmCount); count++)
  {
    pType=(MQLONG *)bufptr;
    pLength=(MQLONG *)(bufptr + sizeof(MQLONG));

    if (*pType == MQCFT_STRING)
    {
      pcfst=(MQCFST *)bufptr;
      if (!dateFound && (pcfst->Parameter == dateParameter))
      {
        strncpy(dateString, pcfst->String, MQ_DATE_LENGTH+1);
        dateString[MQ_DATE_LENGTH]='\0';
        dateFound=TRUE;
      }
      else if (!timeFound && (pcfst->Parameter == timeParameter))
      {
        strncpy(timeString, pcfst->String, MQ_TIME_LENGTH+1);
        timeString[MQ_TIME_LENGTH]='\0';
        timeFound=TRUE;
      }
    }
    else if (*pType == MQCFT_GROUP)
    {
      pcfgr=(MQCFGR *)bufptr;
      parmCount+=pcfgr->ParameterCount;
    }

    bufptr+=*pLength;
  }

  return (dateFound && timeFound)?0:1;
}

