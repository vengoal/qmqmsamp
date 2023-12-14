/* %Z% %W% %I% %E% %U% */
/*********************************************************************/
/*                                                                   */
/* Module name: AMQSRR2A.C                                           */
/*                                                                   */
/* Description:  Based on an MQ Publish/Subscribe system the sample  */
/*               will simulate a results gathering service that      */
/*               constantly reports updates to the score of ongoing  */
/*               soccer matches. The results gatherer is sent event  */
/*               information from one or more instances of a simple  */
/*               soccer match simulator. The results service also    */
/*               retains the state (score) of all current matches    */
/*               being played, so that even after a failure of the   */
/*               results service it can be restarted and continue    */
/*               where it left off without loosing the details of    */
/*               ongoing matches.                                    */
/*                                                                   */
/*               This source file is the results gathering service   */
/*               and works in conjunction with the soccer simulator, */
/*               amqsgr2, any number of instances of amqsgr2 can be  */
/*               running as long as they all specify unique team     */
/*               names.                                              */
/*                                                                   */
/*               To run this sample you will need a queue manager    */
/*               with the queued Pub/Sub interface running.          */
/*               Two extra queues need to be defined on the queue    */
/*               manager we connect to, a stream queue used by the   */
/*               results service samples,                            */
/*               SAMPLE.BROKER.RESULTS.STREAM. The other queue       */
/*               is the queue used by the subscriber to identify     */
/*               itself to the broker, this is the queue that all    */
/*               publications the subscriber subscribes to are sent  */
/*               to, the queue name is RESULTS.SERVICE.SAMPLE.QUEUE. */
/*                                                                   */
/*               The queues are defined as:                          */
/*                                                                   */
/*               define qlocal('SAMPLE.BROKER.RESULTS.STREAM') +     */
/*                      noshare                                      */
/*               define qlocal('RESULTS.SERVICE.SAMPLE.QUEUE')       */
/*                                                                   */
/*               Both queues are defined in the MQSC script          */
/*               amqsresa.tst.                                       */
/*                                                                   */
/*               This sample must be started before any instances    */
/*               of amqsgr2 are started, a message will be           */
/*               displayed when it is possible to start the          */
/*               amqsgr2 sample(s).                                  */
/*                                                                   */
/*  Usage:       amqsrr2 <QMgrName>                                  */
/*                                                                   */
/*  Language:    C                                                   */
/*                                                                   */
/*********************************************************************/
/*                                                                   */
/* Function Flow :                                                   */
/*                                                                   */
/*         main:                                                     */
/*           MQCONN                                                  */
/*           MQOPEN                                                  */
/*           RestoreMatches:                                         */
/*             PubSubCommand:                                        */
/*               BuildMQRFHeader2                                    */
/*               MQPUT                                               */
/*               CheckForResponse:                                   */
/*                 MQGET                                             */
/*             PubSubCommand..                                       */
/*             MQGET                                                 */
/*             ExtractTopicType:                                     */
/*               GetNextToken                                        */
/*               GetNextToken                                        */
/*             PubSubCommand..                                       */
/*           PubSubCommand..                                         */
/*           MQGET                                                   */
/*           ExtractTopicType..                                      */
/*           AddNewMatch:                                            */
/*             UpdateLatestScorePub:                                 */
/*               BuildMQRFHeader2                                    */
/*               MQPUT                                               */
/*           EndMatch                                                */
/*             UpdateLatestScorePub..                                */
/*           UpdateScore                                             */
/*             UpdateLatestScorePub..                                */
/*           PubSubCommand..                                         */
/*           MQCLOSE                                                 */
/*           MQDISC                                                  */
/*                                                                   */
/*********************************************************************/
/*                                                                   */
/*  <copyright                                                       */
/*  notice="lm-source-program"                                       */
/*  pids=""                                                          */
/*  years="2006,2014"                                                */
/*  crc="734019814" >                                                */
/*  Licensed Materials - Property of IBM                             */
/*                                                                   */
/*                                                                   */
/*                                                                   */
/*  (C) Copyright IBM Corp. 2006, 2014 All Rights Reserved.          */
/*                                                                   */
/*  US Government Users Restricted Rights - Use, duplication or      */
/*  disclosure restricted by GSA ADP Schedule Contract with          */
/*  IBM Corp.                                                        */
/*  </copyright>                                                     */
/*                                                                   */
/*********************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include <cmqc.h>                           /* MQI                   */
#include <cmqpsc.h>                         /* MQI Publish/Subscribe */
#include <cmqcfc.h>                         /* MQ PCF                */

/*********************************************************************/
/* The msSleep macro needs some platform specific headers            */
/*********************************************************************/
#if (MQAT_DEFAULT == MQAT_WINDOWS_NT)
  #include <windows.h>
#else
  #if (MQAT_DEFAULT == MQAT_MVS)
    #define _XOPEN_SOURCE_EXTENDED 1
    #define _OPEN_MSGQ_EXT
  #endif
  #include <sys/types.h>
  #include <sys/time.h>
#endif

/*********************************************************************/
/* Millisecond sleep                                                 */
/*********************************************************************/
#if (MQAT_DEFAULT == MQAT_WINDOWS_NT)
  #define msSleep(time)                                              \
      Sleep(time)
#else
  #define msSleep(time)                                               \
   {                                                                  \
      struct timeval tval;                                            \
                                                                      \
      tval.tv_sec  = (time) / 1000;                                   \
      tval.tv_usec = ((time) % 1000) * 1000;                          \
                                                                      \
      select(0, NULL, NULL, NULL, &tval);                             \
   }
#endif

/*********************************************************************/
/* MQ queues used by this sample:                              */
/*********************************************************************/
#define CONTROL_QUEUE         "SYSTEM.BROKER.CONTROL.QUEUE"
#define STREAM_QUEUE          "SAMPLE.BROKER.RESULTS.STREAM"
#define SUBSCRIBER_QUEUE      "RESULTS.SERVICE.SAMPLE.QUEUE"

/*********************************************************************/
/* Topic substrings:                                                 */
/*********************************************************************/
#define TOPIC_PREFIX          "Sport/Soccer/"
#define EVENT_TOPIC_PREFIX    TOPIC_PREFIX "Event/"
#define STATE_TOPIC_PREFIX    TOPIC_PREFIX "State/"
#define LATEST_SCORE_TOPIC    STATE_TOPIC_PREFIX "LatestScore/"
#define MATCH_STARTED         "MatchStarted"
#define MATCH_ENDED           "MatchEnded"
#define SCORE_UPDATE          "ScoreUpdate"

/*********************************************************************/
/* General definitions:                                              */
/*********************************************************************/

#ifndef OK
   #define OK        0                  /* define OK as zero         */
#endif
#ifndef FAILURE
   #define FAILURE   1                  /* define FAILURE as one     */
#endif

#ifndef BOOL
   #define BOOL MQULONG
#endif
#ifndef TRUE
#define TRUE                  1
#endif
#ifndef FALSE
   #define FALSE  0
#endif

#define DEFAULT_MESSAGE_SIZE  512
#define EVENT_CORREL_ID_ARRAY 'A','M','Q','S','R','R','2','A',' ',\
                              'E','v','e','n','t',' ',\
                              'C','o','r','r','e','l','I','d',' '
#define STATE_CORREL_ID_ARRAY 'A','M','Q','S','R','R','2','A',' ',\
                              'S','t','a','t','e',' ',\
                              'C','o','r','r','e','l','I','d',' '
#define MAX_WAIT_TIME         180000         /* Period of inactivity */
#define MAX_RESPONSE_TIME     10000          /* Response wait        */
#define TELE_TYPE_DELAY       25

#define LENGTH_OF_LENGTH_FIELD 4

/*********************************************************************/
/* Globals:                                                          */
/*********************************************************************/
static const MQRFH2 DefaultMQRFH2 = {MQRFH2_DEFAULT};
static const MQBYTE EventCorrelId[] = {EVENT_CORREL_ID_ARRAY};
static const MQBYTE StateCorrelId[] = {STATE_CORREL_ID_ARRAY};

/*********************************************************************/
/* Structures:                                                       */
/*********************************************************************/

/*********************************************************************/
/* Match_Teams : User data published by the amqsgr2 sample           */
/*********************************************************************/
typedef struct
{
  MQCHAR32  Team1;
  MQCHAR32  Team2;
} Match_Teams, *pMatch_Teams;

/*********************************************************************/
/* Match_Node : Node of the linked list of matches being played.     */
/*********************************************************************/
typedef struct List_Node
{
  MQCHAR32  Team1;
  MQCHAR32  Team2;
  MQLONG    Team1Score;
  MQLONG    Team2Score;
  struct    List_Node *pNextMatch;
} Match_Node, *pMatch_Node;

/*********************************************************************/
/* Parser_State : Possible states of the NameValueData parser.       */
/*********************************************************************/
typedef enum
{
  OutOfToken,
  InToken,
  InQuotes,
  EmbeddedQuote,
  EndOfToken
} Parser_State;

/*********************************************************************/
/* Prototypes:                                                       */
/*********************************************************************/
void RestoreMatches( MQHCONN      hConn
                   , MQHOBJ       hControlObj
                   , MQHOBJ       hSubscriberObj
                   , pMatch_Node *ppFirstMatch
                   , PMQLONG      pCompCode
                   , PMQLONG      pReason );

void PubSubCommand( MQHCONN       hConn
                  , MQHOBJ        hBrokerObj
                  , MQHOBJ        hReplyObj
                  , MQCHAR        Command[]
                  , PMQCHAR       pTopic
                  , MQLONG        topicLength
                  , const MQBYTE *pCorrelId
                  , MQLONG        regOptions
                  , PMQLONG       pCompCode
                  , PMQLONG       pReason );

void BuildMQRFHeader2( PMQBYTE   pStart
                     , PMQLONG   pDataLength
                     , PMQCHAR   pCommand
                     , MQLONG    regOptions
                     , MQLONG    pubOptions
                     , PMQCHAR   pTopic );

void CheckForResponse( MQHCONN  hConn
                     , MQHOBJ   hObj
                     , PMQMD    pMd
                     , PMQBYTE  pMessageBlock
                     , MQLONG   blockSize
                     , PMQLONG  pCompCode
                     , PMQLONG  pReason );

MQLONG ExtractTopicType( PMQCHAR  pNameValueData
                       , MQLONG   stringLength
                       , PMQCHAR  TopicPrefix
                       , PMQCHAR *ppTopicType
                       , MQULONG *pTopicTypeLength );

MQLONG GetNextToken( PMQCHAR *ppNameValueData
                   , PMQLONG  pRemainingLength
                   , PMQBYTE *ppToken
                   , MQULONG *pTokenLength );

void AddNewMatch( MQHCONN       hConn
                , pMatch_Teams  pTeams
                , pMatch_Node  *ppFirstMatch
                , MQHOBJ        hStreamObj
                , PMQLONG       pCompCode
                , PMQLONG       pReason );

void EndMatch( MQHCONN       hConn
             , pMatch_Teams  pTeams
             , pMatch_Node  *ppFirstMatch
             , MQHOBJ        hStreamObj
             , PMQLONG       pCompCode
             , PMQLONG       pReason );

void UpdateScore( MQHCONN       hConn
                , PMQCHAR       ScoringTeam
                , pMatch_Node   pFirstMatch
                , MQHOBJ        hStreamObj
                , PMQLONG       pCompCode
                , PMQLONG       pReason );

void UpdateLatestScorePub( MQHCONN      hConn
                         , pMatch_Node  pMatch
                         , MQHOBJ       hStreamObj
                         , PMQLONG      pCompCode
                         , PMQLONG      pReason
                         , BOOL         bMatchEnded );

void PrintNameValueData( PMQCHAR nameValueData
                         , MQLONG  dataLength );

void TeleType( PMQCHAR pChar );

/*********************************************************************/
/* Functions:                                                        */
/*********************************************************************/

/*********************************************************************/
/*                                                                   */
/* Function Name : main                                              */
/*                                                                   */
/* Description   : Entry function of the sample, connects to the     */
/*                 queue manager, Restores the system and processes  */
/*                 all arriving publications.                        */
/*                                                                   */
/* Flow          :                                                   */
/*                                                                   */
/*   MQCONN to broker queue manager                                  */
/*    MQOPEN broker control queue                                    */
/*    MQOPEN broker stream queue                                     */
/*    MQOPEN subscriber queue                                        */
/*     Restore existing matches                                      */
/*     Subscribe to all Event publications                           */
/*      MQGET all publications arriving from subscription            */
/*       Parse NameValueData of publication for the Topic            */
/*       Process depending on topic:                                 */
/*        AddMatch                                                   */
/*        EndMatch                                                   */
/*        UpdateScore                                                */
/*     Deregister Subscription                                       */
/*    MQCLOSE broker control queue                                   */
/*    MQCLOSE broker stream queue                                    */
/*    MQCLOSE subscriber queue                                       */
/*   MQDISC from broker queue manager                                */
/*   Free any remaining match nodes                                  */
/*                                                                   */
/* Input Parms   : int  argc                                         */
/*                  Number of arguments                              */
/*                 char *argv[]                                      */
/*                  Program Arguments                                */
/*                                                                   */
/*********************************************************************/
int main(int argc, char **argv)
{
  MQHCONN      hConn = MQHC_UNUSABLE_HCONN;
  MQHOBJ       hControlObj  = MQHO_UNUSABLE_HOBJ;
  MQHOBJ       hStreamObj  = MQHO_UNUSABLE_HOBJ;
  MQHOBJ       hSubscriberObj  = MQHO_UNUSABLE_HOBJ;
  MQLONG       CompCode;
  MQLONG       Reason;
  MQLONG       ConnReason;
  MQOD         od  = { MQOD_DEFAULT };
  MQGMO        gmo = { MQGMO_DEFAULT };
  MQMD         md  = { MQMD_DEFAULT };
  MQLONG       Options;
  PMQBYTE      pMessageBlock = NULL;
  MQLONG       messageLength;
  MQCHAR32     OpenQueue[3];
  PMQHOBJ      pHObj[3];
  MQLONG       queueCounter;
  pMatch_Node  pFirstMatch = NULL;
  pMatch_Node  pMatchDelete;
  MQCHAR32     subscriptionTopic;
  PMQRFH2      pMQRFHeader2;
  PMQCHAR      pNameValueData;
  PMQBYTE      pUserData;
  PMQCHAR      pTopicType;
  MQULONG      topicTypeLength;
  MQLONG       nameValueDataLength;
  char         QMName[MQ_Q_MGR_NAME_LENGTH+1] = "";

  /*******************************************************************/
  /* Initialise the array of open queue handles.                     */
  /*******************************************************************/
  strcpy(OpenQueue[0], CONTROL_QUEUE);
  pHObj[0] = &hControlObj;
  strcpy(OpenQueue[1], STREAM_QUEUE);
  pHObj[1] = &hStreamObj;
  strcpy(OpenQueue[2], SUBSCRIBER_QUEUE);
  pHObj[2] = &hSubscriberObj;

  /*******************************************************************/
  /* If no queue manager name was given as an argument, connect to   */
  /* the default queue manager (if one exists). Otherwise connect    */
  /* to the one specified.                                           */
  /*******************************************************************/
  if (argc > 1)
    strncpy(QMName, argv[1], MQ_Q_MGR_NAME_LENGTH);

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
    printf("Usage: %s <QManager>\n", argv[0]);
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
  /* Open all three queues that we will use:                         */
  /*   Broker control queue (output)                                 */
  /*   Broker stream queue  (output)                                 */
  /*   Subscriber queue     (input)                                  */
  /*******************************************************************/
  if( CompCode == MQCC_OK )
  {
    for( queueCounter = 0
       ; (queueCounter < 3) && (CompCode == MQCC_OK)
       ; queueCounter++ )
    {
      strncpy(od.ObjectName, OpenQueue[queueCounter],
                                   (size_t)MQ_Q_NAME_LENGTH);
      /***************************************************************/
      /* Set the options for the MQOPEN, appropriate to the queue    */
      /* being opened.                                               */
      /***************************************************************/
      Options = MQOO_FAIL_IF_QUIESCING;
      /***************************************************************/
      /* Open the subscriber queue for exclusive input, therefore,   */
      /* only one application can connect for input at a time, this  */
      /* will prevent multiple instances of amqsrr2 running          */
      /* concurrently.                                               */
      /***************************************************************/
      if( strcmp(OpenQueue[queueCounter], SUBSCRIBER_QUEUE) == 0 )
        Options +=  MQOO_INPUT_EXCLUSIVE;
      else
        Options += MQOO_OUTPUT;

      MQOPEN( hConn
            , &od
            , Options
            , pHObj[queueCounter]
            , &CompCode
            , &Reason );

      if( CompCode != MQCC_OK )
      {
        printf("MQOPEN failed to open \"%s\"\nwith CompCode %d and Reason %d\n",
               od.ObjectName, CompCode, Reason);
        if ( strcmp(CONTROL_QUEUE, OpenQueue[queueCounter]) == 0 )
        {
          printf("This is the Pub/Sub control queue - it is created at the"
                 "time the queued Pub/Sub interface is enabled.\nPlease enable "
                 "the queued Pub/Sub interface and try running this program again.\n");
        }
        else
        {
          printf("Usage: amqsres <QManager>\n");
        }
      }
    }
  }

  /*******************************************************************/
  /* Restore the state of any matches not completed by the last run  */
  /* of this sample.                                                 */
  /*******************************************************************/
  if( CompCode == MQCC_OK )
  {
    RestoreMatches( hConn
                  , hControlObj
                  , hSubscriberObj
                  , &pFirstMatch
                  , &CompCode
                  , &Reason );
  }

  /*******************************************************************/
  /* If we successfully restored any uncompleted matches we can      */
  /* start the results monitoring service.                           */
  /*******************************************************************/
  if( CompCode == MQCC_OK )
  {
    /*****************************************************************/
    /* Subscribe to all match events published by the sample         */
    /* amqsgr2. These are:                                           */
    /*   MatchStarted                                                */
    /*   ScoreUpdate                                                 */
    /*   MatchEnded                                                  */
    /* We specify the correlId as part of our identity so that all   */
    /* publications sent to us will have the EVENT_CORREL_ID value   */
    /* in the correlId field of the md.                              */
    /*****************************************************************/
    strcpy( subscriptionTopic, EVENT_TOPIC_PREFIX);
    strcat( subscriptionTopic, "#");
    PubSubCommand( hConn
                 , hControlObj
                 , hSubscriberObj
                 , MQPS_REGISTER_SUBSCRIBER
                 , subscriptionTopic
                 , (MQLONG)strlen(subscriptionTopic)
                 , EventCorrelId
                 , MQREGO_CORREL_ID_AS_IDENTITY
                 , &CompCode
                 , &Reason );

    if( CompCode == MQCC_OK )
    {
      /***************************************************************/
      /* Allocate a block of memory for the publications to be       */
      /* loaded into by MQGET. We know the maximum size of a         */
      /* publication published by amqsgr2 so we can allocate a       */
      /* block large enough for any message we will receive.         */
      /***************************************************************/
      messageLength = DEFAULT_MESSAGE_SIZE;
      pMessageBlock = (PMQBYTE)malloc(DEFAULT_MESSAGE_SIZE);
      if( pMessageBlock == NULL )
      {
        printf("Unable to allocate storage\n");
        CompCode = MQCC_FAILED;
      }
      else
      {
        /*************************************************************/
        /* Now that a subscription to the event topics has been      */
        /* registered any number of amqsgr2 samples can be started   */
        /* on the same broker. If the amqsgr2 samples are to be      */
        /* run from another broker in the hierarchy it is not        */
        /* possible to take the okay response from a subscription    */
        /* registration request as an indication that the            */
        /* subscription has been propagated to all brokers in the    */
        /* hierarchy.                                                */
        /*************************************************************/
        printf("Results Service is ready for match input,\n");
        printf("instances of amqsgr2 can now be started.\n\n");

        /*************************************************************/
        /* We will now wait for publications on the Event topics to  */
        /* be forwarded to our subscriber queue.                     */
        /* Configure the MQGET to get with wait for a maximum of     */
        /* 3 minutes before exiting the wait. We will only get       */
        /* messages that have the EVENT_CORREL_ID correlId value,    */
        /* MQGET will perform data conversion on the publication it  */
        /* receives.                                                 */
        /*************************************************************/
        gmo.Options = MQGMO_WAIT + MQGMO_CONVERT + MQGMO_NO_SYNCPOINT;
        gmo.WaitInterval = MAX_WAIT_TIME;
        gmo.Version = MQGMO_VERSION_2;
        gmo.MatchOptions = MQMO_MATCH_CORREL_ID;
        memcpy( md.CorrelId, EventCorrelId,
                                        (size_t)MQ_CORREL_ID_LENGTH);

        while( CompCode == MQCC_OK )
        {
          MQGET( hConn
               , hSubscriberObj
               , &md
               , &gmo
               , DEFAULT_MESSAGE_SIZE
               , pMessageBlock
               , &messageLength
               , &CompCode
               , &Reason );

          if( CompCode == MQCC_OK )
          {
            /*********************************************************/
            /* Check that the message is in the MQRFH2 format.       */
            /*********************************************************/
            if( memcmp(md.Format, MQFMT_RF_HEADER_2, MQ_FORMAT_LENGTH) == 0 )
            {
              /*******************************************************/
              /* Split the message data into the three important     */
              /* areas, the MQRFH2 header, the NameValueData that    */
              /* follows it and any user data following that.        */
              /*******************************************************/
              pMQRFHeader2 = (PMQRFH2)pMessageBlock;
              pNameValueData =   (PMQCHAR)(pMessageBlock
                                          + MQRFH_STRUC_LENGTH_FIXED_2
                                          + LENGTH_OF_LENGTH_FIELD);
              nameValueDataLength = pMQRFHeader2->StrucLength
                                           - MQRFH_STRUC_LENGTH_FIXED_2
                                           - LENGTH_OF_LENGTH_FIELD;
              pUserData = pMessageBlock + pMQRFHeader2->StrucLength;

              /*******************************************************/
              /* The MQGET will receive either publications sent     */
              /* from the broker that match our subscription or      */
              /* negative replies as a result of us publishing       */
              /* state publications (in UpdateLatestScorePub).       */
              /*******************************************************/
              if( md.MsgType != MQMT_REPLY )
              {
                /*****************************************************/
                /* The publication could be any one the three        */
                /* events published on, MatchStartes, ScoreUpdate    */
                /* and MatchEnded. We need to locate the Topic       */
                /* value in the NameValueData and then extract       */
                /* the event type from that.                         */
                /*****************************************************/
                CompCode = ExtractTopicType( pNameValueData
                                           , nameValueDataLength
                                           , EVENT_TOPIC_PREFIX
                                           , &pTopicType
                                           , &topicTypeLength );

                /*****************************************************/
                /* The Topic value was successfully located in       */
                /* the NameValueData, now we must process the        */
                /* user data in the publication according to the     */
                /* topic it was published on.                        */
                /*****************************************************/
                if( CompCode == MQCC_OK )
                {
                  if( (topicTypeLength == strlen(MATCH_STARTED))
                    &&(memcmp(pTopicType, MATCH_STARTED, topicTypeLength)
                       == 0) )
                  {
                    /*************************************************/
                    /* A new match has been started.                 */
                    /*************************************************/
                    AddNewMatch( hConn
                               , (pMatch_Teams)pUserData
                               , &pFirstMatch
                               , hStreamObj
                               , &CompCode
                               , &Reason );
                  }
                  else if( (topicTypeLength == strlen(MATCH_ENDED))
                    &&(memcmp(pTopicType, MATCH_ENDED, topicTypeLength)
                       == 0) )
                  {
                    /*************************************************/
                    /* A match has ended.                            */
                    /*************************************************/
                    EndMatch( hConn
                            , (pMatch_Teams)pUserData
                            , &pFirstMatch
                            , hStreamObj
                            , &CompCode
                            , &Reason );
                  }
                  else if( (topicTypeLength == strlen(SCORE_UPDATE))
                    &&(memcmp(pTopicType, SCORE_UPDATE, topicTypeLength)
                       == 0) )
                  {
                    /*************************************************/
                    /* A goal has been scored.                       */
                    /*************************************************/
                    UpdateScore( hConn
                               , (PMQCHAR)pUserData
                               , pFirstMatch
                               , hStreamObj
                               , &CompCode
                               , &Reason );
                  }
                  else
                  {
                    /*************************************************/
                    /* A publication on another topic has been       */
                    /* received this is an error.                    */
                    /*************************************************/
                    printf("Unexpected publication :\n");
                    PrintNameValueData(pNameValueData
                                      , nameValueDataLength);
                    CompCode = MQCC_FAILED;
                  }
                }
              }
              /*******************************************************/
              /* The message is a reply from a publish command.      */
              /*******************************************************/
              else
              {
                printf("Error reply returned\n");
                if( messageLength != pMQRFHeader2->StrucLength )
                {
                  printf("Original Command String:\n");
                  PrintNameValueData((PMQCHAR)pUserData,
                          (messageLength - pMQRFHeader2->StrucLength));
                }
                printf("Processing will continue but the recorded\n");
                printf("state of the matches may be corrupted.\n\n");
              }
            }
            /*********************************************************/
            /* If the message is not in the MQRFH2 format we have    */
            /* an unwanted message.                                  */
            /*********************************************************/
            else
            {
              printf("Unexpected message format: %.8s\n", md.Format );
              CompCode = MQCC_FAILED;
            }
          }
          else if( Reason != MQRC_NO_MSG_AVAILABLE )
          {
            printf("MQGET failed with CompCode %d and Reason %d\n",
                      CompCode, Reason);
          }
        } /* end while */
        /*************************************************************/
        /* The MQGET has timed out, free up the storage allocated    */
        /* for publications.                                         */
        /*************************************************************/
        free( pMessageBlock );
      }

      /***************************************************************/
      /* No publications have been received for our subscription in  */
      /* 3 minutes, it is time to end the results server.            */
      /* This period of inactivity will normally be due to all the   */
      /* match simulators having finished but there is also the      */
      /* chance that either a match simulator was killed before it   */
      /* completed or the broker has been ended, thus stopping the   */
      /* forwarding of publications to this sample. Normally we      */
      /* would want to deregister our subscription to the event      */
      /* topics at this point but for the special case when the      */
      /* broker has been ended we cannot do this as the deregister   */
      /* subscriber command will not be processed by the broker      */
      /* until the broker is restarted and there may be event        */
      /* publications (put by the match simulator(s)) also waiting   */
      /* to be processed by the broker when it starts. If we         */
      /* deregister before these are processed we will never         */
      /* receive the publications and the matches will never be      */
      /* completed. For this reason we will only deregister our      */
      /* subscription if there are no matches currently being played */
      /* (pFirstMatch is NULL) otherwise we will maintain our        */
      /* subscription to the event topics. This will not affect any  */
      /* subsequent runs of this sample as the re-registration will  */
      /* simply overwrite the existing one on the broker.            */
      /***************************************************************/
      if( pFirstMatch == NULL )
      {
        PubSubCommand( hConn
                     , hControlObj
                     , hSubscriberObj
                     , MQPS_DEREGISTER_SUBSCRIBER
                     , subscriptionTopic
                     , (MQLONG)strlen(subscriptionTopic)
                     , EventCorrelId
                     , MQREGO_CORREL_ID_AS_IDENTITY
                     , &CompCode
                     , &Reason );
      }
      printf("Results Service has ended\n\n");
    }
  }

  /*******************************************************************/
  /* MQCLOSE the three queues used by this sample.                   */
  /*******************************************************************/
  for( queueCounter = 0
     ; (queueCounter < 3)
     ; queueCounter++ )
  {
    if( *(pHObj[queueCounter]) != MQHO_UNUSABLE_HOBJ )
    {
      MQCLOSE( hConn
             , pHObj[queueCounter]
             , MQCO_NONE
             , &CompCode
             , &Reason );
      if( CompCode != MQCC_OK )
        printf("MQCLOSE failed with CompCode %d and Reason %d\n",
                                                  CompCode, Reason);
    }
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

  /*******************************************************************/
  /* If any of the amqsgr2 samples did not complete successfully     */
  /* or messages have been delayed it is possible that this sample   */
  /* will still have active entries for these matches, the memory    */
  /* allocated to them is now freed.                                 */
  /* Note: The associated retained publication will still remain on  */
  /*       the broker.                                               */
  /*******************************************************************/
  if( pFirstMatch != NULL )
  {
    printf("One or more matches did not complete:\n");
    while( pFirstMatch != NULL )
    {
      printf("  %s v %s\n", pFirstMatch->Team1, pFirstMatch->Team2);
      pMatchDelete = pFirstMatch;
      pFirstMatch = pFirstMatch->pNextMatch;
      free( pMatchDelete );
    }
  }
  return(0);
}
/*********************************************************************/
/* end of main                                                       */
/*********************************************************************/


/*********************************************************************/
/*                                                                   */
/* Function Name : RestoreMatches                                    */
/*                                                                   */
/* Description   : Whilst this sample is running any changes to      */
/*                 match state (change in score) is logged so that   */
/*                 it can be restored at any time in the future,     */
/*                 for this sample we have chosen to log the state   */
/*                 by publishing retained publications, which are    */
/*                 persistently held on the Publish/Subscribe        */
/*                 queue manager (we publish persistent messages).   */
/*                 To restore the current state of known matches we  */
/*                 subscribe to the LatestScore topics, which are    */
/*                 those that we have published with retain on for   */
/*                 each ongoing match, if any did not complete       */
/*                 whilst this sample was previously running a       */
/*                 retained publication will still exist for each    */
/*                 match and we will be sent them, from these we     */
/*                 can restore the linked list of matches before     */
/*                 we try and process any Event publications that    */
/*                 may have been sent to us whilst we were not       */
/*                 running (our subscription would still be active). */
/*                                                                   */
/* Flow          :                                                   */
/*                                                                   */
/*   Subscribe to all LatestScore publications                       */
/*   Request Updates of all LatestScore publications                 */
/*   MQGET all publications from Request Update                      */
/*    Create a match node for each publication                       */
/*   Deregister Subscription                                         */
/*                                                                   */
/* Input Parms   : MQHCONN  hConn                                    */
/*                  Queue manager connection handle                  */
/*                 MQHOBJ   hControlObj                              */
/*                  Control queue object handle                      */
/*                 MQHOBJ   hSubscriberObj                           */
/*                  Subscriber queue object handle                   */
/*                                                                   */
/* Output Parms  : pMatch_Node *ppFirstMatch                         */
/*                  Pointer to first match node in the linked list   */
/*                 PMQLONG  pCompCode                                */
/*                  Completion Code from MQ commands                 */
/*                 PMQLONG  pReason                                  */
/*                  Reason from MQ commands                          */
/*                                                                   */
/*********************************************************************/
void RestoreMatches( MQHCONN      hConn
                   , MQHOBJ       hControlObj
                   , MQHOBJ       hSubscriberObj
                   , pMatch_Node *ppFirstMatch
                   , PMQLONG      pCompCode
                   , PMQLONG      pReason )
{
  MQCHAR      subscriptionTopic[] = LATEST_SCORE_TOPIC "#";
  MQLONG      CompCode;
  MQLONG      Reason;
  MQLONG      messageLength;
  PMQBYTE     pMessageBlock = NULL;
  pMatch_Node pNewMatch;
  PMQRFH2     pMQRFHeader2;
  PMQCHAR     pNameValueData;
  PMQCHAR     pUserData;
  PMQCHAR     pTopicType;
  MQULONG     topicTypeLength;
  MQGMO       gmo = { MQGMO_DEFAULT };
  MQMD        md  = { MQMD_DEFAULT };
  PMQBYTE     blankSpace;
  BOOL        bRestoredMatches = FALSE;

  /*******************************************************************/
  /* Register as a subscriber to the LatestScore topic, we use the   */
  /* same queue as for the subscription to the Event publications.   */
  /* We register with the MQREGO_PUBLISH_ON_REQUEST_ONLY option, no  */
  /* publications on the topic will be sent to us without us asking  */
  /* (issuing a Request Update), we do this so that we know when all */
  /* the retained publications have been sent to us (the Request     */
  /* Update command issues a response).                              */
  /* We register with the STATE_CORREL_ID correlId as part of our    */
  /* subscriber identity, this means that all publications for this  */
  /* subscription will be sent with the STATE_CORREL_ID correlId.    */
  /* This will distinguish the new State publications from any Event */
  /* publications that have arrived on the subscriber queue since we */
  /* last read it (if we had ended without deregistering our Event   */
  /* subscription - i.e. if we failed unexpectedly). By getting from */
  /* the queue with MQGMO_MATCH_COOREL_ID we can receive all the     */
  /* State publications without disturbing the Event publications,   */
  /* which we will get later.                                        */
  /*******************************************************************/
  PubSubCommand( hConn
               , hControlObj
               , hSubscriberObj
               , MQPS_REGISTER_SUBSCRIBER
               , subscriptionTopic
               , (MQLONG)strlen(subscriptionTopic)
               , StateCorrelId
               , ( MQREGO_CORREL_ID_AS_IDENTITY
                 + MQREGO_PUBLISH_ON_REQUEST_ONLY )
               , pCompCode
               , pReason );

  if( *pCompCode == MQCC_OK )
  {
    /*****************************************************************/
    /* Issue the Request Update to receive all the retained          */
    /* publications that match our wildcard topic. The command is    */
    /* sent to the broker as a request and will only receive the     */
    /* reply from a local broker when all the publications have been */
    /* sent to us.                                                   */
    /*****************************************************************/
    PubSubCommand( hConn
                 , hControlObj
                 , hSubscriberObj
                 , MQPS_REQUEST_UPDATE
                 , subscriptionTopic
                 , (MQLONG)strlen(subscriptionTopic)
                 , StateCorrelId
                 , MQREGO_CORREL_ID_AS_IDENTITY
                 , pCompCode
                 , pReason );

    if( *pCompCode == MQCC_OK )
    {
      /***************************************************************/
      /* Allocate storage to receive a publication.                  */
      /***************************************************************/
      messageLength = DEFAULT_MESSAGE_SIZE;
      pMessageBlock = (PMQBYTE)malloc(DEFAULT_MESSAGE_SIZE);
      if( pMessageBlock == NULL )
      {
        printf("Unable to allocate storage\n");
        *pCompCode = MQCC_FAILED;
      }
      else
      {
        /*************************************************************/
        /* We do not need to get with wait as all the publications   */
        /* will have arrived on our queue by now (under normal       */
        /* circumstances). We will only get those publications that  */
        /* match our correlId.                                       */
        /*************************************************************/
        gmo.Options = MQGMO_NO_WAIT + MQGMO_CONVERT + MQGMO_NO_SYNCPOINT;
        gmo.Version = MQGMO_VERSION_2;
        gmo.MatchOptions = MQMO_MATCH_CORREL_ID;
        memcpy( md.CorrelId, StateCorrelId,
                                     (size_t)MQ_CORREL_ID_LENGTH);

        /*************************************************************/
        /* Get all the publications from the Request Update.         */
        /*************************************************************/
        while( *pCompCode == MQCC_OK )
        {
          MQGET( hConn
               , hSubscriberObj
               , &md
               , &gmo
               , DEFAULT_MESSAGE_SIZE
               , pMessageBlock
               , &messageLength
               , pCompCode
               , pReason );

          if( *pCompCode == MQCC_OK )
          {
            /*********************************************************/
            /* If we restore any matches, print a message to the     */
            /* screen.                                               */
            /*********************************************************/
            if( bRestoredMatches == FALSE )
            {
              printf("Restored Match details:\n\n");
              bRestoredMatches = TRUE;
            }

            /*********************************************************/
            /* Locate the NameValueData and the user data of the     */
            /* publication.                                          */
            /*********************************************************/
            pMQRFHeader2   = (PMQRFH2)pMessageBlock;
            pNameValueData = (PMQCHAR)(pMessageBlock
                                     + MQRFH_STRUC_LENGTH_FIXED_2
                                     + LENGTH_OF_LENGTH_FIELD);
            pUserData = (PMQCHAR)(pMessageBlock
                  + pMQRFHeader2->StrucLength);

            /*********************************************************/
            /* We need to locate the topic in the NameValueData as   */
            /* this contains the names of the two teams playing in   */
            /* the match. ExtractTopicType will return the last part */
            /* of the topic string (following LATEST_SCORE_TOPIC),   */
            /* this contains the names of the two teams playing.     */
            /*********************************************************/
            *pCompCode = ExtractTopicType( pNameValueData
                                         , ( pMQRFHeader2->StrucLength
                                           - MQRFH_STRUC_LENGTH_FIXED_2
                                           - LENGTH_OF_LENGTH_FIELD)
                                         , (char *)LATEST_SCORE_TOPIC
                                         , &pTopicType
                                         , &topicTypeLength );

            /*********************************************************/
            /* If we located the topic, allocate a new Match node    */
            /* to be added to the linked list of matches being       */
            /* played.                                               */
            /*********************************************************/
            if( *pCompCode == MQCC_OK )
            {
              pNewMatch = (pMatch_Node)malloc(sizeof(Match_Node));
              if( pNewMatch == NULL )
              {
                printf("Unable to allocate storage\n");
                *pCompCode = MQCC_FAILED;
              }
              else
              {
                /*****************************************************/
                /* Initialise the new Match node.                    */
                /*****************************************************/
                memset(pNewMatch, '\0', sizeof(Match_Node));

                /*****************************************************/
                /* The two team names in the topic are separated by  */
                /* a space, by locating the position of the space we */
                /* can split the topic into the two team names.      */
                /*****************************************************/
                blankSpace = (PMQBYTE)memchr(pTopicType, ' ', topicTypeLength);
                if( blankSpace != NULL )
                {
                  memcpy(pNewMatch->Team1, pTopicType,
                     (blankSpace - (PMQBYTE)pTopicType));
                  memcpy(pNewMatch->Team2, (blankSpace + 1),
                     (topicTypeLength - (blankSpace - (PMQBYTE)pTopicType) - 1));

                  /***************************************************/
                  /* Extract the latest score from the user data.    */
                  /***************************************************/
                  sscanf(pUserData,"%d %d", &(pNewMatch->Team1Score),
                                              &(pNewMatch->Team2Score));

                  /***************************************************/
                  /* Add the new match node to the linked list.      */
                  /***************************************************/
                  pNewMatch->pNextMatch = *ppFirstMatch;
                  *ppFirstMatch = pNewMatch;

                  printf("%s are playing %s\n",
                           pNewMatch->Team1, pNewMatch->Team2);
                  printf("  %s %d, %s %d\n\n",
                           pNewMatch->Team1, pNewMatch->Team1Score,
                           pNewMatch->Team2, pNewMatch->Team2Score );
                }
                /*****************************************************/
                /* No space was found in the topic name (separates   */
                /* the team names). This is an error.                */
                /*****************************************************/
                else
                {
                  printf("Invalid topic name:\n");
                  PrintNameValueData(pNameValueData,
                    (pMQRFHeader2->StrucLength - MQRFH_STRUC_LENGTH_FIXED_2));
                  *pCompCode = MQCC_FAILED;
                }
              }
            }
          }
        } /* end of while */
        /*************************************************************/
        /* If the MQGET failed with MQRC_NO_MSG_AVAILABLE it means   */
        /* we have successfully received all the publication.        */
        /*************************************************************/
        if( *pReason == MQRC_NO_MSG_AVAILABLE )
        {
          *pCompCode = MQCC_OK;
          *pReason = MQRC_NONE;
        }
        /*************************************************************/
        /* Free the allocated message block.                         */
        /*************************************************************/
        free( pMessageBlock );
      }
    }
    /*****************************************************************/
    /* Deregister our subscription from the State topics as we have  */
    /* no need for subscribing to the state publications once we     */
    /* have restored all the state.                                  */
    /* We perform the deregister even if the above work failed.      */
    /*****************************************************************/
    PubSubCommand( hConn
                 , hControlObj
                 , hSubscriberObj
                 , MQPS_DEREGISTER_SUBSCRIBER
                 , subscriptionTopic
                 , (MQLONG)strlen(subscriptionTopic)
                 , StateCorrelId
                 , MQREGO_CORREL_ID_AS_IDENTITY
                 , &CompCode
                 , &Reason );

    /*****************************************************************/
    /* If the deregister failed but everything else was okay we      */
    /* return the error, otherwise, we return the original error.    */
    /*****************************************************************/
    if( (CompCode != MQCC_OK)
      &&(*pCompCode == MQCC_OK) )
    {
      *pCompCode = CompCode;
      *pReason = Reason;
    }
  }
}
/*********************************************************************/
/* end of RestoreMatches                                             */
/*********************************************************************/


/*********************************************************************/
/*                                                                   */
/* Function Name : PubSubCommand                                     */
/*                                                                   */
/* Description   : Build the Publish/Subscribe command message       */
/*                 and send it to the broker.                        */
/*                                                                   */
/* Flow          :                                                   */
/*                                                                   */
/*  Build the MQRFH structure and NameValueData                      */
/*  MQPUT the command to the queue as a request                      */
/*   Wait for the response to arrive                                 */
/*                                                                   */
/* Input Parms   : MQHCONN  hConn                                    */
/*                  Queue manager connection handle                  */
/*                 MQHOBJ   hBrokerObj                               */
/*                  Control queue object handle                      */
/*                 MQHOBJ   hReplyObj                                */
/*                  Reply queue object handle                        */
/*                 MQCHAR   Command[]                                */
/*                  Publish/Subscibe command to perform              */
/*                 PMQCHAR  pTopic                                   */
/*                  Topic of the command                             */
/*                 MQLONG   topicLength                              */
/*                  Length of topic name                             */
/*                 const MQBYTE *pCorrelId                           */
/*                  CorrelId is use as identity (if any)             */
/*                 MQLONG   regOptions                               */
/*                  Registration options (if any)                    */
/*                                                                   */
/* Output Parms  : PMQLONG  pCompCode                                */
/*                  Completion Code from MQ commands                 */
/*                 PMQLONG  pReason                                  */
/*                  Reason from MQ commands                          */
/*                                                                   */
/*********************************************************************/
void PubSubCommand( MQHCONN       hConn
                  , MQHOBJ        hBrokerObj
                  , MQHOBJ        hReplyObj
                  , MQCHAR        Command[]
                  , PMQCHAR       pTopic
                  , MQLONG        topicLength
                  , const MQBYTE *pCorrelId
                  , MQLONG        regOptions
                  , PMQLONG       pCompCode
                  , PMQLONG       pReason )
{
  MQPMO   pmo = { MQPMO_DEFAULT };
  MQMD    md  = { MQMD_DEFAULT };
  MQLONG  messageLength;
  PMQBYTE pMessageBlock = NULL;

  /*******************************************************************/
  /* Allocate a block of storage to hold the Command message.        */
  /*******************************************************************/
  messageLength = DEFAULT_MESSAGE_SIZE;
  pMessageBlock = (PMQBYTE)malloc(messageLength);
  if( pMessageBlock == NULL )
  {
    printf("Unable to allocate storage\n");
    *pCompCode = MQCC_FAILED;
  }
  else
  {
    /*****************************************************************/
    /* Define an MQRFH2 structure at the start of the allocated      */
    /* storage, fill in the required fields and generate the         */
    /* NameValueData that follows it.                                */
    /*****************************************************************/
    BuildMQRFHeader2( pMessageBlock
                    , &messageLength
                    , Command
                    , regOptions
                    , MQPUBO_NONE
                    , pTopic );

    /*****************************************************************/
    /* Send the command as a request so that a reply is returned to  */
    /* us on completion at the broker.                               */
    /*****************************************************************/
    memcpy(md.Format, MQFMT_RF_HEADER_2, (size_t)MQ_FORMAT_LENGTH);
    md.MsgType = MQMT_REQUEST;
    /*****************************************************************/
    /* Specify the subscriber's queue in the ReplyToQ of the MD.     */
    /* We have not put the subscriber's queue in the MQRFH2          */
    /* NameValueData so the one in the ReplyToQ of the MD will be    */
    /* used as the identity of the subscriber.                       */
    /*****************************************************************/
    memcpy( md.ReplyToQ, SUBSCRIBER_QUEUE, MQ_Q_NAME_LENGTH);
    pmo.Options |= MQPMO_NEW_MSG_ID
                |  MQPMO_NO_SYNCPOINT;
    /*****************************************************************/
    /* All commands sent use the correlId as part of their identity. */
    /*****************************************************************/
    memcpy( md.CorrelId, pCorrelId , sizeof(MQBYTE24));

    /*****************************************************************/
    /* Put the command message to the broker control queue.          */
    /*****************************************************************/
    MQPUT( hConn
         , hBrokerObj
         , &md
         , &pmo
         , messageLength
         , pMessageBlock
         , pCompCode
         , pReason );

    if( *pCompCode != MQCC_OK )
      printf("MQPUT failed with CompCode %d and Reason %d\n",
                                               *pCompCode, *pReason);
    else
    {
      /***************************************************************/
      /* The put was successful, now wait for a response from the    */
      /* broker to inform us if the command was accepted by the      */
      /* broker.                                                     */
      /* We use our command storage block to receive the response    */
      /* into to save on allocating extra storage.                   */
      /***************************************************************/
      CheckForResponse( hConn
                      , hReplyObj
                      , &md
                      , pMessageBlock
                      , DEFAULT_MESSAGE_SIZE
                      , pCompCode
                      , pReason );
    }
    /*****************************************************************/
    /* Free the storage.                                             */
    /*****************************************************************/
    free( pMessageBlock );
  }
}
/*********************************************************************/
/* end of PubSubCommand                                              */
/*********************************************************************/


/*********************************************************************/
/*                                                                   */
/* Function Name : BuildMQRFHeader2                                  */
/*                                                                   */
/* Description   : Build an MQRFH2 header and the accompaning        */
/*                 NameValueData.                                    */
/*                                                                   */
/* Flow          :                                                   */
/*                                                                   */
/*  Initialise the message block to nulls                            */
/*  Define the start of the message as an MQRFH2                     */
/*  Set the default values of the MQRFH2                             */
/*  Define the NameValueData that follows the MQRFH2                 */
/*   Add the command                                                 */
/*   Add registration options if supplied                            */
/*   Add publication options if supplied                             */
/*   Add topic                                                       */
/*  Pad the folder XML to a 4 byte boundary                          */
/*  Set the StrucLength in the MQRFH2 to the total length so far     */
/*                                                                   */
/* Input Parms   : PMQBYTE  pStart                                   */
/*                  Start of message block                           */
/*                 PMQCHAR  pCommand                                 */
/*                  Command string                                   */
/*                 MQLONG   regOptions                               */
/*                  Registration options (if any)                    */
/*                 MQLONG   pubOptions                               */
/*                  Publish options (if any)                         */
/*                 PMQCHAR  pTopic                                   */
/*                  Topic of the command                             */
/*                                                                   */
/* Input/Output  : PMQLONG  pDataLength                              */
/*                  Size of message block on entry and amount of     */
/*                  block used on exit                               */
/*                                                                   */
/*********************************************************************/
void BuildMQRFHeader2( PMQBYTE   pStart
                     , PMQLONG   pDataLength
                     , PMQCHAR   pCommand
                     , MQLONG    regOptions
                     , MQLONG    pubOptions
                     , PMQCHAR   pTopic )
{
  PMQRFH2  pRFHeader = (PMQRFH2)pStart;
  PMQCHAR  pNameValueData;
  MQLONG   pscFolderLength;
  MQLONG   spacesToPad;

  /*******************************************************************/
  /* Clear the buffer before we start (initialise to nulls).         */
  /*******************************************************************/
  memset((PMQBYTE)pStart, '\0', *pDataLength);

  /*******************************************************************/
  /* Copy the MQRFH2 default values into the start of the buffer.    */
  /*******************************************************************/
  memcpy( pRFHeader, &DefaultMQRFH2, (size_t)MQRFH_STRUC_LENGTH_FIXED_2);

  /*******************************************************************/
  /* As we have user data following the MQRFH2 we must set the CCSID */
  /* of the user data in the MQRFH2 for data conversion to be able to*/
  /* be performed by the queue manager. As we do not currently know  */
  /* the CCSID that we are running in we can tell MQ that the  */
  /* data that follows the MQRFH2 is in the same CCSID as the MQRFH2.*/
  /* The MQRFH2 will default to the CCSID of the queue manager       */
  /* (MQCCSI_Q_MGR), so the user data will also inherit this CCSID.  */
  /* The NameValueCCSID needs to be set for the broker.              */
  /*******************************************************************/
  pRFHeader->CodedCharSetId = MQCCSI_INHERIT;
  pRFHeader->NameValueCCSID = 1208L;

  /*******************************************************************/
  /* Start the PSC folder directly after the MQRFH2 structure,       */
  /* leaving space for the length of the psc folder                  */
  /*******************************************************************/
  pNameValueData = (MQCHAR *)pRFHeader +
                     MQRFH_STRUC_LENGTH_FIXED_2 +
                     LENGTH_OF_LENGTH_FIELD;

  /*******************************************************************/
  /* Open the Psc Folder                                             */
  /* The psc folder will look like the following for subscribing     */
  /* <psc>                                                           */
  /*   <Command>RegSub/ReqUpdate</Command>                           */
  /*   <RegOpt>Reg options</RegOpt>                                  */
  /* </psc>                                                          */
  /* and for publishing.                                             */
  /* <psc>                                                           */
  /*   <Command>Publish</Command>                                    */
  /*   <PubOpt>RetainPub</PubOpt>                                    */
  /* </psc>                                                          */
  /*******************************************************************/
  strcpy(pNameValueData, MQRFH2_PUBSUB_CMD_FOLDER_B);

  /*******************************************************************/
  /* Add the command tag to the psc folder                           */
  /*******************************************************************/
  strcat(pNameValueData, MQPSC_COMMAND_B);
  strcat(pNameValueData, pCommand);
  strcat(pNameValueData, MQPSC_COMMAND_E);

  /*******************************************************************/
  /* If registration options were supplied add them to the string,   */
  /* for ease of implementation we insert the decimal representation */
  /* of the options into the string as opposed to the character      */
  /* strings supplied for each option.                               */
  /*******************************************************************/
  if( regOptions != 0 )
  {
    if (regOptions & MQREGO_CORREL_ID_AS_IDENTITY)
    {
      strcat(pNameValueData, MQPSC_REGISTRATION_OPTION_B);
      strcat(pNameValueData, MQPSC_CORREL_ID_AS_IDENTITY);
      strcat(pNameValueData, MQPSC_REGISTRATION_OPTION_E);
    }

    if (regOptions & MQREGO_PUBLISH_ON_REQUEST_ONLY)
    {
      strcat(pNameValueData, MQPSC_REGISTRATION_OPTION_B);
      strcat(pNameValueData, MQPSC_PUB_ON_REQUEST_ONLY);
      strcat(pNameValueData, MQPSC_REGISTRATION_OPTION_E);
    }
  }

  /*******************************************************************/
  /* The only publications option is to Retain the publication.      */
  /*******************************************************************/
  if( pubOptions != 0 )
  {
    strcat(pNameValueData, MQPSC_PUBLICATION_OPTION_B);
    strcat(pNameValueData, MQPSC_RETAIN_PUB);
    strcat(pNameValueData, MQPSC_PUBLICATION_OPTION_E);
  }

  /*******************************************************************/
  /* Add the topic to the NameValueData.                             */
  /*******************************************************************/
  strcat(pNameValueData, MQPSC_TOPIC_B);
  strcat(pNameValueData, pTopic);
  strcat(pNameValueData, MQPSC_TOPIC_E);

  /*******************************************************************/
  /* Close the psc folder as all the information is now complete.    */
  /*******************************************************************/
  strcat(pNameValueData, MQRFH2_PUBSUB_CMD_FOLDER_E);

  /*******************************************************************/
  /* Find the length of the psc folder.                              */
  /*******************************************************************/
  pscFolderLength = (MQLONG)strlen(pNameValueData);

  /*******************************************************************/
  /* Pad this to the four byte boundary.  Each folder in the RFH2    */
  /* needs to be padded to a four byte boundary field.               */
  /*******************************************************************/
  spacesToPad = (4 - (pscFolderLength%4))%4;
  while (spacesToPad-- > 0)
  {
    strcat(pNameValueData," ");
    pscFolderLength++;
  }

  pscFolderLength = (MQLONG)strlen(pNameValueData);

  /*******************************************************************/
  /* Copy the length of the folder field into the four byte gap left */
  /* when setting the pointer to the pNameValueData.                 */
  /*******************************************************************/
  memcpy((MQCHAR *)(pRFHeader) + MQRFH_STRUC_LENGTH_FIXED_2,
         &pscFolderLength, LENGTH_OF_LENGTH_FIELD);

  /*******************************************************************/
  /* Any user data that follows the NameValueData should start on    */
  /* a word boundary, to ensure all platforms are satisfied we align */
  /* to a 16 byte boundary.                                          */
  /* As the NameValueData has been null terminated (by using         */
  /* strcat) any characters between the end of the string and the    */
  /* next 16 byte boundary will be ignored by the broker, but if the */
  /* message is to be data converted we advise any extra characters  */
  /* are set to nulls ('\0') or blanks (' '). In this sample we have */
  /* initialised the whole message block to nulls before we started  */
  /* so all extra characters will be nulls by default.               */
  /*******************************************************************/
  *pDataLength = MQRFH_STRUC_LENGTH_FIXED_2 +
                 (MQLONG)strlen(pNameValueData) +
                 LENGTH_OF_LENGTH_FIELD;

  pRFHeader->StrucLength = *pDataLength;
}
/*********************************************************************/
/* end of BuildMQRFHeader2                                           */
/*********************************************************************/


/*********************************************************************/
/*                                                                   */
/* Function Name : CheckForResponse                                  */
/*                                                                   */
/* Description   : Wait for a reply to arrive for a command sent     */
/*                 to the broker and check for any errors.           */
/*                                                                   */
/* Flow          :                                                   */
/*                                                                   */
/*  MQGET the response message                                       */
/*   Locate the NameValueData in the message                         */
/*    Check for an 'OK' NameValueData                                */
/*    If the response is not okay:                                   */
/*     Set the CompCode and Reason from the message                  */
/*     Display the error                                             */
/*                                                                   */
/* Input Parms   : MQHCONN  hConn                                    */
/*                  Queue manager connection handle                  */
/*                 MQHOBJ   hObj                                     */
/*                  Reply queue object handle                        */
/*                 MMQMD    pMd                                      */
/*                  Pointer to message descriptor from MQPUT         */
/*                 PMQBYTE  pMessageBlock                            */
/*                  Start of message block to receive reply into     */
/*                 MQLONG   blockSize                                */
/*                  Length of message block                          */
/*                                                                   */
/* Output Parms  : PMQLONG  pCompCode                                */
/*                  Completion Code from MQ commands                 */
/*                 PMQLONG  pReason                                  */
/*                  Reason from MQ commands                          */
/*                                                                   */
/*********************************************************************/
void CheckForResponse( MQHCONN  hConn
                     , MQHOBJ   hObj
                     , PMQMD    pMd
                     , PMQBYTE  pMessageBlock
                     , MQLONG   blockSize
                     , PMQLONG  pCompCode
                     , PMQLONG  pReason )
{
  MQGMO    gmo = { MQGMO_DEFAULT };
  MQLONG   messageLength;
  PMQRFH2  pMQRFHeader;
  PMQCHAR  pNameValueData;
  PMQCHAR  pInputNameValueData;
  MQULONG  stringLength;
  MQCHAR   completion[8];
  MQCHAR   reason[4] = "   ";
  PMQCHAR  reason2;
  MQLONG   posCompletion;
  MQLONG   posCompletionClose;
  MQLONG   i;

  /*******************************************************************/
  /* Wait for a response message to arrive on our subscriber queue,  */
  /* the response's correlId will be the same as the messageId that  */
  /* the original message was sent with (returned in the md from the */
  /* MQPUT) so match against this.                                   */
  /*******************************************************************/
  gmo.Options = MQGMO_WAIT + MQGMO_CONVERT + MQGMO_NO_SYNCPOINT;
  gmo.WaitInterval = MAX_RESPONSE_TIME;
  gmo.Version = MQGMO_VERSION_2;
  gmo.MatchOptions = MQMO_MATCH_CORREL_ID;
  memcpy( pMd->CorrelId, pMd->MsgId, sizeof(MQBYTE24));
  memset( pMd->MsgId, '\0', sizeof(MQBYTE24));

  MQGET( hConn
       , hObj
       , pMd
       , &gmo
       , blockSize
       , pMessageBlock
       , &messageLength
       , pCompCode
       , pReason );


  if( *pCompCode != MQCC_OK )
  {
    printf("MQGET failed with CompCode %d and Reason %d\n",
                                               *pCompCode, *pReason);
    if( *pReason == MQRC_NO_MSG_AVAILABLE )
      printf("No response was sent by the broker, check the queued "
             "Pub/Sub interface is enabled.\n");
  }
  else
  {
    /*****************************************************************/
    /* Check that the message is in the MQRFH2format.                */
    /*****************************************************************/
    if( memcmp(pMd->Format, MQFMT_RF_HEADER_2, MQ_FORMAT_LENGTH) == 0 )
    {
      /***************************************************************/
      /* Locate the start of the NameValueData and its length.       */
      /***************************************************************/
      pMQRFHeader = (PMQRFH2)pMessageBlock;
      pNameValueData = (PMQCHAR)(pMessageBlock
                                          + MQRFH_STRUC_LENGTH_FIXED_2)
                                          + 4;
      stringLength = pMQRFHeader->StrucLength
                                          - MQRFH_STRUC_LENGTH_FIXED_2
                                          - 4;

      /***************************************************************/
      /* The start of a response pscr folder is always in the        */
      /* same format:                                                */
      /* <pscr>                                                      */
      /*   <Completion>Completion String eg(ok)</Completion>         */
      /*   <Reason>Reason if not ok</Reason>                         */
      /* </pscr>                                                     */
      /* We can scan the start of the string to check the CompCode   */
      /* and reason of the reply.                                    */
      /***************************************************************/

      /*Need to get completion and error codes from the xml response.*/
      *pReason = 0;


      posCompletion = (MQLONG)(strstr(pNameValueData, MQPSCR_COMPLETION_B) -
                               pNameValueData + strlen(MQPSCR_COMPLETION_B));
      posCompletionClose = (MQLONG)(strstr(pNameValueData,MQPSCR_COMPLETION_E) -
                                    pNameValueData);

      for (i = 0; i<posCompletionClose- posCompletion; i++)
      {
        completion[i] = pNameValueData[i + posCompletion];
      }

      completion[i] = '\0';

      posCompletion = (MQLONG)(strstr(pNameValueData, MQPSCR_REASON_B) -
                               pNameValueData + strlen(MQPSCR_REASON_B));
      posCompletionClose = (MQLONG)(strstr(pNameValueData, MQPSCR_REASON_E) -
                                    pNameValueData);
      if (posCompletion > 0)
      {
        reason2 = pNameValueData + posCompletion;
        strncpy(reason, reason2 , posCompletionClose-posCompletion);
        reason2 = reason;
        *pReason = strtol(reason2, (char**)NULL, 10);

        pNameValueData = (char *)"";
      }
      /*************************************************************/
      /* Check that the completion returned is ok                  */
      /*************************************************************/
      if (strcmp(completion, "ok") == 0)
        *pCompCode = MQCC_OK;
      else
        *pCompCode = MQCC_FAILED;

      if( *pCompCode != MQCC_OK )
      {
        /*************************************************************/
        /* One possible error is acceptable, MQRCCF_NO_RETAINED_MSG, */
        /* which is returned from a Request Update when there is no  */
        /* retained message on the broker. This is an allowable      */
        /* error so we can continue as before.                       */
        /*************************************************************/
        if( *pReason == MQRCCF_NO_RETAINED_MSG )
        {
          *pCompCode = MQCC_OK;
          *pReason = MQRC_NONE;
        }
        /*************************************************************/
        /* Otherwise, display the error message supplied with the    */
        /* user data that was returned, this will be the original    */
        /* commands NameValueData.                                 */
        /*************************************************************/
        else
        {
          /***********************************************************/
          /* Display the RFH2 response information                   */
          /***********************************************************/
          if( messageLength != MQRFH_STRUC_LENGTH_FIXED_2)
          {
            printf("Original Command String:\n");
            pInputNameValueData =
                   (PMQCHAR)(pMessageBlock  + MQRFH_STRUC_LENGTH_FIXED_2
                                          + 4);
            PrintNameValueData(pInputNameValueData,
                           (pMQRFHeader->StrucLength
                                          - MQRFH_STRUC_LENGTH_FIXED_2
                                          - 4));
          }
        }
      }
    }
    /*****************************************************************/
    /* If the message is not in the MQRFH format we have the wrong   */
    /* message.                                                      */
    /*****************************************************************/
    else
    {
      printf("Unexpected message format: %.8s\n", pMd->Format );
      *pCompCode = MQCC_FAILED;
    }
  }
}
/*********************************************************************/
/* end of CheckForResponse                                           */
/*********************************************************************/


/*********************************************************************/
/*                                                                   */
/* Function Name : ExtractTopicType                                  */
/*                                                                   */
/* Description   : Locate the topic value in the NameValueData       */
/*                 and extract the topic suffix that follows the     */
/*                 topic prefix that we pass in.                     */
/*                                                                   */
/* Flow          :                                                   */
/*                                                                   */
/*  While we have not reached the end of the NameValueData or        */
/*  until we find the topic:                                         */
/*   GetNextToken from the string (name)                             */
/*   GetNextToken from the string (value)                            */
/*   If the name token is Topic process the value                    */
/*    Match the start of the topic to the prefix                     */
/*     return the suffix of the topic                                */
/*  end while                                                        */
/*                                                                   */
/* Input Parms   : PMQCHAR  pNameValueData                           */
/*                  Pointer to the start of the NameValueData        */
/*                 MQLONG   stringLength                             */
/*                  Length of the NameValueData                      */
/*                 PMQCHAR  TopicPrefix                              */
/*                  Topic prefix string                              */
/*                                                                   */
/* Output Parms  : PMQCHAR *ppTopicType                              */
/*                  Pointer to start of topic suffix                 */
/*                 MQULONG *pTopicTypeLength                         */
/*                  Length of topic suffix                           */
/*                                                                   */
/*********************************************************************/
MQLONG ExtractTopicType( PMQCHAR  pNameValueData
                       , MQLONG   stringLength
                       , PMQCHAR  TopicPrefix
                       , PMQCHAR *ppTopicType
                       , MQULONG *pTopicTypeLength )
{
  MQLONG  rc = OK;
  BOOL    bTopicFound = FALSE;
  MQLONG  remainingLength = stringLength;
  PMQBYTE pTag;
  MQULONG tagLength;
  PMQBYTE pValue;
  MQULONG valueLength;
  MQULONG topicPrefixLength;

  /*******************************************************************/
  /* Search along the NameValueData until we find the Topic          */
  /* name token, or we get to the end of the string (an error).      */
  /* Note: This only expects a single topic in the NameValueData,    */
  /*       it is possible to have multiple topics specified in a     */
  /*       single publication.                                       */
  /*******************************************************************/
  while( (rc == OK)
       &&(remainingLength > 0)
       &&(bTopicFound == FALSE) )
  {
    /*****************************************************************/
    /* The NameValueData at this point will be  either:              */
    /* <psc><Command>Publish</Command>.....  or                      */
    /* </Command><Topic>topic name</Topic>.....                      */
    /* By calling the GetNextToken, this sets the pointers to the    */
    /* next tag.                                                     */
    /*****************************************************************/
    rc = GetNextToken( &pNameValueData
                     , &remainingLength
                     , &pTag
                     , &tagLength );
    /*****************************************************************/
    /* Locate the next blank delimited token in the string. The      */
    /* tokens always occur in pairs so this will always be a name    */
    /* token and the following token its value.                      */
    /*****************************************************************/
    rc = GetNextToken( &pNameValueData
                     , &remainingLength
                     , &pTag
                     , &tagLength );

    /*****************************************************************/
    /* If a token was found continue, otherwise we have reached the  */
    /* end of the string.                                            */
    /*****************************************************************/
    if( (rc == OK)
      &&(tagLength > 0) )
    {
      /***************************************************************/
      /* Locate the above name token's corresponding value.          */
      /***************************************************************/
      rc = GetNextToken( &pNameValueData
                       , &remainingLength
                       , &pValue
                       , &valueLength );
      if( rc == OK )
      {
        /*************************************************************/
        /* If a token was found continue, otherwise we have a name   */
        /* token with no value, this is an error.                    */
        /*************************************************************/
        if( valueLength == 0 )
        {
          printf("Odd number of tokens found in NameValueData\n");
          rc = FAILURE;
        }
        /*************************************************************/
        /* If the name token is 'Topic' we have found what we        */
        /* were looking for and we must parse the topic value.       */
        /*************************************************************/
        else if( (tagLength == strlen(MQPSC_TOPIC))
               &&(memcmp(pTag, MQPSC_TOPIC, strlen(MQPSC_TOPIC)) == 0) )
        {
          bTopicFound = TRUE;
          /***********************************************************/
          /* If the first character of the topic value returned by   */
          /* the tokenizer is a '"' then the topic is contained in   */
          /* quotes and these can be stripped off.                   */
          /* Note: It is known that no publications we are expecting */
          /*       will contain embedded quotes so there is no       */
          /*       requirement here to remove them. In a general     */
          /*       system replacement of '""' with '"' may be        */
          /*       required.                                         */
          /***********************************************************/
          if( *pValue == '"' )
          {
            pValue++;
            valueLength -= 2;
          }
          /***********************************************************/
          /* Extract the substring of the topic that follows the     */
          /* topic prefix that was passed to this function.          */
          /* e.g. if the topic is Sport/Soccer/Event/MatchStarted    */
          /*      and the prefix given was Sport/Soccer/Event/       */
          /*      we would return 'MatchStarted'.                    */
          /***********************************************************/
          topicPrefixLength = (MQULONG)strlen(TopicPrefix);
          if( (valueLength <= topicPrefixLength)
            ||(memcmp(pValue, TopicPrefix, topicPrefixLength) != 0) )
          {
            /*********************************************************/
            /* The start of the topic found does not match the       */
            /* prefix passed to this function, this is an error.     */
            /*********************************************************/
            printf("Unexpected publication topic\n");
            rc = FAILURE;
          }
          else
          {
            *ppTopicType = (PMQCHAR)(pValue + topicPrefixLength);
            *pTopicTypeLength = valueLength - topicPrefixLength;
          }
        }
      }
    }
  }
  /*******************************************************************/
  /* No topic token was found in the NameValueData, this is an     */
  /* error as the topic is a mandatory parameter for a publication.  */
  /*******************************************************************/
  if( bTopicFound == FALSE )
  {
    printf("No topic found in NameValueData\n");
    rc = FAILURE;
  }
  return(rc);
}
/*********************************************************************/
/* end of ExtractTopicType                                           */
/*********************************************************************/


/*********************************************************************/
/*                                                                   */
/* Function Name : GetNextToken                                      */
/*                                                                   */
/* Description   : Parse the supplied string for the start and end   */
/*                 of a valid token.  This should in general be done */
/*                 by an xml parser.                                 */
/*                                                                   */
/*                                                                   */
/* Flow          :                                                   */
/*                                                                   */
/*  While we have not reached the end of the string or a token:      */
/*   Process the current character:                                  */
/*    '\0'  - End of string                                          */
/*    '>'   - End of token (not in quotes)                           */
/*    '"'   - Start or end of token in quotes or embedded quote      */
/*    '<'   - Beginning of a new Token                               */
/*    Other - Character in token                                     */
/*  end while                                                        */
/*                                                                   */
/* Input/Output  : PMQCHAR  *ppNameValueData                         */
/*                  Pointer to current position in NameValueData    */
/*                  on input and pointer to new position following  */
/*                  any token that was found on output               */
/*                 PMQLONG  pRemainingLength                         */
/*                  Length of NameValueData on input and length      */
/*                  of remaining string on output                    */
/*                                                                   */
/* Output Parms  : PMQBYTE  *ppToken                                 */
/*                  Pointer to start of token found                  */
/*                 MQULONG  *pTokenLength                            */
/*                  Length of token found                            */
/*                                                                   */
/*********************************************************************/
MQLONG GetNextToken( PMQCHAR *ppNameValueData
                   , PMQLONG  pRemainingLength
                   , PMQBYTE *ppToken
                   , MQULONG *pTokenLength )
{
  MQLONG       rc = OK;
  PMQCHAR      pCurrentChar = *ppNameValueData;
  Parser_State CurrentState = OutOfToken;   /* Finite state variable */

  /*******************************************************************/
  /* Set the output values to blank.                                 */
  /*******************************************************************/

  *ppToken = NULL;
  *pTokenLength = 0;

  /*******************************************************************/
  /* Step along the string one character at a time until either we   */
  /* reach the end of the string or the end of a token is reached.   */
  /* The processing of the string is based on a finite state machine */
  /* that has the following states:                                  */
  /*                                                                 */
  /*                                                                 */
  /* OutOfToken--->InToken----------------------->EndOfToken         */
  /*            |                             ^                      */
  /*            |                             |                      */
  /*             ->InQuotes--->EmbeddedQuote--                       */
  /*                  ^             |                                */
  /*                  |             |                                */
  /*                   -------------                                 */
  /*                                                                 */
  /*******************************************************************/
  while( (*pRemainingLength > 0)
       &&(CurrentState != EndOfToken)
       &&(rc == OK) )
  {
    switch( *pCurrentChar )
    {
      /***************************************************************/
      /* '\0' (null character)                                       */
      /* A null character is treated as a string terminator and any  */
      /* data in the string following the null is ignored.           */
      /* If we are either currently in a token (InToken) or the      */
      /* previous character was a '"' inside a quoted string         */
      /* (EmbeddedQuote) we can accept the null as the delimiter of  */
      /* the token and move to the EndOfToken State.                 */
      /***************************************************************/
      case '\0' :
        if( (CurrentState == EmbeddedQuote)
          ||(CurrentState == InToken) )
          CurrentState = EndOfToken;
        /*************************************************************/
        /* This is treated as the last character in the string so    */
        /* remaining length is currently this single character (1).  */
        /*************************************************************/
        *pRemainingLength = 1;
        break;

      /***************************************************************/
      /* '<'                                                         */
      /* This character signifies the open for a new tag value.      */
      /* If we are already parsing a value, then this is the end of  */
      /* the value and a new tag will start here.  Otherwise, this   */
      /* is a new tag and increment the pointer by one as the '<'    */
      /* is not needed in the return value.                          */
      /***************************************************************/
      case '<' :
        if( CurrentState == OutOfToken )
        {
          CurrentState = InToken;
          *ppToken = (PMQBYTE)pCurrentChar + 1;
        }
        else if (CurrentState == InToken)
          CurrentState = EndOfToken;
        break;

      /***************************************************************/
      /* '>'                                                         */
      /* This character signifies the end for a tag.                 */
      /* Change the current state to an end of token.                */
      /***************************************************************/
      case '>' :
        if( (CurrentState == EmbeddedQuote)
          ||(CurrentState == InToken) )
          CurrentState = EndOfToken;
        break;

      /***************************************************************/
      /* '"' (quote character)                                       */
      /***************************************************************/
      case '"' :
        switch( CurrentState )
        {
          /***********************************************************/
          /* If we are currently not yet in a token then the quote   */
          /* signifies that the token is to be enclosed in quotes    */
          /* and the token can be entered at this point (changes     */
          /* state to InQuotes).                                     */
          /***********************************************************/
          case OutOfToken :
            CurrentState = InQuotes;
            *ppToken = (PMQBYTE)pCurrentChar;
            break;
          /***********************************************************/
          /* If we are already inside a quote enclosed token  '"'    */
          /* character can either be the closing quote character     */
          /* (which must be followed by a delimiter character - see  */
          /* above) or it is an embedded quote (immediately          */
          /* followed by anther '"'). We change state to             */
          /* EmbeddedQuote and continue, testing the next character  */
          /* to determine the nature of this quote.                  */
          /***********************************************************/
          case InQuotes :
            CurrentState = EmbeddedQuote;
            break;
          /***********************************************************/
          /* The previous character is also a '"', therefore, the    */
          /* two quotes are an escaped quote embedded in the quoted  */
          /* token, we can change the state back to InQuotes and     */
          /* continue to find the end of the token.                  */
          /***********************************************************/
          case EmbeddedQuote :
            CurrentState = InQuotes;
            break;
          /***********************************************************/
          /* The only other state we could be in at this stage is    */
          /* InToken, it is invalid to have a '"' character embedded */
          /* in an un-quotes token.                                  */
          /***********************************************************/
          default :
            rc = FAILURE;
            break;
        }
        break;

      /***************************************************************/
      /* Any other character                                         */
      /***************************************************************/
      default :
        switch( CurrentState )
        {
          /***********************************************************/
          /* Any non-delimiter character that follows either the     */
          /* start of the string or a delimiter character is taken   */
          /* as the start of a token, as this character cannot be a  */
          /* '"' (tested above) the token is not enclosed in quotes  */
          /* and the state is changed to InToken.                    */
          /***********************************************************/
          case OutOfToken :
            CurrentState = InToken;
            *ppToken = (PMQBYTE)pCurrentChar;
            break;
          /***********************************************************/
          /* An embedded quote ('"') found in a quoted token must be */
          /* followed either by another '"' (escaped quotes) or a    */
          /* delimiter character ('>' or '\0'), otherwise it is an   */
          /* error.                                                  */
          /***********************************************************/
          case EmbeddedQuote :
            rc = FAILURE;
            break;
          default :
            break;
        }
        break;
    } /* end of switch( *pCurrentChar ) */
    /*****************************************************************/
    /* Step to the next character in the string and decrement the    */
    /* length of the remaining string.                               */
    /*****************************************************************/
    pCurrentChar++;
    (*pRemainingLength)--;
  } /* end of while */

  if( rc == OK )
  {
    /*****************************************************************/
    /* We have either reached the end of the string or the end of a  */
    /* token, check the state to determine which it is.              */
    /*****************************************************************/
    switch( CurrentState )
    {
      /***************************************************************/
      /* InToken, EmbeddedQuote or EndOfToken are all valid states   */
      /* to be in after finding a complete token. Calculate the      */
      /* length of the token before returning to the calling         */
      /* function. If we are in the EndOfToken state we will have    */
      /* moved on an extra character so take that into account.      */
      /***************************************************************/
      case InToken :
      case EmbeddedQuote :
        *pTokenLength = (MQULONG)((PMQBYTE)pCurrentChar - *ppToken);
        break;
      case EndOfToken :
        *pTokenLength = (MQULONG)((PMQBYTE)(pCurrentChar - 1) - *ppToken);
        break;
      /***************************************************************/
      /* To be in the OutOfToken state we could not have found even  */
      /* the start of a token, this could still be a valid state as  */
      /* the end of the string has been reached.                     */
      /***************************************************************/
      case OutOfToken :
        break;
      /***************************************************************/
      /* Any other state (InQuotes) are invalid states to end in, it */
      /* implies the start of a token has been found but not the end */
      /***************************************************************/
      default :
        rc = FAILURE;
        break;
    }
  }

  /*******************************************************************/
  /* Move the string pointer to the point in the string that we have */
  /* got to so that the next call to this function will start at     */
  /* this point.                                                     */
  /*******************************************************************/
  *ppNameValueData = pCurrentChar;

  if( rc != OK )
    printf("Invalid NameValueData\n");

  return(rc);
}
/*********************************************************************/
/* end of GetNextToken                                               */
/*********************************************************************/


/*********************************************************************/
/*                                                                   */
/* Function Name : AddNewMatch                                       */
/*                                                                   */
/* Description   : Add the match details to the match linked list    */
/*                 and publish a retained publication with the       */
/*                 details.                                          */
/*                                                                   */
/* Flow          :                                                   */
/*                                                                   */
/*   Create new match node                                           */
/*    Initialise to 0-0 score                                        */
/*   Publish on 'Sport/Soccer/State/LatestScore/team1 team2'         */
/*                                                                   */
/* Input Parms   : MQHCONN  hConn                                    */
/*                  Connecting handle to queue manager               */
/*                 pMatch_Teams pTeams                               */
/*                  Structure of two team names                      */
/*                 MQHOBJ   hStreamObj                               */
/*                  Object handle of stream queue                    */
/*                                                                   */
/* Input/Output  : pMatch_Node *ppFirstMatch                         */
/*                  Pointer to start of Match linked list on input,  */
/*                  if this is the first match a pointer to the      */
/*                  new match node is returned                       */
/*                                                                   */
/* Outputparms   : PMQLONG  pCompCode                                */
/*                  Completion code of MQ commands                   */
/*                 PMQLONG  pReason                                  */
/*                  Reason returned from MQ commands                 */
/*                                                                   */
/*********************************************************************/
void AddNewMatch( MQHCONN       hConn
                , pMatch_Teams  pTeams
                , pMatch_Node  *ppFirstMatch
                , MQHOBJ        hStreamObj
                , PMQLONG       pCompCode
                , PMQLONG       pReason )
{
  pMatch_Node  pNewMatch = NULL;
  MQCHAR       buffer[256];
  pMatch_Node  pMatch;
  PMQCHAR      pMatchingTeam;
  PMQCHAR      pMatchingOpponent;

  /*******************************************************************/
  /* Search the existing matches for the new teams, if we find one   */
  /* of the teams is already in a match all we can do is issue an    */
  /* error to the screen and continue, it is up to the user to stop  */
  /* the amqsgr2 sample with the matching team name.                 */
  /*******************************************************************/
  pMatch = *ppFirstMatch;
  while( (pMatch != NULL)
       &&(strcmp(pTeams->Team1, pMatch->Team1) != 0)
       &&(strcmp(pTeams->Team1, pMatch->Team2) != 0)
       &&(strcmp(pTeams->Team2, pMatch->Team1) != 0)
       &&(strcmp(pTeams->Team2, pMatch->Team2) != 0) )
  {
    pMatch = pMatch->pNextMatch;
  }

  /*******************************************************************/
  /* The new team names were not found in an existing match, create  */
  /* a new match node.                                               */
  /*******************************************************************/
  if( pMatch == NULL )
  {
    /*****************************************************************/
    /* Allocate a new linked list node to hold the new match details.*/
    /*****************************************************************/
    pNewMatch = (pMatch_Node)malloc(sizeof(Match_Node));
    if( pNewMatch == NULL )
    {
      printf("Unable to allocate storage\n");
      *pCompCode = MQCC_FAILED;
    }
    else
    {
      /***************************************************************/
      /* Initialise the match node, extracting the names of the      */
      /* teams playing from the user data of the publication.        */
      /***************************************************************/
      memcpy(pNewMatch->Team1, pTeams->Team1, sizeof(pTeams->Team1));
      memcpy(pNewMatch->Team2, pTeams->Team2, sizeof(pTeams->Team2));
      pNewMatch->Team1Score = 0;
      pNewMatch->Team2Score = 0;


      /***************************************************************/
      /* Add the new node to the front of the list.                  */
      /***************************************************************/
      pNewMatch->pNextMatch = *ppFirstMatch;
      *ppFirstMatch = pNewMatch;

      /***************************************************************/
      /* Publish a retained message to record the current state of   */
      /* this match so that we can recover after a failure.          */
      /***************************************************************/
      UpdateLatestScorePub( hConn
                          , pNewMatch
                          , hStreamObj
                          , pCompCode
                          , pReason
                          , FALSE );

      sprintf(buffer,"LATEST: %s 0, %s 0\n\n",
              pTeams->Team1, pTeams->Team2 );
      TeleType(buffer);
    }
  }
  /*******************************************************************/
  /* One of the new teams is already playing in a match, display an  */
  /* error, indicating which team is already playing and against     */
  /* who.                                                            */
  /*******************************************************************/
  else
  {
    if( strcmp(pTeams->Team1, pMatch->Team1) ==0 )
    {
      pMatchingTeam = pTeams->Team1;
      pMatchingOpponent = pMatch->Team2;
    }
    else if( strcmp(pTeams->Team1, pMatch->Team2) ==0 )
    {
      pMatchingTeam = pTeams->Team1;
      pMatchingOpponent = pMatch->Team1;
    }
    else if( strcmp(pTeams->Team2, pMatch->Team1) ==0 )
    {
      pMatchingTeam = pTeams->Team2;
      pMatchingOpponent = pMatch->Team2;
    }
    else if( strcmp(pTeams->Team2, pMatch->Team2) ==0 )
    {
      pMatchingTeam = pTeams->Team2;
      pMatchingOpponent = pMatch->Team1;
    }
    sprintf(buffer,"ERROR: %s is already playing against %s\n",
             pMatchingTeam, pMatchingOpponent );
    TeleType(buffer);
    sprintf(buffer,"       further results for this match will be inaccurate\n\n");
    TeleType(buffer);
  }
}
/*********************************************************************/
/* end of AddNewMatch                                                */
/*********************************************************************/


/*********************************************************************/
/*                                                                   */
/* Function Name : EndMatch                                          */
/*                                                                   */
/* Description   : Delete the retained publication for this match    */
/*                 and remove the match node from the linked list.   */
/*                                                                   */
/* Flow          :                                                   */
/*                                                                   */
/*  Locate match node                                                */
/*   Delete retained publication for this match                      */
/*   Delete match node                                               */
/*                                                                   */
/* Input Parms   : MQHCONN  hConn                                    */
/*                  Connecting handle to queue manager               */
/*                 pMatch_Teams pTeams                               */
/*                  Structure of two team names                      */
/*                 MQHOBJ   hStreamObj                               */
/*                  Object handle of stream queue                    */
/*                                                                   */
/* Input/Output  : pMatch_Node *ppFirstMatch                         */
/*                  Pointer to start of Match linked list on input,  */
/*                  if this is the last match a NULL pointer is      */
/*                  returned as there will be no nodes in the list   */
/*                                                                   */
/* Outputparms   : PMQLONG  pCompCode                                */
/*                  Completion code of MQ commands                   */
/*                 PMQLONG  pReason                                  */
/*                  Reason returned from MQ commands                 */
/*                                                                   */
/*********************************************************************/
void EndMatch( MQHCONN       hConn
             , pMatch_Teams  pTeams
             , pMatch_Node  *ppFirstMatch
             , MQHOBJ        hStreamObj
             , PMQLONG       pCompCode
             , PMQLONG       pReason )
{
  pMatch_Node  pMatch;
  pMatch_Node  pPreviousMatch;
  MQCHAR       buffer[256];

  /*******************************************************************/
  /* Locate the match that has ended in the match list, compare the  */
  /* names of the teams in the user data with those in the list.     */
  /*******************************************************************/
  pPreviousMatch = NULL;
  pMatch = *ppFirstMatch;
  while( (pMatch != NULL)
       &&( (strcmp(pTeams->Team1, pMatch->Team1) != 0)
         ||(strcmp(pTeams->Team2, pMatch->Team2) != 0) ) )
  {
    pPreviousMatch = pMatch;
    pMatch = pMatch->pNextMatch;
  }

  if( pMatch != NULL )
  {
    /*****************************************************************/
    /* If we located the match, delete the retained publication for  */
    /* this match.                                                   */
    /*****************************************************************/
    UpdateLatestScorePub( hConn
                        , pMatch
                        , hStreamObj
                        , pCompCode
                        , pReason
                        , TRUE );

    /*****************************************************************/
    /* Display the final score.                                      */
    /*****************************************************************/
    sprintf(buffer,"FULLTIME: %s %d, %s %d\n\n",
             pMatch->Team1, pMatch->Team1Score,
             pMatch->Team2, pMatch->Team2Score );
    TeleType(buffer);

    /*****************************************************************/
    /* Remove this match from the match list.                        */
    /*****************************************************************/
    if( pPreviousMatch == NULL )
      *ppFirstMatch = pMatch->pNextMatch;
    else
      pPreviousMatch->pNextMatch = pMatch->pNextMatch;
    free( pMatch );
  }
  /*******************************************************************/
  /* The match was not found in the list, report an error.           */
  /*******************************************************************/
  else
  {
    sprintf(buffer, "Match between %s and %s was not found\n\n",
                                         pTeams->Team1, pTeams->Team2);
    TeleType(buffer);
  }
}
/*********************************************************************/
/* end of EndMatch                                                   */
/*********************************************************************/


/*********************************************************************/
/*                                                                   */
/* Function Name : UpdateScore                                       */
/*                                                                   */
/* Description   : Update the score in the match that the team in    */
/*                 the publication was playing in and publish with   */
/*                 the new details.                                  */
/*                                                                   */
/* Flow          :                                                   */
/*                                                                   */
/*   Locate match node                                               */
/*    Update score in match node                                     */
/*    Publish on 'Sport/Soccer/State/LatestScore/team1 team2'        */
/*                                                                   */
/* Input Parms   : MQHCONN  hConn                                    */
/*                  Connecting handle to queue manager               */
/*                 PMQCHAR  ScoringTeam                              */
/*                  Name of team                                     */
/*                 pMatch_Node pFirstMatch                           */
/*                  Pointer to start of Match linked list            */
/*                 MQHOBJ   hStreamObj                               */
/*                  Object handle of stream queue                    */
/*                                                                   */
/* Outputparms   : PMQLONG  pCompCode                                */
/*                  Completion code of MQ commands                   */
/*                 PMQLONG  pReason                                  */
/*                  Reason returned from MQ commands                 */
/*                                                                   */
/*********************************************************************/
void UpdateScore( MQHCONN       hConn
                , PMQCHAR       ScoringTeam
                , pMatch_Node   pFirstMatch
                , MQHOBJ        hStreamObj
                , PMQLONG       pCompCode
                , PMQLONG       pReason )
{
  pMatch_Node  pMatch;
  MQCHAR       buffer[256];

  /*******************************************************************/
  /* Locate the match that scored in the match list.                 */
  /*******************************************************************/
  pMatch = pFirstMatch;
  while( (pMatch != NULL)
       &&(strcmp(ScoringTeam, pMatch->Team1) != 0)
       &&(strcmp(ScoringTeam, pMatch->Team2) != 0) )
  {
    pMatch = pMatch->pNextMatch;
  }

  if( pMatch != NULL )
  {
    /*****************************************************************/
    /* If Team1 scored increment their score, otherwise, increment   */
    /* Team2's score.                                                */
    /*****************************************************************/
    if( strcmp(ScoringTeam, pMatch->Team1) == 0 )
      (pMatch->Team1Score)++;
    else
      (pMatch->Team2Score)++;

    /*****************************************************************/
    /* Update the score held in the retained publication for this    */
    /* match.                                                        */
    /*****************************************************************/
    UpdateLatestScorePub( hConn
                        , pMatch
                        , hStreamObj
                        , pCompCode
                        , pReason
                        , FALSE );

    sprintf(buffer, "LATEST: %s %d, %s %d\n\n",
             pMatch->Team1, pMatch->Team1Score,
             pMatch->Team2, pMatch->Team2Score );
    TeleType(buffer);
  }
  /*******************************************************************/
  /* The match was not found in the list, report an error.           */
  /*******************************************************************/
  else
  {
    sprintf(buffer,"%s is not playing in a match\n\n", ScoringTeam);
    TeleType(buffer);
  }
}
/*********************************************************************/
/* end of UpdateScore                                                */
/*********************************************************************/


/*********************************************************************/
/*                                                                   */
/* Function Name : UpdateLatestScorePub                              */
/*                                                                   */
/* Description   : Either publish a retained publication with the    */
/*                 latest score and the names of the teams playing   */
/*                 in the match match. Or delete an existing         */
/*                 retained publication. This maintains the current  */
/*                 state of all ongoing matches.                     */
/*                                                                   */
/* Flow          :                                                   */
/*                                                                   */
/*  Allocate a message block                                         */
/*  Generate the topic Sport/Soccer/State/LatestScore/team1 team2    */
/*  Define the MQRFH and NameValueData for a retained publication    */
/*  Add the latest score to the publication as user data             */
/*  MQPUT the publication to the stream queue                        */
/*                                                                   */
/* Input Parms   : MQHCONN  hConn                                    */
/*                  Connecting handle to queue manager               */
/*                 pMatch_Node pMatch                                */
/*                  Pointer to match node in linked list             */
/*                 MQHOBJ   hStreamObj                               */
/*                  Object handle of stream queue                    */
/*                 BOOL     bMatchEnded                              */
/*                  Was this function called because a match ended?  */
/*                                                                   */
/* Outputparms   : PMQLONG  pCompCode                                */
/*                  Completion code of MQ commands                   */
/*                 PMQLONG  pReason                                  */
/*                  Reason returned from MQ commands                 */
/*                                                                   */
/*********************************************************************/
void UpdateLatestScorePub( MQHCONN      hConn
                         , pMatch_Node  pMatch
                         , MQHOBJ       hStreamObj
                         , PMQLONG      pCompCode
                         , PMQLONG      pReason
                         , BOOL         bMatchEnded )
{
  PMQBYTE pMessageBlock = NULL;
  MQLONG  messageLength;
  MQCHAR  Command[24];
  MQPMO   pmo = { MQPMO_DEFAULT };
  MQMD    md  = { MQMD_DEFAULT };
  MQCHAR  Topic[100];
  PMQRFH2 pRFHeader;
  PMQCHAR pUserData;

  /*******************************************************************/
  /* Allocate storage to hold the State publication whilst we build  */
  /* it.                                                             */
  /*******************************************************************/
  messageLength = DEFAULT_MESSAGE_SIZE;
  pMessageBlock = (PMQBYTE)malloc(messageLength);
  if( pMessageBlock == NULL )
  {
    printf("Unable to allocate storage\n");
    *pCompCode = MQCC_FAILED;
  }
  else
  {
    /*****************************************************************/
    /* Initialise the storage to nulls.                              */
    /*****************************************************************/
    memset((PMQBYTE)pMessageBlock, '\0', messageLength);

    /*****************************************************************/
    /* If the match has ended we need to delete the retained         */
    /* publication held on the broker for this match, otherwise we   */
    /* are updating the latest score of a match and we need to        */
    /* publish to the broker.                                        */
    /*****************************************************************/
    if( bMatchEnded )
      strcpy(Command, MQPSC_DELETE_PUBLICATION );
    else
      strcpy(Command, MQPSC_PUBLISH );

    /*****************************************************************/
    /* Generate the topic for the publication, each match has a      */
    /* unique topic to identify it as only one publication can be    */
    /* retained per topic, the unique topic allows us to retain a    */
    /* publication for each match.                                   */
    /*****************************************************************/
    strcpy(Topic, LATEST_SCORE_TOPIC);
    strcat(Topic, pMatch->Team1);
    strcat(Topic, " ");
    strcat(Topic, pMatch->Team2);

    /*****************************************************************/
    /* Build the MQRFH2 Header and NameValueData for the             */
    /* publication.                                                  */
    /* (Publication options are only required when publishing not    */
    /* deleting a publication).                                      */
    /*****************************************************************/
    BuildMQRFHeader2( pMessageBlock
                    , &messageLength
                    , Command
                    , MQREGO_NONE
                    , ( bMatchEnded ? MQPUBO_NONE
                                    : MQPUBO_RETAIN_PUBLICATION )
                    , Topic );

    /*****************************************************************/
    /* If we are publishing the latest score, we add a variable      */
    /* length string as user data after the NameValueData, this      */
    /* records the scores of the teams playing in this match.        */
    /* When adding user data to a publication the format (if any) of */
    /* the data must be specified in the MQRFH2 and also the encoding*/
    /* and coded character set Id of the data that is added. In our  */
    /* case the format of the user data is a string (MQFMT_STRING),  */
    /* the encoding (not actually required for a string) is the      */
    /* native encoding (set by default). As we do not currently know */
    /* the CCSID that we are running in we will assume we are        */
    /* running in the same CCSID as the queue manager, so we can say */
    /* the data that follows the MQRFH2 is in the same CCSID as the  */
    /* MQRFH2. The MQRFH2 will default to the CCSID of the queue     */
    /* manager (MQCCSI_Q_MGR), so the user data will also inherit    */
    /* this CCSID.                                                   */
    /*****************************************************************/
    if( bMatchEnded == FALSE )
    {
      pRFHeader = (PMQRFH2)pMessageBlock;
      memcpy( pRFHeader->Format, MQFMT_STRING,
                                         (size_t)MQ_FORMAT_LENGTH);
      pRFHeader->CodedCharSetId = MQCCSI_INHERIT;
      pUserData = (PMQCHAR)(pMessageBlock + messageLength);
      messageLength += sprintf(pUserData,"%d %d",
                               pMatch->Team1Score, pMatch->Team2Score);
    }

    memcpy(md.Format, MQFMT_RF_HEADER_2, (size_t)MQ_FORMAT_LENGTH);
    pmo.Options |= MQPMO_NEW_MSG_ID
                |  MQPMO_NO_SYNCPOINT;
    /*****************************************************************/
    /* We put the publications as persistent messages, they will     */
    /* therefore, survive a queue manager re-start.                  */
    /*****************************************************************/
    md.Persistence = MQPER_PERSISTENT;
    /*****************************************************************/
    /* We ask the broker to only send us a reply from putting the    */
    /* publication if an error occurs (MQRO_NAN : negative replies   */
    /* only). This report option may not be recognised if the        */
    /* message is put at a down level queue manager from the broker, */
    /* the broker receiving the message will still honour the option */
    /* though.                                                       */
    /* If we were to wait for the reply (only in the case when an    */
    /* error occurs) to arrive it would impact our performance.      */
    /* Instead we specify the subscriber's queue as the reply queue  */
    /* and the EventCorrelId to be returned in any reply message,    */
    /* by doing this any replies will be picked up by the MQGET in   */
    /* main that processes all in-coming publications.               */
    /*****************************************************************/
    md.MsgType = MQMT_DATAGRAM;
    md.Report = MQRO_NAN + MQRO_PASS_CORREL_ID;
    memcpy( md.ReplyToQ, SUBSCRIBER_QUEUE, MQ_Q_NAME_LENGTH);
    memcpy( md.CorrelId, EventCorrelId, (size_t)MQ_CORREL_ID_LENGTH);

    MQPUT( hConn
         , hStreamObj
         , &md
         , &pmo
         , messageLength
         , pMessageBlock
         , pCompCode
         , pReason );

    if( *pCompCode != MQCC_OK )
      printf("MQPUT failed with CompCode %d and Reason %d\n",
                                               *pCompCode, *pReason);

    /*****************************************************************/
    /* Free the allocated message block.                             */
    /*****************************************************************/
    free( pMessageBlock );
  }
}
/*********************************************************************/
/* end of UpdateLatestScorePub                                       */
/*********************************************************************/


/*********************************************************************/
/*                                                                   */
/* Function Name : PrintNameValueData                                */
/*                                                                   */
/* Description   : It is not guaranteed that the NameValueData       */
/*                 will be NULL terminated so we cannot simply       */
/*                 use printf to display the text, we have to        */
/*                 display each character individually and stop      */
/*                 when we reach the end of the text.                */
/*                                                                   */
/* Flow          :                                                   */
/*                                                                   */
/*   Step along the string                                           */
/*    Print each character                                           */
/*                                                                   */
/* Input Parms   : PMQCHAR  NameValueData                            */
/*                  Pointer to start of text to print                */
/*                 MQLONG   dataLength                               */
/*                  Length of text to print                          */
/*                                                                   */
/*********************************************************************/
void PrintNameValueData( PMQCHAR NameValueData
                         , MQLONG  dataLength )
{
  MQLONG     i;
  PMQCHAR   pChar = NameValueData;
  FILE     *stream;

  stream = stdout;

  /*******************************************************************/
  /* Indent the string.                                              */
  /*******************************************************************/
  printf(" ");

  /*******************************************************************/
  /* Print each character in the NameValueData until we reach the    */
  /* end or a NULL in the string is found.                           */
  /*******************************************************************/
  for( i = 0
     ; (i < dataLength) && (*pChar != 0)
     ; i++, pChar++)
    putc( NameValueData[i], stream);

  printf("\n");
}
/*********************************************************************/
/* end of PrintNameValueData                                         */
/*********************************************************************/


/*********************************************************************/
/*                                                                   */
/* Function Name : TeleType                                          */
/*                                                                   */
/* Description   : TeleType tries to simulate a basic tele-typer     */
/*                 that displays information as it arrives (as if    */
/*                 typed by hand).                                   */
/*                                                                   */
/* Note          : This function is not a requirement, only for      */
/*                 aesthetic reasons.                                */
/*                                                                   */
/* Flow          :                                                   */
/*                                                                   */
/*   Step along the string                                           */
/*    Print each character, pause longer for a space (' ') or digit  */
/*                                                                   */
/* Input Parms   : PMQCHAR  pChar                                    */
/*                  Pointer to start of string to print              */
/*                                                                   */
/*********************************************************************/
void TeleType( PMQCHAR pChar )
{
  FILE  *stream;

  /*******************************************************************/
  /* Put to standard output.                                         */
  /*******************************************************************/
  stream = stdout;

  /*******************************************************************/
  /* Continue until a null is encountered (end of string).           */
  /*******************************************************************/
  while( *pChar != '\0' )
  {
    /*****************************************************************/
    /* Add an extra pause for any spaces.                            */
    /*****************************************************************/
    if( *pChar == ' ' )
    {
      msSleep(TELE_TYPE_DELAY*2);
    }
    /*****************************************************************/
    /* Add an even longer pause before a digit (adds tension).       */
    /*****************************************************************/
    else if( isdigit( *pChar ) )
    {
      msSleep(TELE_TYPE_DELAY*4);
    }
    /*****************************************************************/
    /* Put the character.                                            */
    /*****************************************************************/
    putc( *pChar, stream );

    /*****************************************************************/
    /* Brief pause.                                                  */
    /*****************************************************************/
    msSleep(TELE_TYPE_DELAY);
    /*****************************************************************/
    /* Flush the buffer (print the character to the screen).         */
    /*****************************************************************/
    fflush( stream );
    pChar++;
  }

}
/*********************************************************************/
/* end of TeleType                                                   */
/*********************************************************************/


/*********************************************************************/
/*                                                                   */
/* end of amqsrr2a.c                                                 */
/*                                                                   */
/*********************************************************************/
