static const char sccsid[] = "%Z% %W% %I% %E% %U%";
/******************************************************************************/
/*                                                                            */
/* Module name: AMQSPR1A.C                                                    */
/*                                                                            */
/* Description: Sample C program that publishes to the default stream using   */
/*              RFH(1) format messages on a given topic.                      */
/*                                                                            */
/*  <copyright                                                                */
/*  notice="lm-source-program"                                                */
/*  pids="5724-H72"                                                           */
/*  years="1998,2016"                                                         */
/*  crc="1627760578" >                                                        */
/*  Licensed Materials - Property of IBM                                      */
/*                                                                            */
/*  5724-H72                                                                  */
/*                                                                            */
/*  (C) Copyright IBM Corp. 1998, 2016 All Rights Reserved.                   */
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
/*   AMQSPR1A is a sample C program for MQ Publish/Subscribe, it              */
/*   demonstrates publishing using RFH(1) messages.                           */
/*                                                                            */
/*   Gets text from StdIn and creates a publication using the user data area  */
/*   following the RFH(1) header.                                             */
/*                                                                            */
/*   Publishes to the default stream checking the reply messages on the reply */
/*   queue.                                                                   */
/*                                                                            */
/*   Exits on a null input line.                                              */
/*                                                                            */
/* Program logic:                                                             */
/*                                                                            */
/*   MQCONN to default queue manager (or optionally supplied name)            */
/*   MQOPEN stream queue for output                                           */
/*   MQOPEN reply queue for reply messages                                    */
/*   Allocate message block for publications                                  */
/*   Define the MQRFH and NameValueString for the publications                */
/*   Define the MQMD of the message                                           */
/*   While data is read in:                                                   */
/*     Add string data to the publication message                             */
/*     Publish the message - MQPUT to stream queue                            */
/*     Check the reply messages - MQGET from reply queue                      */
/*   End while                                                                */
/*   MQCLOSE stream queue                                                     */
/*   MQCLOSE reply queue                                                      */
/*   MQDISC from queue manager                                                */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* AMQSPR1A takes 3 input parameters:                                         */
/*                                                                            */
/*   1st - topic to publish to                                                */
/*   2nd - name of the queue to identify the publisher and used for replies   */
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
#include <cmqpsc.h>                              /* MQI Publish/Subscribe     */

/******************************************************************************/
/* Defines                                                                    */
/******************************************************************************/
#define MAX_PUB_LENGTH       100+1               /* maximum publication len   */
#define MAX_TOPIC_LENGTH     256+1               /* maximum topic length      */
#define MAX_MSG_LENGTH       1024                /* maximum message length    */
#define REPLY_WAIT_TIME      10000               /* reply wait time (10s)     */
#define DEFAULT_STREAM       "SYSTEM.BROKER.DEFAULT.STREAM"

#ifndef FALSE
   #define FALSE             0
#endif
#ifndef TRUE
   #define TRUE              (!FALSE)
#endif

/*********************************************************************/
/* An MQRFH structure that contains the default values for all its   */
/* fields.                                                           */
/*********************************************************************/
MQRFH DefaultMQRFH = { MQRFH_DEFAULT };

/******************************************************************************/
/* Function Prototypes                                                        */
/******************************************************************************/
void CheckResponse( MQHCONN   hConn
                  , MQHOBJ    hObj
                  , PMQBYTE   pPublishMsgId
                  , PMQLONG   pCompCode
                  , PMQLONG   pReason );

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
  MQHCONN      hConn = MQHC_UNUSABLE_HCONN;
  MQHOBJ       hObjStream  = MQHO_UNUSABLE_HOBJ;
  MQHOBJ       hObjReply  = MQHO_UNUSABLE_HOBJ;
  MQLONG       Options;
  MQLONG       CompCode;
  MQLONG       Reason;
  MQLONG       ConnReason;
  MQOD         od  = { MQOD_DEFAULT };
  MQPMO        pmo = { MQPMO_DEFAULT };
  MQMD         md  = { MQMD_DEFAULT };

  char         ReplyQName[MQ_Q_NAME_LENGTH+1];
  char         Topic[MAX_TOPIC_LENGTH];
  char         QMName[MQ_Q_MGR_NAME_LENGTH+1] = "";

  FILE        *fp;
  char         Buffer[MAX_PUB_LENGTH];
  size_t       buflen;
  PMQBYTE      pMessageBlock = NULL;
  MQLONG       messageLength;
  PMQRFH       pMQRFHeader;
  char        *pNameValueString;
  char        *pUserData;
  MQBOOL       keepLooping=TRUE;

  fp = stdin;

  /*******************************************************************/
  /* Check the arguments supplied.                                   */
  /*******************************************************************/
  if( (argc < 3)
    ||(argc > 4)
    ||(strlen(argv[1]) > (MAX_TOPIC_LENGTH - 1)) )
  {
    printf("Usage: %s Topic ReplyQName <QManager>\n", argv[0]);
    exit(99);
  }
  else
  {
    strcpy(Topic, argv[1]);
    strncpy(ReplyQName, argv[2], MQ_Q_NAME_LENGTH);
  }

  /*******************************************************************/
  /* If no queue manager name was given as an argument, connect to   */
  /* the default queue manager (if one exists). Otherwise connect    */
  /* to the one specified.                                           */
  /*******************************************************************/
  if (argc > 3)
    strncpy(QMName, argv[3], MQ_Q_MGR_NAME_LENGTH);

  /*******************************************************************/
  /* Connect to the queue manager.                                   */
  /*******************************************************************/
  MQCONN( QMName
        , &hConn
        , &CompCode
        , &ConnReason );
  if( CompCode == MQCC_FAILED )
  {
    printf("MQCONN failed with CompCode %d and Reason %d\n",
            CompCode, ConnReason);
  }
  /*******************************************************************/
  /* If the queue manager was already connected we can ignore the    */
  /* warning for now and continue.                                   */
  /*******************************************************************/
  else if( ConnReason == MQRC_ALREADY_CONNECTED )
  {
    CompCode = MQCC_OK;
  }

  /*******************************************************************/
  /* Open the Broker's Stream queue for publications                 */
  /*******************************************************************/
  if( CompCode == MQCC_OK )
  {
    /*****************************************************************/
    /* Step 1.                                                       */
    /*****************************************************************/
    /* Open The broker's default stream queue for putting messages   */
    /* to.                                                           */
    /*****************************************************************/
    strncpy(od.ObjectName, DEFAULT_STREAM, MQ_Q_NAME_LENGTH);
    Options = MQOO_OUTPUT + MQOO_FAIL_IF_QUIESCING;
    /*****************************************************************/
    /* End of Step 1, goto Step 2.                                   */
    /*****************************************************************/

    MQOPEN( hConn
          , &od
          , Options
          , &hObjStream
          , &CompCode
          , &Reason );
    if( CompCode != MQCC_OK )
    {
      printf("MQOPEN failed to open \"%s\"\nwith CompCode %d and Reason %d\n",
             od.ObjectName, CompCode, Reason);
    }
  }

  /*******************************************************************/
  /* Open the reply queue for replies from the broker.               */
  /*******************************************************************/
  if( CompCode == MQCC_OK )
  {
    strncpy(od.ObjectName, ReplyQName, MQ_Q_NAME_LENGTH);
    Options = MQOO_INPUT_SHARED + MQOO_FAIL_IF_QUIESCING;
    MQOPEN( hConn
          , &od
          , Options
          , &hObjReply
          , &CompCode
          , &Reason );
    if( CompCode != MQCC_OK )
    {
      printf("MQOPEN failed to open \"%s\"\nwith CompCode %d and Reason %d\n",
             od.ObjectName, CompCode, Reason);
    }
  }

  /*******************************************************************/
  /* Now that we have connected to the queue manager and open the    */
  /* necessary queues we can generate publication messages to send   */
  /* to the MQSeries Publish/Subscribe broker containing character   */
  /* strings as user data.                                           */
  /*******************************************************************/

  if( CompCode == MQCC_OK )
  {
    /*****************************************************************/
    /* As the only difference between the publications we send in    */
    /* this sample is the contents of the user data that follows the */
    /* MQRFH and NameValueString of the message it is possible to    */
    /* define the contents of the MQRFH and NameValueString before   */
    /* we have the user data, we can also re-use the same MQRFH and  */
    /* NameValueString for all publications.                         */
    /*****************************************************************/

    /*****************************************************************/
    /* Allocate a storage block for the publications to be built in  */
    /* this will hold the MQRFH followed by the NameValueString then */
    /* the user data, which will be a character string for this      */
    /* example. Each of these are separate data types but they must  */
    /* be in one continuous block of memory to be put as an MQSeries */
    /* message, so we define the storage block large enough to hold  */
    /* the MQRFH, NameValueString and the user data (MAX_MSG_LENGTH).*/
    /* pMessageBlock is a pointer to the start of the message block. */
    /*****************************************************************/
    messageLength = MAX_MSG_LENGTH;
    pMessageBlock = (PMQBYTE)malloc(messageLength);
    if( pMessageBlock == NULL )
    {
      printf("Unable to allocate storage\n");
    }
    else
    {
      /***************************************************************/
      /* Clear the buffer before we start (initialise to nulls).     */
      /* This is not a requirement, it is just being tidy.           */
      /***************************************************************/
      memset(pMessageBlock, 0, messageLength);

      /***************************************************************/
      /* Step 2.                                                     */
      /***************************************************************/
      /* Define the MQRFH at the start of the storage block, assign  */
      /* all the default values to the fields in the MQRFH. Some     */
      /* values will need to be changed later for the message to be  */
      /* processed correctly.                                        */
      /* TIP: Copy the defined structure DefaultMQRFH (which         */
      /*      contains the default settings) into the message block. */
      /***************************************************************/
      pMQRFHeader = (PMQRFH)pMessageBlock;
      memcpy(pMQRFHeader, &DefaultMQRFH, sizeof(MQRFH));
      /***************************************************************/
      /* End of Step 2, goto Step 3.                                 */
      /***************************************************************/

      /***************************************************************/
      /* Step 3.                                                     */
      /***************************************************************/
      /* Start the NameValueString directly after the MQRFH          */
      /* structure.                                                  */
      /***************************************************************/
      pNameValueString = (char *)(pMessageBlock + sizeof(MQRFH));
      /***************************************************************/
      /* End of Step 3, goto Step 4.                                 */
      /***************************************************************/

      /***************************************************************/
      /* Step 4.                                                     */
      /***************************************************************/
      /* Add the name/value pairs to the NameValueString.            */
      /* TIP: For a publish message the NameValueString must         */
      /*      contain the type of command that it is, and the        */
      /*      topic that it is on. Other name/value pairs are valid  */
      /*      if required (do we need to register the publisher?).   */
      /*      See the Publish/Subscribe manual for details of the    */
      /*      constants that can be used.                            */
      /***************************************************************/
      strcpy(pNameValueString, MQPS_COMMAND_B);
      strcat(pNameValueString, MQPS_PUBLISH);
      strcat(pNameValueString, MQPS_PUBLICATION_OPTIONS_B);
      strcat(pNameValueString, MQPS_NO_REGISTRATION);
      strcat(pNameValueString, MQPS_TOPIC_B);
      strcat(pNameValueString, Topic);
      /***************************************************************/
      /* End of Step 4, goto Step 5.                                 */
      /***************************************************************/

      /***************************************************************/
      /* Step 5.                                                     */
      /***************************************************************/
      /* Define the StrucLength field in the MQRFH to be the size of */
      /* MQRFH and the length of the NameValueString.                */
      /***************************************************************/
      pMQRFHeader->StrucLength = (MQLONG)(sizeof(MQRFH) +
                                          strlen(pNameValueString) + 1);
      /***************************************************************/
      /* End of Step 5, goto Step 6.                                 */
      /***************************************************************/

      /***************************************************************/
      /* Now we need to align the end of the MQRFH and              */
      /* NameValueString structure on a 16 byte boundary. This is so */
      /* that the next structure that follows is always aligned on   */
      /* a word boundary, this allows data conversion to be          */
      /* performed if required.                                      */
      /***************************************************************/
      pMQRFHeader->StrucLength = ((((pMQRFHeader->StrucLength) + 15)/16)*16);

      /***************************************************************/
      /* Step 6.                                                     */
      /***************************************************************/
      /* Define the necessary fields in the MQRFH to show that a     */
      /* character string follows the MQRFH and NameValueString      */
      /* structure.                                                  */
      /* TIP: MQCCSI_INHERIT can be used in the CodedCharSetId field */
      /*      to indicate that the following data is in the same     */
      /*      character set as the MQRFH. The Format of the user     */
      /*      data is also required.                                 */
      /***************************************************************/
      memcpy(pMQRFHeader->Format, MQFMT_STRING, MQ_FORMAT_LENGTH);
      pMQRFHeader->CodedCharSetId = MQCCSI_INHERIT;
      /***************************************************************/
      /* End of Step 6, goto Step 7.                                 */
      /***************************************************************/

      /***************************************************************/
      /* Step 7.                                                     */
      /***************************************************************/
      /* Start the user data buffer after the NameValueString.       */
      /* TIP: The StrucLength field in the MQRFH indicates how far   */
      /*      into the buffer the next structure starts.             */
      /***************************************************************/
      pUserData = (char *)(pMessageBlock + pMQRFHeader->StrucLength);
      /***************************************************************/
      /* End of Step 7, goto Step 8.                                 */
      /***************************************************************/

      /***************************************************************/
      /* We set the MD for an MQRFH message. Send the message as a   */
      /* request message so that the broker always sends us a        */
      /* reply to the reply queue.                                   */
      /* We are only asking for replies on all publications so that  */
      /* the implementation is simple. The usual case would be to    */
      /* send publications as datagrams with the report option       */
      /* MQRO_NAN, which says 'only send a reply if there is an      */
      /* error'. Therefore, we would only receive replies from the   */
      /* broker if the publication we sent was incorrect.            */
      /***************************************************************/
      memcpy(md.Format, MQFMT_RF_HEADER, MQ_FORMAT_LENGTH);
      md.MsgType = MQMT_REQUEST;
      strcpy( md.ReplyToQ, ReplyQName);
      pmo.Options |= MQPMO_NEW_MSG_ID;
      pmo.Options |= MQPMO_NO_SYNCPOINT;

      /***************************************************************/
      /* At this point we should have a completely defined MQRFH and */
      /* NameValueString, we can now start to read in the user data  */
      /* (character strings) and add them to the message block       */
      /* following the NameValueString. The message can then be      */
      /* published to the broker. As we do not need to change the    */
      /* contents of the MQRFH or NameValueString for different      */
      /* character strings read in we re-use the MQRFH and           */
      /* NameValueString in the message block and simply overwrite   */
      /* the previous user data string with the new one.             */
      /***************************************************************/

      /***************************************************************/
      /* Read lines from the file, put then into a temporary buffer  */
      /* copy this buffer into the message and then send the         */
      /* message to the broker's stream. Loop until a null line or   */
      /* end of file is read, or there is a failure.                 */
      /***************************************************************/
      while (CompCode != MQCC_FAILED && keepLooping)
      {
        printf("Enter text for publication on topic '%s':\n", Topic);

        /*************************************************************/
        /* If a null line has not been entered process the data read */
        /* in.                                                       */
        /*************************************************************/
        if (fgets(Buffer, MAX_PUB_LENGTH, fp) != NULL)
        {
          buflen = strlen(Buffer);           /* length without null  */
          if (Buffer[buflen - 1] == '\n')     /*last char is new-line*/
          {
            Buffer[buflen - 1] = '\0';      /*change new-line to null*/
            --buflen;                        /* reduce buffer length */
          }
        }
        else
          buflen = 0;                /* treat EOF same as null line  */

        /*************************************************************/
        /* Put each message to the stream queue                      */
        /*************************************************************/
        if (buflen > 0)
        {
          /***********************************************************/
          /* Step 8.                                                 */
          /***********************************************************/
          /* Copy the character string buffer (Buffer[]) into the    */
          /* message block at the appropriate position.              */
          /* TIP : Remember to adjust the message length to include  */
          /*       the extra user data as well as the MQRFH and      */
          /*       NameValueString. To allow a subscribing           */
          /*       application to perform string operations on the   */
          /*       user data the null terminator should be included  */
          /*       in the length.                                    */
          /***********************************************************/
          strcpy(pUserData, Buffer);
          messageLength = (MQLONG)(pMQRFHeader->StrucLength +
                                   strlen(pUserData) + 1);
          /***********************************************************/
          /* End of Step 8.                                          */
          /***********************************************************/
          /* This is the end of the exercises in this sample, return */
          /* to the exercise sheet.                                  */
          /***********************************************************/

          /***********************************************************/
          /* Now that we have a complete publish message we can put  */
          /* it to the broker's stream queue.                        */
          /***********************************************************/
          MQPUT( hConn
               , hObjStream
               , &md
               , &pmo
               , messageLength
               , pMessageBlock
               , &CompCode
               , &Reason );

          if (CompCode != MQCC_OK)
          {
            printf("MQPUT failed with CompCode %d and Reason %d\n",
                  CompCode, Reason);
          }
          else
          {
            /*********************************************************/
            /* If the MQPUT succeeded check that an okay reply is    */
            /* returned from the broker.                             */
            /*********************************************************/
            CheckResponse( hConn
                         , hObjReply
                         , md.MsgId
                         , &CompCode
                         , &Reason );

            if( CompCode == MQCC_OK )
              printf(" Publication accepted.\n");
          }
        }
        /*************************************************************/
        /* No data was read in so break out of the while.            */
        /*************************************************************/
        else
          keepLooping = FALSE;

      } /* end of while */

      /***************************************************************/
      /* Free the message block that was allocated at the start.     */
      /***************************************************************/
      free( pMessageBlock );
    }
  }

  /*******************************************************************/
  /* MQCLOSE the stream queue used by this sample.                   */
  /*******************************************************************/
  if( hObjStream != MQHO_UNUSABLE_HOBJ )
  {
    MQCLOSE( hConn
           , &hObjStream
           , MQCO_NONE
           , &CompCode
           , &Reason );
    if( CompCode != MQCC_OK )
      printf("MQCLOSE failed with CompCode %d and Reason %d\n",
             CompCode, Reason);
  }

  /*******************************************************************/
  /* MQCLOSE the reply queue used by this sample.                    */
  /*******************************************************************/
  if( hObjReply != MQHO_UNUSABLE_HOBJ )
  {
    MQCLOSE( hConn
           , &hObjReply
           , MQCO_NONE
           , &CompCode
           , &Reason );
    if( CompCode != MQCC_OK )
      printf("MQCLOSE failed with CompCode %d and Reason %d\n",
             CompCode, Reason);
  }

  /*******************************************************************/
  /* Disconnect from the queue manager only if the connection        */
  /* worked and we were not already connected.                       */
  /*******************************************************************/
  if( (hConn != MQHC_UNUSABLE_HCONN)
    &&(ConnReason != MQRC_ALREADY_CONNECTED) )
  {
    MQDISC( &hConn
          , &CompCode
          , &Reason );
    if( CompCode != MQCC_OK )
      printf("MQDISC failed with CompCode %d and Reason %d\n",
              CompCode, Reason);
  }

  return(CompCode);
}
/*********************************************************************/
/* end of main                                                       */
/*********************************************************************/

/*********************************************************************/
/*                                                                   */
/* Function Name : CheckResponse                                     */
/*                                                                   */
/* Description   : Get the response that should have been sent from  */
/*                 the broker on receiving the publication, if one   */
/*                 is received check for an okay response, if one    */
/*                 is not returned it may be because the broker      */
/*                 is not running.                                   */
/*                                                                   */
/* Flow          :                                                   */
/*                                                                   */
/*    Define the MQGMO and MQMD for the MQGET                        */
/*    MQGET with wait from the reply queue for the response          */
/*     Check if the response is okay                                 */
/*      If it is not display the error message                       */
/*    Return the CompCode to the calling function                    */
/*                                                                   */
/* Input Parms   : MQHCONN  hConn                                    */
/*                  Queue Manager connection handle                  */
/*                 MQHOBJ   hObj                                     */
/*                  Object handle of queue                           */
/*                 PMQBYTE  pPublishMsgId                            */
/*                  Pointer to MsgId of published message            */
/*                                                                   */
/* Ouput Parms   : PMQLONG  pCompCode                                */
/*                  Pointer to completion code                       */
/*                 PMQLONG  pReason                                  */
/*                  Pointer to reason                                */
/*                                                                   */
/*********************************************************************/
void CheckResponse( MQHCONN   hConn
                  , MQHOBJ    hObj
                  , PMQBYTE   pPublishMsgId
                  , PMQLONG   pCompCode
                  , PMQLONG   pReason )
{
  MQGMO        gmo   = { MQGMO_DEFAULT };
  MQMD         md    = { MQMD_DEFAULT };
  PMQBYTE      pMessageBlock = NULL;
  MQLONG       messageLength;
  PMQRFH       pMQRFHeader;
  PMQCHAR      pNameValueString;
  PMQCHAR      pInputNameValueString;

  /*******************************************************************/
  /* Allocate a storage block for the publications to be received    */
  /* into.                                                           */
  /*******************************************************************/
  messageLength = MAX_MSG_LENGTH;
  pMessageBlock = (PMQBYTE)malloc(messageLength);
  if( pMessageBlock == NULL )
  {
    printf("Unable to allocate storage\n");
    *pCompCode = MQCC_FAILED;
  }
  else
  {
    /*****************************************************************/
    /* Wait for a response message to arrive on our reply queue, the */
    /* response's correlId will be the same as the messageId that    */
    /* the original message was sent with (returned in the md from   */
    /* the MQPUT) so match against this.                             */
    /*****************************************************************/
    memcpy( md.CorrelId, pPublishMsgId, sizeof(MQBYTE24));
    gmo.Version = MQGMO_VERSION_2;
    gmo.MatchOptions = MQMO_MATCH_CORREL_ID;
    gmo.Options = MQGMO_WAIT + MQGMO_CONVERT + MQGMO_NO_SYNCPOINT;
    gmo.WaitInterval = REPLY_WAIT_TIME;

    MQGET( hConn
         , hObj
         , &md
         , &gmo
         , messageLength
         , pMessageBlock
         , &messageLength
         , pCompCode
         , pReason );

    if( *pCompCode != MQCC_OK )
    {
      printf("MQGET failed with CompCode %d and Reason %d\n",
                                                 *pCompCode, *pReason);
      if( *pReason == MQRC_NO_MSG_AVAILABLE )
        printf("No response was sent by the broker, check the broker is running.\n");
    }
    else
    {
      /***************************************************************/
      /* Check that the message is in the MQRFH format.              */
      /***************************************************************/
      if( memcmp(md.Format, MQFMT_RF_HEADER, MQ_FORMAT_LENGTH) == 0 )
      {
        /*************************************************************/
        /* Locate the start of the NameValueString.                  */
        /*************************************************************/
        pMQRFHeader = (PMQRFH)pMessageBlock;
        pNameValueString = (PMQCHAR)(pMessageBlock
                                            + MQRFH_STRUC_LENGTH_FIXED);

        /*************************************************************/
        /* The start of a response NameValueString is always in the  */
        /* same format:                                              */
        /*   MQPSCompCode x MQPSReason y MQPSReasonText string ...   */
        /* We can scan the start of the string to check the CompCode */
        /* and reason of the reply. The NameValueString of a response*/
        /* is always null terminated so we can use string functions. */
        /*************************************************************/
        sscanf(pNameValueString, "MQPSCompCode %d MQPSReason %d ",
                                                   pCompCode, pReason);
        if( *pCompCode != MQCC_OK )
        {
          /***********************************************************/
          /* A non-okay response was received, display the error     */
          /* message supplied along with the user data that was      */
          /* returned (if any), this will be the original command's  */
          /* NameValueString.                                        */
          /***********************************************************/
          printf("ERROR response returned :\n");
          printf(" %s\n",pNameValueString);
          /***********************************************************/
          /* Display the user data if supplied.                      */
          /***********************************************************/
          if( messageLength != pMQRFHeader->StrucLength )
          {
            printf("Original Command String:\n");
            pInputNameValueString =
                   (PMQCHAR)(pMessageBlock + pMQRFHeader->StrucLength);
            /*********************************************************/
            /* We know that our original NameValueString was null    */
            /* terminated so we can simply printf the string.        */
            /*********************************************************/
            printf(" %s\n",pInputNameValueString);
          }
        }
      }
      /***************************************************************/
      /* If the message is not in the MQRFH format we have the wrong */
      /* message (this should never happen).                         */
      /***************************************************************/
      else
      {
        printf("Unexpected message format: %.8s\n", md.Format );
        *pCompCode = MQCC_FAILED;
      }
    }
    /*****************************************************************/
    /* Free the message block.                                       */
    /*****************************************************************/
    free(pMessageBlock);
  }
}
/*********************************************************************/
/* end of CheckResponse                                              */
/*********************************************************************/

/*********************************************************************/
/*                                                                   */
/* end of amqspuba.c                                                 */
/*                                                                   */
/*********************************************************************/
