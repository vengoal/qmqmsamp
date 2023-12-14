static const char sccsid[] = "%Z% %W% %I% %E% %U%";
 /********************************************************************/
 /*                                                                  */
 /* Program name: AMQSRUAA                                           */
 /*                                                                  */
 /* Description: Sample C program that subscribes to MQ resource     */
 /*              usage topics and formats the resulting published    */
 /*              PCF data.                                           */
 /*              This sample also acts as primary documentation      */
 /*              for customers/vendors wishing to gain a greater     */
 /*              understanding of the (flexible) structure and       */
 /*              nature of MQ's resource usage publications.         */
 /*   <copyright                                                     */
 /*   notice="lm-source-program"                                     */
 /*   pids="5724-H72"                                                */
 /*   years="1994,2016"                                              */
 /*   crc="1728343970" >                                             */
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
 /*   AMQSRUAA is a sample C program to subscribe and get messages   */
 /*   showing how MQ is using resources. It is a basic example of    */
 /*   how to request and consume this type of administrative data.   */
 /*                                                                  */
 /*                                                                  */
 /*   sample                                                         */
 /*      -- subscribes non-durably to the topics identified by the   */
 /*        input parameters.                                         */
 /*                                                                  */
 /*      -- calls MQGET repeatedly to get messages from the topics,  */
 /*         and writes to stdout.                                    */
 /*                                                                  */
 /*      -- writes a message for each MQI reason other than          */
 /*         MQRC_NONE; stops if there is a MQI completion code       */
 /*         of MQCC_FAILED, or when the requested number of          */
 /*         resource usage publications have been consumed.          */
 /*                                                                  */
 /*                                                                  */
 /*                                                                  */
 /*   Program logic:                                                 */
 /*      Determine name of admin topics from the input parameters.   */
 /*      MQSUB topics for INPUT                                      */
 /*      while no MQI failures,                                      */
 /*      .  MQGET next message                                       */
 /*      .  format the results                                       */
 /*      .  (no message available counts as failure, and loop ends)  */
 /*      MQCLOSE the topic                                           */
 /*                                                                  */
 /*                                                                  */
 /* The queue manager publishes resource usage data to topics to be  */
 /* consumed by subscribers to those topics. At queue manager        */
 /* start up the queue manager publishes a set of messages on        */
 /* "meta-topics" describing what resource usage topics are          */
 /* supported by this queue manager, and the content of the          */
 /* messages published to those topics.                              */
 /* Administrative tools subscribe to the meta-data to discover      */
 /* what resource usage information is available, and on what        */
 /* topics, and then subscribe to the advertised topics in order     */
 /* to consume resource usage data.                                  */
 /*                                                                  */
 /* It is worth noting that pretty much the only static information  */
 /* used by amqsruaa.c is the "META_PREFIX" definition which         */
 /* identifies the root topic for the meta-data publications and     */
 /* no other statically defined data needs to be used.               */
 /*                                                                  */
 /* Well written MQ admin programs consuming this type of data       */
 /* should try to follow this model.                                 */
 /*                                                                  */
 /* Much of the published data represents well understood MQ         */
 /* concepts, such as the rate at which messages are being           */
 /* produced and consumed. This type of data is very unlikely to     */
 /* change across MQ configurations.                                 */
 /* A subset of the published data is more reflective of MQ          */
 /* internals, for example data relating to rates of queue           */
 /* avoidance (put to waiting getter), or data relating to lock      */
 /* contention. This type of data is likely to change across         */
 /* MQ configurations. A well written program that subscribes to and */
 /* consumes this type of data should not need changing when new     */
 /* resource usage information is added, nor when the currently      */
 /* published topics and content are changed.                        */
 /*                                                                  */
 /********************************************************************/
 /*                                                                  */
 /*   AMQSRUAA has the following parameters                          */
 /*       optional:                                                  */
 /*                 (1) Queue manager name (-m)                      */
 /*                 (2) The CLASS(s) (-c)                            */
 /*                 (3) The TYPE(s) (-t)                             */
 /*                 (4) The Object name(s) (-o)                      */
 /*                 (5) The number of publications to process (-n)   */
 /*                 (6) The debug level (-d)                         */
 /*                                                                  */
 /*   Examples:                                                      */
 /*     amqsrua -m QM1 -c CPU -t QMgrSummary -n 5                    */
 /*     amqsrua -m QM1 -c STATMQI -t PUT -t GET                      */
 /*     amqsrua -m QM1 -c STATQ  -o QUEUE1  -t PUT -t GET -n 10      */
 /*     amqsrua -m QM1 -c DISK -t Log -n 10                          */
 /*                                                                  */
 /********************************************************************/
 /* Notes:                                                           */
 /*                                                                  */
 /* 1. The primary purpose of amqsruaa is to demonstrate how to      */
 /*    interpret the meta-data to make appropriate subscriptions     */
 /*    and how to process the ensuing publications. In order to keep */
 /*    this focus, amqsruaa.c is intentionally very simplistic in    */
 /*    its error handling, and is NOT intended to be a good example  */
 /*    of comprehensive MQ error handling.                           */
 /*    Similarly the usability of amqsruaa as a sample executable    */
 /*    is somewhat constrained by the desire to keep the source      */
 /*    code focused on it's primary purpose.                         */
 /*                                                                  */
 /*                                                                  */
 /********************************************************************/
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <ctype.h>
     /* includes for MQI  */
 #include <cmqc.h>
 #include <cmqcfc.h>
 /* The maximum number of input classes supported by amqsruaa.c    */
 #define MAX_CLASS_COUNT 10
 /* The maximum size of the string value of a class supported here */
 #define MAX_CLASS_SIZE 32
 /* The maximum number of input types supported by amqsruaa.c      */
 #define MAX_TYPE_COUNT 100
 /* The maximum size of the string value of a type supported here  */
 #define MAX_TYPE_SIZE 32

 /* The root of the topic tree where MQ publishes the meta-data    */
 /* describing what resource usage publications will be published  */
 /* by this queue manager.                                         */
 #define META_PREFIX "$SYS/MQ/INFO/QMGR/%s/Monitor/METADATA/"

#if MQAT_DEFAULT == MQAT_WINDOWS_NT
  #define Int64 "I64"
#elif defined(MQ_64_BIT)
  #define Int64 "l"
#else
  #define Int64 "ll"
#endif

/* As explained above, amqsruaa error handling is intentionally very */
/* simplistic. The program will return either 0 (expected number of  */
/* pulications received, or 20 ( some error occurred).               */
#define ERROR_RETURN 20

#define sruUsageString "%s [-m QMGR-NAME] [-c CLASS-NAME]\n" \
                       "\t [-o object name] [-t TYPE-NAME]\n" \
                       "\t [-n publication count]\n"

/*********************************************************************/
/*                                                                   */
/* amqsruaa reads the meta data to understand what resource usage    */
/* information is supported by the current queue manager             */
/* configuration.                                                    */
/* This information is built into a tree to be used in selecting     */
/* (if not explicitly specified) the data to be consumed and in      */
/* formatting the produced data.                                     */
/* The following typedef's are private to amqsruaa.c and represent   */
/* this programs view of the meta-data.                              */
/*********************************************************************/

/*********************************************************************/
/* Resource usage publications are PCF messages consisting of a      */
/* sequence of PCF elements. The PCF elements published for each     */
/* class/type pair are advertised in the meta-data. We store a leaf  */
/* in the tree describing each element, thus allowing the elements   */
/* to be processed accordingly.                                      */
/*                                                                   */
/*********************************************************************/
 typedef struct _sruELEM
 {
   struct _sruELEM *  next;
   MQLONG             id;
   MQLONG             type;
   MQLONG             descLen;
   char               Buffer[1];
 } sruELEM;

/*********************************************************************/
/* MQ resource usage publications are associated with a type         */
/* within a class. Each publication includes the class and type      */
/* which allows the class/type/element definitions to be found       */
/* and the resulting publications to be handled appropriately.       */
/* The class/type/element descriptions are published as meta-data    */
/* at queue manager startup.                                         */
/*********************************************************************/
 typedef struct _sruTYPE
 {
   struct _sruTYPE *  next;
   MQLONG             id;
   MQLONG             typeLen;
   MQLONG             descLen;
   MQLONG             topicLen;
   MQLONG             topicCCSID;
   sruELEM           *elem;
   MQLONG             flags;
   char               Buffer[1];
 } sruTYPE;

/*********************************************************************/
/* MQ resource usage publications are associated with a class.       */
/* The classes represent the top level in the tree of meta-data      */
/* decribing the available resource usage information.               */
/*********************************************************************/
 typedef struct _sruCLASS
 {
   struct _sruCLASS * next;
   MQLONG             id;
   MQLONG             classLen;
   MQLONG             descLen;
   MQLONG             topicLen;
   MQLONG             topicCCSID;
   sruTYPE          * type;
   MQLONG             flags;
   char               Buffer[1];
 } sruCLASS;

/*********************************************************************/
/*                                                                   */
/* Data globally addressable within amqsruaa.c                       */
/*                                                                   */
/*********************************************************************/
 static char       QMName[50];             /* queue manager name     */
 static char       Class[MAX_CLASS_COUNT][MAX_CLASS_SIZE]={0};
 static char      *ClassObjName[MAX_TYPE_COUNT]={0};
 static char       Type[MAX_TYPE_COUNT][MAX_TYPE_SIZE]={0};
 static char      *TypeObjName[MAX_TYPE_COUNT]={0};
 static int        ClassTypeCount[MAX_CLASS_COUNT] = {0};
 static int        ClassCount=0;
 static int        TypeCount=0;
 static MQHCONN    Hcon=MQHC_UNUSABLE_HCONN; /* connection handle    */
 static MQHOBJ     Hobj = MQHO_NONE;         /* MQGET object handle  */
 static sruCLASS * ClassChain=NULL;          /* root of tree         */
 static int        sruDebug=0;

/*********************************************************************/
/*                                                                   */
/* Prototypes for functions local to amqsruaa.c                      */
/*                                                                   */
/*********************************************************************/
 int sruPromptForClass();
 int sruPromptForType();
 int sruSubscribe();
 int sruDiscover();
 int sruQueryQMName();
 int sruFormatMessage( PMQCFH buffer
                     , MQMD * pMd );
 void sruFormatElem( PMQCFIN cfin
                   , sruELEM *pElem
                   , MQINT64 interval
                   , char  * pObjName );
 int sruRegisterClass( MQLONG id
                     , char * class
                     , MQLONG class_len
                     , char * desc
                     , MQLONG desc_len
                     , char * topic
                     , MQLONG topic_len
                     , MQLONG topic_ccsid
                     , MQLONG flags );

int sruRegisterType(  MQLONG class_id
                    , MQLONG type_id
                    , char * type
                    , MQLONG type_len
                    , char * desc
                    , MQLONG desc_len
                    , char * topic
                    , MQLONG topic_len
                    , MQLONG topic_ccsid
                    , MQLONG flags);

int sruUpdateType(  MQLONG class_id
                    , MQLONG type_id
                    , char * topic
                    , MQLONG topic_len
                    , MQLONG topic_ccsid );

int sruRegisterElement( MQLONG class_id
                      , MQLONG type_id
                      , MQLONG elem_id
                      , MQLONG elemType
                      , char * desc
                      , MQLONG desc_len );

sruCLASS * sruIdToClass( MQLONG class );
sruTYPE  * sruIdToType( sruCLASS *pClass, MQLONG type );
sruELEM  * sruIdToElem(  sruTYPE * pType, MQLONG elem );

/*********************************************************************/
/*                                                                   */
/* Main:                                                             */
/*                                                                   */
/*********************************************************************/
 int main(int argc, char **argv)
 {

   /*   Declare MQI structures needed                                */
   MQOD     od = {MQOD_DEFAULT};    /* Object Descriptor             */
   MQSD     sd = {MQSD_DEFAULT};    /* Subscription Descriptor       */
   MQMD     md = {MQMD_DEFAULT};    /* Message Descriptor            */
   MQGMO   gmo = {MQGMO_DEFAULT};   /* get message options           */
      /** note, sample uses defaults where it can **/

   MQLONG   CompCode;               /* completion code               */
   MQLONG   Reason;                 /* reason code                   */
   MQBYTE   buffer[4096];           /* message buffer                */
   MQLONG   buflen;                 /* buffer length                 */
   MQLONG   messlen;                /* message length received       */
   MQLONG   MaxGets=-1;
   MQLONG   GetCount=0;
   int      count;
   int      rc=0;
   char *   parg;

   if( sruDebug > 0 )
     printf("Sample AMQSRUAA start\n");

   /******************************************************************/
   /*   Parse arguments                                              */
   /******************************************************************/
   for (count=1; count < argc; count++)
   {
     if (strncmp("-m", argv[count], 2) == 0)
     {
       if ((*(parg=&(argv[count][2])) == '\0') && (count+1 < argc))
       {
         parg=argv[++count];
       }
       strncpy(QMName, parg, MQ_Q_MGR_NAME_LENGTH);
     }
     else if (strncmp("-c", argv[count], 2) == 0)
     {
       if ((*(parg=&(argv[count][2])) == '\0') && (count+1 < argc))
       {
         parg=argv[++count];
       }
       if( MAX_CLASS_COUNT > ClassCount)
         strncpy(Class[ClassCount++], parg, MAX_CLASS_SIZE);
     }
     else if (strncmp("-t", argv[count], 2) == 0)
     {
       if ((*(parg=&(argv[count][2])) == '\0') && (count+1 < argc))
       {
         parg=argv[++count];
       }
       if( MAX_TYPE_COUNT > TypeCount)
         strncpy(Type[TypeCount++], parg, MAX_TYPE_SIZE);
       if( ClassCount )
         ClassTypeCount[ClassCount-1] ++;
     }
     else if (strncmp("-o", argv[count], 2) == 0)
     {
       if ((*(parg=&(argv[count][2])) == '\0') && (count+1 < argc))
       {
         parg=argv[++count];
       }
       if( TypeCount )
         TypeObjName[TypeCount-1] =  parg;
       if( ( ClassCount )
         &&( NULL == ClassObjName[ClassCount-1] ) )
         ClassObjName[ClassCount-1] =  parg;
     }
     else if (strncmp("-d", argv[count], 2) == 0)
     {
       if ((*(parg=&(argv[count][2])) == '\0') && (count+1 < argc))
       {
         parg=argv[++count];
       }
       sruDebug = atoi( parg );
     }
     else if (strncmp("-n", argv[count], 2) == 0)
     {
       if ((*(parg=&(argv[count][2])) == '\0') && (count+1 < argc))
       {
         parg=argv[++count];
       }
       MaxGets = atoi( parg );
     }
     else
     {
       printf(sruUsageString, argv[0]);
       rc = ERROR_RETURN;
       goto mod_exit;
     }
   }

   /******************************************************************/
   /*                                                                */
   /*   Connect to queue manager                                     */
   /*                                                                */
   /******************************************************************/
   MQCONN(QMName,                  /* queue manager                  */
          &Hcon,                   /* connection handle              */
          &CompCode,               /* completion code                */
          &Reason);               /* reason code                    */

   /* report reason and stop if it failed     */
   if (CompCode == MQCC_FAILED)
   {
     printf("MQCONN ended with reason code %d\n", Reason);
     rc = ERROR_RETURN;
     goto mod_exit;
   }

   /* if there was a warning report the cause and continue */
   if (CompCode == MQCC_WARNING)
   {
     printf("MQCONN generated a warning with reason code %d\n", Reason);
     printf("Continuing...\n");
   }

   /* Find the name of the queue manager that we connected to */
   rc = sruQueryQMName();
   if( rc )
     goto mod_exit;

   /******************************************************************/
   /*                                                                */
   /*  Create a TDQ on which to receive publications.                */
   /*                                                                */
   /******************************************************************/
   strcpy( od.ObjectName, "SYSTEM.DEFAULT.MODEL.QUEUE");
   MQOPEN( Hcon
         , &od
         , MQOO_INPUT_EXCLUSIVE
         , &Hobj
         , &CompCode
         , &Reason );
   if( MQCC_OK != CompCode )
   {
     printf("MQOPEN ended with reason code %d\n", Reason);
     rc = ERROR_RETURN;
     goto mod_exit;
   }

   /******************************************************************/
   /*                                                                */
   /*   Discover which resource usage information is supported by    */
   /*   the queue manager that we're connected to by subscribing     */
   /*   to the meta-data topics and processing the resulting         */
   /*   publications.                                                */
   /*                                                                */
   /******************************************************************/
   rc = sruDiscover();
   if( rc )
     goto mod_exit;

   /******************************************************************/
   /*                                                                */
   /*   If no class/type was explicitly requested then prompt for    */
   /*   appropriate input.                                           */
   /*                                                                */
   /******************************************************************/
   if( 0 == ClassCount )
   {
     if( sruPromptForClass() < 0 )
     {
       rc = ERROR_RETURN;
       goto mod_exit;
     }
   }
   if( 0 == TypeCount )
   {
     if( sruPromptForType() < 0 )
     {
       rc = ERROR_RETURN;
       goto mod_exit;
     }
   }

   /******************************************************************/
   /*                                                                */
   /*   Subscribe for messages on the appropriate topics.            */
   /*                                                                */
   /******************************************************************/
   rc = sruSubscribe();
   if( rc )
     goto mod_exit;


   /******************************************************************/
   /*                                                                */
   /*   Get messages from the destination queue                      */
   /*   Loop until there is a failure, or the requested number       */
   /*   of publications has been processed.                          */
   /*                                                                */
   /******************************************************************/
   gmo.Options =   MQGMO_WAIT         /* wait for new messages       */
                 | MQGMO_NO_SYNCPOINT /* no transaction              */
                 | MQGMO_CONVERT      /* convert if necessary        */
                 | MQGMO_NO_PROPERTIES;

   gmo.WaitInterval = 30000;        /* 30 second limit for waiting   */
   gmo.Version = MQGMO_CURRENT_VERSION;
   gmo.MatchOptions = MQMO_NONE;

   rc = ERROR_RETURN; /* Default return code */
   while (CompCode != MQCC_FAILED)
   {
     if( MaxGets >= 0 )
     {
       if(GetCount >= MaxGets )
       {
         rc = 0 ;
         break;
       }
     }
     GetCount ++ ;

     /****************************************************************/
     /* Note that we don't (currently) expect any large resource     */
     /* usage messages, and when dealing with small messages it's    */
     /* less important (performance) to use a dynamically sized      */
     /* buffer.                                                      */
     /****************************************************************/
     buflen = sizeof(buffer); /* fixed buffer size available for GET */

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
     if( sruDebug > 0 )
       printf("Calling MQGET : %d seconds wait time\n",
              gmo.WaitInterval / 1000);

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
     if (Reason != MQRC_NONE)
     {
       if (Reason == MQRC_NO_MSG_AVAILABLE)
       {                         /* special report for normal end    */
         printf("no more messages\n");
       }
       else                      /* general report for other reasons */
       {
         printf("MQGET ended with reason code %d\n", Reason);

         /* treat truncated message as a failure for this sample   */
         /* as none of the current resource usage publications is  */
         /* expected to be more than 4096 bytes long.              */
         if (Reason == MQRC_TRUNCATED_MSG_FAILED)
           CompCode = MQCC_FAILED;

       }
     }

     /****************************************************************/
     /*   Format and display each message received                   */
     /****************************************************************/
     if (CompCode != MQCC_FAILED)
     {
       sruFormatMessage( (PMQCFH) buffer, &md );
       printf( "\n");
     }

   }

   /******************************************************************/
   /*   Subscription handles, and the TDQ handle, are implicitly     */
   /*   closed at MQDISC.                                            */
   /*                                                                */
   /******************************************************************/

   /******************************************************************/
   /*                                                                */
   /*   Disconnect from the queue manager.                           */
   /*                                                                */
   /******************************************************************/
   if( Hcon!= MQHC_UNUSABLE_HCONN)
   {
     MQDISC(&Hcon,                     /* connection handle          */
            &CompCode,                 /* completion code            */
            &Reason);                  /* reason code                */

     /* report reason, if any     */
     if (Reason != MQRC_NONE)
     {
       printf("MQDISC ended with reason code %d\n", Reason);
       rc = ERROR_RETURN;
     }
   }

mod_exit:
   /******************************************************************/
   /*                                                                */
   /* END OF AMQSSUBA                                                */
   /*                                                                */
   /******************************************************************/
   if( sruDebug > 0 )
     printf("Sample AMQSRUAA end rc=%d\n", rc);
   return(rc);

}

/*********************************************************************/
/* sruPromptForClass:                                                */
/* If no class was explicitly requested then list the classes        */
/* supported, prompt for a class, and then validate the supplied     */
/* class.                                                            */
/*                                                                   */
/*********************************************************************/
int sruPromptForClass()
{
  int class=-1;
  int strLen=0;
  int idx;
  sruCLASS *pClass;
  char      Buffer[128];
  /* List the supported classes */
  for( pClass = ClassChain; pClass ; pClass = pClass->next )
  {
    printf("%.*s : %.*s\n"
          , pClass->classLen
          , pClass->Buffer
          , pClass->descLen
          , pClass->Buffer+pClass->classLen );
  }
  /* Prompt for one of those classes */
  printf("Enter Class selection\n");
  printf("==> ");
  fgets( Buffer, sizeof(Buffer), stdin );
  for( idx = 0 ; idx < sizeof(Buffer) ; idx ++ )
  {
    if( ( 0 == isalpha( Buffer[idx] ) )
      &&( 0 == isdigit( Buffer[idx] ) ) )
    {
      Buffer[idx] = 0 ;
      strLen = idx ;
      break;
    }
  }
  /* Validate the supplied class is one of those supported */
  for( pClass = ClassChain; pClass ; pClass = pClass->next )
  {
    if( (strLen == pClass->classLen )
      &&(0 == memcmp( pClass->Buffer, Buffer, strLen) ) )
    {
      strncpy(Class[ClassCount++], Buffer, MAX_CLASS_SIZE);
      return pClass->id;
    }
  }

  printf("Invalid Class (%s) supplied\n", Buffer);
  return -1;
}

/*********************************************************************/
/* sruPromptForType:                                                 */
/* If no type was explicitly requested then list the types           */
/* supported (withing the 'current' class), prompt for a type, and   */
/* then validate the supplied type.                                  */
/*                                                                   */
/*********************************************************************/
int sruPromptForType( )
{
  size_t       strLen=0;
  int idx;
  sruCLASS *pClass;
  sruTYPE  *pType;
  char      Buffer[128];
  strLen = strlen( Class[0] );
  /* Find the relevant class meta-data */
  for( pClass = ClassChain; pClass ; pClass = pClass->next )
  {
    if( (pClass->classLen == strLen )
      &&(0 == memcmp( Class[0], pClass->Buffer, strLen ) ) )
    {
      /* List the types supported by this class */
      for( pType = pClass->type; pType ; pType = pType->next )
      {
         printf("%.*s : %.*s\n"
              , pType->typeLen
              , pType->Buffer
              , pType->descLen
              , pType->Buffer+pType->typeLen );
      }
      /* Prompt for one of those types */
      printf("Enter Type selection\n" );
      printf("==> ");
      fgets( Buffer, sizeof(Buffer), stdin );
      for( idx = 0 ; idx < sizeof(Buffer) ; idx ++ )
      {
        if( ( 0 == isalpha( Buffer[idx] ) )
          &&( 0 == isdigit( Buffer[idx] ) ) )
        {
          Buffer[idx] = 0 ;
          strLen = idx ;
          break;
        }
      }
      /* Validate the supplied type is one of those supported */
      for( pType = pClass->type; pType ; pType = pType->next )
      {
       if( (strLen == pType->typeLen )
         &&( 0 == memcmp( pType->Buffer, Buffer, strLen) ) )
       {
         strncpy(Type[ClassCount-1], Buffer, MAX_TYPE_SIZE);
         ClassTypeCount[ClassCount-1] ++;
         TypeCount ++ ;
         return( pType->id );
       }
      }
      printf("Invalid Type (%s) supplied\n", Buffer);
      return -1;
    }
  }
  printf("Invalid Class (%s) supplied\n", Class[0]);
  return -1 ;
}

/*********************************************************************/
/* sruSubscribe:                                                     */
/* Subscribe to the required resource usage publications.            */
/*                                                                   */
/* Lookup the topic strings under which resource usage information   */
/* will be published for the requested class/type pairs.             */
/* Subscribe to those topics.                                        */
/*                                                                   */
/*********************************************************************/
int sruSubscribe()
{
  MQSD     sd = {MQSD_DEFAULT};    /* Subscription Descriptor       */
  MQHOBJ   Hsub = MQHO_NONE;       /* object handle                 */
  char     Buffer[4096];
  char     ObjectName[MQ_OBJECT_NAME_LENGTH+1];
  int      idx, idx1, idx2, idx3;
  size_t    strLen;
  char     *pObjName=0;
  sruCLASS *pClass;
  sruTYPE  *pType;
  MQLONG    CompCode, Reason;

  if( sruDebug > 0 )
    printf( "ClassCount: %d TypeCount:%d\n", ClassCount, TypeCount );
  for( idx1 = 0, idx3=0 ; idx1 < ClassCount ; idx1 ++ )
  {
    strLen = strlen( Class[idx1] );
    for( pClass = ClassChain; pClass; pClass = pClass->next )
    {
      if( (pClass->classLen == strLen )
        &&(0 == memcmp(pClass->Buffer, Class[idx1], strLen)))
        break;
    }
    if( NULL == pClass )
    {
      printf("Class %s not recognised\n", Class[idx1]);
      return -1;
    }
    for( idx2 = 0 ; idx2 < ClassTypeCount[idx1]; idx2 ++, idx3 ++ )
    {
      strLen = strlen( Type[idx3] );
      for( pType = pClass->type; pType ; pType = pType->next )
      {
        if( (pType->typeLen == strLen )
          &&(0 == memcmp( pType->Buffer, Type[idx3], strLen) ) )
          break;
      }
      if( pType )
      {
        /********************************************************************************/
        /* If the meta-data included MQIAMO_MONITOR_FLAGS_OBJNAME then the data         */
        /* is available on a per-object basis. Under these circumstances we must have   */
        /* an object name to qualify the topic.                                         */
        /********************************************************************************/
        if( pType->flags & MQIAMO_MONITOR_FLAGS_OBJNAME )
        {
          if( TypeObjName[idx3] )
              pObjName = TypeObjName[idx3];
          else if( ClassObjName[idx1] )
              pObjName = ClassObjName[idx1];
          if( !pObjName )
          {
            printf("An object name is required for Class(%.*s) Type(%.*s)\n"
                  , pClass->classLen, pClass->Buffer, pType->typeLen, pType->Buffer );
            printf("Enter object name\n");

            printf("==> ");
            memset( ObjectName, 0, MQ_OBJECT_NAME_LENGTH);
            fgets( ObjectName, MQ_OBJECT_NAME_LENGTH, stdin );
            for( idx = 0 ; idx < MQ_OBJECT_NAME_LENGTH ; idx ++ )
            {
              if( '\n' == ObjectName[idx] )
              {
                ObjectName[idx] = 0 ;
                break;
              }
            }
            ObjectName[MQ_OBJECT_NAME_LENGTH] = 0 ;
            pObjName = ObjectName;
          }
          sprintf( Buffer, pType->Buffer + pType->typeLen + pType->descLen, pObjName );
          sd.ObjectString.VSPtr = Buffer;
          sd.ObjectString.VSLength = (MQLONG)(pType->topicLen + strlen(pObjName) - strlen("%s"));
          sd.ObjectString.VSCCSID = pType->topicCCSID;
        }
        else
        {
          sd.ObjectString.VSPtr = pType->Buffer + pType->typeLen + pType->descLen;
          sd.ObjectString.VSLength = pType->topicLen;
          sd.ObjectString.VSCCSID = pType->topicCCSID;
        }
        if( sruDebug > 0 )
          printf( "Subscribing to topic: %.*s\n", sd.ObjectString.VSLength, sd.ObjectString.VSPtr );
        sd.Options =   MQSO_CREATE | MQSO_NON_DURABLE | MQSO_FAIL_IF_QUIESCING ;
        MQSUB(Hcon, &sd, &Hobj, &Hsub, &CompCode, &Reason);
        if( MQCC_OK != CompCode )
        {
          printf("Subscribe to %.*s failed for %u:%u\n"
                , sd.ObjectString.VSLength, sd.ObjectString.VSPtr, CompCode, Reason );
          return ERROR_RETURN;
        }
      }
      else
      {
        printf("Class: %s Type: %s not recognised\n", Class[idx1], Type[idx3]);
        return ERROR_RETURN;
      }
    }
  }

  return 0 ;
}

/*********************************************************************/
/* sruDiscover:                                                      */
/* subscribe to the meta data topics and read the meta data retained */
/* publications to determine what resource usage data will be        */
/* published by the currently connected queue manager.               */
/*                                                                   */
/* the queue manager publishes a set of PCF messages, as retained    */
/* messages, describing what resource usage information can be       */
/* subscribed to in the current queue manager environment.           */
/*                                                                   */
/* Note that this information is relatively dynamic, and might       */
/* change with the queue manager configuration, or service level.    */
/*                                                                   */
/* This data is ideally re-processed each time the queue manager     */
/* is restarted, and 'applications' don't depend upon hard coded     */
/* values other than the location of the root of the meta data.      */
/*                                                                   */
/*                                                                   */
/*********************************************************************/
int sruDiscover()
{
   MQHOBJ   Hsub = MQHO_NONE;       /* object handle                 */
   MQSD     sd = {MQSD_DEFAULT};    /* Subscription Descriptor       */
   MQMD     md = {MQMD_DEFAULT};    /* Message Descriptor            */
   MQGMO    gmo = {MQGMO_DEFAULT};  /* get message options           */
   MQLONG   CompCode, Reason;
   char     Buffer[4096];
   PMQCFH   cfh;
   PMQCFIN  cfin;
   PMQCFST  cfst;
   PMQCFGR  cfgr;
   char *   topic;
   char *   type;
   char *   class;
   char *   desc;
   MQLONG   topic_len;
   MQLONG   topic_ccsid;
   MQLONG   type_len;
   MQLONG   class_len;
   MQLONG   desc_len;
   MQLONG   msg_len;
   MQLONG   id, class_id, type_id, elem_id, elem_type, flags;
   int      idx;
   int      idx1;
   int      rc=ERROR_RETURN;
   sruCLASS *pClass;
   sruTYPE  *pType;
   char      NLS_suffix[8]={0};
   char *    lang;

   /******************************************************************/
   /* The meta-data is published in a set of languages. Use the      */
   /* first five bytes of the LANG environment variable as the       */
   /* suffix.                                                        */
   /* By restricting presentation of 'constant' data to the strings  */
   /* in the meta-data we achieve a level of NLS independance.       */
   /******************************************************************/
   lang = getenv("LANG");
   if( (lang ) && (strcmp(lang, "C")))
   {
     sprintf(NLS_suffix, "/%.5s", lang );
   }

   /******************************************************************/
   /*                                                                */
   /* Subscribe using our previously created TDQ destination queue   */
   /*                                                                */
   /******************************************************************/
   sprintf( Buffer, META_PREFIX "CLASSES%s", QMName, NLS_suffix );
   sd.ObjectString.VSPtr = Buffer;
   sd.ObjectString.VSLength = (MQLONG)strlen(Buffer);
   sd.Options =   MQSO_CREATE | MQSO_NON_DURABLE | MQSO_FAIL_IF_QUIESCING ;

   if( sruDebug > 0 )
      printf( "Subscribing to topic: %.*s\n", sd.ObjectString.VSLength, sd.ObjectString.VSPtr );

   MQSUB(Hcon, &sd, &Hobj, &Hsub, &CompCode, &Reason);
   /* report reason, if any; stop if failed      */
   if (Reason != MQRC_NONE)
   {
     printf("MQSUB ended with reason code %d\n", Reason);
     goto mod_exit;
   }

   gmo.Version = MQGMO_CURRENT_VERSION;
   gmo.Options =   MQGMO_NO_SYNCPOINT | MQGMO_CONVERT | MQGMO_NO_PROPERTIES | MQGMO_WAIT;
   gmo.MatchOptions = MQMO_NONE;
   /* We don't assume that the meta-data is published 'immediately'     */
   /* Give the queue manager a few seconds to produce this data.        */
   gmo.WaitInterval = 10000;
   md.Encoding       = MQENC_NATIVE;
   md.CodedCharSetId = MQCCSI_Q_MGR;

   MQGET(Hcon, Hobj, &md, &gmo, sizeof(Buffer), Buffer, &msg_len, &CompCode, &Reason);
   if( MQCC_OK != CompCode )
   {
     if( (MQRC_NO_MSG_AVAILABLE == Reason)
       &&(lang ) )
     {
       MQCLOSE(Hcon, &Hsub, MQCO_NONE, &CompCode, &Reason);
       /* It looks like the requested language is not available, use the default */
       sprintf( Buffer, META_PREFIX "CLASSES", QMName );
       sd.ObjectString.VSPtr = Buffer;
       sd.ObjectString.VSLength = (MQLONG)strlen(Buffer);
       sd.Options =   MQSO_CREATE | MQSO_NON_DURABLE | MQSO_FAIL_IF_QUIESCING ;
       if( sruDebug > 0 )
          printf( "Subscribing to topic: %.*s\n", sd.ObjectString.VSLength, sd.ObjectString.VSPtr );
       MQSUB(Hcon, &sd, &Hobj, &Hsub, &CompCode, &Reason);
       /* report reason, if any; stop if failed      */
       if (Reason != MQRC_NONE)
       {
         printf("MQSUB ended with reason code %d\n", Reason);
         goto mod_exit;
       }
       MQGET(Hcon, Hobj, &md, &gmo, sizeof(Buffer), Buffer, &msg_len, &CompCode, &Reason);
     }
     if( MQCC_OK != CompCode )
     {
       printf("MQGET ended with reason code %d\n", Reason);
       goto mod_exit;
     }
   }

   MQCLOSE(Hcon, &Hsub, MQCO_NONE, &CompCode, &Reason);
   if( MQCC_OK != CompCode )
   {
     printf("MQCLOSE ended with reason code %d\n", Reason);
     goto mod_exit;
   }

   /****************************************************************************/
   /* Parse the resulting publication and populate the first level of the      */
   /* class/type/element tree based upon the content.                          */
   /*                                                                          */
   /* Note that it is good practice not to depend upon the order of elements   */
   /* in a PCF message.                                                        */
   /****************************************************************************/
   cfh = (PMQCFH)Buffer;
   cfin = (PMQCFIN)(Buffer+cfh->StrucLength);
   for( idx = 0; idx < cfh->ParameterCount ; idx++ )
   {
     if( (cfin->Type == MQCFT_GROUP )
       &&(cfin->Parameter == MQGACF_MONITOR_CLASS ))
     {
       cfgr = (PMQCFGR)cfin;
       cfin = (PMQCFIN)(((char *)cfgr)+cfgr->StrucLength);
       topic= class= desc="";
       topic_len= class_len= desc_len=0;
       id = -1;
       for(idx1=0 ; idx1 < cfgr->ParameterCount; idx1++)
       {
         cfst = (PMQCFST)cfin;
         if( (cfin->Type == MQCFT_INTEGER)
           &&(cfin->Parameter == MQIAMO_MONITOR_CLASS) )
         {
           class_id = cfin->Value;
           cfin = (PMQCFIN)(((char *)cfin)+cfin->StrucLength);
         }
         else if( (cfin->Type == MQCFT_STRING)
           &&(cfin->Parameter == MQCAMO_MONITOR_CLASS) )
         {
           class_len = cfst->StringLength;
           class = cfst->String;;
           cfin = (PMQCFIN)(((char *)cfst)+cfst->StrucLength);
         }
         else if( (cfin->Type == MQCFT_STRING)
           &&(cfin->Parameter == MQCAMO_MONITOR_DESC) )
         {
           desc_len = cfst->StringLength;
           desc = cfst->String;;
           cfin = (PMQCFIN)(((char *)cfst)+cfst->StrucLength);
         }
         else if( (cfin->Type == MQCFT_STRING)
           &&(cfin->Parameter == MQCA_TOPIC_STRING) )
         {
           topic_len = cfst->StringLength;
           topic = cfst->String;;
           topic_ccsid = cfst->CodedCharSetId;
           cfin = (PMQCFIN)(((char *)cfst)+cfst->StrucLength);
         }
         else if( (cfin->Type == MQCFT_INTEGER )
                &&(cfin->Parameter == MQIAMO_MONITOR_FLAGS ))
         {
           flags = cfin->Value;
           cfin = (PMQCFIN)(((char *)cfin)+cfin->StrucLength);
         }
         else
           cfin = (PMQCFIN)(((char *)cfin)+cfin->StrucLength);
       }
       rc = sruRegisterClass( class_id, class, class_len, desc, desc_len, topic, topic_len, topic_ccsid, flags);
       if( rc )
           goto mod_exit;
     }
     else
       cfin = (PMQCFIN)(((char *)cfin)+cfin->StrucLength);
   }

   /****************************************************************************/
   /* For each of the discovered classes, discover what types are supported    */
   /* by those classes and populate the second level of the class/type/        */
   /* element tree.                                                            */
   /****************************************************************************/
   for(pClass = ClassChain ; pClass ; pClass=pClass->next)
   {
     sprintf( Buffer, "%.*s", pClass->topicLen
                           , pClass->Buffer+ pClass->classLen+pClass->descLen );
     sd.ObjectString.VSPtr = Buffer;
     sd.ObjectString.VSLength = pClass->topicLen;
     sd.ObjectString.VSCCSID = pClass->topicCCSID;
     sd.Options =   MQSO_CREATE | MQSO_NON_DURABLE | MQSO_FAIL_IF_QUIESCING ;

     if( sruDebug > 0 )
        printf( "Subscribing to topic: %.*s\n", sd.ObjectString.VSLength, sd.ObjectString.VSPtr );

     MQSUB(Hcon, &sd, &Hobj, &Hsub, &CompCode, &Reason);
     /* report reason, if any; stop if failed      */
     if (Reason != MQRC_NONE)
     {
       printf("MQSUB ended with reason code %d\n", Reason);
       goto mod_exit;
     }

     gmo.Version = MQGMO_CURRENT_VERSION;
     gmo.Options =   MQGMO_NO_SYNCPOINT | MQGMO_CONVERT | MQGMO_NO_PROPERTIES | MQGMO_WAIT;
     gmo.MatchOptions = MQMO_NONE;
     /* We don't assume that the meta-data is published 'immediately'     */
     /* Give the queue manager a few seconds to produce this data.        */
     gmo.WaitInterval = 10000;
     md.Encoding       = MQENC_NATIVE;
     md.CodedCharSetId = MQCCSI_Q_MGR;

     MQGET(Hcon, Hobj, &md, &gmo, sizeof(Buffer), Buffer, &msg_len, &CompCode, &Reason);
     if( MQCC_OK != CompCode )
     {
       printf("MQGET ended with reason code %d\n", Reason);
       rc = ERROR_RETURN;
       goto mod_exit;
     }

     MQCLOSE(Hcon, &Hsub, MQCO_NONE, &CompCode, &Reason);
     if( MQCC_OK != CompCode )
     {
       printf("MQCLOSE ended with reason code %d\n", Reason);
       rc = ERROR_RETURN;
       goto mod_exit;
     }

     cfh = (PMQCFH)Buffer;
     cfin = (PMQCFIN)(Buffer+cfh->StrucLength);
     for( idx = 0 ; idx < cfh->ParameterCount; idx++ )
     {
       if( (cfin->Type == MQCFT_INTEGER )
         &&(cfin->Parameter == MQIAMO_MONITOR_CLASS ))
       {
         class_id = cfin->Value;
         cfin = (PMQCFIN)(((char *)cfin)+cfin->StrucLength);
       }
       else if( (cfin->Type == MQCFT_INTEGER )
              &&(cfin->Parameter == MQIAMO_MONITOR_FLAGS ))
       {
         flags = cfin->Value;
         cfin = (PMQCFIN)(((char *)cfin)+cfin->StrucLength);
       }
       else if( (cfin->Type == MQCFT_GROUP )
         &&(cfin->Parameter == MQGACF_MONITOR_TYPE ))
       {
         cfgr = (PMQCFGR)cfin;
         cfin = (PMQCFIN)(((char *)cfgr)+cfgr->StrucLength);
         topic= type= desc="";
         topic_len= type_len= desc_len=0;
         id = -1;
         for(idx1=0 ; idx1 < cfgr->ParameterCount; idx1++)
         {
           cfst = (PMQCFST)cfin;
           if( (cfin->Type == MQCFT_INTEGER)
             &&(cfin->Parameter == MQIAMO_MONITOR_TYPE) )
           {
             type_id = cfin->Value;
             cfin = (PMQCFIN)(((char *)cfin)+cfin->StrucLength);
           }
           else if( (cfin->Type == MQCFT_STRING)
             &&(cfin->Parameter == MQCAMO_MONITOR_TYPE) )
           {
             type_len = cfst->StringLength;
             type = cfst->String;;
             cfin = (PMQCFIN)(((char *)cfst)+cfst->StrucLength);
           }
           else if( (cfin->Type == MQCFT_STRING)
             &&(cfin->Parameter == MQCAMO_MONITOR_DESC) )
           {
             desc_len = cfst->StringLength;
             desc = cfst->String;
             cfin = (PMQCFIN)(((char *)cfst)+cfst->StrucLength);
           }
           else if( (cfin->Type == MQCFT_STRING)
             &&(cfin->Parameter == MQCA_TOPIC_STRING) )
           {
             topic_len = cfst->StringLength;
             topic = cfst->String;
             topic_ccsid = cfst->CodedCharSetId;
             cfin = (PMQCFIN)(((char *)cfst)+cfst->StrucLength);
           }
           else
             cfin = (PMQCFIN)(((char *)cfin)+cfin->StrucLength);
         }
         rc = sruRegisterType( class_id, type_id, type, type_len, desc, desc_len, topic, topic_len, topic_ccsid, flags);
         if( rc )
           goto mod_exit;
       }
       else
         cfin = (PMQCFIN)(((char *)cfin)+cfin->StrucLength);
     }
   }

   /****************************************************************************/
   /* For each type, within each class, discover what elements are published   */
   /* in those publications, and add the element descriptions to the           */
   /* class/type/element tree.                                                 */
   /****************************************************************************/
   for(pClass = ClassChain ; pClass ; pClass=pClass->next)
   {
     for( pType = pClass->type; pType; pType = pType->next )
     {
       sprintf( Buffer, "%.*s", pType->topicLen
                             , pType->Buffer+ pType->typeLen+pType->descLen );
       sd.ObjectString.VSPtr = Buffer;
       sd.ObjectString.VSLength = pType->topicLen;
       sd.ObjectString.VSCCSID = pType->topicCCSID;
       sd.Options =   MQSO_CREATE | MQSO_NON_DURABLE | MQSO_FAIL_IF_QUIESCING ;

       if( sruDebug > 0 )
          printf( "Subscribing to topic: %.*s\n", sd.ObjectString.VSLength, sd.ObjectString.VSPtr );
       MQSUB(Hcon, &sd, &Hobj, &Hsub, &CompCode, &Reason);
       /* report reason, if any; stop if failed      */
       if (Reason != MQRC_NONE)
       {
         printf("MQSUB ended with reason code %d\n", Reason);
         rc = ERROR_RETURN;
         goto mod_exit;
       }

       gmo.Version = MQGMO_CURRENT_VERSION;
       gmo.Options =   MQGMO_NO_SYNCPOINT | MQGMO_CONVERT | MQGMO_NO_PROPERTIES | MQGMO_WAIT;
       gmo.MatchOptions = MQMO_NONE;
       /* We don't assume that the meta-data is published 'immediately'     */
       /* Give the queue manager a few seconds to produce this data.        */
       gmo.WaitInterval = 10000;
       md.Encoding       = MQENC_NATIVE;
       md.CodedCharSetId = MQCCSI_Q_MGR;

       MQGET(Hcon, Hobj, &md, &gmo, sizeof(Buffer), Buffer, &msg_len, &CompCode, &Reason);
       if( MQCC_OK != CompCode )
       {
         printf("MQGET ended with reason code %d\n", Reason);
         rc = ERROR_RETURN;
         goto mod_exit;
       }

       MQCLOSE(Hcon, &Hsub, MQCO_NONE, &CompCode, &Reason);
       if( MQCC_OK != CompCode )
       {
         printf("MQCLOSE ended with reason code %d\n", Reason);
         rc = ERROR_RETURN;
         goto mod_exit;
       }

       cfh = (PMQCFH)Buffer;
       cfin = (PMQCFIN)(Buffer+cfh->StrucLength);
       for( idx = 0 ; idx < cfh->ParameterCount; idx++ )
       {
         if( (cfin->Type == MQCFT_INTEGER )
           &&(cfin->Parameter == MQIAMO_MONITOR_CLASS ))
         {
           class_id = cfin->Value;
           cfin = (PMQCFIN)(((char *)cfin)+cfin->StrucLength);
         }
         else if( (cfin->Type == MQCFT_INTEGER )
                &&(cfin->Parameter == MQIAMO_MONITOR_TYPE ))
         {
           type_id = cfin->Value;
           cfin = (PMQCFIN)(((char *)cfin)+cfin->StrucLength);
         }
         else if( (cfin->Type == MQCFT_STRING)
           &&(cfin->Parameter == MQCA_TOPIC_STRING) )
         {
           cfst = (PMQCFST)cfin;
           topic_len = cfst->StringLength;
           topic = cfst->String;;
           topic_ccsid = cfst->CodedCharSetId;
           cfin = (PMQCFIN)(((char *)cfst)+cfst->StrucLength);
         }
         else if( (cfin->Type == MQCFT_GROUP )
           &&(cfin->Parameter == MQGACF_MONITOR_ELEMENT ))
         {
           cfgr = (PMQCFGR)cfin;
           cfin = (PMQCFIN)(((char *)cfgr)+cfgr->StrucLength);
           topic= type= desc="";
           topic_len= type_len= desc_len=0;
           id = -1;
           for(idx1=0 ; idx1 < cfgr->ParameterCount; idx1++)
           {
             cfst = (PMQCFST)cfin;
             if( (cfin->Type == MQCFT_INTEGER)
               &&(cfin->Parameter == MQIAMO_MONITOR_ELEMENT) )
             {
               elem_id = cfin->Value;
               cfin = (PMQCFIN)(((char *)cfin)+cfin->StrucLength);
             }
             else if( (cfin->Type == MQCFT_INTEGER)
               &&(cfin->Parameter == MQIAMO_MONITOR_DATATYPE) )
             {
               elem_type = cfin->Value;
               cfin = (PMQCFIN)(((char *)cfin)+cfin->StrucLength);
             }
             else if( (cfin->Type == MQCFT_STRING)
               &&(cfin->Parameter == MQCAMO_MONITOR_DESC) )
             {
               desc_len = cfst->StringLength;
               desc = cfst->String;;
               cfin = (PMQCFIN)(((char *)cfst)+cfst->StrucLength);
             }
             else
               cfin = (PMQCFIN)(((char *)cfin)+cfin->StrucLength);
           }
           rc = sruRegisterElement( class_id, type_id, elem_id, elem_type, desc, desc_len);
           if( rc )
             goto mod_exit;
         }
         else
           cfin = (PMQCFIN)(((char *)cfin)+cfin->StrucLength);
       }
       rc = sruUpdateType( class_id, type_id, topic, topic_len, topic_ccsid);
       if( rc )
         goto mod_exit;
     }
   }

mod_exit:
  return (int)Reason ;
}

/*********************************************************************/
/* sruFormatMessage:                                                 */
/* Format a resource usage publication.                              */
/*                                                                   */
/*********************************************************************/
int sruFormatMessage( PMQCFH cfh
                    , MQMD * pMd )
{
  MQLONG class=-1, type=-1;
  sruCLASS * pClass;
  sruTYPE  * pType;
  sruELEM  * pElem;
  MQLONG     Count;
  PMQCFIN    cfin;
  PMQCFST    cfst;
  PMQCFIN64  cfin64;
  MQINT64    interval=-1;
  MQINT64    int64;
  char     * pObjName=0;
  MQINT64    days=0, hours=0,minutes=0,seconds=0, millisecs=0;

  printf( "Publication received PutDate:%8.8s PutTime:%8.8s"
        , pMd->PutDate, pMd->PutTime );
  cfin = (PMQCFIN)(((char *)cfh)+cfh->StrucLength);
  /* Once again, it is good practise not to depend upon   */
  /* the order of elements in a PCF message.              */
  /* First pass to establish the Class, Type and interval */
  for( Count = 0 ; Count < cfh->ParameterCount; Count ++ )
  {
    if( (cfin->Type == MQCFT_INTEGER )
      &&(cfin->Parameter == MQIAMO_MONITOR_CLASS ))
    {
      class = cfin->Value;
    }
    else if( (cfin->Type == MQCFT_INTEGER )
           &&(cfin->Parameter == MQIAMO_MONITOR_TYPE ))
    {
      type = cfin->Value;
    }
    else if( (cfin->Type == MQCFT_INTEGER64 )
           &&(cfin->Parameter == MQIAMO64_MONITOR_INTERVAL ))
    {
      printf( " Interval:");
      cfin64 = (PMQCFIN64) cfin;
      memcpy(&interval,&cfin64->Value,sizeof(interval));
      millisecs = (interval % 1000000)/1000;
      seconds = interval / 1000000 ;
      if( seconds )
      {
        minutes = seconds / 60 ;
        seconds = seconds % 60;
        if( minutes )
        {
          hours = minutes / 60;
          minutes = minutes % 60;
          if( hours )
          {
            days = hours / 24 ;
            hours = hours % 24 ;
            if (days)
               printf( "%" Int64 "u days,", days );
            printf( "%" Int64 "u hours,", hours );
          }
          printf( "%" Int64 "u minutes,", minutes );
        }
      }
      printf( "%" Int64 "u.%3.3" Int64 "u seconds", seconds, millisecs );
    }
    else if( (cfin->Type == MQCFT_STRING )
           &&(cfin->Parameter == MQCA_Q_NAME ))
    {
      cfst = (PMQCFST)cfin;
      pObjName = cfst->String;
    }
    cfin = (PMQCFIN)(((char *)cfin)+cfin->StrucLength);
  }
  printf( "\n");


  pClass = sruIdToClass( class );
  if( NULL == pClass )
  {
    printf( "Unrecognised resource usage message\n");
    return ERROR_RETURN;
  }
  pType = sruIdToType( pClass, type );
  if( NULL == pType )
  {
    printf( "Unrecognised resource usage message for class %.*s %d\n",
             pClass->classLen, pClass->Buffer, type);
    return ERROR_RETURN;
  }


  cfin = (PMQCFIN)(((char *)cfh)+cfh->StrucLength);
  for( Count = 0 ; Count < cfh->ParameterCount; Count ++ )
  {
    if( (cfin->Type == MQCFT_INTEGER)
      &&( (cfin->Parameter == MQIAMO_MONITOR_CLASS )
        ||(cfin->Parameter == MQIAMO_MONITOR_TYPE )
        ||(cfin->Parameter == MQIACF_OBJECT_TYPE ) ) )
    {
    }
    else if( (cfin->Type == MQCFT_INTEGER64)
           &&(cfin->Parameter == MQIAMO64_MONITOR_INTERVAL ) )
    {
    }
    else
    {
      pElem = sruIdToElem( pType, cfin->Parameter );
      if( pElem )
      {
         sruFormatElem( cfin, pElem, interval, pObjName );
      }
      else
      {
        if(cfin->Type == MQCFT_INTEGER)
        {
          printf("Parameter(%u) Value(%u) not recognised\n"
                , cfin->Parameter, cfin->Value );
        }
        else if(cfin->Type == MQCFT_INTEGER64)
        {
          cfin64 = (PMQCFIN64)cfin;
          memcpy(&int64, &cfin64->Value, sizeof(int64));
          printf("Parameter(%u) Value(%" Int64 "u) not recognised\n"
                , cfin64->Parameter, int64 );
        }
      }
    }
    cfin = (PMQCFIN)(((char *)cfin)+cfin->StrucLength);
  }

  return 0;
}

/*********************************************************************/
/* sruFormatElem:                                                    */
/* The element data in the meta-data includes some information on    */
/* the 'type' of the element, and therefore how that element might   */
/* be displayed.                                                     */
/*                                                                   */
/*                                                                   */
/*********************************************************************/
void sruFormatElem( PMQCFIN cfin
                  , sruELEM *pElem
                  , MQINT64 interval
                  , char  * pObjName )
{
  PMQCFIN64 cfin64;
  float f;
  MQINT64 rate;
  MQINT64 int64;
  char    buffer[64];

  if( pObjName )
  {
    printf( "%-48.48s ", pObjName );
  }
  cfin64 = (PMQCFIN64) cfin ;
  switch( pElem->type )
  {
    case MQIAMO_MONITOR_PERCENT:
         if( cfin->Type == MQCFT_INTEGER )
           f = (float)cfin->Value ;
         else
         {
           memcpy(&int64, &cfin64->Value, sizeof(int64));
           f = (float)int64;
         }
         printf( "%.*s %.2f%%\n", pElem->descLen, pElem->Buffer, f/100 );
         break;
    case MQIAMO_MONITOR_HUNDREDTHS:
         if( cfin->Type == MQCFT_INTEGER )
           f = (float)cfin->Value ;
         else
         {
           memcpy(&int64, &cfin64->Value, sizeof(int64));
           f = (float)int64;
         }
         printf( "%.*s %.2f\n", pElem->descLen, pElem->Buffer, f/100 );
         break;
    case MQIAMO_MONITOR_KB:
         if( cfin->Type == MQCFT_INTEGER )
           printf( "%.*s %dKB\n", pElem->descLen, pElem->Buffer, cfin->Value );
         else
         {
           memcpy(&int64, &cfin64->Value, sizeof(int64));
           printf( "%.*s %" Int64 "dKB\n", pElem->descLen, pElem->Buffer, int64 );
         }
         break;
    case MQIAMO_MONITOR_MB:
         if( cfin->Type == MQCFT_INTEGER )
           printf( "%.*s %dMB\n", pElem->descLen, pElem->Buffer, cfin->Value );
         else
         {
           memcpy(&int64, &cfin64->Value, sizeof(int64));
           printf( "%.*s %" Int64 "dMB\n", pElem->descLen, pElem->Buffer, int64 );
         }
         break;
    case MQIAMO_MONITOR_GB:
         if( cfin->Type == MQCFT_INTEGER )
           printf( "%.*s %dGB\n", pElem->descLen, pElem->Buffer, cfin->Value );
         else
         {
           memcpy(&int64, &cfin64->Value, sizeof(int64));
           printf( "%.*s %" Int64 "dGB\n", pElem->descLen, pElem->Buffer, int64 );
         }
         break;
    case MQIAMO_MONITOR_MICROSEC:
         if( cfin->Type == MQCFT_INTEGER )
           printf( "%.*s %d uSec\n", pElem->descLen, pElem->Buffer, cfin->Value );
         else
         {
           memcpy(&int64, &cfin64->Value, sizeof(int64));
           printf( "%.*s %" Int64 "d uSec\n", pElem->descLen, pElem->Buffer, int64 );
         }
         break;
    case MQIAMO_MONITOR_DELTA:
         /* We've been given a delta value, the duration over which the delta */
         /* occurred is 'interval' micro-seconds.                             */
         if( cfin->Type == MQCFT_INTEGER )
           rate = cfin->Value ;
         else
         {
           memcpy(&int64, &cfin64->Value, sizeof(int64));
           rate = int64;
         }
         if( interval < 10000 )
         {
           /* Very short intervals can result in misleading rates, we therefore */
           /* simply ignore rates for intervals of less than 10 milliseconds.   */
           buffer[0] = 0 ;
         }
         else
         {
           rate = rate * 1000000 ;
           rate = rate + (interval/2);
           rate = rate / interval ;
           if( rate == 0 )
             buffer[0] = 0 ;
           else
             sprintf( buffer, "%" Int64 "u/sec", rate );
         }
         if( cfin->Type == MQCFT_INTEGER )
           printf( "%.*s %d %s\n", pElem->descLen, pElem->Buffer, cfin->Value, buffer );
         else
         {
           memcpy(&int64, &cfin64->Value, sizeof(int64));
           printf( "%.*s %" Int64 "d %s\n", pElem->descLen, pElem->Buffer, int64, buffer );
         }
         break;
    default:
         if( cfin->Type == MQCFT_INTEGER )
           printf( "%.*s %d\n", pElem->descLen, pElem->Buffer, cfin->Value );
         else
         {
           memcpy(&int64, &cfin64->Value, sizeof(int64));
           printf( "%.*s %" Int64 "d\n", pElem->descLen, pElem->Buffer, int64 );
         }
         break;
  }
  return;
}


/*********************************************************************/
/* sruRegisterClass:                                                 */
/* remember what classes are supported.                              */
/*                                                                   */
/*********************************************************************/
int sruRegisterClass( MQLONG id
                    , char * class
                    , MQLONG class_len
                    , char * desc
                    , MQLONG desc_len
                    , char * topic
                    , MQLONG topic_len
                    , MQLONG topic_ccsid
                    , MQLONG flags )
{
   sruCLASS *  pClass;
   sruCLASS ** ppClass;
   if( sruDebug > 0 )
     fprintf( stdout, "%4.4u %10.*s %.*s\n"
            , id, class_len, class, desc_len, desc );
   pClass  =(sruCLASS *)malloc(sizeof(sruCLASS) + class_len + desc_len + topic_len +1 );
   if( NULL == pClass )
   {
      printf("malloc failed");
      return ERROR_RETURN;
   }
   pClass->id = id ;
   pClass->type = NULL ;
   pClass->classLen = class_len;
   pClass->next = NULL;
   pClass->descLen = desc_len;
   pClass->topicLen = topic_len;
   pClass->topicCCSID = topic_ccsid;
   pClass->flags = flags;
   memcpy( pClass->Buffer, class, class_len );
   memcpy( pClass->Buffer+class_len, desc, desc_len );
   memcpy( pClass->Buffer+class_len+desc_len, topic, topic_len );
   pClass->Buffer[class_len+desc_len+topic_len] = 0 ;


   for( ppClass = &ClassChain ; ; ppClass=&(*ppClass)->next)
   {
     if( NULL == *ppClass )
     {
       *ppClass = pClass;
       break;
     }
   }
  return 0;
}

/*********************************************************************/
/* sruRegisterType:                                                  */
/* remember what types are supported.                                */
/*                                                                   */
/*********************************************************************/
int sruRegisterType(  MQLONG class_id
                    , MQLONG type_id
                    , char * type
                    , MQLONG type_len
                    , char * desc
                    , MQLONG desc_len
                    , char * topic
                    , MQLONG topic_len
                    , MQLONG topic_ccsid
                    , MQLONG flags )
{
   sruCLASS *  pClass;
   sruTYPE *   pType;
   sruTYPE **  ppType;
   if( sruDebug > 0 )
     fprintf( stdout, "%4.4u %4.4u %10.*s %.*s\n"
            , class_id, type_id, type_len, type, desc_len, desc );


   pClass = sruIdToClass( class_id );
   if( pClass )
   {
       pType  =(sruTYPE *)malloc(sizeof(sruTYPE) + type_len + desc_len + topic_len +1 );
       if( NULL == pType )
       {
          printf("malloc failed");
          return ERROR_RETURN;
       }
       pType->id = type_id ;
       pType->typeLen = type_len;
       pType->descLen = desc_len;
       pType->topicLen = topic_len;
       pType->topicCCSID = topic_ccsid;
       pType->flags = flags;
       pType->elem = NULL;
       memcpy( pType->Buffer, type, type_len );
       memcpy( pType->Buffer+type_len, desc, desc_len );
       memcpy( pType->Buffer+type_len+desc_len, topic, topic_len );
       pType->Buffer[type_len+desc_len+topic_len] = 0 ;
       pType->next = NULL;
       for( ppType = &pClass->type ; ; ppType=&(*ppType)->next)
       {
         if( NULL == *ppType )
         {
           *ppType = pType;
           break;
         }
       }
       return 0;
   }
   printf("unknown class: %u", class_id);
   return ERROR_RETURN;
}

/*********************************************************************/
/* sruUpdateType:                                                    */
/* Store the topic string for resource usage publications, rather    */
/* than that for metadata (discovery).                               */
/*********************************************************************/
int sruUpdateType(  MQLONG class_id
                    , MQLONG type_id
                    , char * topic
                    , MQLONG topic_len
                    , MQLONG topic_ccsid )
{
   sruCLASS *  pClass;
   sruTYPE *   pType;
   if( sruDebug > 0 )
     fprintf( stdout, "%4.4u %4.4u %.*s\n"
            , class_id, type_id, topic_len, topic );


   pClass = sruIdToClass( class_id );
   if( pClass )
   {
       pType = sruIdToType( pClass, type_id );
       if( NULL == pType )
       {
         printf("topic: %u.%u (%.*s)  unexpectedly not found"
               , class_id, type_id, topic_len, topic );
         return ERROR_RETURN;
       }
       /* The 'real' string should always be shorter than the metadata string */
       if( pType->topicLen < topic_len )
       {
         printf("topic metadata: %u.%u (%.*s)  unexpectedly short"
               , class_id, type_id, topic_len, topic );
         return -1;
       }
       memcpy( pType->Buffer + pType->typeLen + pType->descLen, topic, topic_len );
       pType->topicLen = topic_len;
       pType->topicCCSID = topic_ccsid;
       return 0;
   }
   printf("unknown class: %u", class_id);
   return ERROR_RETURN;
}

/*********************************************************************/
/* sruRegisterElement:                                               */
/* remember what elements are supported.                             */
/*                                                                   */
/*********************************************************************/
int sruRegisterElement( MQLONG class_id
                      , MQLONG type_id
                      , MQLONG elem_id
                      , MQLONG elemType
                      , char * desc
                      , MQLONG desc_len )
{
   sruCLASS *  pClass;
   sruTYPE *   pType;
   sruELEM *   pElem;
   sruELEM **  ppElem;
   if( sruDebug > 0 )
     fprintf( stdout, "%4.4u %4.4u %4.4u %4.4u %.*s\n"
            , class_id, type_id, elem_id, elemType, desc_len, desc );

   pClass = sruIdToClass( class_id );
   if( pClass )
   {
     pType = sruIdToType( pClass, type_id) ;
     if( pType )
     {
       pElem  =(sruELEM *)malloc(sizeof(sruELEM) + desc_len +1);
       if( NULL == pElem )
       {
          printf("malloc failed");
          return ERROR_RETURN;
       }
       pElem->id = elem_id ;
       pElem->type = elemType;
       pElem->descLen = desc_len;
       memcpy( pElem->Buffer, desc, desc_len );
       pElem->Buffer[desc_len] = 0 ;
       pElem->next = NULL;
       for( ppElem = &pType->elem ; ; ppElem=&(*ppElem)->next)
       {
         if( NULL == *ppElem )
         {
           *ppElem = pElem;
           break;
         }
       }
       return 0;
     }
   }
   printf("unknown class: %u", class_id);
   return ERROR_RETURN;
}

/*********************************************************************/
/* sruIdToClass:                                                     */
/* translate a class identifier to an sruCLASS pointer.              */
/*                                                                   */
/*********************************************************************/
sruCLASS * sruIdToClass( MQLONG class )
{
  sruCLASS * pClass;
  for( pClass = ClassChain; pClass ; pClass=pClass->next )
  {
    if( pClass->id == class)
      break;
  }
  return pClass;
}
/*********************************************************************/
/* sruIdToType:                                                      */
/* translate a type identifier to an sruTYPE pointer.                */
/*                                                                   */
/*********************************************************************/
sruTYPE  * sruIdToType( sruCLASS *pClass, MQLONG type )
{
  sruTYPE * pType;
  for( pType = pClass->type; pType ; pType=pType->next )
  {
    if( pType->id == type)
      break;
  }
  return pType;
}
/*********************************************************************/
/* sruIdToElem:                                                      */
/* translate an element identifier to an sruELEM pointer.            */
/*                                                                   */
/*********************************************************************/
sruELEM  * sruIdToElem( sruTYPE * pType, MQLONG elem )
{
  sruELEM * pElem;
  for( pElem = pType->elem; pElem ; pElem=pElem->next )
  {
    if( pElem->id == elem)
      break;
  }
  return pElem;
}


int  sruQueryQMName()
{
  int      rc = 0;
  int      index;
  MQOD     od = {MQOD_DEFAULT};    /* Object Descriptor             */
  MQLONG   CompCode, Reason ;
  MQLONG   Selector;
  MQHOBJ   LocalHobj;


  /******************************************************************/
  /*                                                                */
  /*   Open the queue manager object to find out its name           */
  /*                                                                */
  /******************************************************************/
  od.ObjectType = MQOT_Q_MGR;       /* open the queue manager object*/
  MQOPEN(Hcon,                      /* connection handle            */
         &od,                       /* object descriptor for queue  */
         MQOO_INQUIRE +             /* open it for inquire          */
           MQOO_FAIL_IF_QUIESCING,  /* but not if MQM stopping      */
         &LocalHobj,                /* object handle                */
         &CompCode,                 /* MQOPEN completion code       */
         &Reason);                  /* reason code                  */

  /* report error, if any      */
  if (CompCode)
  {
    printf("MQOPEN ended with reason code %d\n", Reason);
    printf("Unable to open queue manager for inquire\n");
    rc = ERROR_RETURN;
    goto mod_exit;
  }


  /****************************************************************/
  /*                                                              */
  /*   Inquire the name of the queue manager                      */
  /*                                                              */
  /****************************************************************/
  Selector = MQCA_Q_MGR_NAME;
  MQINQ(Hcon,                     /* connection handle            */
        LocalHobj,                /* object handle for q manager  */
        1,                        /* inquire only one selector    */
        &Selector,                /* the selector to inquire      */
        0,                        /* no integer attributes needed */
        NULL,                     /* so no buffer supplied        */
        MQ_Q_MGR_NAME_LENGTH,     /* inquiring a q manager name   */
        QMName,                   /* the buffer for the name      */
        &CompCode,                /* MQINQ completion code        */
        &Reason);                 /* reason code                  */
  if( CompCode )
  {
    /* report error, if any */
    printf("MQINQ ended with reason code %d\n", Reason);
    rc = ERROR_RETURN;
  }

  /* Remove and blank padding from the name */
  for( index = 0 ; index < MQ_Q_MGR_NAME_LENGTH ; index ++ )
  {
    if( 0 == QMName[index ] )
      break;
    if( ' ' == QMName[index ] )
    {
      QMName[index ] = 0 ;
      break;
    }
  }
  /******************************************************************/
  /*                                                                */
  /*   Close the queue manager object (if it was opened)            */
  /*                                                                */
  /******************************************************************/
  MQCLOSE(Hcon,                    /* connection handle           */
          &LocalHobj,              /* object handle               */
          MQCO_NONE,               /* no close options            */
          &CompCode,               /* completion code             */
          &Reason);                /* reason code                 */

  /* report error, if any     */
  if (CompCode)
  {
    printf("MQCLOSE ended with reason code %d\n", Reason);
    rc = ERROR_RETURN;
  }

mod_exit:
   return rc;

}

