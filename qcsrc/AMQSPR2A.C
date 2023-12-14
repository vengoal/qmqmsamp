static const char sccsid[] = "%Z% %W% %I% %E% %U%";
/******************************************************************************/
/*                                                                            */
/* Module name: AMQSPR2A.C                                                    */
/*                                                                            */
/* Description: Sample C program that publishes to the default subscription   */
/*              point using RFH2 format messages on the supplied topic.       */
/*                                                                            */
/*  <copyright                                                                */
/*  notice="lm-source-program"                                                */
/*  pids="5724-H72"                                                           */
/*  years="2006,2016"                                                         */
/*  crc="2043144279" >                                                        */
/*  Licensed Materials - Property of IBM                                      */
/*                                                                            */
/*  5724-H72                                                                  */
/*                                                                            */
/*  (C) Copyright IBM Corp. 2006, 2016 All Rights Reserved.                   */
/*                                                                            */
/*  US Government Users Restricted Rights - Use, duplication or               */
/*  disclosure restricted by GSA ADP Schedule Contract with                   */
/*  IBM Corp.                                                                 */
/*  </copyright>                                                              */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Function:                                                                  */
/*                                                                            */
/*   AMQSPR2A is a sample C program for MQ Publish/Subscribe, it              */
/*   demonstrates publishing using RFH2 messages.                             */
/*                                                                            */
/*   Gets text from StdIn and creates a publication using the user data area  */
/*   following the RFH2 header.                                               */
/*                                                                            */
/*   Publishes to the default subscription point.                             */
/*                                                                            */
/*   Exits on a null input line.                                              */
/*                                                                            */
/*                                                                            */
/* Program logic:                                                             */
/*                                                                            */
/*   MQCONN to default queue manager (or optionally supplied name)            */
/*   MQOPEN default supscription point queue for output                       */
/*   MQOPEN reply queue for replies                                           */
/*   Define the MQRFH2 for the publications                                   */
/*   Define the MQMD of the message                                           */
/*   While data is read in:                                                   */
/*     Add string data to the publication message                             */
/*     Publish the message - MQPUT to default publication queue               */
/*     Check the reply messages - MQGET from reply queue                      */
/*   End while                                                                */
/*   MQCLOSE default subscription point queue                                 */
/*   MQCLOSE reply queue                                                      */
/*   MQDISC from queue manager                                                */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* AMQSPR2A takes 3 input parameters:                                         */
/*                                                                            */
/*   1st - topic to publish to                                                */
/*   2nd - name of the queue to identify the publisher and used for replies   */
/*   3rd - queue manager name (optional)                                      */
/*                                                                            */
/* The Topic string is limited to 256 characters (including the null          */
/* terminator) and must not contain blanks or double quotes ('"').            */
/*                                                                            */
/* The publication text is limited to 100 characters                          */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/* Includes                                                                   */
/******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cmqc.h>                                /* MQI                       */
#include <cmqpsc.h>                              /* MQI Publish/Subscribe     */
#include <cmqcfc.h>                              /* MQI PCF                   */
#include <cmqxc.h>                               /* MQI Exits and MQCD        */

/******************************************************************************/
/* Defines                                                                    */
/******************************************************************************/
#ifndef OK
   #define OK                0                   /* define OK as zero         */
#endif
#ifndef NOT_OK
   #define NOT_OK            1                   /* define NOT_OK as one      */
#endif
#ifndef FALSE
   #define FALSE             0
#endif
#ifndef TRUE
   #define TRUE              (!FALSE)
#endif
#define MAX_PUB_LENGTH       100+1               /* maximum publication len   */
#define MAX_TOPIC_LENGTH     256                 /* maximum topic length      */
#define MAX_MSG_LENGTH       1024                /* maximum message length    */
#define REPLY_WAIT_TIME      10000               /* reply wait time (10s)     */
#define PUBLICATION_QUEUE    "SYSTEM.BROKER.DEFAULT.STREAM"

/*----------------------------------------------------------------------------*/
/* Default US English CCSID for compiler on iSeries and z/OS, if this is not  */
/* the value used by your compiler you will need to change this.              */
/*----------------------------------------------------------------------------*/
#if (MQAT_DEFAULT == MQAT_OS400)
   #define COMPILER_CCSID    37
#elif (MQAT_DEFAULT == MQAT_MVS)
   #define COMPILER_CCSID    1047
#endif

/******************************************************************************/
/* Structures                                                                 */
/******************************************************************************/

/******************************************************************************/
/* Global variables                                                           */
/******************************************************************************/
MQHCONN hCon;                                    /* handle to connection      */

/******************************************************************************/
/* Function Prototypes                                                        */
/******************************************************************************/
MQINT32 SendPubSubCmd(PMQCHAR CmdStr,
                      MQHOBJ  hCmdObj,
                      MQHOBJ  hReplyObj,
                      PMQCHAR ReplyQName,
                      MQCHAR  Topic[MAX_TOPIC_LENGTH],
                      MQINT32 Options,
                      PMQCHAR pUserData,
                      MQINT32 UserDataLen);

MQINT32 BuildMQRFH2(PMQCHAR  CmdStr,
                    PMQRFH2  pRFH2,
                    MQCHAR   Topic[MAX_TOPIC_LENGTH],
                    MQINT32  Options);

MQINT32 CheckResponseMessage(PMQCHAR   CmdStr,
                             MQHOBJ    hReplyObj,
                             MQBYTE24  MsgId);

MQINT32 Convert(PMQCHAR   CmdStr,
                MQINT32   SrcCCSID,
                MQINT32   SrcLen,
                PMQCHAR   pSrcBuf,
                MQINT32   TgtCCSID);

/******************************************************************************/
/* Macros                                                                     */
/******************************************************************************/
/*----------------------------------------------------------------------------*/
/* Checks the comp code, if not MQCC_OK prints a message and sets return code */
/*----------------------------------------------------------------------------*/
#define mqCheck(func, compCode, reason)                                        \
{                                                                              \
   if (compCode != MQCC_OK)                                                    \
   {                                                                           \
      printf(func " failed with reason=%d %XX\n", reason, reason);             \
      printf("preceding line %d\n", __LINE__);                                 \
      if (rc == OK)                            /* don't overwrite a bad rc  */ \
         rc = compCode;                                                        \
   } /* endif */                                                               \
}

/******************************************************************************/
/*                                                                            */
/* Function: Main                                                             */
/* ==============                                                             */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Input Parameters:                                                          */
/* -----------------                                                          */
/* int argc                       - number of arguments supplied              */
/* char *argv[]                   - program arguments                         */
/*                                                                            */
/* Returns:                                                                   */
/* --------                                                                   */
/* OK                             - all is well                               */
/*                                                                            */
/******************************************************************************/
int main(int argc, char *argv[])
{
   int rc=OK;                                    /* return code               */
   /***************************************************************************/
   /* MQI structures                                                          */
   /***************************************************************************/
   MQOD pubQOD={MQOD_DEFAULT};                   /* publication Q object desc */
   MQOD repQOD={MQOD_DEFAULT};                   /* reply Q object desc       */
   /***************************************************************************/
   /* MQI variables                                                           */
   /***************************************************************************/
   MQHOBJ hPubQObj;                              /* handle to pub Q object    */
   MQHOBJ hRepQObj;                              /* handle to reply Q object  */
   MQCHAR topic[MAX_TOPIC_LENGTH];               /* topic                     */
   MQCHAR replyQName[MQ_Q_NAME_LENGTH+1];        /* reply queue name          */
   MQCHAR QMgrName[MQ_Q_MGR_NAME_LENGTH+1]="";   /* default Q manager name    */
   MQINT32 options;                              /* options                   */
   MQINT32 reason;                               /* reason code               */
   MQINT32 connReason;                           /* MQCONN reason code        */
   MQINT32 compCode;                             /* completion code           */
   MQINT32 openPQCompCode=MQCC_FAILED;           /* MQOPEN pub queue CC       */
   MQINT32 openRQCompCode=MQCC_FAILED;           /* MQOPEN reply queue CC     */
   MQCHAR msgBuf[MAX_MSG_LENGTH];                /* message buffer            */
   MQINT32 msgBufLen;                            /* message buffer length     */
   /***************************************************************************/
   /* Other variables                                                         */
   /***************************************************************************/
   MQBOOL keepLooping=TRUE;                      /* keep looping              */

   /***************************************************************************/
   /* First check we have been given correct arguments                        */
   /***************************************************************************/
   if ((argc < 3) || (argc > 4) || (strlen(argv[1]) > (MAX_TOPIC_LENGTH - 1)))
   {
      printf("Input is: %s 'topic' 'queue name' 'queue manager name'.\n"
             "Note the queue manager name is optional and the topic must "
             "not exceed 256 characters\n", argv[0]);
      exit(99);
   } /* endif */

   strcpy(topic, argv[1]);                       /* get the topic             */

   strncpy(replyQName, argv[2], MQ_Q_NAME_LENGTH); /* get the reply queue name */

   if (argc == 4)                                /* get the queue manager     */
      strncpy(QMgrName, argv[3], MQ_Q_MGR_NAME_LENGTH); /* name supplied      */

   /***************************************************************************/
   /* Connect to queue manager                                                */
   /***************************************************************************/
   if (rc == OK)
   {
      MQCONN(QMgrName, &hCon, &compCode, &connReason);
      mqCheck("MQCONN", compCode, connReason);
      if (compCode == MQCC_FAILED)
         exit(connReason);                       /* no point in going on      */
   } /* endif */

   /***************************************************************************/
   /* Open the publication queue for output                                   */
   /***************************************************************************/
   if (rc == OK)
   {
      strcpy(pubQOD.ObjectName, PUBLICATION_QUEUE);
      options = MQOO_OUTPUT + MQOO_FAIL_IF_QUIESCING;
      MQOPEN(hCon, &pubQOD, options, &hPubQObj, &openPQCompCode, &reason);
      mqCheck("MQOPEN", openPQCompCode, reason);
   } /* endif */

   /***************************************************************************/
   /* Open the reply queue for input - replies got from this queue            */
   /***************************************************************************/
   if (rc == OK)
   {
      strcpy(repQOD.ObjectName, replyQName);
      options = MQOO_INPUT_SHARED + MQOO_FAIL_IF_QUIESCING;
      MQOPEN(hCon, &repQOD, options, &hRepQObj, &openRQCompCode, &reason);
      mqCheck("MQOPEN", openRQCompCode, reason);
   } /* endif */

   /***************************************************************************/
   /* Set any publication options required for publishing here                */
   /***************************************************************************/
   /* options = MQPUBO_CORREL_ID_AS_IDENTITY +                                */
   /*           MQPUBO_OTHER_SUBSCRIBERS_ONLY +                               */
   /*           MQPUBO_RETAIN_PUBLICATION;                                    */
   if (rc == OK)
      options = 0;

   /***************************************************************************/
   /* Get user input from stdin and publish to publication queue              */
   /***************************************************************************/
   while (rc == OK && keepLooping)
   {
      printf("Enter text for publication on topic '%s':\n", topic);

      if (fgets(msgBuf, MAX_PUB_LENGTH, stdin) != NULL)
      {
         msgBufLen = (MQINT32)strlen(msgBuf);    /* length without null       */
         if (msgBuf[msgBufLen - 1] == '\n')      /* last char is a new-line   */
         {
            msgBuf[msgBufLen - 1]  = '\0';       /* change new-line to null   */
            msgBufLen--;                         /* reduce buffer length      */
         } /* endif */
      }
      else
      {
          msgBufLen = 0;                         /* treat EOF as null line    */
      } /* endif */

      /************************************************************************/
      /* Publish the message                                                  */
      /************************************************************************/
      if (msgBufLen)
      {
         rc = SendPubSubCmd(MQPSC_PUBLISH,
                            hPubQObj,            /* command object handle     */
                            hRepQObj,            /* reply object handle       */
                            replyQName,          /* reply queue name          */
                            topic,               /* topic                     */
                            options,             /* options                   */
                            msgBuf,              /* ptr to user data          */
                            msgBufLen);          /* user data length          */
      }
      else
      {
         keepLooping = FALSE;                    /* stop looping              */
      } /* endif */
   } /* endwhile */

   /***************************************************************************/
   /* Close the reply queue if opened                                         */
   /***************************************************************************/
   if (openRQCompCode != MQCC_FAILED)
   {
      MQCLOSE(hCon, &hRepQObj, MQCO_NONE, &compCode, &reason);
      mqCheck("MQCLOSE", compCode, reason);
   } /* endif */

   /***************************************************************************/
   /* Close the publication queue if opened                                   */
   /***************************************************************************/
   if (openPQCompCode != MQCC_FAILED)
   {
      MQCLOSE(hCon, &hPubQObj, MQCO_NONE, &compCode, &reason);
      mqCheck("MQCLOSE", compCode, reason);
   } /* endif */

   /***************************************************************************/
   /* Disconnect from queue manager if not already connected                  */
   /***************************************************************************/
   if (connReason != MQRC_ALREADY_CONNECTED)
   {
      MQDISC(&hCon, &compCode, &reason);
      mqCheck("MQDISC", compCode, reason);
   } /* endif */

   return rc;
}                                                /* ------ END OF MAIN ------ */


/******************************************************************************/
/*                                                                            */
/* Function: SendPubSubCmd                                                    */
/*                                                                            */
/* Description: Send the queued Pub/Sub interace a command                    */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Function:                                                                  */
/* ---------                                                                  */
/* Send the queued Pub/Sub interface a command by doing an MQPUT to the       */
/* appropriate queue.                                                         */
/*                                                                            */
/* Input Parameters:                                                          */
/* -----------------                                                          */
/* PMQCHAR CmdStr                 - command string                            */
/* PMQHOBJ hCmdObj                - handle to command object                  */
/* PMQHOBJ hReplyObj              - handle to reply object                    */
/* MQCHAR Topic[MAX_TOPIC_LENGTH] - topic                                     */
/* MQINT32 Options                - options defined using PCF constants       */
/* PMQCHAR pUserData              - ptr to user data                          */
/* MQINT32 UserDataLen            - user data length                          */
/*                                                                            */
/* In/Out Parameters:                                                         */
/* ------------------                                                         */
/* None                                                                       */
/*                                                                            */
/* Output Parameters:                                                         */
/* ------------------                                                         */
/* None                                                                       */
/*                                                                            */
/* Returns:                                                                   */
/* --------                                                                   */
/* OK                             - all is well                               */
/*                                - returns from MQI                          */
/*                                                                            */
/******************************************************************************/
MQINT32 SendPubSubCmd(PMQCHAR CmdStr,
                      MQHOBJ  hCmdObj,
                      MQHOBJ  hReplyObj,
                      PMQCHAR ReplyQName,
                      MQCHAR  Topic[MAX_TOPIC_LENGTH],
                      MQINT32 Options,
                      PMQCHAR pUserData,
                      MQINT32 UserDataLen)
{
   MQINT32 rc=OK;                                /* return code               */
   MQINT32 compCode;                             /* completion code           */
   MQINT32 reason;                               /* reason                    */
   MQCHAR msgBuf[MAX_MSG_LENGTH];                /* message buffer            */
   MQINT32 msgBufLen;                            /* message buffer length     */
   MQMD putMD={MQMD_DEFAULT};                    /* message descriptor        */
   MQPMO putMO={MQPMO_DEFAULT};                  /* put message options       */

   /***************************************************************************/
   /* Build the RFH2                                                          */
   /***************************************************************************/
   rc = BuildMQRFH2(CmdStr, (PMQRFH2) msgBuf, Topic, Options);

   /***************************************************************************/
   /* Set up some things for the MQPUT                                        */
   /***************************************************************************/
   if (rc == OK)
   {
      /************************************************************************/
      /* Set the MQPUT MD format to RFH2                                      */
      /************************************************************************/
      memcpy(putMD.Format, MQFMT_RF_HEADER_2, MQ_FORMAT_LENGTH);

      /************************************************************************/
      /* Set the put MQPUT MD message type to request so we get a response    */
      /************************************************************************/
      putMD.MsgType = MQMT_REQUEST;

      /************************************************************************/
      /* Set the reply to queue name for the response message we requested.   */
      /* Note we have not put a queue name in the MQRFH2, so the one in the   */
      /* ReplyToQ of the MD will default to being our identity.               */
      /************************************************************************/
      strcpy(putMD.ReplyToQ, ReplyQName);

      putMO.Options |= MQPMO_NEW_MSG_ID;
      putMO.Options |= MQPMO_NO_SYNCPOINT;

      /************************************************************************/
      /* If we have any user data, tag it onto the end of the RFH2            */
      /************************************************************************/
      if (UserDataLen)
      {
         msgBufLen = ((PMQRFH2) msgBuf)->StrucLength + UserDataLen;

         if (msgBufLen <= MAX_MSG_LENGTH)
         {
            memcpy(msgBuf + ((PMQRFH2) msgBuf)->StrucLength, pUserData, UserDataLen);
         }
         else
         {
            rc = NOT_OK;
            printf("SendPubSubCmd for %s error, message length too big\n", CmdStr);
         } /* endif */
      }
      else
      {
         msgBufLen = ((PMQRFH2) msgBuf)->StrucLength;
      } /* endif */
   } /* endif */

   /***************************************************************************/
   /* Do the MQPUT                                                            */
   /***************************************************************************/
   if (rc == OK)
   {
      MQPUT(hCon, hCmdObj, &putMD, &putMO, msgBufLen, msgBuf, &compCode, &reason);
      mqCheck("MQPUT", compCode, reason);
   } /* endif */

   /***************************************************************************/
   /* Check the response message says our command was OK                      */
   /***************************************************************************/
   if (rc == OK)
   {
      rc = CheckResponseMessage(CmdStr, hReplyObj, putMD.MsgId);
   } /* endif */

   return rc;
}


/******************************************************************************/
/*                                                                            */
/* Function: BuildMQRFH2                                                      */
/*                                                                            */
/* Description: Build an MQRFH2 including the name value pairs                */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Function:                                                                  */
/* ---------                                                                  */
/* Build an MQRFH2 by extracting the various options etc. and creating the    */
/* appropriate name value pairs.                                              */
/*                                                                            */
/* Input Parameters:                                                          */
/* -----------------                                                          */
/* PMQCHAR CmdStr                 - command string                            */
/* PMQRFH2 pRFH2                  - ptr to RFH2                               */
/* MQCHAR topic[MAX_TOPIC_LENGTH] - topic                                     */
/* MQINT32 Options                - options defined using PCF constants       */
/*                                                                            */
/* In/Out Parameters:                                                         */
/* ------------------                                                         */
/* None                                                                       */
/*                                                                            */
/* Output Parameters:                                                         */
/* ------------------                                                         */
/* None                                                                       */
/*                                                                            */
/* Returns:                                                                   */
/* --------                                                                   */
/* OK                             - all is well                               */
/*                                                                            */
/******************************************************************************/
MQINT32 BuildMQRFH2(PMQCHAR  CmdStr,
                    PMQRFH2  pRFH2,
                    MQCHAR   Topic[MAX_TOPIC_LENGTH],
                    MQINT32  Options)
{
   MQINT32 rc=OK;                                /* return code               */
   MQINT32 nameValueLength;                      /* RFH2 NameValuelength      */
   PMQCHAR pNameValueData;                       /* ptr to RFH2 NameValeData  */
   MQRFH2 defaultMQRFH2={MQRFH2_DEFAULT};        /* default RFH2              */
   MQINT32 padLen;                               /* padding length            */

   /***************************************************************************/
   /* Build up the MQRFH2                                                     */
   /*                                                                         */
   /* NameValueLength and NameValueData pairs follow the MQRFH2, the length   */
   /* is derived from the StrucLength. For example:                           */
   /*                                                                         */
   /* ----------------------                                                  */
   /* | RFH2               |     36 - MQRFH_STRUC_LENGTH_FIXED_2              */
   /* ----------------------                                                  */
   /* | NameValueLength=8  |     4                                            */
   /* | NameValueData      |     8  - must be a multiple of 4                 */
   /* | NameValueLength=16 |     4                                            */
   /* | NameValueData      |     16 - must be a multiple of 4                 */
   /* | NameValueLength=24 |     4                                            */
   /* | NameValueData      |     24 - must be a multiple of 4                 */
   /* ----------------------     --                                           */
   /* |                    |     96 = StrucLength **                          */
   /* | User data=160      |    160                                           */
   /* |                    |    ---                                           */
   /* ----------------------    256  = message length                         */
   /*                                                                         */
   /* ** See note below about aligning to sixteen bytes                       */
   /*                                                                         */
   /* Example psc folder for Register Subscriber:                             */
   /* <psc>                                                                   */
   /*    <Command>RegSub</Command>                                            */
   /*    <RegOpt>PubOnReqOnly</RegOpt>                                        */
   /*    <RegOpt>CorrelAsId</RegOpt>                                          */
   /*    <Topic>Sport/Soccer/LatestScore/#</Topic>                            */
   /* </psc>                                                                  */
   /*                                                                         */
   /* Example psc folder for Publish:                                         */
   /* <psc>                                                                   */
   /*    <Command>Publish</Command>                                           */
   /*    <PubOpt>RetainPub</PubOpt>                                           */
   /*    <Topic>Sport/Soccer/LatestScore/Chelsea 4 Sotton 1</Topic>           */
   /* </psc>                                                                  */
   /*                                                                         */
   /* Example psc folder for Request Update:                                  */
   /* <psc>                                                                   */
   /*    <Command>ReqUpdate</Command>                                         */
   /*    <RegOpt>CorrelAsId</RegOpt>                                          */
   /*    <Topic>Sport/Soccer/LatestScore/#</Topic>                            */
   /* </psc>                                                                  */
   /*                                                                         */
   /***************************************************************************/

   /***************************************************************************/
   /* Set up the default RFH2                                                 */
   /***************************************************************************/
   memcpy(pRFH2, &defaultMQRFH2, sizeof(MQRFH2));

   /***************************************************************************/
   /* Set the RFH2 command                                                    */
   /***************************************************************************/
   pNameValueData = (PMQCHAR) pRFH2 + MQRFH_STRUC_LENGTH_FIXED_2 + 4;

   /***************************************************************************/
   /* Open the Pub/Sub command folder                                         */
   /***************************************************************************/
   strcpy(pNameValueData, MQRFH2_PUBSUB_CMD_FOLDER_B);

   /***************************************************************************/
   /* set the command                                                         */
   /***************************************************************************/
   strcat(pNameValueData, MQPSC_COMMAND_B);
   strcat(pNameValueData, CmdStr);
   strcat(pNameValueData, MQPSC_COMMAND_E);

   /***************************************************************************/
   /* Add any options for the command                                         */
   /***************************************************************************/
   if (strcmp(CmdStr, MQPSC_DELETE_PUBLICATION) == 0)
   {
      if (Options != 0)
      {
         if (Options & MQREGO_LOCAL)
         {
            strcat(pNameValueData, MQPSC_DELETE_OPTION_B);
            strcat(pNameValueData, MQPSC_LOCAL);
            strcat(pNameValueData, MQPSC_DELETE_OPTION_E);
         } /* endif */
      } /* endif */
   }
   else if ((strcmp(CmdStr, MQPSC_DEREGISTER_SUBSCRIBER) == 0) ||
            (strcmp(CmdStr, MQPSC_REGISTER_SUBSCRIBER) == 0) ||
            (strcmp(CmdStr, MQPSC_REQUEST_UPDATE) == 0))
   {
      /************************************************************************/
      /* Here are just a few of the many possible registration options        */
      /************************************************************************/
      if (Options != 0)
      {
         if (Options & MQREGO_CORREL_ID_AS_IDENTITY)
         {
            strcat(pNameValueData, MQPSC_REGISTRATION_OPTION_B);
            strcat(pNameValueData, MQPSC_CORREL_ID_AS_IDENTITY);
            strcat(pNameValueData, MQPSC_REGISTRATION_OPTION_E);
         } /* endif */

         if (Options & MQREGO_FULL_RESPONSE)
         {
            strcat(pNameValueData, MQPSC_REGISTRATION_OPTION_B);
            strcat(pNameValueData, MQPSC_FULL_RESPONSE);
            strcat(pNameValueData, MQPSC_REGISTRATION_OPTION_E);
         } /* endif */

         if (Options & MQREGO_LOCAL)
         {
            strcat(pNameValueData, MQPSC_REGISTRATION_OPTION_B);
            strcat(pNameValueData, MQPSC_LOCAL);
            strcat(pNameValueData, MQPSC_REGISTRATION_OPTION_E);
         } /* endif */

         if (Options & MQREGO_PUBLISH_ON_REQUEST_ONLY)
         {
            strcat(pNameValueData, MQPSC_REGISTRATION_OPTION_B);
            strcat(pNameValueData, MQPSC_PUB_ON_REQUEST_ONLY);
            strcat(pNameValueData, MQPSC_REGISTRATION_OPTION_E);
         } /* endif */
      } /* endif */
   }
   else if (strcmp(CmdStr, MQPSC_PUBLISH) == 0)
   {
      /************************************************************************/
      /* The publish information we get from the user is put at the end of    */
      /* the NameValue pairs, it is of the format string.                     */
      /************************************************************************/
      memcpy(pRFH2->Format, MQFMT_STRING, MQ_FORMAT_LENGTH);

      /************************************************************************/
      /* Here are some of the possible publication options                    */
      /************************************************************************/
      if (Options != 0)
      {
         if (Options & MQPUBO_CORREL_ID_AS_IDENTITY)
         {
            strcat(pNameValueData, MQPSC_REGISTRATION_OPTION_B);
            strcat(pNameValueData, MQPSC_CORREL_ID_AS_IDENTITY);
            strcat(pNameValueData, MQPSC_REGISTRATION_OPTION_E);
         } /* endif */

         if (Options & MQPUBO_OTHER_SUBSCRIBERS_ONLY)
         {
            strcat(pNameValueData, MQPSC_PUBLICATION_OPTION_B);
            strcat(pNameValueData, MQPSC_OTHER_SUBS_ONLY );
            strcat(pNameValueData, MQPSC_PUBLICATION_OPTION_E);
         } /* endif */

         if (Options & MQPUBO_RETAIN_PUBLICATION)
         {
            strcat(pNameValueData, MQPSC_PUBLICATION_OPTION_B);
            strcat(pNameValueData, MQPSC_RETAIN_PUB);
            strcat(pNameValueData, MQPSC_PUBLICATION_OPTION_E);
         } /* endif */
      } /* endif */
   }
   else
   {
      rc = NOT_OK;
      printf("BuildMQRFH2 for %s error, unknown command\n", CmdStr);
   } /* endif */

   /***************************************************************************/
   /* Add the topic                                                           */
   /***************************************************************************/
   if (rc == OK)
   {
      strcat(pNameValueData, MQPSC_TOPIC_B);
      strcat(pNameValueData, Topic);
      strcat(pNameValueData, MQPSC_TOPIC_E);
   } /* endif */

   /***************************************************************************/
   /* This sample uses the default subscription point, so there is no need to */
   /* add this particular property. If the non-default is required, add it    */
   /* and make sure that a topic object exists with exactly the same name for */
   /* its topic string. The topic object name must also be added into the     */
   /* SYSTEM.QPUBSUB.SUBPOINT.NAMELIST. Generally you would keep your topic   */
   /* object name and its associated string the same, however topic object    */
   /* names are limited to 48 (MQ_TOPIC_NAME_LENGTH) characters where as      */
   /* subscription points are limited to 128 (MQ_SUB_POINT_LENGTH) characters.*/
   /* A simple scheme to get round this difference in lengths, as adopted by  */
   /* the migration tool'migmbbrk', is for any subscription points longer     */
   /* than 48 characters, use the first and last 23 characters as the first   */
   /* and last parts of the object name, with characters 24 and 25 being a    */
   /* sequence number in hex starting at 00. Obviously FF (255) is the        */
   /* maximum , but as namelists are limited to 256 entries, this is not a    */
   /* problem.                                                                */
   /*                                                                         */
   /* For example in runmqsc:                                                 */
   /*                                                                         */
   /*    def topic('MySpecialSubscriptionPoint')             \                */
   /*        topicstr('MySpecialSubscriptionPoint')          \                */
   /*        descr('This is my special subscription point')  \                */
   /*        like(SYSTEM.BASE.TOPIC)                                          */
   /*                                                                         */
   /*    display topic ('MySpecialSubscriptionPoint')                         */
   /*                                                                         */
   /*    display namelist (SYSTEM.QPUBSUB.SUBPOINT.NAMELIST)                  */
   /*                                                                         */
   /*    alter namelist (SYSTEM.QPUBSUB.SUBPOINT.NAMELIST)   \                */
   /*          names(SYSTEM.BROKER.DEFAULT.SUBPOINT,         \                */
   /*                'MySpecialSubscriptionPoint')                            */
   /*                                                                         */
   /*    display namelist (SYSTEM.QPUBSUB.SUBPOINT.NAMELIST)                  */
   /*                                                                         */
   /* For improved performance you can also create a queue of the same name   */
   /* and add it to the SYSTEM.QPUBSUB.QUEUE.NAMELIST.                        */
   /*                                                                         */
   /***************************************************************************/
   /* if (rc == OK)                                                           */
   /* {                                                                       */
   /*    strcat(pNameValueData, MQPSC_SUBSCRIPTION_POINT_B);                  */
   /*    strcat(pNameValueData, "MySpecialSubscriptionPoint");                */
   /*    strcat(pNameValueData, MQPSC_SUBSCRIPTION_POINT_E);                  */
   /* } *//* endif *//*                                                       */

   /***************************************************************************/
   /* Close the Pub/Sub command folder                                        */
   /***************************************************************************/
   if (rc == OK)
   {
      strcat(pNameValueData, MQRFH2_PUBSUB_CMD_FOLDER_E);
   } /* endif */

   /***************************************************************************/
   /* Set the Pub/Sub command folder length and the RFH2 StructLength field   */
   /***************************************************************************/
   if (rc == OK)
   {
      /************************************************************************/
      /* As we are the last (only) NameValue pair it is acceptable to include */
      /* the null terminator character in the NameValueData, this is not      */
      /* required by MQ but it does allow us to use string functions on it in */
      /* the future.                                                          */
      /************************************************************************/
      nameValueLength = (MQINT32)(strlen(pNameValueData) + 1);

      pRFH2->StrucLength = MQRFH_STRUC_LENGTH_FIXED_2 + 4 + nameValueLength;

      /************************************************************************/
      /* As stated above, we must have a NameValueLength that is a multiple   */
      /* of 4, however some platforms (iSeries) require certain user data     */
      /* following the RFH2 to be aligned to a sixteen byte boundry, so by    */
      /* ensuring the latter requirement we ensure the former requirement. As */
      /* stated above we are the last (only) NameValue pair so it is          */
      /* acceptable to pad the NameValueData with null terminator characters. */
      /************************************************************************/
      padLen = pRFH2->StrucLength % 16;

      if (padLen)
      {
         for (padLen=16-padLen; padLen; padLen--, nameValueLength++, pRFH2->StrucLength++)
            *(pNameValueData + nameValueLength) = '\0';
      } /* endif */

      /************************************************************************/
      /* Set the RFH2 NameValueLength                                         */
      /************************************************************************/
      /* Don't risk causing a SIGBUS, memcpy the four bytes                   */
      memcpy((PMQCHAR) pRFH2 + MQRFH_STRUC_LENGTH_FIXED_2, &nameValueLength, 4);
   } /* endif */

   /***************************************************************************/
   /* On platforms which use EBCDIC, the NameValueData must be converted to   */
   /* the NameValueCCSID we set in the RFH2.                                  */
   /***************************************************************************/
#if (MQAT_DEFAULT == MQAT_OS400) || (MQAT_DEFAULT == MQAT_MVS)
      if (rc == OK)
      {
         rc = Convert(CmdStr,                    /* command string            */
                      COMPILER_CCSID,            /* source CCSID              */
                      nameValueLength,           /* source length             */
                      pNameValueData,            /* source buffer             */
                      pRFH2->NameValueCCSID);    /* target CCSID              */
      } /* endif */
#endif

   return rc;
}

/******************************************************************************/
/*                                                                            */
/* Function: CheckResponseMessage                                             */
/*                                                                            */
/* Description: Check response message                                        */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Function:                                                                  */
/* ---------                                                                  */
/* Checks the RFH2 response message for an RFH2 Pub/Sub command are OK        */
/*                                                                            */
/* Input Parameters:                                                          */
/* -----------------                                                          */
/* PMQCHAR CmdStr                 - command string                            */
/* PMQHOBJ hReplyObj              - handle to reply object                    */
/* MQBYTE24 MsgId                 - message Id                                */
/*                                                                            */
/* In/Out Parameters:                                                         */
/* ------------------                                                         */
/* None                                                                       */
/*                                                                            */
/* Output Parameters:                                                         */
/* ------------------                                                         */
/* None                                                                       */
/*                                                                            */
/* Returns:                                                                   */
/* --------                                                                   */
/* OK                             - all is well                               */
/*                                - returns from MQI                          */
/*                                                                            */
/******************************************************************************/
MQINT32 CheckResponseMessage(PMQCHAR   CmdStr,
                             MQHOBJ    hReplyObj,
                             MQBYTE24  MsgId)
{
   MQINT32 rc=OK;                                /* return code               */
   MQMD getMD={MQMD_DEFAULT};                    /* get message descriptor    */
   MQGMO getMO={MQGMO_DEFAULT};                  /* get message options       */
   MQCHAR msgBuf[MAX_MSG_LENGTH];                /* message buffer            */
   MQINT32 msgBufLen;                            /* message buffer length     */
   MQINT32 msgLen;                               /* message length received   */
   PMQRFH2 pRFH2;                                /* ptr to RFH2               */
   MQINT32 nameValueLength;                      /* RFH2 NameValuelength      */
   PMQCHAR pNameValueData;                       /* ptr to RFH2 NameValeData  */
   PMQCHAR pPropValueStart;                      /* -> property value start   */
   PMQCHAR pPropValueEnd;                        /* -> property value end     */
   size_t  propValueLength;                      /* property value length     */
   MQCHAR propValueStr[256]="";                  /* property value string     */
   MQINT32 compCode;                             /* completion code           */
   MQINT32 reason;                               /* reason                    */
   MQBOOL foundFolder=FALSE;                     /* found folder              */

   msgBufLen = sizeof(msgBuf) - 1;

   getMO.Options = MQGMO_WAIT +
                   MQGMO_CONVERT +
                   MQGMO_NO_SYNCPOINT +
                   MQGMO_ACCEPT_TRUNCATED_MSG;
   getMO.WaitInterval = REPLY_WAIT_TIME;
   getMO.Version = MQGMO_VERSION_2;
   getMO.MatchOptions = MQMO_MATCH_CORREL_ID;

   /***************************************************************************/
   /* The response's CorrelId will be the same as the MsgId that the original */
   /* message was MQPUT with (returned in the MD from the MQPUT) so match     */
   /* against this.                                                           */
   /***************************************************************************/
   memcpy(getMD.CorrelId, MsgId, MQ_CORREL_ID_LENGTH);

   if (rc == OK)
   {
      MQGET(hCon, hReplyObj, &getMD, &getMO, msgBufLen, msgBuf, &msgLen,
            &compCode, &reason);

      if (reason == MQRC_NO_MSG_AVAILABLE)
      {
         rc = NOT_OK;
         printf("CheckResponseMessage for %s error, no response was sent by MQ, "
                "check the queued Pub/Sub interface is enabled.\n", CmdStr);
      }
      else if (reason != MQRC_TRUNCATED_MSG_ACCEPTED)
      {
         mqCheck("MQGET", compCode, reason);
         if (rc != OK)
            printf("CheckResponseMessage for %s error\n", CmdStr);
      } /* endif */
   } /* endif */

   if (rc == OK)
   {
      /************************************************************************/
      /* Check this is an RFH2 and various fields are as we expect            */
      /************************************************************************/
      pRFH2 = (PMQRFH2) msgBuf;

      if (memcmp(getMD.Format, MQFMT_RF_HEADER_2, MQ_FORMAT_LENGTH) != 0)
      {
         rc = NOT_OK;
         printf("CheckResponseMessage for %s error, MD.Format not RFH2, MD.Format: %.8s\n",
                CmdStr, getMD.Format);
      } /* endif */

      if (rc == OK && memcmp(pRFH2->StrucId, MQRFH_STRUC_ID, 4) != 0)
      {
         rc = NOT_OK;
         printf("CheckResponseMessage for %s error, StrucId not RFH, StrucId: %.4s\n",
                 CmdStr, pRFH2->StrucId);
      } /* endif */

      if (rc == OK && pRFH2->Version != MQRFH_VERSION_2)
      {
         rc = NOT_OK;
         printf("CheckResponseMessage for %s error, Version not MQRFH_VERSION_2, Version: %d\n",
                CmdStr, pRFH2->Version);
      } /* endif */

      /************************************************************************/
      /* Check that this is the reply message or is it an exception report    */
      /************************************************************************/
      if (rc == OK && getMD.MsgType == MQMT_REPLY)
      {
         /*********************************************************************/
         /* Extract the CompCode and Reason from the RFH2 pub/sub command     */
         /* response (pscr) folder.                                           */
         /*                                                                   */
         /* NameValueLength and NameValueData pairs follow the MQRFH2, the    */
         /* length is derived from the StrucLength. For example:              */
         /*                                                                   */
         /* ----------------------                                            */
         /* | RFH2               |     36 - MQRFH_STRUC_LENGTH_FIXED_2        */
         /* ----------------------                                            */
         /* | NameValueLength=8  |     4                                      */
         /* | NameValueData      |     8  - must be a multiple of 4           */
         /* | NameValueLength=16 |     4                                      */
         /* | NameValueData      |     16 - must be a multiple of 4           */
         /* ----------------------     --                                     */
         /*                            68 = StrucLength                       */
         /*                                                                   */
         /* The start of a pub/sub command response (pscr) folder is always   */
         /* in the same format, that is the completion is first and always    */
         /* provided, for example:                                            */
         /*                                                                   */
         /* <pscr>                                                            */
         /*    <Completion>ok</Completion>                                    */
         /* </pscr>                                                           */
         /*                                                                   */
         /* Or in the case of the registration option, full response being    */
         /* set:                                                              */
         /*                                                                   */
         /*  <pscr>                                                           */
         /*      <Completion>ok</Completion>                                  */
         /*      <Command>RegSub</Command>                                    */
         /*      <Topic>MyTopic</Topic>                                       */
         /*      <QMgrName>MyQueueManager</QMgrName>                          */
         /*      <QName>MyQueue</QName>                                       */
         /*      <UserId>Fred</UserId>                                        */
         /*  </pscr>                                                          */
         /*                                                                   */
         /* The Response folder only exists if the completion is not 'ok'.    */
         /* There may be other properties that indicate the cause of the      */
         /* error or warning and there may be more than one response folder.  */
         /* Also in the case of the completion not being 'ok', the original   */
         /* pub/sub command (psc) folder will be tagged on as another         */
         /* NameValueData string and not added to the pub/sub command         */
         /* response (pscr) folder, regardless of the registration option,    */
         /* full response being set.                                          */
         /*                                                                   */
         /* for example:                                                      */
         /*                                                                   */
         /* NameValueLength = length of pscr folder below                     */
         /* <pscr>                                                            */
         /*    <Completion>error</Completion>                                 */
         /*    <Response>                                                     */
         /*       <Reason>3150</Reason>                                       */
         /*    </Response>                                                    */
         /* </pscr>                                                           */
         /* NameValueLength = length of psc folder below                      */
         /* <psc>                                                             */
         /*    command mssage to which the broker is responding               */
         /* </psc>                                                            */
         /*                                                                   */
         /* Or:                                                               */
         /*                                                                   */
         /* NameValueLength = length of pscr folder below                     */
         /* <pscr>                                                            */
         /*    <Completion>warning</Completion>                               */
         /*    <Response>                                                     */
         /*       <Reason>3081</Reason>                                       */
         /*       <Topic>topic1</Topic>                                       */
         /*    </Response>                                                    */
         /*    <Response>                                                     */
         /*       <Reason>3081</Reason>                                       */
         /*       <Topic>topic2</Topic>                                       */
         /*    </Response>                                                    */
         /* </pscr>                                                           */
         /* NameValueLength = length of psc folder below                      */
         /* <psc>                                                             */
         /*    command mssage to which the broker is responding               */
         /* </psc>                                                            */
         /*                                                                   */
         /*********************************************************************/

         /*********************************************************************/
         /* Make sure we have a NameValueLength before we try to get it.      */
         /*********************************************************************/
         if (pRFH2->StrucLength > MQRFH_STRUC_LENGTH_FIXED_2 + 4)
         {
            pNameValueData = msgBuf + MQRFH_STRUC_LENGTH_FIXED_2 + 4;
            /* Don't risk causing a SIGBUS, memcpy the four bytes             */
            memcpy(&nameValueLength, msgBuf + MQRFH_STRUC_LENGTH_FIXED_2, 4);
         }
         else
         {
            rc = NOT_OK;
            printf("CheckResponseMessage for %s error, no NameValue pair\n",
                   CmdStr);
         } /* endif */

         /*********************************************************************/
         /* There could be multiple folders and since on some platforms we    */
         /* will need to convert the data to our CCSID, we will loop round    */
         /* looking for the response pscr folder                              */
         /*********************************************************************/
         while (rc == OK && !foundFolder)
         {
            /******************************************************************/
            /* On platforms which use EBCDIC, the NameValueData must be       */
            /* converted back to our CCSID.                                   */
            /******************************************************************/
#if (MQAT_DEFAULT == MQAT_OS400) || (MQAT_DEFAULT == MQAT_MVS)
            {
               rc = Convert(CmdStr,                 /* command string         */
                            pRFH2->NameValueCCSID,  /* source CCSID           */
                            nameValueLength,        /* source length          */
                            pNameValueData,         /* source buffer          */
                            COMPILER_CCSID);        /* target CCSID           */
            }
#endif

            /******************************************************************/
            /* Check to see if this is the response pscr folder               */
            /******************************************************************/
            if (rc == OK && (nameValueLength > (MQINT32) strlen(MQRFH2_PUBSUB_RESP_FOLDER_B)) &&
                (memcmp(pNameValueData, MQRFH2_PUBSUB_RESP_FOLDER_B,
                 strlen(MQRFH2_PUBSUB_RESP_FOLDER_B)) == 0))
            {
               foundFolder = TRUE;               /* set flag                  */
            }
            else if (rc == OK)
            {
               /***************************************************************/
               /* Bump up to the next NameVale pair, if there is another      */
               /***************************************************************/
               if (pRFH2->StrucLength > (pNameValueData - msgBuf + nameValueLength + 4))
               {
                  pNameValueData = pNameValueData + nameValueLength + 4;
                  /* Don't risk causing a SIGBUS, memcpy the four bytes       */
                  memcpy(&nameValueLength, pNameValueData - 4, 4);
               }
               else
               {
                  rc = NOT_OK;
                  printf("CheckResponseMessage for %s error, response folder not found\n",
                         CmdStr);
               } /* endif */
            } /* endif */
         } /* endwhile */

         /*********************************************************************/
         /* Extract the completion and reason codes. There could be multiple  */
         /* reasons and there may be other properties, but we will just do    */
         /* the first one.                                                    */
         /*********************************************************************/
         if (rc == OK)
         {
            pPropValueStart = strstr(pNameValueData, MQPSCR_COMPLETION_B);

            if (pPropValueStart != NULL)
            {
               pPropValueStart += strlen(MQPSCR_COMPLETION_B);

               pPropValueEnd = strstr(pNameValueData, MQPSCR_COMPLETION_E);

               if (pPropValueEnd != NULL)
               {
                  propValueLength =  pPropValueEnd - pPropValueStart;

                  if (memcmp(pPropValueStart, MQPSCR_OK, propValueLength) == 0)
                     compCode = MQCC_OK;
                  else if (memcmp(pPropValueStart, MQPSCR_WARNING, propValueLength) == 0)
                     compCode = MQCC_WARNING;
                  else if (memcmp(pPropValueStart, MQPSCR_ERROR, propValueLength) == 0)
                     compCode = MQCC_FAILED;

                  if (compCode == MQCC_OK)
                  {
                     reason = MQRC_NONE;
                  }
                  else
                  {
                     pPropValueStart = strstr(pNameValueData, MQPSCR_REASON_B);

                     if (pPropValueStart != NULL)
                     {
                        pPropValueStart += strlen(MQPSCR_REASON_B);

                        pPropValueEnd = strstr(pNameValueData, MQPSCR_REASON_E);

                        if (pPropValueEnd != NULL)
                        {
                           propValueLength =  pPropValueEnd - pPropValueStart;

                           memcpy(propValueStr, pPropValueStart, propValueLength);
                           propValueStr[propValueLength] = '\0';
                           reason = atoi(propValueStr);
                        }
                        else
                        {
                           rc = NOT_OK;
                           printf("CheckResponseMessage for %s error, Reason not complete\n", CmdStr);
                        } /* endif */
                     }
                     else
                     {
                        rc = NOT_OK;
                        printf("CheckResponseMessage for %s error, Reason not found\n", CmdStr);
                     } /* endif */
                  } /* endif */
               }
               else
               {
                  rc = NOT_OK;
                  printf("CheckResponseMessage for %s error, CompCode not complete\n", CmdStr);
               } /* endif */

            }
            else
            {
               rc = NOT_OK;
               printf("CheckResponseMessage for %s error, CompCode not found\n", CmdStr);
            } /* endif */
         } /* endif */

         /*********************************************************************/
         /* We now have the completion and reason codes, if things are not    */
         /* OK print out a messge and set rc.                                 */
         /*********************************************************************/
         if (rc == OK)
         {
            /******************************************************************/
            /* One possible error is acceptable, MQRCCF_NO_RETAINED_MSG,      */
            /* which is returned from a Request Update when there is no       */
            /* retained message avaliable.                                    */
            /******************************************************************/
            if (reason == MQRCCF_NO_RETAINED_MSG)
            {
               compCode = MQCC_OK;
               reason = MQRC_NONE;
            }
            else
            {
               mqCheck("CheckResponseMessage", compCode, reason);

               if (rc != OK)
                  printf("CheckResponseMessage for %s error\n", CmdStr);
            } /* endif */
         } /* endif */
      }
      else if (rc == OK && getMD.MsgType == MQMT_REPORT)
      {
         rc = NOT_OK;
         printf("CheckResponseMessage for %s error, Exception Report received! feedback=%d %XX\n",
                CmdStr, getMD.Feedback, getMD.Feedback);
      }
      else if (rc == OK)
      {
         rc = NOT_OK;
         printf("CheckResponseMessage for %s error, unexpected message type %d\n",
                CmdStr, getMD.MsgType);
      } /* endif */
   } /* endif */

   return rc;
}


#if (MQAT_DEFAULT == MQAT_OS400) || (MQAT_DEFAULT == MQAT_MVS)
/******************************************************************************/
/*                                                                            */
/* Function: Convert                                                          */
/*                                                                            */
/* Description: Convert the NameValueData into the appropriate CCSID          */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Function:                                                                  */
/* ---------                                                                  */
/* Convert the NameValueData into the appropriate CCSID                       */
/*                                                                            */
/* Input Parameters:                                                          */
/* -----------------                                                          */
/* PMQCHAR   CmdStr               - command string                            */
/* MQINT32   SrcCCSID             - source CCSID                              */
/* MQINT32   SrcLen               - source length                             */
/* MQINT32   TgtCCSID             - target CCSID                              */
/*                                                                            */
/* In/Out Parameters:                                                         */
/* ------------------                                                         */
/* PMQCHAR   pSrcBuf,             - ptr to source buffer                      */
/*                                                                            */
/* Output Parameters:                                                         */
/* ------------------                                                         */
/* None                                                                       */
/*                                                                            */
/* Returns:                                                                   */
/* --------                                                                   */
/* OK                             - all is well                               */
/*                                - returns from MQI                          */
/*                                                                            */
/******************************************************************************/
MQINT32 Convert(PMQCHAR   CmdStr,
                MQINT32   SrcCCSID,
                MQINT32   SrcLen,
                PMQCHAR   pSrcBuf,
                MQINT32   TgtCCSID)
{
   MQINT32 rc=OK;                                /* return code               */
   MQINT32 compCode;                             /* completion code           */
   MQINT32 reason;                               /* reason                    */
   MQINT32 options;                              /* char conversion options   */
   MQCHAR tgtBuf[MAX_MSG_LENGTH];                /* target buffer             */
   MQINT32 tgtLen;                               /* target length             */

   options = MQDCC_DEFAULT_CONVERSION +
             MQDCC_SOURCE_ENC_NATIVE +
             MQDCC_TARGET_ENC_NATIVE;

   MQXCNVC(hCon,                                 /* handle to MQ connection   */
           options,                              /* options                   */
           SrcCCSID,                             /* source CCSID              */
           SrcLen,                               /* source length             */
           pSrcBuf,                              /* source buffer             */
           TgtCCSID,                             /* target CCSID              */
           MAX_MSG_LENGTH,                       /* target buffer length      */
           tgtBuf,                               /* target buffer             */
           &tgtLen,                              /* target length             */
           &compCode,                            /* completion code           */
           &reason);                             /* reason                    */

   mqCheck("MQXCNVC", compCode, reason);

   /***************************************************************************/
   /* Copy the converted NameValueData back into the RFH2                     */
   /***************************************************************************/
   if (rc == OK)
   {
      /************************************************************************/
      /* The Pub/Sub strings in cmqpsc.h should not alter in length due to    */
      /* the conversion above, if they do we are in trouble as the lengths    */
      /* set in the RFH2 will not be as we set or are expecting.              */
      /************************************************************************/
      if (tgtLen == SrcLen)
      {
         memcpy(pSrcBuf, tgtBuf, SrcLen);
      }
      else
      {
         rc = NOT_OK;
         printf("Convert for %s conversion error, source length: %d, target length: %d\n",
                CmdStr, SrcLen, tgtLen);
      } /* endif */
   } /* endif */

   return rc;
}
#endif

