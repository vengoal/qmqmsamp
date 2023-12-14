/* %Z% %W% %I% %E% %U% */
/*********************************************************************/
/*                                                                   */
/* Module name: AMQSGR2A.C                                           */
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
/*               This source file is the soccer match simulator and  */
/*               works in conjunction with the results service,      */
/*               amqsgr2, any number of instances of this match      */
/*               simulator sample can be running as long as they all */
/*               specify unique team names.                          */
/*                                                                   */
/*               To run this sample you will need a queue manager    */
/*               with the queued Pub/Sub interface running.          */
/*               amqssrr2 must be started before any instances       */
/*               of amqsgr2 are started, a message will be           */
/*               displayed by amqsrr2 when it is possible to start   */
/*               this sample.                                        */
/*                                                                   */
/*               To be able to run the match simulator, amqsgr2,     */
/*               a single extra queue needs to be defined on the     */
/*               queue manager that the sample connects to, this     */
/*               is the stream queue used by the results service     */
/*               samples, SAMPLE.BROKER.RESULTS.STREAM.              */
/*                                                                   */
/*               The queue is defined as:                            */
/*                                                                   */
/*               define qlocal('SAMPLE.BROKER.RESULTS.STREAM') +     */
/*                      noshare                                      */
/*                                                                   */
/*               This queue is defined in the MQSC script            */
/*               amqsgama.tst (if amqsgr2 is to be run on the same   */
/*               queue manager as amqsrr2 amqsresa.tst can be used   */
/*               to define all queues required for both samples).    */
/*                                                                   */
/*  Usage:       amqsgr2 teamName1 teamName2 <QMgrName>              */
/*                                                                   */
/*               The two team names are limited to 31 characters and */
/*               must not contain blanks or double quotes ('"').     */
/*                                                                   */
/*  Language:    C                                                   */
/*                                                                   */
/*********************************************************************/
/*                                                                   */
/*  Function Flow :                                                  */
/*                                                                   */
/*          main:                                                    */
/*            MQCONN                                                 */
/*            MQOPEN                                                 */
/*            BuildMQRFHeader2                                       */
/*            PutPublication:                                        */
/*              MQPUT                                                */
/*            sleep                                                  */
/*            BuildMQRFHeader2                                       */
/*            PutPublication..                                       */
/*            BuildMQRFHeader2                                       */
/*            PutPublication..                                       */
/*            MQCLOSE                                                */
/*            MQDISC                                                 */
/*                                                                   */
/*********************************************************************/
/*                                                                   */
/*   <copyright                                                       */
/*   notice="lm-source-program"                                       */
/*   pids="5724-H72"                                                  */
/*   years="2006,2014"                                                */
/*   crc="3930877176" >                                               */
/*   Licensed Materials - Property of IBM                             */
/*                                                                    */
/*   5724-H72                                                         */
/*                                                                    */
/*   (C) Copyright IBM Corp. 2006, 2014 All Rights Reserved.          */
/*                                                                    */
/*   US Government Users Restricted Rights - Use, duplication or      */
/*   disclosure restricted by GSA ADP Schedule Contract with          */
/*   IBM Corp.                                                        */
/*   </copyright>                                                     */
/*                                                                   */
/*********************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <cmqc.h>                           /* MQI                   */
#include <cmqpsc.h>                         /* MQI Publish/Subscribe */

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
   #define msSleep(time)                                              \
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
/* General definitions:                                              */
/*********************************************************************/
#define STREAM               "SAMPLE.BROKER.RESULTS.STREAM"
#define TOPIC_PREFIX         "Sport/Soccer/Event/"
#define MATCH_STARTED        "MatchStarted"
#define MATCH_ENDED          "MatchEnded"
#define SCORE_UPDATE         "ScoreUpdate"
#define MATCH_LENGTH          30000        /* 30 Second match length */
#define REAL_TIME_RATIO       333
#define AVERAGE_NUM_OF_GOALS  5
#define DEFAULT_MESSAGE_SIZE  512          /* Maximum buffer size    */
                                           /* required for a message */

/*********************************************************************/
/* Globals:                                                          */
/*********************************************************************/
static const MQRFH2 DefaultMQRFH2 = {MQRFH2_DEFAULT};

/*********************************************************************/
/* Structures:                                                       */
/*********************************************************************/

/*********************************************************************/
/* Match_Teams: User data for MatchStarted and MatchEnded publication*/
/*********************************************************************/
typedef struct
{
  MQCHAR32  Team1;
  MQCHAR32  Team2;
} Match_Teams, *pMatch_Teams;

/*********************************************************************/
/* Prototypes:                                                       */
/*********************************************************************/
void BuildMQRFHeader2( PMQBYTE   pStart
                     , PMQLONG   pDataLength
                     , MQCHAR    TopicType[] );

void PutPublication( MQHCONN   hConn
                   , MQHOBJ    hObj
                   , PMQBYTE   pMessage
                   , MQLONG    messageLength
                   , PMQLONG   pCompCode
                   , PMQLONG   pReason );

/*********************************************************************/
/* Functions:                                                        */
/*********************************************************************/

/*********************************************************************/
/*                                                                   */
/* Function Name : main                                              */
/*                                                                   */
/* Description   : Entry function of the sample, connects to the     */
/*                 queue manager, publishes events occuring in the   */
/*                 match, start, end and goals scored.               */
/*                                                                   */
/* Flow          :                                                   */
/*                                                                   */
/*   Verify arguments                                                */
/*   MQCONN to broker queue manager                                  */
/*    MQOPEN broker stream queue                                     */
/*     Initialise match timer                                        */
/*     Allocate message block                                        */
/*     Generate the MQRFH2 for a MatchStarted publication            */
/*     Add the teams names as user data                              */
/*     Put the publication to the stream queue                       */
/*     While match time remains :                                    */
/*      Sleep for a random period                                    */
/*      Attempt to score (fifty percent chance)                      */
/*       Generate the RFH2 for a ScoreUpdate publication             */
/*       Randomly select the team that scored                        */
/*        Add the team name to the publication as user data          */
/*       Put the publication to the stream queue                     */
/*     Generate the MQRFH2 for a MatchEnded publication              */
/*     Add the team names to the publication as user data            */
/*     Put the publication to the stream queue                       */
/*    MQCLOSE broker stream queue                                    */
/*   MQDISC from broker queue manager                                */
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
  MQHOBJ       hObj  = MQHO_UNUSABLE_HOBJ;
  MQLONG       CompCode;
  MQLONG       Reason;
  MQOD         od  = { MQOD_DEFAULT };
  MQLONG       Options;
  PMQBYTE      pMessageBlock = NULL;
  MQLONG       messageLength;
  MQLONG       timeRemaining;
  MQLONG       delay;
  PMQCHAR      pScoringTeam;
  pMatch_Teams pTeams;
  MQCHAR32     team1;
  MQCHAR32     team2;
  char         QMName[MQ_Q_MGR_NAME_LENGTH+1] = "";
  MQLONG       randomNumber;
  MQLONG       ConnReason;

  /*******************************************************************/
  /* Check the arguments supplied.                                   */
  /*******************************************************************/
  if( (argc < 3)
    ||(argc > 4)
    ||(strlen(argv[1]) > 31)
    ||(strlen(argv[2]) > 31) )
  {
    printf("Usage: %s team1 team2 <QManager>\n", argv[0]);
    printf("       Maximum 31 characters per team name,\n");
    printf("       no spaces or '\"' characters allowed.\n");
    exit(0);
  }
  else
  {
    strcpy(team1, argv[1]);
    strcpy(team2, argv[2]);
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
    strncpy(od.ObjectName, STREAM, (size_t)MQ_Q_NAME_LENGTH);
    Options = MQOO_OUTPUT + MQOO_FAIL_IF_QUIESCING;
    MQOPEN( hConn
          , &od
          , Options
          , &hObj
          , &CompCode
          , &Reason );
    if( CompCode != MQCC_OK )
    {
      printf("MQOPEN failed to open \"%s\"\nwith CompCode %d and Reason %d\n",
             od.ObjectName, CompCode, Reason);
    }
  }

  if( CompCode == MQCC_OK )
  {
    /*****************************************************************/
    /* Set the random seed based on the current time (in seconds)    */
    /* and the first and last characters of the team names.          */
    /* This should ensure different results for simultaneous         */
    /* matches and for the same match played multiple times.         */
    /*****************************************************************/
    srand( (unsigned)(time( NULL ))
         + (unsigned)(team1[0] + team2[(strlen(team2) - 1)]) );

    timeRemaining = MATCH_LENGTH;

    /*****************************************************************/
    /* Allocate a storage block for the publications to be built in. */
    /*****************************************************************/
    messageLength = DEFAULT_MESSAGE_SIZE;
    pMessageBlock = (PMQBYTE)malloc(messageLength);
    if( pMessageBlock == NULL )
    {
      printf("Unable to allocate storage\n");
    }
    else
    {
      if( CompCode == MQCC_OK )
      {
        /*************************************************************/
        /* Immediately publish a 'match started' message, listing    */
        /* the two teams playing. Build the MQRFH2 and               */
        /* NameValueData of the publication.                         */
        /*************************************************************/
        BuildMQRFHeader2( pMessageBlock
                        , &messageLength
                        , (char *)MATCH_STARTED );

        /*************************************************************/
        /* Add a Match_Teams structure as user data to the           */
        /* publication.                                              */
        /*************************************************************/
        pTeams = (pMatch_Teams)(pMessageBlock + messageLength);
        strcpy(pTeams->Team1, team1);
        strcpy(pTeams->Team2, team2);
        messageLength += sizeof(Match_Teams);

        printf("Match between %s and %s\n", team1, team2);

        /*************************************************************/
        /* Put the publication to the stream queue.                  */
        /*************************************************************/
        PutPublication( hConn
                      , hObj
                      , pMessageBlock
                      , messageLength
                      , &CompCode
                      , &Reason );

        if( CompCode != MQCC_OK )
        {
          printf("MQPUT failed with CompCode %d and Reason %d\n",
                                                    CompCode, Reason);
        }
        else
        {
          /***********************************************************/
          /* To simulate the Soccer match (played over 30 seconds)   */
          /* score goals at random intervals for either team.        */
          /***********************************************************/
          while( (timeRemaining > 0)
               &&(CompCode == MQCC_OK) )
          {
            /*********************************************************/
            /* Sleep for a random period before trying to score a    */
            /* goal (a maximum of a fifth of the game length). We    */
            /* add the REAL_TIME_RATIO which is the number of sleep  */
            /* units that represent one minute of actual match play, */
            /* this prevents two goals in one minute.                */
            /* Note on some platforms RAND_MAX is a signed int max,  */
            /* so must be careful in the calculation not to exceed   */
            /* this value.                                           */
            /*********************************************************/
            randomNumber = rand();
            delay = REAL_TIME_RATIO +
                    (MQLONG) (((double)randomNumber / (double)RAND_MAX) *
                              (MATCH_LENGTH / AVERAGE_NUM_OF_GOALS));
            /*********************************************************/
            /* If the delay is longer than the remainder of the      */
            /* match set the delay to be the remaining time in the   */
            /* match.                                                */
            /*********************************************************/
            if( delay > timeRemaining )
              delay = timeRemaining;
            msSleep(delay);
            timeRemaining -= delay;
            /*********************************************************/
            /* If the match has not yet finished, try and score a    */
            /* goal.                                                 */
            /*********************************************************/
            if( timeRemaining > 0 )
            {
              /*******************************************************/
              /* There is a fifty percent chance of scoring a goal,  */
              /* if the delay period was an even number a goal is    */
              /* scored, otherwise, it was a miss.                   */
              /*******************************************************/
              if( (randomNumber % 2) == 0 )
              {
                /*****************************************************/
                /* Build the MQRFH2 and NameValueData of the         */
                /* publication to be published for the goal scored.  */
                /*****************************************************/
                messageLength = DEFAULT_MESSAGE_SIZE;
                BuildMQRFHeader2( pMessageBlock
                                , &messageLength
                                , (char *)SCORE_UPDATE );

                printf("GOAL! ");

                /*****************************************************/
                /* Add the scoring team to the publication as user   */
                /* data. Randomly choose which team scored.          */
                /*****************************************************/
                pScoringTeam = (PMQCHAR)pMessageBlock + messageLength;
                if( rand() < (RAND_MAX/2) )
                {
                  strcpy(pScoringTeam, team1);
                  printf(team1);
                }
                else
                {
                  strcpy(pScoringTeam, team2);
                  printf(team2);
                }

                /*****************************************************/
                /* Display the time scored in proportion to a        */
                /* ninety minute game.                               */
                /*****************************************************/
                printf(" scores after %d minutes\n",
                    ((MATCH_LENGTH - timeRemaining)/REAL_TIME_RATIO));

                messageLength += sizeof(MQCHAR32);

                /*****************************************************/
                /* Put the publication to the stream queue.          */
                /*****************************************************/
                PutPublication( hConn
                              , hObj
                              , pMessageBlock
                              , messageLength
                              , &CompCode
                              , &Reason );

                if( CompCode != MQCC_OK )
                  printf("MQPUT failed with CompCode %d and Reason %d\n",
                          CompCode, Reason);
              }
            }
          } /* end of while( timeRemaining ) */

          if( CompCode == MQCC_OK )
          {
            printf("Full time\n");
            /*********************************************************/
            /* The match time has elapsed, we now publish this fact. */
            /* Build the MQRFH2 and NameValueData for a              */
            /* 'match ended' publication.                            */
            /*********************************************************/
            messageLength = DEFAULT_MESSAGE_SIZE;
            BuildMQRFHeader2( pMessageBlock
                            , &messageLength
                            , (char *)MATCH_ENDED );

            /*********************************************************/
            /* Add the teams playing the match to the publication.   */
            /*********************************************************/
            pTeams = (pMatch_Teams)(pMessageBlock + messageLength);
            strcpy(pTeams->Team1, team1);
            strcpy(pTeams->Team2, team2);
            messageLength += sizeof(Match_Teams);

            /*********************************************************/
            /* Put the publication to the stream queue.              */
            /*********************************************************/
            PutPublication( hConn
                          , hObj
                          , pMessageBlock
                          , messageLength
                          , &CompCode
                          , &Reason );

            if( CompCode != MQCC_OK )
              printf("MQPUT failed with CompCode %d and Reason %d\n",
                      CompCode, Reason);
          }
        }
      }
      free( pMessageBlock );
    } /* end of else (pMessageBlock != NULL) */
  }

  /*******************************************************************/
  /* MQCLOSE the queue used by this sample.                          */
  /*******************************************************************/
  if( hObj != MQHO_UNUSABLE_HOBJ )
  {
    MQCLOSE( hConn
           , &hObj
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
/* Function Name : BuildMQRFHeader2                                  */
/*                                                                   */
/* Description   : Build the MQRFH2 header and accompaning           */
/*                 NameValueData.                                    */
/*                                                                   */
/* Flow          :                                                   */
/*                                                                   */
/*  Initialise the message block to nulls                            */
/*  Define the start of the message as an MQRFH2                     */
/*  Set the default values of the MQRFH2                             */
/*  Set the format of the user data in the MQFRH2                    */
/*  Set the CCSID of the user data in the MQRFH2                     */
/*  Define the NameValueData that follows the MQRFH2                 */
/*   Add the psc folder                                              */
/*   Add the command                                                 */
/*   Add topic                                                       */
/*   Close the psc folder                                            */
/*   Add the length of the psc folder                                */
/*  Pad the NameValueData to a 16 byte boundary                      */
/*  Set the StrucLength in the MQRFH2 to the total length so far     */
/*                                                                   */
/* Input Parms   : PMQBYTE pStart                                    */
/*                  Start of message block                           */
/*                 MQCHAR  TopicType[]                               */
/*                  Topic name suffix string                         */
/*                                                                   */
/* Input/Output  : PMQLONG pDataLength                               */
/*                  Size of message block on entry and amount of     */
/*                  block used on exit                               */
/*                                                                   */
/*********************************************************************/
void BuildMQRFHeader2( PMQBYTE   pStart
                     , PMQLONG   pDataLength
                     , MQCHAR    TopicType[] )
{
  PMQRFH2   pRFHeader = (PMQRFH2)pStart;
  PMQCHAR  pNameValueData;
  /*******************************************************************/
  /* Each folder in the RFH2 must be prepended with a 4 byte length  */
  /* field indicating the length of the folder in RFH2.              */
  /*******************************************************************/
  int    lengthOfLengthField = 4;
  MQLONG pscFolderLength;
  int    spacesToPad;

  /*******************************************************************/
  /* Clear the buffer before we start (initialise to nulls).         */
  /*******************************************************************/
  memset((PMQBYTE)pStart, 0, *pDataLength);

  /*******************************************************************/
  /* Copy the MQRFH2 default values into the start of the buffer.    */
  /*******************************************************************/
  memcpy( pRFHeader, &DefaultMQRFH2, (size_t)MQRFH_STRUC_LENGTH_FIXED_2);

  /*******************************************************************/
  /* Set the format of the user data to be MQFMT_STRING, even though */
  /* some of the publications use a structure to pass user data the  */
  /* data within this structure is entirely MQCHAR and can be        */
  /* treated as MQFMT_STRING by the data conversion routines.        */
  /*******************************************************************/
  memcpy( pRFHeader->Format, MQFMT_STRING, (size_t)MQ_FORMAT_LENGTH);

  /*******************************************************************/
  /* As we have user data following the MQRFH2 we must set the CCSID */
  /* of the user data in the MQRFH2 for data conversion to be able to*/
  /* be performed by the queue manager. As we do not currently know  */
  /* the CCSID that we are running in we can tell MQ that the        */
  /* data that follows the MQRFH2 is in the same CCSID as the MQRFH2.*/
  /* The MQRFH2 will default to the CCSID of the queue manager       */
  /* (MQCCSI_Q_MGR), so the user data will also inherit this CCSID.  */
  /* The NameValueCCSID also needs to be set                         */
  /*******************************************************************/
  pRFHeader->CodedCharSetId = MQCCSI_INHERIT;
  pRFHeader->NameValueCCSID = 1208L;

  /*******************************************************************/
  /* Start the NameValueData after the MQRFH2 structure.  Giving     */
  /* extra 4 byte space for the length of the psc folder.            */
  /*******************************************************************/
  pNameValueData = (MQCHAR *)(pRFHeader) +
                     MQRFH_STRUC_LENGTH_FIXED_2 +
                     lengthOfLengthField;

  /*******************************************************************/
  /* Open the Psc Folder.                                            */
  /* The psc folder will look like                                   */
  /* <psc>                                                           */
  /*   <Command>Publish</Command>                                    */
  /*   <Topic>TopicType</Topic>                                      */
  /* </psc>                                                          */
  /*******************************************************************/
  strcpy(pNameValueData, MQRFH2_PUBSUB_CMD_FOLDER_B);

  /*******************************************************************/
  /* Open the Command folder and put the publish tag in.             */
  /*******************************************************************/
  strcat(pNameValueData, MQPSC_COMMAND_B );
  strcat(pNameValueData, MQPS_PUBLISH);
  strcat(pNameValueData, MQPSC_COMMAND_E);

  /*******************************************************************/
  /* Open the Topic folder.  Put the value of the Topic into this    */
  /* folder.                                                         */
  /*******************************************************************/
  strcat(pNameValueData, MQPSC_TOPIC_B);

  /*******************************************************************/
  /* Attach the topic prefix to the topic                            */
  /*******************************************************************/
  strcat(pNameValueData, TOPIC_PREFIX);
  strcat(pNameValueData, TopicType);
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
         &pscFolderLength,4);

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
                  lengthOfLengthField;

  pRFHeader->StrucLength = (MQLONG)*pDataLength;
}
/*********************************************************************/
/* end of BuildMQRF2Header                                           */
/*********************************************************************/


/*********************************************************************/
/*                                                                   */
/* Function Name : PutPublication                                    */
/*                                                                   */
/* Description   : Put a message to the MQ queue.                    */
/*                                                                   */
/* Flow          :                                                   */
/*                                                                   */
/*   Configure the MQPUT for a datagram message                      */
/*   MQPUT the message to the queue                                  */
/*                                                                   */
/* Input Parms   : MQHCONN  hConn                                    */
/*                  Queue manager connection handle                  */
/*                 MQHOBJ   hObj                                     */
/*                  Queue object handle                              */
/*                 PMQBYTE  pMessage                                 */
/*                  Pointer to the start of the message block        */
/*                 MQLONG   messageLength                            */
/*                  Lengh of message data                            */
/*                                                                   */
/* Output Parms  : PMQLONG  pCompCode                                */
/*                  Completion Code returned from MQPUT              */
/*                 PMQLONG  pReason                                  */
/*                  Reason returned from MQPUT                       */
/*                                                                   */
/*********************************************************************/
void PutPublication( MQHCONN   hConn
                   , MQHOBJ    hObj
                   , PMQBYTE   pMessage
                   , MQLONG    messageLength
                   , PMQLONG   pCompCode
                   , PMQLONG   pReason )
{
  MQPMO   pmo = { MQPMO_DEFAULT };
  MQMD    md  = { MQMD_DEFAULT };

  /*******************************************************************/
  /* Set the md for a datagram MQRFH2 message.                       */
  /*******************************************************************/
  memcpy(md.Format, MQFMT_RF_HEADER_2, (size_t)MQ_FORMAT_LENGTH);
  md.MsgType = MQMT_DATAGRAM;
  md.Persistence = MQPER_PERSISTENT;
  pmo.Options |= MQPMO_NEW_MSG_ID
              |  MQPMO_NO_SYNCPOINT;

  /*******************************************************************/
  /* MQPUT the message to the queue.                                 */
  /*******************************************************************/
  MQPUT( hConn
       , hObj
       , &md
       , &pmo
       , messageLength
       , pMessage
       , pCompCode
       , pReason );
}
/*********************************************************************/
/* end of PutPublication                                             */
/*********************************************************************/


/*********************************************************************/
/*                                                                   */
/* end of amqsgr2a.c                                               */
/*                                                                   */
/*********************************************************************/
