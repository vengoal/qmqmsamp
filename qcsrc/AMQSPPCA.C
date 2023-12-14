static const char sccsid[] = "%Z% %W% %I% %E% %U%";
/******************************************************************************/
/*                                                                            */
/* Module name: AMQSPPCA.C                                                    */
/*                                                                            */
/* Description: Sample C program that publishes to the default stream using   */
/*              PCF via the MQAI interface on a given topic.                  */
/*                                                                            */
/*   <copyright                                                               */
/*   notice="lm-source-program"                                               */
/*   pids="5724-H72"                                                          */
/*   years="1998,2016"                                                        */
/*   crc="2658256850" >                                                       */
/*   Licensed Materials - Property of IBM                                     */
/*                                                                            */
/*   5724-H72                                                                 */
/*                                                                            */
/*   (C) Copyright IBM Corp. 1998, 2016 All Rights Reserved.                  */
/*                                                                            */
/*   US Government Users Restricted Rights - Use, duplication or              */
/*   disclosure restricted by GSA ADP Schedule Contract with                  */
/*   IBM Corp.                                                                */
/*   </copyright>                                                             */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Function:                                                                  */
/*                                                                            */
/*   AMQSPPCA is a sample C program for MQ Publish/Subscribe, it              */
/*   demonstrates publishing using PCF via the MQAI interface.                */
/*                                                                            */
/*   Gets text from StdIn and creates a publication using the StringData tag  */
/*   (PCF MQCACF_STRING_DATA).                                                */
/*                                                                            */
/*   Publishes to the default stream.                                         */
/*                                                                            */
/*   Exits on a null input line.                                              */
/*                                                                            */
/* Program logic:                                                             */
/*                                                                            */
/*   MQCONN to default queue manager (or optionally supplied name)            */
/*   MQOPEN stream queue for output                                           */
/*   MQOPEN reply queue for any replies                                       */
/*   Create an MQAI bag for the publications                                  */
/*   Define the MQMD of the message                                           */
/*   While data is read in:                                                   */
/*     Add string data to the publication message                             */
/*     Publish the message - MQPUT to stream queue                            */
/*   End while                                                                */
/*   MQCLOSE stream queue                                                     */
/*   MQCLOSE reply queue                                                      */
/*   MQDISC from queue manager                                                */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* AMQSPPCA takes 3 input parameters:                                         */
/*                                                                            */
/*   1st - topic to publish to                                                */
/*   2nd - name of the queue to identify the publisher                        */
/*   3rd - queue manager name (optional)                                      */
/*                                                                            */
/* The Topic string is limited to 256 characters and must not contain blanks  */
/* or double quotes ('"').                                                    */
/*                                                                            */
/* The publication text is limited to 100 characters                          */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/* Includes                                                                   */
/******************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <cmqc.h>                                /* MQI                       */
#include <cmqcfc.h>                              /* MQ PCF                    */
#include <cmqbc.h>                               /* MQAI                      */

/******************************************************************************/
/* Defines                                                                    */
/******************************************************************************/
#ifndef OK
  #define OK                 0                   /* define OK as zero         */
#endif
#ifndef FALSE
   #define FALSE             0
#endif
#ifndef TRUE
   #define TRUE              (!FALSE)
#endif
#define MAX_STRING_SIZE      100+1               /* maximum string length     */
#define MAX_TOPIC_LENGTH     256+1               /* maximum topic length      */
#define REPLY_WAIT_TIME      10000               /* reply wait time (10s)     */
#define DEFAULT_STREAM       "SYSTEM.BROKER.DEFAULT.STREAM"

/******************************************************************************/
/*                                                                            */
/*  Macro : ccCheck                                                           */
/*                                                                            */
/*  Checks the comp code, if not MQCC_OK prints a message and sets the return */
/*  code                                                                      */
/*                                                                            */
/******************************************************************************/
#define ccCheck(func, compCode, reason)                                        \
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
/* Function Prototypes                                                        */
/******************************************************************************/
/* checkResponse */
MQLONG checkResponse(MQHCONN  hConn,
                     MQHOBJ  hReplyObj,
                     MQBYTE24 correlId,
                     PMQLONG  pCompCode,
                     PMQLONG  pReason);

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
   /*  Declare file and character for sample input                   */
   FILE *fp;

   MQHCONN hCon;                                 /* connection handle         */
   MQHBAG  hBag;                                 /* MQAI bag handle           */
   MQOD    strQOD={MQOD_DEFAULT};                /* stream queue object desc  */
   MQOD    repQOD={MQOD_DEFAULT};                /* reply queue object desc   */
   MQHOBJ  hStrQObj;                             /* handle to stream Q object */
   MQHOBJ  hRepQObj;                             /* handle to reply Q object  */
   MQLONG  reason;                               /* reason code               */
   MQINT32 connReason;                           /* MQCONN reason code        */
   MQINT32 compCode;                             /* completion code           */
   MQINT32 openSQCompCode=MQCC_FAILED;           /* MQOPEN stream queue CC    */
   MQINT32 openRQCompCode=MQCC_FAILED;           /* MQOPEN reply queue CC     */
   MQPMO   pmo={MQPMO_DEFAULT};                  /* Put message options       */
   MQMD    md={MQMD_DEFAULT};                    /* message descriptor        */
   MQLONG  options;                              /* MQOPEN options            */
   MQCHAR  replyQName[MQ_Q_NAME_LENGTH+1];       /* Publisher's reply queue   */
   MQCHAR  Topic[MAX_TOPIC_LENGTH];              /* topic                     */
   MQLONG  buflen;                               /* buffer length             */
   MQCHAR  buffer[MAX_STRING_SIZE];              /* message buffer            */
   MQCHAR  QMName[MQ_Q_MGR_NAME_LENGTH+1] = "";  /* queue manager name        */
   MQBOOL  keepLooping=TRUE;                     /* keep looping              */

   /******************************************************************/
   /*                                                                */
   /*   Check arguments                                              */
   /*                                                                */
   /******************************************************************/
   if (argc < 3)
   {
     printf("Usage: pubsamp topic PublisherQ <QManager>\n");
     exit(0);
   }
   else
   {
     strncpy(Topic, argv[1], 256);
     strncpy(replyQName, argv[2], MQ_Q_NAME_LENGTH);
   }

   if (argc > 3)
     strncpy(QMName, argv[3], MQ_Q_MGR_NAME_LENGTH);

   /***************************************************************************/
   /* Connect to queue manager                                                */
   /***************************************************************************/
   if (rc == OK)
   {
      MQCONN(QMName, &hCon, &compCode, &connReason);
      ccCheck("MQCONN", compCode, connReason);
      if (compCode == MQCC_FAILED)
         exit(connReason);                       /* no point in going on      */
   } /* endif */

   /***************************************************************************/
   /* Open the stream queue for output - publications put to this queue       */
   /***************************************************************************/
   if (rc == OK)
   {
      strcpy(strQOD.ObjectName, DEFAULT_STREAM);
      options = MQOO_OUTPUT + MQOO_FAIL_IF_QUIESCING;
      MQOPEN(hCon, &strQOD, options, &hStrQObj, &openSQCompCode, &reason);
      ccCheck("MQOPEN", openSQCompCode, reason);
   } /* endif */

   /***************************************************************************/
   /* Open the reply queue for input - replies got from this queue            */
   /***************************************************************************/
   if (rc == OK)
   {
      strcpy(repQOD.ObjectName, replyQName);
      options = MQOO_INPUT_SHARED + MQOO_FAIL_IF_QUIESCING;
      MQOPEN(hCon, &repQOD, options, &hRepQObj, &openRQCompCode, &reason);
      ccCheck("MQOPEN", openRQCompCode, reason);
   } /* endif */

   if (rc == OK)
   {
     /**************************************************************/
     /*                                                            */
     /*   Create an MQAI bag for the publications                  */
     /*                                                            */
     /**************************************************************/
     mqCreateBag( MQCBO_COMMAND_BAG, &hBag, &compCode, &reason );
     ccCheck("mqCreateBag", compCode, reason);

     if (rc == OK)
     {

       /* Generate a publish PCF message */
       mqSetInteger(hBag,MQIASY_COMMAND, MQIND_NONE,
              MQCMD_PUBLISH,
              &compCode, &reason);
       ccCheck("mqSetInteger", compCode, reason);

       if (rc == OK)
       {
         /* Set the topic to be published on */
         mqAddString(hBag, MQCACF_TOPIC, MQBL_NULL_TERMINATED,
                Topic,&compCode,&reason);
         ccCheck("mqAddString", compCode, reason);
       }

       if (rc == OK)
       {
         /* Set the QName of the publisher */
         mqAddString(hBag, MQCA_Q_NAME, MQBL_NULL_TERMINATED,
                replyQName,&compCode,&reason);
         ccCheck("mqAddString", compCode, reason);
       }

       if (rc == OK)
       {
         /* Do not register this publisher on the broker */
         mqAddInteger(hBag,MQIACF_PUBLICATION_OPTIONS,
                MQPUBO_NO_REGISTRATION,
                &compCode, &reason);
         ccCheck("mqAddInteger", compCode, reason);
       }

       if (rc == OK)
       {
         /* Add a default string data entry to the bag to set later*/
         strcpy(buffer, "default");
         mqAddString(hBag, MQCACF_STRING_DATA, MQBL_NULL_TERMINATED,
                buffer,&compCode,&reason);
         ccCheck("mqAddString", compCode, reason);
       }

       if (rc == OK)
       {

         /**********************************************************/
         /*                                                        */
         /*   Read lines from the file, put then into the bag and  */
         /*   send them to the broker's default stream. Loop until */
         /*   null line or end of file, or there is a failure.     */
         /*                                                        */
         /**********************************************************/
         printf("Enter text for publications on topic '%s':\n",
                   Topic);
         fp = stdin;

         memcpy(md.Format, MQFMT_PCF, (size_t) MQ_FORMAT_LENGTH);

         /*********************************************************************/
         /* Set the MD message type to request so we get a response           */
         /*********************************************************************/
         md.MsgType = MQMT_REQUEST;

         /*********************************************************************/
         /* Set the reply to queue name for the response message we           */
         /* requested.                                                        */
         /*********************************************************************/
         strcpy(md.ReplyToQ, replyQName);

         pmo.Options |= MQPMO_NEW_MSG_ID
                      | MQPMO_NO_SYNCPOINT;

         while (rc == OK && keepLooping)
         {
           if (fgets(buffer, sizeof(buffer), fp) != NULL)
           {
             buflen = (MQLONG)strlen(buffer);      /* length without null  */
             if (buffer[buflen-1] == '\n') /* last char is new-line*/
             {
               buffer[buflen-1]  = '\0'; /* change new-line to null*/
               --buflen;                   /* reduce buffer length */
             }
           }
           else
             buflen = 0;        /* treat EOF same as null line  */

           /********************************************************/
           /*                                                      */
           /*   Put each buffer to the broker control queue        */
           /*                                                      */
           /********************************************************/
           if (buflen > 0)
           {

             /* Replace the default string with the new message */
             mqSetString(hBag, MQCACF_STRING_DATA, MQIND_NONE,
                       buflen, buffer,&compCode,&reason);
             ccCheck("mqSetString", compCode, reason);

             if (rc == OK)
             {

               /* Put the bag to the broker's default stream */
               mqPutBag(hCon,              /* Connect handle       */
                      hStrQObj,            /* object handle        */
                      &md,                 /* message descriptor   */
                      &pmo,                /* put message options  */
                      hBag,                /* MQAI publish bag     */
                      &compCode,           /* completion code      */
                      &reason);            /* reason code          */
               ccCheck("mqPutBag", compCode, reason);

               if (rc == OK)
               {
                 /****************************************************************/
                 /*                                                              */
                 /*   Check the reply sent from the broker                       */
                 /*                                                              */
                 /****************************************************************/
                 rc = checkResponse(hCon, hRepQObj, md.MsgId, &compCode, &reason);
                 ccCheck("checkResponse", compCode, reason);
               }
             }
           }
           else
           {
             keepLooping = FALSE;                /* stop looping              */
           } /* endif */
         } /* While */
       }

       /* Delete the publish bag */
       mqDeleteBag(&hBag, &compCode, &reason);
       ccCheck("mqDeleteBag", compCode, reason);
     }
   }

   /***************************************************************************/
   /* Close the reply queue if opened                                         */
   /***************************************************************************/
   if (openRQCompCode != MQCC_FAILED)
   {
      MQCLOSE(hCon, &hRepQObj, MQCO_NONE, &compCode, &reason);
      ccCheck("MQCLOSE", compCode, reason);
   } /* endif */

   /***************************************************************************/
   /* Close the stream queue if opened                                        */
   /***************************************************************************/
   if (openSQCompCode != MQCC_FAILED)
   {
      MQCLOSE(hCon, &hStrQObj, MQCO_NONE, &compCode, &reason);
      ccCheck("MQCLOSE", compCode, reason);
   } /* endif */

   /***************************************************************************/
   /* Disconnect from queue manager if not already connected                  */
   /***************************************************************************/
   if (connReason != MQRC_ALREADY_CONNECTED)
   {
      MQDISC(&hCon, &compCode, &reason);
      ccCheck("MQDISC", compCode, reason);
   } /* endif */

   return rc;

} /* end of main */

/********************************************************************/
/*                                                                  */
/*  Function : checkResponse                                        */
/*                                                                  */
/*  Receive all responses sent from the broker and check for        */
/*  errors contained in the messages                                */
/*                                                                  */
/********************************************************************/
MQLONG checkResponse( MQHCONN  hConn,     /* connection handle      */
                      MQHOBJ   hReplyObj, /* reply object handle    */
                      MQBYTE24 correlId,  /* correl id              */
                      PMQLONG  pCompCode, /* completion code        */
                      PMQLONG  pReason)   /* reason code            */
{
  int      rc=OK;                         /* return code            */
  MQMD     md = { MQMD_DEFAULT };         /* message descriptor     */
  MQGMO    gmo = { MQGMO_DEFAULT };       /* get mesage options     */
  MQHBAG   replyBag = MQHB_UNUSABLE_HBAG; /* reply bag handle       */
  MQLONG   Value;                         /* bag value              */
  int      i;                             /* counter                */
  MQLONG   Items;                         /* number of bag items    */
  MQLONG   OutSelector;                   /* selector               */
  MQLONG   OutType;                       /* selector type          */
  MQLONG   responseCode = MQCC_FAILED;    /* Failed if no response  */
                                          /* is received            */

  /* Create the reply bag into which we will receive the replys */
  mqCreateBag( MQCBO_COMMAND_BAG, &replyBag, pCompCode, pReason );
  ccCheck("mqCreateBag", *pCompCode, *pReason);

  /******************************************************************/
  /* loop round receiving replies until the last one is received    */
  /* or we time out waiting.                                        */
  /******************************************************************/
  if (rc == OK)
  {
    gmo.Options = MQGMO_WAIT + MQGMO_CONVERT + MQGMO_NO_SYNCPOINT;
    gmo.WaitInterval = REPLY_WAIT_TIME;
    /* match on the correl id */
    gmo.MatchOptions = MQMO_MATCH_CORREL_ID;

    while (rc == OK)
    {
      memcpy( md.MsgId, MQMI_NONE, MQ_MSG_ID_LENGTH );
      memcpy( md.CorrelId, correlId, MQ_MSG_ID_LENGTH );

      /**************************************************************/
      /* Get the reply message into the reply bag.                  */
      /**************************************************************/
       mqGetBag( hConn, hReplyObj, &md, &gmo, replyBag,
                 pCompCode, pReason );
       ccCheck("mqGetBag", *pCompCode, *pReason);

      /**************************************************************/
      /* Print out the compcode and reason and any user             */
      /* selectors.                                                 */
      /**************************************************************/
      if (rc == OK)
      {
        /* extract the completion code from the bag */
        mqInquireInteger( replyBag, MQIASY_COMP_CODE, MQIND_NONE,
                                &Value, pCompCode, pReason );
        ccCheck("mqInquireInteger", *pCompCode, *pReason);

        if (rc == OK)
        {
          if (Value != MQCC_OK)
          {
            printf("Completion Code: %d\n",Value);
          }
          /* Set the broker's response code */
          responseCode = Value;
        }
      }

      if (rc == OK)
      {
        /* extract the reason code from the bag */
        mqInquireInteger( replyBag, MQIASY_REASON, MQIND_NONE,
                             &Value, pCompCode, pReason );
        ccCheck("mqInquireInteger", *pCompCode, *pReason);

        if (rc == OK)
        {
          if (Value != MQRC_NONE)
          {
            printf("Reason Code: %d\n",Value);
          }
        }
      }

      /**************************************************************/
      /*   Now print out any user selectors containing diagnostics  */
      /**************************************************************/
      if (rc == OK)
      {
        /* count the number of user selectors in the bag */
        mqCountItems( replyBag, MQSEL_ANY_USER_SELECTOR,
                                    &Items, pCompCode, pReason );
        ccCheck("mqCountItems", *pCompCode, *pReason);

        if (rc == OK)
        {
          /* step through each user selector */
          for ( i=0; i<Items; i++ )
          {
            /* find out what selector we are looking at */
            mqInquireItemInfo( replyBag, MQSEL_ANY_USER_SELECTOR,
                                 i, &OutSelector,  &OutType,
                                    pCompCode, pReason );
            ccCheck("mqInquireItemInfo", *pCompCode, *pReason);

            if (rc != OK)
              break;

            /* find the value of this selector */
            mqInquireInteger( replyBag, MQSEL_ANY_USER_SELECTOR,
                                 i, &Value, pCompCode, pReason );
            ccCheck("mqInquireInteger", *pCompCode, *pReason);

            if (rc != OK)
              break;

            /* print out the data */
            printf("Parameter: %d, Value: %d\n",OutSelector,Value);

          } /* end for */
        }
      }

      /**********************************************************/
      /* Exit the reply loop if the last reply message was      */
      /* received.                                              */
      /**********************************************************/
      if (rc == OK)
      {
        /* see if the MQCFC_LAST flag is set */
        mqInquireInteger( replyBag, MQIASY_CONTROL, MQIND_NONE,
                              &Value, pCompCode, pReason );
        ccCheck("mqInquireInteger", *pCompCode, *pReason);

        if (rc == OK)
        {
          if ( Value == MQCFC_LAST )
            break;
        }
      }
    } /* end while */
  }

  /****************************************************************/
  /* Delete the reply bag if we managed to create one.            */
  /****************************************************************/
  if ( replyBag != MQHB_UNUSABLE_HBAG )
  {
    mqDeleteBag( &replyBag, pCompCode, pReason );
    ccCheck("mqDeleteBag", *pCompCode, *pReason);
  }

  return responseCode;

} /* end of checkResponse */

/*********************************************************************/
/*                                                                   */
/* end of pubsamp.c                                                  */
/*                                                                   */
/*********************************************************************/
