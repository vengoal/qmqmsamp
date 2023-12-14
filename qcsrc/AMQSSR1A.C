static const char sccsid[] = "%Z% %W% %I% %E% %U%";
/******************************************************************************/
/*                                                                            */
/* Module name: AMQSSR1A.C                                                    */
/*                                                                            */
/* Description: Sample C program that subscribes using RFH(1) format messages */
/*              to the default stream and displays any arriving publications. */
/*              The subscriber is deregistered on exit.                       */
/*                                                                            */
/*  <copyright                                                                */
/*  notice="lm-source-program"                                                */
/*  pids="5724-H72"                                                           */
/*  years="1998,2016"                                                         */
/*  crc="228887054" >                                                         */
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
/*   AMQSSR1A is a sample C program for MQ Publish/Subscribe, it              */
/*   demonstrates subscribing using RFH(1) messages.                          */
/*                                                                            */
/*   A subscription message for the supplied topic, is put to MQ's            */
/*   Publish/Subscribe control queue (SYSTEM.BROKER.CONTROL.QUEUE) nominating */
/*   the default stream (SYSTEM.BROKER.DEFAULT.STREAM) and the supplied       */
/*   subscriber queue, which is then checked for the reply messages returned. */
/*                                                                            */
/*   Any publications forwarded to the supplied subscriber queue are          */
/*   removed from the queue and displayed. The program assumes each           */
/*   publication contains text in the user data area following the RFH(1)     */
/*   header.                                                                  */
/*                                                                            */
/*   After 60 seconds of inactivity on the subscriber's queue the             */
/*   subscriber is deregistered and the sample exits.                         */
/*                                                                            */
/* Program logic:                                                             */
/*                                                                            */
/*   MQCONN to default queue manager (or optionally supplied name)            */
/*   MQOPEN control queue for output                                          */
/*   MQOPEN subscriber queue for any publications or responses                */
/*   Register the subscriber - MQPUT to control queue                         */
/*   Check the reply messages - MQGET from subscriber queue                   */
/*   Wait for publications                                                    */
/*   Get publication and display data - MQGET from subscriber queue           */
/*   Deregister the subscriber - MQPUT to control queue                       */
/*   MQCLOSE subscriber queue                                                 */
/*   MQCLOSE control queue                                                    */
/*   MQDISC from queue manager                                                */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* AMQSSR1A takes 3 input parameters:                                         */
/*                                                                            */
/*   1st - topic to subscribe to                                              */
/*   2nd - name of the subscriber queue                                       */
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
#define MAX_TOPIC_LENGTH     256+1               /* maximum topic length      */
#define MAX_MSG_LENGTH       1024+1              /* maximum message length    */
#define PUBLISH_WAIT_TIME    60000               /* publish wait time (60s)   */
#define REPLY_WAIT_TIME      10000               /* reply wait time (10s)     */
#define DEFAULT_STREAM       "SYSTEM.BROKER.DEFAULT.STREAM"

/*********************************************************************/
/* An MQRFH structure and MQMD that contains the default values for  */
/* all their fields.                                                 */
/*********************************************************************/
MQRFH DefMQRFH = { MQRFH_DEFAULT };
MQMD DefMQMD   = { MQMD_DEFAULT };

/******************************************************************************/
/* Function Prototypes                                                        */
/******************************************************************************/
void SendBrokerCommand( MQHCONN  hConn
                      , MQHOBJ   hBrkObj
                      , MQHOBJ   hSubObj
                      , char     Command[]
                      , char    *pTopic
                      , char    *pQName
                      , PMQLONG  pCompCode
                      , PMQLONG  pReason );

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
  /* MQSeries variables */
  MQHCONN      hConn = MQHC_UNUSABLE_HCONN;
  MQHOBJ       hBrkObj = MQHO_UNUSABLE_HOBJ;
  MQHOBJ       hSubObj = MQHO_UNUSABLE_HOBJ;
  MQLONG       Options;
  MQLONG       CompCode;
  MQLONG       Reason;
  MQLONG       ConnReason;
  MQOD         od  = { MQOD_DEFAULT };
  MQGMO        gmo = { MQGMO_DEFAULT };
  MQMD         md  = { MQMD_DEFAULT };

  /* Supplied Arguments */
  char         SubscriberQ[MQ_Q_NAME_LENGTH+1];   /* Subscriber queue */
  char         Topic[MAX_TOPIC_LENGTH];           /* Topic            */
  char         QMName[MQ_Q_MGR_NAME_LENGTH+1] = ""; /* Queue Manager */

  /* Broker Message variables */
  char        *pUserData;
  PMQBYTE      pMessageBlock = NULL;
  MQLONG       messageLength;
  PMQRFH       pMQRFHeader;

  /*******************************************************************/
  /* Check the arguments supplied.                                   */
  /*******************************************************************/
  if( (argc < 3)
    ||(argc > 4)
    ||(strlen(argv[1]) > (MAX_TOPIC_LENGTH - 1)) )
  {
    printf("Usage: %s Topic SubscriberQ <QManager>\n", argv[0]);
    exit(99);
  }
  else
  {
    strcpy(Topic, argv[1]);
    strncpy(SubscriberQ, argv[2], MQ_Q_NAME_LENGTH);
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
  /* Open the Broker's control queue for registration                */
  /*******************************************************************/
  if( CompCode == MQCC_OK )
  {
    /*****************************************************************/
    /* Step 1.                                                       */
    /*****************************************************************/
    /* Open The broker's control queue for putting command messages  */
    /* to.                                                           */
    /*****************************************************************/
    strncpy(od.ObjectName, "SYSTEM.BROKER.CONTROL.QUEUE" , (size_t)MQ_Q_NAME_LENGTH);
    Options = MQOO_OUTPUT + MQOO_FAIL_IF_QUIESCING;
    /*****************************************************************/
    /* End of Step 1, goto Step 2.                                   */
    /*****************************************************************/
    MQOPEN( hConn
          , &od
          , Options
          , &hBrkObj
          , &CompCode
          , &Reason );
    if( CompCode != MQCC_OK )
    {
      printf("MQOPEN failed to open \"%s\"\nwith CompCode %d and Reason %d\n",
             od.ObjectName, CompCode, Reason);
    }
  }

  /*******************************************************************/
  /* Open the subscriber queue for publications from the broker.     */
  /*******************************************************************/
  if( CompCode == MQCC_OK )
  {
    strncpy(od.ObjectName, SubscriberQ, (size_t)MQ_Q_NAME_LENGTH);
    Options = MQOO_INPUT_EXCLUSIVE + MQOO_FAIL_IF_QUIESCING;
    MQOPEN( hConn
          , &od
          , Options
          , &hSubObj
          , &CompCode
          , &Reason );
    if( CompCode != MQCC_OK )
    {
      printf("MQOPEN failed to open \"%s\"\nwith CompCode %d and Reason %d\n",
             od.ObjectName, CompCode, Reason);
    }
  }

  /*******************************************************************/
  /* Register us as a subscriber to the topic.                       */
  /*******************************************************************/
  if( CompCode == MQCC_OK )
  {
    /*****************************************************************/
    /* The SendBrokerCommand function can be found later in this     */
    /* sample file, it takes all paramaters it requires to form a    */
    /* command message to send to the broker's control queue.        */
    /* The first command we need to send to the broker is a          */
    /* subscriber registration to tell the broker that we are        */
    /* interested in the supplied topic and wish all related         */
    /* publications to be sent to us.                                */
    /*****************************************************************/

    /*****************************************************************/
    /* Step 2.                                                       */
    /*****************************************************************/
    /* The fourth parameter of SendBrokerCommand is a character      */
    /* string of the broker command. Insert the defined string value */
    /* for a subscriber registration.                                */
    /* TIP: A list of PubSub definitions can be found in the         */
    /*      MQSeries Publish/Subscribe manual.                       */
    /*****************************************************************/
    SendBrokerCommand( hConn
                     , hBrkObj
                     , hSubObj
                     , MQPS_REGISTER_SUBSCRIBER
                     , Topic
                     , SubscriberQ
                     , &CompCode
                     , &Reason );
    /*****************************************************************/
    /* End of Step 2, goto Step 3.                                   */
    /*****************************************************************/
  }

  if( CompCode == MQCC_OK )
  {
    printf("Subscriber registered.\n");
    /*****************************************************************/
    /* Allocate a storage block for the messages to be received in.  */
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
      /* Get with wait on the subscriber's queue to receive any      */
      /* publications sent to the subscriber and display the user    */
      /* data contained. End the wait after 60 seconds of inactivity.*/
      /***************************************************************/
      /***************************************************************/
      /* In this sample we are not interested in message properties, */
      /* however we may receive some depending upon the publisher,   */
      /* therefore we need to ensure they are discarded on the MQGET */
      /* call, so that we receive the text string we are expecting.  */
      /* Sample amqsbcg0.c shows the use of message properties in an */
      /* application.                                                */
      /***************************************************************/
      gmo.Options = MQGMO_WAIT + MQGMO_CONVERT + MQGMO_NO_SYNCPOINT + MQGMO_NO_PROPERTIES;
      gmo.WaitInterval = PUBLISH_WAIT_TIME;

      printf("Browsing Subscriber queue...\n");

      /***************************************************************/
      /* While messages are still being received display them.       */
      /***************************************************************/
      while (CompCode != MQCC_FAILED)
      {
        /*************************************************************/
        /* Set the MQMD to its default settings before we get a      */
        /* message.                                                  */
        /*************************************************************/
        memcpy( &md, &DefMQMD, sizeof(MQMD));

        MQGET( hConn
             , hSubObj
             , &md
             , &gmo
             , MAX_MSG_LENGTH - 1
             , pMessageBlock
             , &messageLength
             , &CompCode
             , &Reason );

        if (CompCode == MQCC_OK)
        {
          /***********************************************************/
          /* Step 3.                                                 */
          /***********************************************************/
          /* We are only expecting messages in the MQRFH format,     */
          /* Check that the message returned from MQGET is an MQRFH  */
          /* message.                                                */
          /***********************************************************/
          if( memcmp(md.Format, MQFMT_RF_HEADER , MQ_FORMAT_LENGTH) == 0 )
          /***********************************************************/
          /* End of Step 3, goto Step 4.                             */
          /***********************************************************/
          {
            /*********************************************************/
            /* Step 4.                                               */
            /*********************************************************/
            /* The only part of the publication that we are          */
            /* interested in in this sample is the string data that  */
            /* has been added to the message after the MQRFH and     */
            /* NameValueString structures by the amqspub sample      */
            /* (Exercise 1). To be able to read this part of the     */
            /* publication message we must find where the string     */
            /* starts in the message, use the StrucLength field of   */
            /* the MQRFH at the start of the publication to find     */
            /* the position of the sting and print the string to     */
            /* the screen.                                           */
            /*********************************************************/
            pMQRFHeader = (PMQRFH)pMessageBlock;
            pUserData = (char *)(pMessageBlock + pMQRFHeader->StrucLength);
            if( pMQRFHeader->StrucLength < messageLength )
            {
              *(pMessageBlock + messageLength) = '\0'; /* add term   */
              printf("message <%s>\n", pUserData);
            }
            else
            {
              printf( "No user data found!\n");
            }
            /*********************************************************/
            /* End of Step 4, goto Step 5.                           */
            /*********************************************************/
          }
          else
          {
            printf("Unexpected message format: %.8s\n", md.Format );
            CompCode = MQCC_FAILED;
          }
        }
        /*************************************************************/
        /* The MQGET failed without timeing out. If the MQGET did    */
        /* time out (Reason = MQRC_NO_MSG_AVAILABLE) it means we     */
        /* have not received a publication for 60 seconds so we can  */
        /* assume the publisher sample(s) have finished and we can   */
        /* deregister the subscription.                              */
        /*************************************************************/
        else if (Reason != MQRC_NO_MSG_AVAILABLE)
          printf("MQGET failed with reason=%d\n", Reason);
      } /* end of while */
      /***************************************************************/
      /* Free the message block.                                     */
      /***************************************************************/
      free( pMessageBlock );
    }

    /*****************************************************************/
    /* Step 5                                                        */
    /*****************************************************************/
    /* As in Step 2 the command parameter is missing from the call   */
    /* to SendBrokerCommand, at this point we wish to deregister     */
    /* our subscription from the topic,  Insert the defined string   */
    /* value for a subscriber deregistration.                        */
    /* TIP: A list of PubSub definitions can be found in the         */
    /*      MQSeries Publish/Subscribe manual.                       */
    /*****************************************************************/
    SendBrokerCommand( hConn
                     , hBrkObj
                     , hSubObj
                     , MQPS_DEREGISTER_SUBSCRIBER
                     , Topic
                     , SubscriberQ
                     , &CompCode
                     , &Reason );
    /*****************************************************************/
    /* End of Step 5.                                                */
    /*****************************************************************/
    /* This is the end of the exercises in this sample, return to    */
    /* the exercise sheet.                                           */
    /*****************************************************************/

    if( CompCode == MQCC_OK )
      printf("Subscriber deregistered.\n");
  }

  /*******************************************************************/
  /* MQCLOSE the control queue used by this sample.                  */
  /*******************************************************************/
  if( hBrkObj != MQHO_UNUSABLE_HOBJ )
  {
    MQCLOSE( hConn
           , &hBrkObj
           , MQCO_NONE
           , &CompCode
           , &Reason );
    if( CompCode != MQCC_OK )
      printf("MQCLOSE failed with CompCode %d and Reason %d\n",
             CompCode, Reason);
  }

  /*******************************************************************/
  /* MQCLOSE the subscriber queue used by this sample.               */
  /*******************************************************************/
  if( hSubObj != MQHO_UNUSABLE_HOBJ )
  {
    MQCLOSE( hConn
           , &hSubObj
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

  return(0);
}
/*********************************************************************/
/* end of main                                                       */
/*********************************************************************/

/*********************************************************************/
/*                                                                   */
/* Function Name : SendBrokerCommand                                 */
/*                                                                   */
/* Description   : Form an MQRFH command message and put it to the   */
/*                 broker's control queue.                           */
/*                                                                   */
/* Flow          :                                                   */
/*                                                                   */
/*    Allocate the message block                                     */
/*     Define the MQRFH at the start of the message                  */
/*     Form the NameValueString following the MQRFH                  */
/*     MQPUT the message block to the control queue                  */
/*      Check the response from the broker...                        */
/*                                                                   */
/* Input Parms   : MQHCONN  hConn                                    */
/*                  Queue Manager connection handle                  */
/*                 MQHOBJ   hBrkObj                                  */
/*                  Object handle of control queue                   */
/*                 MQHOBJ   hSubObj                                  */
/*                  Object handle of subscriber queue (responses)    */
/*                 char     Command[]                                */
/*                  MQPS command string                              */
/*                 char    *pTopic                                   */
/*                  Pointer to the topic string                      */
/*                 char    *pQName                                   */
/*                  Pointer to the name of the subscriber queue      */
/*                                                                   */
/* Ouput Parms   : PMQLONG  pCompCode                                */
/*                  Pointer to completion code                       */
/*                 PMQLONG  pReason                                  */
/*                  Pointer to reason                                */
/*                                                                   */
/*********************************************************************/
void SendBrokerCommand( MQHCONN  hConn
                      , MQHOBJ   hBrkObj
                      , MQHOBJ   hSubObj
                      , char     Command[]
                      , char    *pTopic
                      , char    *pQName
                      , PMQLONG  pCompCode
                      , PMQLONG  pReason )
{
 PMQBYTE pMessageBlock = NULL;
 MQLONG   messageLength;
 PMQRFH  pMQRFHeader;
 PMQCHAR  pNameValueString;
 MQLONG NameValueStringLength;
 MQMD  md = { MQMD_DEFAULT };
 MQPMO pmo = { MQPMO_DEFAULT };

  /*******************************************************************/
  /* Allocate a storage block for the command to be built in.        */
  /*******************************************************************/
  messageLength = MAX_MSG_LENGTH;
  pMessageBlock = (PMQBYTE)malloc(messageLength);
  if( pMessageBlock == NULL )
  {
    printf("Unable to allocate storage\n");
  }
  else
  {
    /*****************************************************************/
    /* Clear the buffer before we start (initialise to nulls).       */
    /*****************************************************************/
    memset(pMessageBlock, 0, MAX_MSG_LENGTH);

    /*****************************************************************/
    /* Define the MQRFH values.                                      */
    /*****************************************************************/
    pMQRFHeader = (PMQRFH)(pMessageBlock);
    memcpy( pMQRFHeader, &DefMQRFH, (size_t)MQRFH_STRUC_LENGTH_FIXED);

    /*****************************************************************/
    /* Start the NameValueString directly after the MQRFH            */
    /* structure and add the token pairs for the command.            */
    /*****************************************************************/
    pNameValueString = ((MQCHAR *)pMQRFHeader) + MQRFH_STRUC_LENGTH_FIXED;
    strcpy(pNameValueString, MQPS_COMMAND_B);
    strcat(pNameValueString, Command);
    strcat(pNameValueString, MQPS_STREAM_NAME_B);
    strcat(pNameValueString, DEFAULT_STREAM);
    strcat(pNameValueString, MQPS_Q_NAME_B);
    strcat(pNameValueString, pQName);
    strcat(pNameValueString, MQPS_TOPIC_B);
    strcat(pNameValueString, pTopic);

    /*****************************************************************/
    /* Include the null terminator character in the                  */
    /* NameValueString. This is not required by the broker but it    */
    /* does allow us to use string functions on it in the future.    */
    /*****************************************************************/
    NameValueStringLength = (MQLONG)(strlen(pNameValueString) + 1);

    /*****************************************************************/
    /* Calculate the end of the MQRFH and NameValueString            */
    /* structure, it is recommended that this is aligned to a        */
    /* sixteen byte boundary.                                        */
    /*****************************************************************/
    messageLength = MQRFH_STRUC_LENGTH_FIXED
                                 + ((NameValueStringLength+15)/16)*16;
    pMQRFHeader->StrucLength = messageLength;

    /*****************************************************************/
    /* Set the md for an MQRFH message. Send the message as a        */
    /* request message so that the broker always sends us a          */
    /* response to the reply queue.                                  */
    /*****************************************************************/
    memcpy(md.Format, MQFMT_RF_HEADER, MQ_FORMAT_LENGTH);
    md.MsgType = MQMT_REQUEST;
    strcpy( md.ReplyToQ, pQName);
    pmo.Options |= MQPMO_NEW_MSG_ID;
    pmo.Options |= MQPMO_NO_SYNCPOINT;

    MQPUT( hConn
         , hBrkObj
         , &md
         , &pmo
         , messageLength
         , pMessageBlock
         , pCompCode
         , pReason );

    if (*pCompCode != MQCC_OK)
    {
      printf("MQPUT failed with CompCode %d and Reason %d\n",
            *pCompCode, *pReason);
    }
    else
    {
      /***************************************************************/
      /* If the MQPUT succeeded check that an okay response is       */
      /* returned from the broker.                                   */
      /***************************************************************/
      CheckResponse( hConn
                   , hSubObj
                   , md.MsgId
                   , pCompCode
                   , pReason );
    }
    /*****************************************************************/
    /* Free the message block.                                       */
    /*****************************************************************/
    free( pMessageBlock );
  }
}
/*********************************************************************/
/* end of SendBrokerCommand                                          */
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
/* end of amqssuba.c                                                 */
/*                                                                   */
/*********************************************************************/




