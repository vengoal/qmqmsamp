/* %Z% %W% %I% %E% %U% */
/*********************************************************************/
/*                                                                   */
/* Program name: AMQSSBXA                                            */
/*                                                                   */
/* Description: Sample C program that subscribes and gets messages   */
/*              from a topic (example using MQSUB) allowing the use  */
/*              of extended options on the MQSUB call, over and      */
/*              above those available on the simpler MQSUB sample,   */
/*              AMQSSUBA.                                            */
/*                                                                   */
/*              In addition to the message payload, MQ publish /     */
/*              subscribe message properties for each message are    */
/*              received and displayed.                              */
/*                                                                   */
/*   <copyright                                                      */
/*   notice="lm-source-program"                                      */
/*   pids="5724-H72,"                                                */
/*   years="2008,2012"                                               */
/*   crc="3319335100" >                                              */
/*   Licensed Materials - Property of IBM                            */
/*                                                                   */
/*   5724-H72,                                                       */
/*                                                                   */
/*   (C) Copyright IBM Corp. 2008, 2012 All Rights Reserved.         */
/*                                                                   */
/*   US Government Users Restricted Rights - Use, duplication or     */
/*   disclosure restricted by GSA ADP Schedule Contract with         */
/*   IBM Corp.                                                       */
/*   </copyright>                                                    */
/*********************************************************************/
/*                                                                   */
/* Function:                                                         */
/*                                                                   */
/*                                                                   */
/*   AMQSSBXA is a sample C program to subscribe and get messages    */
/*   from a topic. It demonstrates the use of the MQSUB call with    */
/*   multiple options specified.                                     */
/*                                                                   */
/*   sample                                                          */
/*      -- subscribes to the topic string / topic object specified   */
/*         through the -t / -o parameters using either a managed     */
/*         destination or the destination queue specified with the   */
/*         -q parameter and other options as specified through       */
/*         additional parameters.                                    */
/*                                                                   */
/*      -- calls MQGET repeatedly with a message handle to get       */
/*         messages and message properties published and writes to   */
/*         stdout. Assumes that each message represents a line of    */
/*         text.                                                     */
/*                                                                   */
/*      -- writes a message for each MQI reason other than           */
/*         MQRC_NONE; stops if there is a MQI completion code        */
/*         of MQCC_FAILED                                            */
/*                                                                   */
/*   Program logic:                                                  */
/*      Parse Command Line Parameters                                */
/*      Connect to Queue Manager                                     */
/*      Open Destination Queue if specified                          */
/*      Subscribe to Topic Object / String specified                 */
/*      Create Message Handle                                        */
/*      while no MQI failures,                                       */
/*      .  MQGET next message using CORRELID, and message handle.    */
/*      .  print the result                                          */
/*      .  (no message available counts as failure, and loop ends)   */
/*      Delete Message Handle                                        */
/*      Close Subscription Handle as specified                       */
/*      Close Destination Queue Handle                               */
/*      Disconnect from Queue Manager                                */
/*                                                                   */
/*********************************************************************/
/*                                                                   */
/*   AMQSSBXA has the following parameters                           */
/*       required, at least one of:                                  */
/*            -t <string>  Topic string                              */
/*            -o <name>    Topic object name                         */
/*       optional:                                                   */
/*            -m <name>    Queue manager name                        */
/*            -b <type>    Connection binding type (def = standard)  */
/*                          standard     MQCNO_STANDARD_BINDING      */
/*                          shared       MQCNO_SHARED_BINDING        */
/*                          fastpath     MQCNO_FASTPATH_BINDING      */
/*                          isolated     MQCNO_ISOLATED_BINDING      */
/*            -q <name>    Destination queue name                    */
/*            -w <seconds> Wait interval on MQGET (def = 30)         */
/*                          unlimited     MQWI_UNLIMITED             */
/*                          none          no wait                    */
/*                          n             wait interval in seconds   */
/*            -d <subname> Create/resume named durable subscription  */
/*            -k           Keep durable subscription on MQCLOSE      */
/*                                                                   */
/*********************************************************************/
/*                                                                   */
/*   Example invocations:                                            */
/*                                                                   */
/*   # Create an unnamed non-durable subscription to /news on QM1    */
/*                                                                   */
/*     amqssbx -m QM1 -t /news                                       */
/*                                                                   */
/*   # Create or resume a durable subscription named MySub           */
/*   # to /sport/results on QM1                                      */
/*                                                                   */
/*     amqssbx -m QM1 -d MySub -t /sport/results                     */
/*                                                                   */
/*   # Create or resume a durable subscription named LotterySub to   */
/*   # lottery.results and end immediately when no more messages     */
/*   # exist. Do not remove subscripton when program ends.           */
/*                                                                   */
/*     amqssbx -m QM1 -d LotterySub -t lottery.results -w none -k    */
/*                                                                   */
/*********************************************************************/

/*********************************************************************/
/* Header files                                                      */
/*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <cmqc.h>            /* MQI structure / function prototypes  */

#define PARM_FLAG_TOPIC_STRING             't'
#define PARM_FLAG_TOPIC_OBJECT             'o'
#define PARM_FLAG_Q_MGR_NAME               'm'
#define PARM_FLAG_BINDING_TYPE             'b'
#define PARM_FLAG_DESTINATION_Q_NAME       'q'
#define PARM_FLAG_WAIT_INTERVAL            'w'
#define PARM_FLAG_DURABLE_SUB_NAME         'd'
#define PARM_FLAG_KEEP_DURABLE             'k'

#define PARM_VALUE_BINDING_STANDARD        "standard"
#define PARM_VALUE_BINDING_SHARED          "shared"
#define PARM_VALUE_BINDING_FASTPATH        "fastpath"
#define PARM_VALUE_BINDING_ISOLATED        "isolated"
#define PARM_VALUE_WAIT_INTERVAL_UNLIMITED "unlimited"
#define PARM_VALUE_WAIT_INTERVAL_NONE      "none"

#define DEFAULT_WAIT_INTERVAL              30000      /* 30 seconds  */

#define DEFAULT_BUFFER_SIZE                4096       /* 4 Kilobytes */
#define DEFAULT_PROPERTY_NAME_BUFFER_SIZE  256        /* 256 bytes   */
#define DEFAULT_PROPERTY_VALUE_BUFFER_SIZE 4096       /* 4 Kilobytes */

#if MQAT_DEFAULT == MQAT_WINDOWS_NT /* printf 64-bit integer type */
  #define  Int64 "I64"
#elif defined(MQ_64_BIT)
  #define  Int64 "l"
#else
  #define  Int64 "ll"
#endif

/*********************************************************************/
/*                                                                   */
/*   Global Variables                                                */
/*                                                                   */
/*********************************************************************/
static int Parm_Index = 1;

/*********************************************************************/
/*                                                                   */
/*   Function Prototypes                                             */
/*                                                                   */
/*********************************************************************/
void   printUsage(void);
MQLONG printProperties(MQHCONN hconn,
                       MQHMSG  hmsg,
                       char *filter);
int    getParm(int argc,
               char **argv,
               char **pFlag,
               char **pValue);

/*********************************************************************/
/* Function name:    main                                            */
/*                                                                   */
/* Description:      Main program entry point                        */
/*********************************************************************/
int main(int argc, char **argv)
{
  MQLONG    rc = MQRC_NONE;              /* Application return code  */

  char     *pFlag = NULL;                /* Input parameter flag     */
  char     *pValue = NULL;               /* Input parameter value    */
  int       DisplayUsage = 0;            /* Display usage message    */

  char     *ParmTopicString = NULL;      /* -t value pointer         */
  char     *ParmTopicObjectName = NULL;  /* -o value pointer         */
  char     *ParmQMgrName = NULL;         /* -m value pointer         */
  char     *ParmBindingType = NULL;      /* -b value pointer         */
  char     *ParmDestinationQName = NULL; /* -q value pointer         */
  char     *ParmWaitInterval = NULL;     /* -w value pointer         */
  char     *ParmDurableSubName = NULL;   /* -d value pointer         */
  char     *ParmKeepDurable = NULL;      /* -k flag pointer          */

  MQCHAR48  qmgrname = "";               /* Queue manager name       */

  MQLONG    compcode = MQCC_OK;          /* MQI Completion code      */
  MQLONG    reason = MQRC_NONE;          /* MQI Reason code          */

  MQCNO     cno = {MQCNO_DEFAULT};       /* Connect Options          */
  MQHCONN   hconn = MQHC_UNUSABLE_HCONN; /* Connection Handle        */
  MQLONG    creason = MQRC_NONE;         /* MQCONN MQI Reason code   */

  MQOD      od = {MQOD_DEFAULT};         /* Destination Descriptor   */
  MQLONG    oo = MQOO_INPUT_AS_Q_DEF;    /* Destination Open Opts    */
  MQHOBJ    hobj = MQHO_NONE;            /* Destination Handle       */

  MQCMHO    cmho = { MQCMHO_DEFAULT };   /* Msg Handle Create Opts   */
  MQDMHO    dmho = { MQDMHO_DEFAULT };   /* Msg Handle Delete Opts   */
  MQHMSG    hmsg = MQHM_UNUSABLE_HMSG;   /* Msg Handle               */

  MQSD      sd = {MQSD_DEFAULT};         /* Subscription Descriptor  */
  MQHOBJ    hsub = MQHO_NONE;            /* Subscription Handle      */

  MQGMO     gmo = {MQGMO_DEFAULT};       /* Get Message Options      */
  MQMD      md = {MQMD_DEFAULT};         /* Message Descriptor       */
  PMQBYTE   buffer = NULL;               /* Message Buffer           */
  MQLONG    buffersize = 0;              /* Current Buffer Size      */
  MQLONG    datalength = 0;              /* Message Data Length      */

  MQLONG    co = MQCO_NONE;              /* Close Options            */

  /*******************************************************************/
  /*                                                                 */
  /*   START OF AMQSSBXA                                             */
  /*                                                                 */
  /*******************************************************************/

  printf("Sample AMQSSBXA start\n\n");

  /*******************************************************************/
  /*                                                                 */
  /*   Parse Command Line Parameters                                 */
  /*                                                                 */
  /*******************************************************************/

  while (getParm(argc,argv,&pFlag,&pValue))
  {
    if (pFlag)
    {
      switch(*pFlag)
      {
        /*************************************************************/
        /* Queue manager name                                        */
        /*************************************************************/
        case PARM_FLAG_Q_MGR_NAME:
          if (ParmQMgrName || !pValue)
            DisplayUsage = 1;
          else
            ParmQMgrName = pValue;
          break;

        /*************************************************************/
        /* Binding type                                              */
        /*************************************************************/
        case PARM_FLAG_BINDING_TYPE:
          if (ParmBindingType || !pValue)
            DisplayUsage = 1;
          else
          {
            if (!strcmp(pValue, PARM_VALUE_BINDING_STANDARD) ||
                !strcmp(pValue, PARM_VALUE_BINDING_SHARED) ||
                !strcmp(pValue, PARM_VALUE_BINDING_FASTPATH) ||
                !strcmp(pValue, PARM_VALUE_BINDING_ISOLATED))
              ParmBindingType = pValue;
            else
              DisplayUsage = 1;
          }
          break;

        /*************************************************************/
        /* Topic string                                              */
        /*************************************************************/
        case PARM_FLAG_TOPIC_STRING:
          if (ParmTopicString || !pValue)
            DisplayUsage = 1;
          else
            ParmTopicString = pValue;
          break;

        /*************************************************************/
        /* Topic object name                                         */
        /*************************************************************/
        case PARM_FLAG_TOPIC_OBJECT:
          if (ParmTopicObjectName || !pValue)
            DisplayUsage = 1;
          else
            ParmTopicObjectName = pValue;
          break;

        /*************************************************************/
        /* Destination queue name                                    */
        /*************************************************************/
        case PARM_FLAG_DESTINATION_Q_NAME:
          if (ParmDestinationQName || !pValue)
            DisplayUsage = 1;
          else
            ParmDestinationQName = pValue;
          break;

        /*************************************************************/
        /* Wait Interval                                             */
        /*************************************************************/
        case PARM_FLAG_WAIT_INTERVAL:
          if (ParmWaitInterval || !pValue)
            DisplayUsage = 1;
          else
            ParmWaitInterval = pValue;
          break;

        /*************************************************************/
        /* Durable Subscription Name                                 */
        /*************************************************************/
        case PARM_FLAG_DURABLE_SUB_NAME:
          if (ParmDurableSubName || !pValue)
            DisplayUsage = 1;
          else
            ParmDurableSubName = pValue;
          break;

        /*************************************************************/
        /* Keep durable subscription on MQCLOSE                      */
        /*************************************************************/
        case PARM_FLAG_KEEP_DURABLE:
          if (ParmKeepDurable || pValue)
            DisplayUsage = 1;
          else
            ParmKeepDurable = pFlag;
          break;

        /*************************************************************/
        /* Unexpected flag                                           */
        /*************************************************************/
        default:
          DisplayUsage = 1;
          break;
      }
    }
    else
    {
      /***************************************************************/
      /* No flag given display usage                                 */
      /***************************************************************/
      DisplayUsage = 1;
    }

    /*****************************************************************/
    /* Usage needs to be displayed, break out of the while loop      */
    /*****************************************************************/
    if (DisplayUsage)
      break;
  }

  /*******************************************************************/
  /* Must have either a topic object name, topic string or both      */
  /*******************************************************************/
  if (!ParmTopicObjectName && !ParmTopicString)
    DisplayUsage = 1;

  /*******************************************************************/
  /* If there was a problem with the parameter parsing, display the  */
  /* usage statement and exit.                                       */
  /*******************************************************************/
  if (DisplayUsage)
  {
    printUsage();
    goto MOD_EXIT;
  }

  /*******************************************************************/
  /*                                                                 */
  /*   Connect to Queue manager                                      */
  /*                                                                 */
  /*******************************************************************/

  if (ParmQMgrName)
    strncpy(qmgrname, ParmQMgrName, MQ_Q_MGR_NAME_LENGTH);

  if (ParmBindingType)
  {
    if (!strcmp(ParmBindingType, PARM_VALUE_BINDING_STANDARD))
      cno.Options |= MQCNO_STANDARD_BINDING;
    else if (!strcmp(ParmBindingType, PARM_VALUE_BINDING_SHARED))
      cno.Options |= MQCNO_SHARED_BINDING;
    else if (!strcmp(ParmBindingType, PARM_VALUE_BINDING_FASTPATH))
      cno.Options |= MQCNO_FASTPATH_BINDING;
    else if (!strcmp(ParmBindingType, PARM_VALUE_BINDING_ISOLATED))
      cno.Options |= MQCNO_ISOLATED_BINDING;
  }

  MQCONNX(qmgrname,                 /* queue manager name            */
          &cno,                     /* connect options               */
          &hconn,                   /* connection handle (output)    */
          &compcode,                /* completion code               */
          &creason);                /* reason code                   */

  if (creason != MQRC_NONE)
    printf("MQCONNX ended with reason code %d\n", creason);

  if (compcode == MQCC_FAILED)
  {
    rc = creason;
    printf("Unable to connect to queue manager\n");
    goto MOD_EXIT;
  }

  /*******************************************************************/
  /*                                                                 */
  /*   Destination queue specified, open it for INPUT                */
  /*                                                                 */
  /*******************************************************************/

  if (ParmDestinationQName)
  {
    strncpy(od.ObjectName, ParmDestinationQName, MQ_Q_NAME_LENGTH);

    oo = MQOO_INPUT_AS_Q_DEF | MQOO_FAIL_IF_QUIESCING;

    MQOPEN(hconn,                   /* connection handle             */
           &od,                     /* object descriptor             */
           oo,                      /* open options                  */
           &hobj,                   /* object handle (output)        */
           &compcode,               /* completion code               */
           &reason);                /* reason code                   */

    if (reason != MQRC_NONE)
      printf("MQOPEN ended with reason code %d\n", reason);

    if (compcode == MQCC_FAILED)
    {
      rc = reason;
      printf("Unable to open provided destination queue for input\n");
      goto MOD_EXIT;
    }
  }

  /*******************************************************************/
  /*                                                                 */
  /*   Subscribe to Topic Object / String specified                  */
  /*                                                                 */
  /*******************************************************************/

  if (ParmTopicObjectName)
    strncpy(sd.ObjectName, ParmTopicObjectName, MQ_OBJECT_NAME_LENGTH);

  if (ParmTopicString)
  {
    sd.ObjectString.VSCCSID = MQCCSI_APPL;
    sd.ObjectString.VSLength = (MQLONG)strlen(ParmTopicString);
    sd.ObjectString.VSPtr = ParmTopicString;
  }

  sd.Options = MQSO_FAIL_IF_QUIESCING |  /* Fail if QMgr quiescing   */
               MQSO_CREATE;              /* Allow creation           */

  if (ParmDurableSubName)
  {
    sd.SubName.VSCCSID = MQCCSI_APPL;
    sd.SubName.VSLength = (MQLONG)strlen(ParmDurableSubName);
    sd.SubName.VSPtr = ParmDurableSubName;

    sd.Options |= MQSO_DURABLE |         /* Durable subscription     */
                  MQSO_RESUME;           /* Resume if already exists */
  }

  if (!ParmDestinationQName)
    sd.Options |= MQSO_MANAGED;           /* Managed subscription    */

  MQSUB(hconn,                      /* connection handle             */
        &sd,                        /* subscription descriptor       */
        &hobj,                      /* object handle (input/output)  */
        &hsub,                      /* subscription handle (output)  */
        &compcode,                  /* completion code               */
        &reason);                   /* reason code                   */

  if (reason != MQRC_NONE)
    printf("MQSUB ended with reason code %d\n", reason);

  if (compcode == MQCC_FAILED)
  {
    rc = reason;
    printf("Unable to subscribe\n");
    goto MOD_EXIT;
  }

  /*******************************************************************/
  /*                                                                 */
  /*   Create Message Handle                                         */
  /*                                                                 */
  /*******************************************************************/

  MQCRTMH(hconn,                    /* connection handle             */
          &cmho,                    /* create message handle options */
          &hmsg,                    /* message handle                */
          &compcode,                /* completion code               */
          &reason);                 /* reason code                   */

  if (reason != MQRC_NONE)
    printf("MQCRTMH ended with reason code %d\n", reason);

  if (compcode == MQCC_FAILED)
  {
    rc = reason;
    printf("Unable to create message handle\n");
    goto MOD_EXIT;
  }

  /*******************************************************************/
  /*                                                                 */
  /*   Get messages from the Destination Queue                       */
  /*                                                                 */
  /*******************************************************************/

  gmo.Version = MQGMO_VERSION_4;          /* Required for Msg Handle */

  gmo.MsgHandle = hmsg;

  gmo.Options = MQGMO_FAIL_IF_QUIESCING    | /* QMgr quiescing       */
                MQGMO_PROPERTIES_IN_HANDLE | /* Props in Msg Handle  */
                MQGMO_NO_SYNCPOINT         | /* No Transaction       */
                MQGMO_CONVERT;               /* Convert Msg Data     */

  if (ParmWaitInterval)
  {
    if (!strcmp(ParmWaitInterval, PARM_VALUE_WAIT_INTERVAL_UNLIMITED))
    {
      gmo.Options |= MQGMO_WAIT;
      gmo.WaitInterval = MQWI_UNLIMITED;
    }
    else if (!strcmp(ParmWaitInterval, PARM_VALUE_WAIT_INTERVAL_NONE))
    {
      gmo.Options |= MQGMO_NO_WAIT;
    }
    else
    {
      gmo.Options |= MQGMO_WAIT;
      gmo.WaitInterval = ((MQLONG)strtol(ParmWaitInterval, NULL, 0)) * 1000;
    }
  }
  else
  {
    gmo.Options |= MQGMO_WAIT;
    gmo.WaitInterval = DEFAULT_WAIT_INTERVAL;
  }

  /*******************************************************************/
  /* Only get messages which were delivered to this suscription by   */
  /* specifying MQMO_MATCH_CORREL_ID with the correlation id from    */
  /* the subscription.                                               */
  /*                                                                 */
  /* This is not needed when we are using a managed destination or   */
  /* we know that no other messages will be on the queue but it      */
  /* doesn't do any harm to specify it anyway.                       */
  /*******************************************************************/
  gmo.MatchOptions = MQMO_MATCH_CORREL_ID;
  memcpy(md.CorrelId, sd.SubCorrelId, MQ_CORREL_ID_LENGTH);

  /*******************************************************************/
  /* Allocate buffer space, re-allocated later as required.          */
  /*******************************************************************/
  buffersize = DEFAULT_BUFFER_SIZE;

  buffer = malloc(buffersize);

  if (buffer == NULL)
  {
    rc = MQRC_STORAGE_NOT_AVAILABLE;
    printf("Unable to allocate message buffer\n");
    goto MOD_EXIT;
  }

  while (compcode != MQCC_FAILED)
  {
    /*****************************************************************/
    /* MQGET sets Encoding and CodedCharSetId to the values in the   */
    /* message returned, so these fields should be reset to their    */
    /* default values before each call to ensure that MQGMO_CONVERT  */
    /* works correctly.                                              */
    /*****************************************************************/
    md.Encoding       = MQENC_NATIVE;
    md.CodedCharSetId = MQCCSI_Q_MGR;

    if (gmo.Options & MQGMO_WAIT)
    {
      if (gmo.WaitInterval == MQWI_UNLIMITED)
        printf("Calling MQGET : with unlimited wait\n");
      else
        printf("Calling MQGET : wait for %d seconds\n",
               gmo.WaitInterval / 1000);
    }

    MQGET(hconn,                    /* connection handle             */
          hobj,                     /* destination queue handle      */
          &md,                      /* message descriptor            */
          &gmo,                     /* get message options           */
          buffersize,               /* buffer size                   */
          buffer,                   /* message buffer                */
          &datalength,              /* message data length (output)  */
          &compcode,                /* completion code               */
          &reason);                 /* reason code                   */

    if (reason != MQRC_NONE)
    {
      switch(reason)
      {
        /*************************************************************/
        /* No more messages available, this is used to end the loop  */
        /*************************************************************/
        case MQRC_NO_MSG_AVAILABLE:
          printf("no more messages\n");
          break;

        /*************************************************************/
        /* Not enough buffer space, reallocate and try again         */
        /*************************************************************/
        case MQRC_TRUNCATED_MSG_FAILED:
        {
          PMQBYTE newbuffer = NULL;

          newbuffer = realloc(buffer, datalength);

          if (newbuffer == NULL)
          {
            rc = MQRC_STORAGE_NOT_AVAILABLE;
            printf("Unable to re-allocate message buffer\n");
            compcode = MQCC_FAILED;
          }
          else
          {
            buffer = newbuffer;
            buffersize = datalength;
          }
        }
        break;

        /*************************************************************/
        /* QMgr is ending, end processing                            */
        /*************************************************************/
        case MQRC_Q_MGR_STOPPING:
        case MQRC_Q_MGR_QUIESCING:
          rc = reason;
          break;

        /*************************************************************/
        /* Report any other reason                                   */
        /*************************************************************/
        default:
          if (compcode == MQCC_FAILED)
            rc = reason;

          printf("MQGET ended with reason code %d\n", reason);
          break;
      }
    }

    /*****************************************************************/
    /* Display each message received                                 */
    /*****************************************************************/
    if ((compcode != MQCC_FAILED) &&
        (reason != MQRC_TRUNCATED_MSG_FAILED))
    {
      /***************************************************************/
      /* Display MQ publish/subscribe message properties             */
      /***************************************************************/
      rc = printProperties(hconn, hmsg, "mqps.%");

      /***************************************************************/
      /* Display The message data                                    */
      /***************************************************************/
      printf("Message Data:\n  '%.*s'\n\n", datalength, buffer);

      if (rc != MQRC_NONE)
        goto MOD_EXIT;
    }
  }

MOD_EXIT:

  /*******************************************************************/
  /*                                                                 */
  /*   Free the Message Buffer                                       */
  /*                                                                 */
  /*******************************************************************/

  free(buffer);                               /* free(NULL) is valid */

  /*******************************************************************/
  /*                                                                 */
  /*   Delete Message Handle                                         */
  /*                                                                 */
  /*******************************************************************/

  if (hmsg != MQHM_UNUSABLE_HMSG)
  {
    MQDLTMH(hconn,                  /* connection handle             */
            &hmsg,                  /* message handle                */
            &dmho,                  /* delete message handle options */
            &compcode,              /* completion code               */
            &reason);               /* reason code                   */

    if (reason != MQRC_NONE)
      printf("MQDLTMH ended with reason code %d\n", reason);
  }

  /*******************************************************************/
  /*                                                                 */
  /*   If the QMgr is not ending, close any handles still open       */
  /*                                                                 */
  /*******************************************************************/

  if ((rc != MQRC_Q_MGR_STOPPING) && (rc != MQRC_Q_MGR_QUIESCING))
  {
    /*****************************************************************/
    /*                                                               */
    /*   Close Subscription Handle                                   */
    /*                                                               */
    /*****************************************************************/

    if ((hsub != MQHO_NONE) && (hsub != MQHO_UNUSABLE_HOBJ))
    {
      if (ParmKeepDurable)
        co = MQCO_KEEP_SUB;                /* Keep Subscription      */
      else
        co = MQCO_REMOVE_SUB;              /* Remove Subscription    */

      MQCLOSE(hconn,                /* connection handle             */
              &hsub,                /* subscription handle           */
              co,                   /* close options                 */
              &compcode,            /* completion code               */
              &reason);             /* reason code                   */

      if (reason != MQRC_NONE)
        printf("MQCLOSE of subscription ended with reason code %d\n",
               reason);
    }

    /*****************************************************************/
    /*                                                               */
    /*   Close Destination Queue Handle                              */
    /*                                                               */
    /*****************************************************************/

    if ((hobj != MQHO_NONE) && (hobj != MQHO_UNUSABLE_HOBJ))
    {
      co = MQCO_NONE;

      MQCLOSE(hconn,                /* connection handle             */
              &hobj,                /* destination queue handle      */
              co,                   /* close options                 */
              &compcode,            /* completion code               */
              &reason);             /* reason code                   */

      if (reason != MQRC_NONE)
        printf("MQCLOSE of destination queue ended with reason code %d\n",
               reason);
    }
  }

  /*******************************************************************/
  /*                                                                 */
  /*   Disconnect from Queue manager                                 */
  /*                                                                 */
  /*******************************************************************/

  if (hconn != MQHC_UNUSABLE_HCONN)
  {
    if (creason != MQRC_ALREADY_CONNECTED )
    {
      MQDISC(&hconn,                /* connection handle             */
             &compcode,             /* completion code               */
             &reason);              /* reason code                   */

      if (reason != MQRC_NONE)
        printf("MQDISC ended with reason code %d\n", reason);
    }
  }

  /*******************************************************************/
  /*                                                                 */
  /*   END OF AMQSSBXA                                               */
  /*                                                                 */
  /*******************************************************************/

  printf("\nSample AMQSSBXA end\n");

  return (int)rc;
}

/*********************************************************************/
/* Function name:    printUsage                                      */
/*                                                                   */
/* Description:      Prints the usage statement                      */
/*********************************************************************/
void printUsage(void)
{
  printf("Usage:\n");
  printf("  Required parameters, at least one of:\n");
  printf("    -t <string>    Topic String\n");
  printf("    -o <name>      Topic Object Name\n\n");
  printf("  Optional parameters:\n");
  printf("    -m <name>      Queue Manager Name\n");
  printf("    -b <type>      Connection Binding Type, one of:\n");
  printf("                     standard       MQCNO_STANDARD_BINDING\n");
  printf("                     shared         MQCNO_SHARED_BINDING\n");
  printf("                     fastpath       MQCNO_FASTPATH_BINDING\n");
  printf("                     isolated       MQCNO_ISOLATED_BINDING\n");
  printf("    -q <name>      Provided Destination Queue Name\n");
  printf("    -w <time>      Wait Interval, one of:\n");
  printf("                     unlimited      MQWI_UNLIMITED\n");
  printf("                     none           MQGMO_NO_WAIT\n");
  printf("                     <seconds>      Numeric wait interval\n");
  printf("    -d <string>    Durable Subscription Name\n");
  printf("    -k             Keep durable subscription on exit\n");

  return;
}

/*********************************************************************/
/* Function name:    printProperties                                 */
/*                                                                   */
/* Description:      Prints the name of each property that matches   */
/*                   the supplied name filter together with its      */
/*                   value in the appropriate format, viz:           */
/*                                                                   */
/*                    boolean values as TRUE or FALSE                */
/*                    byte string values as a series of hex digits   */
/*                    floating-point values as a number (%g)         */
/*                    integer values as a number (%d)                */
/*                    null values as NULL                            */
/*                    string values as characters (%s)               */
/*********************************************************************/
MQLONG printProperties(MQHCONN hconn, MQHMSG hmsg, char *filter)
{
  MQLONG  rc = MQRC_NONE;

  int     i;                              /* loop counter            */
  int     j;                              /* another loop counter    */
  MQIMPO  impo = {MQIMPO_DEFAULT};        /* inquire prop options    */
  MQLONG  namebuffersize;             /* returned name buffer length */
  PMQCHAR namebuffer = NULL;              /* returned name buffer    */
  MQCHARV inqname = {MQPROP_INQUIRE_ALL}; /* browse all properties   */
  MQPD    pd = {MQPD_DEFAULT};            /* property descriptor     */
  MQLONG  type;                           /* property type           */
  MQLONG  valuebuffersize;                /* value buffer size       */
  PMQBYTE valuebuffer = NULL;             /* value buffer            */
  MQLONG  valuelength;                    /* value length            */
  MQLONG  compcode = MQCC_OK;             /* MQINQMP completion code */
  MQLONG  reason = MQRC_NONE;             /* MQINQMP reason code     */

  /*******************************************************************/
  /* Initialise the property name with filter if one was specified   */
  /*******************************************************************/
  if (filter)
  {
    inqname.VSPtr = filter;
    inqname.VSLength = MQVS_NULL_TERMINATED;
  }

  /*******************************************************************/
  /* Initialize storage                                              */
  /*******************************************************************/
  valuebuffersize = DEFAULT_PROPERTY_VALUE_BUFFER_SIZE;
  valuebuffer = (PMQBYTE)malloc(valuebuffersize);

  if (valuebuffer == NULL)
  {
    rc = MQRC_STORAGE_NOT_AVAILABLE;
    printf("Unable to allocate property value buffer\n");
    goto MOD_EXIT;
  }

  namebuffersize = DEFAULT_PROPERTY_NAME_BUFFER_SIZE;
  namebuffer = (PMQCHAR)malloc(namebuffersize);

  if (namebuffer == NULL)
  {
    rc = MQRC_STORAGE_NOT_AVAILABLE;
    printf("Unable to allocate property name buffer\n");
    goto MOD_EXIT;
  }

  /*******************************************************************/
  /* Initialise the inquire prop options structur                    */
  /*******************************************************************/
  impo.Options |= MQIMPO_CONVERT_VALUE;
  impo.ReturnedName.VSPtr = namebuffer;
  impo.ReturnedName.VSBufSize = namebuffersize;

  /*******************************************************************/
  /* Dump the message properties                                     */
  /*******************************************************************/
  if (filter)
    printf("Message Properties matching '%s':\n", filter);
  else
    printf("Message Properties:\n");

  /*******************************************************************/
  /* Loop until MQINQMP unsuccessful                                 */
  /*******************************************************************/
  for (i = 0; compcode == MQCC_OK; i++)
  {
    MQINQMP(hconn,                  /* connection handle             */
            hmsg,                   /* message handle                */
            &impo,                  /* inquire msg properties opts   */
            &inqname,               /* property name                 */
            &pd,                    /* property descriptor           */
            &type,                  /* property type                 */
            valuebuffersize,        /* value buffer size             */
            valuebuffer,            /* value buffer                  */
            &valuelength,           /* value length                  */
            &compcode,              /* completion code               */
            &reason);               /* reason code                   */

    if (compcode != MQCC_OK)
    {
      switch(reason)
      {
        case MQRC_PROPERTY_NOT_AVAILABLE:
          /***********************************************************/
          /* The message contains no more properties, report if this */
          /* is the first time around the loop, i.e. none are found. */
          /***********************************************************/
          if (i == 0)
          {
            printf("  None\n");
          }
          break;

        case MQRC_PROPERTY_VALUE_TOO_BIG:
        {
          PMQBYTE newbuffer = NULL;

          /***********************************************************/
          /* The value buffer is too small, reallocate the buffer    */
          /* and inquire the same property again.                    */
          /***********************************************************/
          newbuffer = (PMQBYTE)realloc(valuebuffer, valuelength);

          if (newbuffer == NULL)
          {
            rc = MQRC_STORAGE_NOT_AVAILABLE;
            printf("Unable to re-allocate property value buffer\n");
            goto MOD_EXIT;
          }

          compcode = MQCC_OK;
          valuebuffer = newbuffer;
          valuebuffersize = valuelength;

          impo.Options = MQIMPO_CONVERT_VALUE | MQIMPO_INQ_PROP_UNDER_CURSOR;
        }
        break;

        case MQRC_PROPERTY_NAME_TOO_BIG:
        {
          PMQBYTE newbuffer = NULL;

          /***********************************************************/
          /* The name buffer is too small, reallocate the buffer and */
          /* inquire the same property again.                        */
          /***********************************************************/
          newbuffer = (PMQBYTE)realloc(namebuffer, impo.ReturnedName.VSLength);

          if (newbuffer == NULL)
          {
            rc = MQRC_STORAGE_NOT_AVAILABLE;
            printf("Unable to re-allocate property name buffer\n");
            goto MOD_EXIT;
          }

          compcode = MQCC_OK;
          namebuffer = newbuffer;
          namebuffersize = impo.ReturnedName.VSLength;

          impo.ReturnedName.VSPtr = namebuffer;
          impo.ReturnedName.VSBufSize = namebuffersize;
          impo.Options = MQIMPO_CONVERT_VALUE | MQIMPO_INQ_PROP_UNDER_CURSOR;
        }
        break;

        default:
          /***********************************************************/
          /* MQINQMP failed for some other reason                    */
          /***********************************************************/
          rc = reason;
          printf("MQINQMP ended with reason code %d\n", reason);
          goto MOD_EXIT;
      }
    }
    else
    {
      printf("  %.*s : ", impo.ReturnedName.VSLength, impo.ReturnedName.VSPtr);

      /***************************************************************/
      /* Print the property value in an appropriate format           */
      /***************************************************************/
      switch (type)
      {
        /*************************************************************/
        /* Boolean value                                             */
        /*************************************************************/
        case MQTYPE_BOOLEAN:
          printf("%s\n", *(PMQBOOL)valuebuffer ? "TRUE" : "FALSE");
          break;

        /*************************************************************/
        /* Byte-string value                                         */
        /*************************************************************/
        case MQTYPE_BYTE_STRING:
          printf("X'");
          for (j = 0 ; j < valuelength ; j++)
            printf("%02X", valuebuffer[j]);
          printf("'\n");
          break;

        /*************************************************************/
        /* 32-bit floating-point number value                        */
        /*************************************************************/
        case MQTYPE_FLOAT32:
          printf("%.12g\n", *(PMQFLOAT32)valuebuffer);
          break;

        /*************************************************************/
        /* 64-bit floating-point number value                        */
        /*************************************************************/
        case MQTYPE_FLOAT64:
          printf("%.18g\n", *(PMQFLOAT64)valuebuffer);
          break;

        /*************************************************************/
        /* 8-bit integer value                                       */
        /*************************************************************/
        case MQTYPE_INT8:
          printf("%d\n", valuebuffer[0]);
          break;

        /*************************************************************/
        /* 16-bit integer value                                      */
        /*************************************************************/
        case MQTYPE_INT16:
          printf("%hd\n", *(PMQINT16)valuebuffer);
          break;

        /*************************************************************/
        /* 32-bit integer value                                      */
        /*************************************************************/
        case MQTYPE_INT32:
          printf("%d\n", *(PMQLONG)valuebuffer);
          break;

        /*************************************************************/
        /* 64-bit integer value                                      */
        /*************************************************************/
        case MQTYPE_INT64:
          printf("%"Int64"d\n", *(PMQINT64)valuebuffer);
          break;

        /*************************************************************/
        /* Null value                                                */
        /*************************************************************/
        case MQTYPE_NULL:
          printf("NULL\n");
          break;

        /*************************************************************/
        /* String value                                              */
        /*************************************************************/
        case MQTYPE_STRING:
          printf("'%.*s'\n", valuelength, valuebuffer);
          break;

        /*************************************************************/
        /* A value with an unrecognized type                         */
        /*************************************************************/
        default:
          printf("<unrecognized data type>\n");
          break;
      }

      /***************************************************************/
      /* Inquire on the next property                                */
      /***************************************************************/
      impo.Options = MQIMPO_CONVERT_VALUE | MQIMPO_INQ_NEXT;
    }
  }

MOD_EXIT:

  free(valuebuffer);                          /* free(NULL) is valid */
  free(namebuffer);

  return rc;
}

/*********************************************************************/
/* Function name:    getParm                                         */
/*                                                                   */
/* Description:      Return parameters from the command line         */
/*********************************************************************/
int getParm(int    argc,
            char **argv,
            char **pFlag,
            char **pValue)
{
  char *p = NULL;

  *pFlag = *pValue = NULL;

  if (Parm_Index >= argc)
    goto MOD_EXIT;

  p = argv[Parm_Index++];

  if (*p == '-')
  {
    *pFlag = ++p;                    /* This is a flagged parm      */

    if (!**pFlag)                    /* No actual flag specified    */
      goto MOD_EXIT;

    p++;                             /* Advance to actual parameter */

    if (!*p)                         /* Is it there ?               */
    {
      if (Parm_Index >= argc)
        goto MOD_EXIT;

      if (*argv[Parm_Index] != '-')
        *pValue = argv[Parm_Index++];
    }
    else
      *pValue = p;
  }
  else
    *pValue = p;

MOD_EXIT:

  return (int)(*pFlag || *pValue);
}
