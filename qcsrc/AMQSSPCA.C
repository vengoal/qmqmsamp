static const char sccsid[] = "%Z% %W% %I% %E% %U%";
/******************************************************************************/
/*                                                                            */
/* Module name: AMQSSPCA.C                                                    */
/*                                                                            */
/* Description: Sample C program that subscribes using PCF via the MQAI       */
/*              interface to the default stream and displays any arriving     */
/*              publications. The subscriber is deregistered on exit.         */
/*                                                                            */
/*   <copyright                                                                */
/*   notice="lm-source-program"                                                */
/*   pids="5724-H72"                                                           */
/*   years="1998,2016"                                                         */
/*   crc="2600716141" >                                                        */
/*   Licensed Materials - Property of IBM                                      */
/*                                                                             */
/*   5724-H72                                                                  */
/*                                                                             */
/*   (C) Copyright IBM Corp. 1998, 2016 All Rights Reserved.                   */
/*                                                                             */
/*   US Government Users Restricted Rights - Use, duplication or               */
/*   disclosure restricted by GSA ADP Schedule Contract with                   */
/*   IBM Corp.                                                                 */
/*   </copyright>                                                              */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Function:                                                                  */
/*                                                                            */
/*   AMQSSPCA is a sample C program for MQ Publish/Subscribe, it              */
/*   demonstrates subscribing using PCF via the MQAI interface.               */
/*                                                                            */
/*   A subscription message for the supplied topic, is put to MQ's            */
/*   Publish/Subscribe control queue (SYSTEM.BROKER.CONTROL.QUEUE) nominating */
/*   the default stream (SYSTEM.BROKER.DEFAULT.STREAM) and the supplied       */
/*   subscriber queue, which is then checked for any replies returned.        */
/*                                                                            */
/*   Any publications forwarded to the supplied subscriber queue are          */
/*   removed from the queue and displayed. The program assumes each           */
/*   publication contains a single line of text in the StringData tag         */
/*   (PCF MQCACF_STRING_DATA).                                                */
/*                                                                            */
/*   After 60 seconds of inactivity on the subscriber's queue the             */
/*   subscriber is deregistered and the sample exits.                         */
/*                                                                            */
/* Program logic:                                                             */
/*                                                                            */
/*   MQCONN to default queue manager (or optionally supplied name)            */
/*   MQOPEN control queue for output                                          */
/*   MQOPEN subscriber queue for publications and replies                     */
/*   Register the subscriber - mqPutBag to control queue                      */
/*   Check the reply messages - mqGetBag from subscriber queue                */
/*   Wait for publications                                                    */
/*   Get publication and display data - mqGetBag from subscriber queue        */
/*   Deregister the subscriber - mqPutBag to control queue                    */
/*   MQCLOSE subscriber queue                                                 */
/*   MQCLOSE control queue                                                    */
/*   MQDISC from queue manager                                                */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* AMQSSPCA takes 3 input parameters:                                         */
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
#include <cmqcfc.h>                              /* MQ PCF                    */
#include <cmqbc.h>                               /* MQAI                      */

/******************************************************************************/
/* Defines                                                                    */
/******************************************************************************/
#define MAX_PUB_LENGTH       100+1               /* maximum publication len   */
#define MAX_TOPIC_LENGTH     256+1               /* maximum topic length      */
#define PUBLISH_WAIT_TIME    60000               /* publish wait time (60s)   */
#define REPLY_WAIT_TIME      10000               /* reply wait time (10s)     */
#define DEFAULT_STREAM       "SYSTEM.BROKER.DEFAULT.STREAM"
#define BROKER_CONTROL       "SYSTEM.BROKER.CONTROL.QUEUE"

/********************************************************************/
/*                                                                  */
/*  Macro : ccCheck                                                 */
/*                                                                  */
/*  Display error message for non-successful completion codes       */
/*                                                                  */
/********************************************************************/
#define ccCheck(func, compCode, reason)                              \
{                                                                    \
  if (compCode != MQCC_OK)                                           \
  {                                                                  \
    printf(func" failed with reason=%d\n", reason);                  \
  }                                                                  \
}

/******************************************************************************/
/* Function Prototypes                                                        */
/******************************************************************************/
/* sendBrkMessage */
MQLONG sendBrkMessage(MQHCONN  hConn,
                      MQHOBJ   hBrkObj,
                      MQHOBJ   hReplyObj,
                      MQHBAG   hBag,
                      MQLONG   command,
                      char     StreamName[],
                      char     Topic[],
                      char     QName[],
                      PMQLONG  pCompCode,
                      PMQLONG  pReason);

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

  MQHCONN hConn;                   /* connection handle             */
  MQHBAG  hBag = MQHB_UNUSABLE_HBAG; /* bag handle                  */
  MQHOBJ  hBrkObj;                 /* broker object handle          */
  MQHOBJ  hSubObj;                 /* Subscriber object handle      */
  MQLONG  CompCode;                /* completion code               */
  MQLONG  Reason;                  /* reason code                   */
  MQLONG  replyCode;               /* Broker reply code             */

  MQOD    od  = { MQOD_DEFAULT };  /* object descriptor             */
  MQGMO   gmo = { MQGMO_DEFAULT }; /* get message options           */
  MQMD    md  = { MQMD_DEFAULT };  /* message descriptor            */
  MQLONG  O_options;               /* MQOPEN options                */
  MQLONG  C_options;               /* MQCLOSE options               */

  MQLONG  cmd;                     /* PCF command type              */
  char    SubscriberQ[MQ_Q_NAME_LENGTH+1]; /* Subscriber's queue    */
  char    Topic[MAX_TOPIC_LENGTH]; /* topic                         */
  char    buffer[MAX_PUB_LENGTH];  /* buffer string                 */
  MQLONG  buflen;                  /* buffer string length          */
  char    QMName[MQ_Q_MGR_NAME_LENGTH+1] = "";/* queue manager name */

  /******************************************************************/
  /*                                                                */
  /*   Check arguments                                              */
  /*                                                                */
  /******************************************************************/
  if (argc < 3)
  {
    printf("Usage: subsamp topic SubscriberQ <QManager>\n");
    exit(0);
  }
  else
  {
    strncpy(Topic, argv[1], 256);
    strncpy(SubscriberQ, argv[2], MQ_Q_NAME_LENGTH);
  }

  /******************************************************************/
  /*                                                                */
  /*   Connect to queue manager                                     */
  /*                                                                */
  /******************************************************************/
  /* if specified set the QManager name (defaults to "") */
  if (argc > 3)
    strncpy(QMName, argv[3], MQ_Q_MGR_NAME_LENGTH);
  MQCONN(QMName,                  /* queue manager                  */
         &hConn,                  /* connection handle              */
         &CompCode,               /* completion code                */
         &Reason);                /* reason code                    */
  ccCheck("MQCONN", CompCode, Reason);

  if (CompCode == MQCC_OK)
  {

    /****************************************************************/
    /*                                                              */
    /*   Open the Broker Control Queue for Subscription             */
    /*                                                              */
    /****************************************************************/
    strncpy(od.ObjectName, BROKER_CONTROL, (size_t)MQ_Q_NAME_LENGTH);
    O_options = MQOO_OUTPUT           /* open queue for output      */
           + MQOO_FAIL_IF_QUIESCING;  /* but not if MQM stopping    */
    MQOPEN(hConn,                     /* connection handle          */
           &od,                       /* object descriptor for queue*/
           O_options,                 /* open options               */
           &hBrkObj,                  /* object handle              */
           &CompCode,                 /* completion code            */
           &Reason);                  /* reason code                */
    ccCheck("MQOPEN", CompCode, Reason);

    if (CompCode == MQCC_OK)
    {

      /**************************************************************/
      /*                                                            */
      /*  Open the Subscriber's queue for replies from the broker   */
      /*  and arriving publications.                                */
      /*                                                            */
      /**************************************************************/
      strncpy(od.ObjectName, SubscriberQ, (size_t)MQ_Q_NAME_LENGTH);
      O_options = MQOO_INPUT_EXCLUSIVE/* open queue for input       */
            + MQOO_FAIL_IF_QUIESCING; /* but not if MQM stopping    */
      MQOPEN(hConn,                   /* connection handle          */
             &od,                     /* object descriptor for queue*/
             O_options,               /* open options               */
             &hSubObj,                /* object handle              */
             &CompCode,               /* MQOPEN completion code     */
             &Reason);                /* reason code                */
      ccCheck("MQOPEN", CompCode, Reason);

      if (CompCode == MQCC_OK)
      {

        /************************************************************/
        /*                                                          */
        /*   Create an MQAI bag for the registrations and           */
        /*   publications                                           */
        /*                                                          */
        /************************************************************/
        mqCreateBag( MQCBO_COMMAND_BAG, &hBag, &CompCode, &Reason );
        ccCheck("mqCreateBag", CompCode, Reason);

        if (CompCode == MQCC_OK)
        {

          /**********************************************************/
          /*                                                        */
          /*   Register as a Subscriber with the broker             */
          /*                                                        */
          /**********************************************************/
          printf("Registering Subscriber for topic '%s'\n",
                      Topic);

          replyCode = sendBrkMessage(hConn,
                          hBrkObj,
                          hSubObj,
                          hBag,
                          MQCMD_REGISTER_SUBSCRIBER,
                          DEFAULT_STREAM,
                          Topic,
                          SubscriberQ,
                          &CompCode,
                          &Reason);
          ccCheck("sendBrkMessage", CompCode, Reason);


          if (replyCode == MQCC_OK)
          {

            /********************************************************/
            /*                                                      */
            /*  Get with wait on the subscriber's queue to receive  */
            /*  any publications sent to the subscriber and         */
            /*  display the user data contained. End the wait       */
            /*  after 60 seconds of inactivity                      */
            /*                                                      */
            /********************************************************/
            gmo.Options = MQGMO_WAIT + MQGMO_CONVERT
                            + MQGMO_NO_SYNCPOINT;
            gmo.WaitInterval = PUBLISH_WAIT_TIME;
            gmo.Version = MQGMO_VERSION_2;
            gmo.MatchOptions = MQGMO_NONE;

            printf("Browsing Subscriber queue...\n");

            /* While there are still messages arriving... */
            while (CompCode != MQCC_FAILED)
            {
              /* clear the bag ready for input */
              mqClearBag(hBag, &CompCode, &Reason);
              ccCheck("mqClearBag", CompCode, Reason);

              if (CompCode == MQCC_OK)
              {
                /* get a bag from the subscriber queue */
                mqGetBag(hConn, hSubObj, &md, &gmo,
                         hBag, &CompCode, &Reason);
              }

              /* If the get was successful, process the bag */
              if (CompCode == MQCC_OK)
              {

                /* Find what type of PCF command the message is */
                mqInquireInteger(hBag, MQIASY_COMMAND, MQIND_NONE,
                            &cmd, &CompCode, &Reason);
                ccCheck("mqInquireInteger", CompCode, Reason);

                if (CompCode == MQCC_OK)
                {
                  /* Process the message accordingly */
                  switch (cmd)
                  {
                  /* Publish message */
                  case MQCMD_PUBLISH:
                    /* Extract the string data from the bag */
                    mqInquireString(hBag, MQCACF_STRING_DATA,
                        MQIND_NONE, MAX_PUB_LENGTH,
                        buffer, &buflen, NULL,
                        &CompCode, &Reason);
                    ccCheck("mqInquireString", CompCode, Reason);

                    if (CompCode == MQCC_OK)
                    {
                      /* Display the string */
                      buffer[buflen] = '\0';
                      printf("%s\n", buffer);
                    }
                    break;
                  /* Not a publish message */
                  default:
                    printf("Unexpected PCF command (%d)\n", cmd);
                    break;
                  }
                }
              }
              else
                if (Reason != MQRC_NO_MSG_AVAILABLE)
                  printf("mqGetBag failed with reason=%d\n", Reason);
            }

            /********************************************************/
            /*                                                      */
            /*   Deregister the Subscriber from the broker          */
            /*                                                      */
            /********************************************************/
            printf("Deregistering Subscriber from topic '%s'\n",
                     Topic);

            replyCode = sendBrkMessage(hConn,
                          hBrkObj,
                          hSubObj,
                          hBag,
                          MQCMD_DEREGISTER_SUBSCRIBER,
                          DEFAULT_STREAM,
                          Topic,
                          SubscriberQ,
                          &CompCode,
                          &Reason);
            ccCheck("replyCode", CompCode, Reason);

          }

          /* Delete the bag */
          mqDeleteBag(&hBag, &CompCode, &Reason);
          ccCheck("mqDeleteBag", CompCode, Reason);

        }

        /************************************************************/
        /*                                                          */
        /*   Close the Subscriber queue                             */
        /*                                                          */
        /************************************************************/
        C_options = 0;                  /* no close options         */
        MQCLOSE(hConn,                  /* connection handle        */
               &hSubObj,                /* object handle            */
               C_options,               /* close options            */
               &CompCode,               /* completion code          */
               &Reason);                /* reason code              */
        ccCheck("MQCLOSE", CompCode, Reason);

      }

      /**************************************************************/
      /*                                                            */
      /*   Close the Control queue                                  */
      /*                                                            */
      /**************************************************************/
      C_options = 0;                  /* no close options           */
      MQCLOSE(hConn,                  /* connection handle          */
            &hBrkObj,                 /* object handle              */
            C_options,                /* close options              */
            &CompCode,                /* completion code            */
            &Reason);                 /* reason code                */
      ccCheck("MQCLOSE", CompCode, Reason);

    }

    /****************************************************************/
    /*                                                              */
    /*   Disconnect from MQM                                        */
    /*                                                              */
    /****************************************************************/
    MQDISC(&hConn,                  /* connection handle            */
           &CompCode,               /* completion code              */
           &Reason);                /* reason code                  */
    ccCheck("MQDISC", CompCode, Reason);

  }

  return(0);

} /* end of main */


/********************************************************************/
/*                                                                  */
/*                             FUNCTIONS                            */
/*                                                                  */
/********************************************************************/

/********************************************************************/
/*                                                                  */
/*  Function : sendBrkMessage                                       */
/*                                                                  */
/*  Fill the MQAI bag with the given data and send the bag to the   */
/*  broker                                                          */
/*                                                                  */
/********************************************************************/
MQLONG sendBrkMessage( MQHCONN  hConn,      /* connection handle    */
                     MQHOBJ   hBrkObj,      /* broker object handle */
                     MQHOBJ   hReplyObj,    /* reply object handle  */
                     MQHBAG   hBag,         /* bag handle           */
                     MQLONG   command,      /* PCF command          */
                     char     StreamName[], /* stream name          */
                     char     Topic[],      /* topic                */
                     char     QName[],      /* reply queue          */
                     PMQLONG  pCompCode,    /* completion code      */
                     PMQLONG  pReason)      /* reason code          */
{
  MQLONG     replyCode;                 /* broker reply code        */
  MQMD       md = { MQMD_DEFAULT };     /* message descriptor       */
  MQPMO      pmo = { MQPMO_DEFAULT };   /* put message options      */

  /* Clear the MQAI bag */
  mqClearBag(hBag, pCompCode, pReason);
  ccCheck("mqClearBag", *pCompCode, *pReason);

  if (*pCompCode == MQCC_OK)
  {
    /* Set the command of the PCF message */
    mqSetInteger(hBag, MQIASY_COMMAND, MQIND_NONE,
           command,
           pCompCode, pReason);
    ccCheck("mqSetInteger", *pCompCode, *pReason);
  }

  if (*pCompCode == MQCC_OK)
  {
    /* Add the topic to subscribe to */
    mqAddString(hBag, MQCACF_TOPIC,(MQLONG)strlen(Topic),
           Topic,pCompCode,pReason);
    ccCheck("mqAddString", *pCompCode, *pReason);
  }

  if (*pCompCode == MQCC_OK)
  {
    /* Add the stream to subscribe to */
    mqAddString(hBag, MQCACF_STREAM_NAME,(MQLONG)strlen(StreamName),
           StreamName,pCompCode,pReason);
    ccCheck("mqAddString", *pCompCode, *pReason);
  }

  if (*pCompCode == MQCC_OK)
  {
    /* Add the QName to identify us */
    mqAddString(hBag, MQCA_Q_NAME,(MQLONG)strlen(QName),
           QName,pCompCode,pReason);
    ccCheck("mqAddString", *pCompCode, *pReason);
  }

  if (*pCompCode == MQCC_OK)
  {
    memcpy(md.Format,                         /* PCF format       */
              MQFMT_PCF, (size_t)MQ_FORMAT_LENGTH);


    strcpy(md.ReplyToQ, QName); /* Set the ReplyToQ for replies   */
    md.MsgType = MQMT_REQUEST;  /* request message                */

    pmo.Options |= MQPMO_NEW_MSG_ID
                 | MQPMO_NO_SYNCPOINT;

    /****************************************************************/
    /*                                                              */
    /*   Put the bag to the broker's control queue                  */
    /*                                                              */
    /****************************************************************/
    mqPutBag(hConn,            /* connection handle                 */
          hBrkObj,             /* object handle                     */
          &md,                 /* message descriptor                */
          &pmo,                /* put message options               */
          hBag,                /* PCF Bag                           */
          pCompCode,           /* completion code                   */
          pReason);            /* reason code                       */
    ccCheck("mqPutBag", *pCompCode, *pReason);
  }

  if (*pCompCode == MQCC_OK)
  {
    /****************************************************************/
    /*                                                              */
    /*   Check the reply sent from the broker                       */
    /*                                                              */
    /****************************************************************/
    replyCode = checkResponse(hConn, hReplyObj, md.MsgId,
                  pCompCode, pReason);
    ccCheck("checkResponse", *pCompCode, *pReason);
  }

  return replyCode;

} /* end of sendBrkMessage */


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
  if ( *pCompCode == MQCC_OK )
  {
    gmo.Options = MQGMO_WAIT + MQGMO_CONVERT + MQGMO_NO_SYNCPOINT;
    gmo.WaitInterval = REPLY_WAIT_TIME;
    /* match on the correl id */
    gmo.MatchOptions = MQMO_MATCH_CORREL_ID;

    while ( *pCompCode == MQCC_OK )
    {
      memcpy( md.MsgId, MQMI_NONE, MQ_MSG_ID_LENGTH );
      memcpy( md.CorrelId, correlId, MQ_MSG_ID_LENGTH );

      /**************************************************************/
      /* Get the reply message into the reply bag.                  */
      /**************************************************************/
       mqGetBag( hConn, hReplyObj, &md, &gmo, replyBag,
                 pCompCode, pReason );

      /**************************************************************/
      /* Print out the compcode and reason and any user             */
      /* selectors.                                                 */
      /**************************************************************/
      if ( *pCompCode == MQCC_OK )
      {
        /* extract the completion code from the bag */
        mqInquireInteger( replyBag, MQIASY_COMP_CODE, MQIND_NONE,
                                &Value, pCompCode, pReason );
        ccCheck("mqInquireInteger", *pCompCode, *pReason);

        if (*pCompCode == MQCC_OK)
        {
          if (Value != MQCC_OK)
          {
            printf("Completion Code: %d\n",Value);
          }
          /* Set the broker's response code */
          responseCode = Value;
        }
      }

      if (*pCompCode == MQCC_OK)
      {
        /* extract the reason code from the bag */
        mqInquireInteger( replyBag, MQIASY_REASON, MQIND_NONE,
                             &Value, pCompCode, pReason );
        ccCheck("mqInquireInteger", *pCompCode, *pReason);

        if ( *pCompCode == MQCC_OK )
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
      if ( *pCompCode == MQCC_OK )
      {
        /* count the number of user selectors in the bag */
        mqCountItems( replyBag, MQSEL_ANY_USER_SELECTOR,
                                    &Items, pCompCode, pReason );
        ccCheck("mqCountItems", *pCompCode, *pReason);

        if ( *pCompCode == MQCC_OK )
        {
          /* step through each user selector */
          for ( i=0; i<Items; i++ )
          {
            /* find out what selector we are looking at */
            mqInquireItemInfo( replyBag, MQSEL_ANY_USER_SELECTOR,
                                 i, &OutSelector,  &OutType,
                                    pCompCode, pReason );
            ccCheck("mqInquireItemInfo", *pCompCode, *pReason);

            if (  *pCompCode != MQCC_OK )
              break;

            /* find the value of this selector */
            mqInquireInteger( replyBag, MQSEL_ANY_USER_SELECTOR,
                                 i, &Value, pCompCode, pReason );
            ccCheck("mqInquireInteger", *pCompCode, *pReason);

            if ( *pCompCode != MQCC_OK )
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
      if ( *pCompCode == MQCC_OK )
      {
        /* see if the MQCFC_LAST flag is set */
        mqInquireInteger( replyBag, MQIASY_CONTROL, MQIND_NONE,
                              &Value, pCompCode, pReason );
        ccCheck("mqInquireInteger", *pCompCode, *pReason);

        if ( *pCompCode == MQCC_OK )
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

/********************************************************************/
/*                                                                  */
/* end of subsamp.c                                                 */
/*                                                                  */
/********************************************************************/

