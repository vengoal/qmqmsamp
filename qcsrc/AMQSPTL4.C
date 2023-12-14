/* %Z% %W% %I% %E% %U% */
 /********************************************************************/
 /*                                                                  */
 /* Program name: AMQSPTL4                                           */
 /*                                                                  */
 /* Description: Sample C program that puts messages to a list of    */
 /*              message queues (example using MQPUT and lists).     */
 /*                                                                  */
 /*   <copyright                                                     */
 /*   notice="lm-source-program"                                     */
 /*   pids="5724-H72,"                                               */
 /*   years="1993,2012"                                              */
 /*   crc="376627635" >                                              */
 /*   Licensed Materials - Property of IBM                           */
 /*                                                                  */
 /*   5724-H72,                                                      */
 /*                                                                  */
 /*   (C) Copyright IBM Corp. 1993, 2012 All Rights Reserved.        */
 /*                                                                  */
 /*   US Government Users Restricted Rights - Use, duplication or    */
 /*   disclosure restricted by GSA ADP Schedule Contract with        */
 /*   IBM Corp.                                                      */
 /*   </copyright>                                                   */
 /*                                                                  */
 /********************************************************************/
 /*                                                                  */
 /* Function:                                                        */
 /*                                                                  */
 /*                                                                  */
 /*   AMQSPTL4 is a sample C program to put messages on a list of    */
 /*   message queues.                                                */
 /*   AMQSPTL4 is an example of the use of MQPUT using distribution  */
 /*   lists.                                                         */
 /*   AMQSPTL4 is based upon AMQSPUT4.                               */
 /*                                                                  */
 /*      -- messages are sent to the queues named by the parameters  */
 /*                                                                  */
 /*      -- gets lines from StdIn, and adds each to each target      */
 /*         queue, taking each line of text as the content           */
 /*         of a datagram message; the sample stops when a null      */
 /*         line (or EOF) is read.                                   */
 /*         New-line characters are removed.                         */
 /*         If a line is longer than 99 characters it is broken up   */
 /*         into 99-character pieces. Each piece becomes the         */
 /*         content of a datagram message.                           */
 /*         If the length of a line is a multiple of 99 plus 1       */
 /*         e.g. 199, the last piece will only contain a new-line    */
 /*         character so will terminate the input.                   */
 /*                                                                  */
 /*      -- writes a message for each MQI reason other than          */
 /*         MQRC_NONE; stops if there is a MQI completion code       */
 /*         of MQCC_FAILED                                           */
 /*                                                                  */
 /*    Program logic:                                                */
 /*         MQOPEN target queues for OUTPUT                          */
 /*         while end of input file not reached,                     */
 /*         .  read next line of text                                */
 /*         .  MQPUT datagram message with text line as data         */
 /*         MQCLOSE target queues                                    */
 /*                                                                  */
 /*                                                                  */
 /********************************************************************/
 /*                                                                  */
 /*   AMQSPTL4 accepts an array of Queue/QueueMgr names as input.    */
 /*                                                                  */
 /*   AMQSPTL4 Queue1 QMgr1 .... QueueN QMgrN                        */
 /*                                                                  */
 /*                                                                  */
 /********************************************************************/
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
    /* includes for MQI */
 #include <cmqc.h>
static void print_usage(void);
static void print_responses( char * comment
                           , PMQRR  pRR
                           , MQLONG NumQueues
                           , PMQOR  pOR);

 int main(int argc, char **argv)
 {
   typedef enum {False, True} Bool;

   /*   Declare file and character for sample input                  */
   FILE *fp;

   /*   Declare MQI structures needed                                */
   MQOD     od = {MQOD_DEFAULT};    /* Object Descriptor             */
   MQMD     md = {MQMD_DEFAULT};    /* Message Descriptor            */
   MQPMO   pmo = {MQPMO_DEFAULT};   /* put message options           */
      /** note, sample uses defaults where it can **/

   MQHCONN  Hcon;                   /* connection handle             */
   MQHOBJ   Hobj;                   /* object handle                 */
   MQLONG   O_options;              /* MQOPEN options                */
   MQLONG   C_options;              /* MQCLOSE options               */
   MQLONG   CompCode;               /* completion code               */
   MQLONG   OpenCode;               /* MQOPEN completion code        */
   MQLONG   Reason;                 /* reason code                   */
   MQLONG   buflen;                 /* buffer length                 */
   char     buffer[100];            /* message buffer                */

   MQLONG   Index ;                 /* Index into list of queues     */
   MQLONG   NumQueues ;             /* Number of queues              */

   PMQRR    pRR=NULL;               /* Pointer to response records   */
   PMQOR    pOR=NULL;               /* Pointer to object records     */
   Bool     DisconnectRequired=False;/* Already connected switch     */
   Bool     Connected=False;        /* Connect succeeded switch     */

   /******************************************************************/
   /* The use of Put Message Records (PMR's) allows some message     */
   /* attributes to be specified on a per destination basis. These   */
   /* attributes then override the values in the MD for a particular */
   /* destination.                                                   */
   /* The function provided by the sample AMQSPTL0 does not require  */
   /* the use of PMR's but they are used by the sample simply to     */
   /* demonstrate their use.                                         */
   /* The sample chooses to provide values for MsgId and CorrelId    */
   /* on a per destination basis.                                    */
   /******************************************************************/
   typedef struct
   {
    MQBYTE24 MsgId;
    MQBYTE24 CorrelId;
   } PutMsgRec, *pPutMsgRec;
   pPutMsgRec pPMR=NULL;            /* Pointer to put msg records    */
   /******************************************************************/
   /* The PutMsgRecFields in the PMO indicates what fields are in    */
   /* the array addressed by PutMsgRecPtr in the PMO.                */
   /* In our example we have provided the MsgId and CorrelId and so  */
   /* we must set the corresponding MQPMRF_... bits.                 */
   /******************************************************************/
   MQLONG PutMsgRecFields=MQPMRF_MSG_ID | MQPMRF_CORREL_ID;


   printf("Sample AMQSPTL4 start\n");
   /******************************************************************/
   /*   We require at least one pair of names, and an even number of */
   /*   names.                                                       */
   /******************************************************************/
   if ((argc < 3)||!(argc%2))
   {
     print_usage();
     exit(99);
   }

   NumQueues = argc/2 ;     /* Number of Queue/QueueMgr name pairs   */
   /******************************************************************/
   /*   Allocate response records, object records and put message    */
   /*   records.                                                     */
   /******************************************************************/
   pRR  = (PMQRR)malloc( NumQueues * sizeof(MQRR));
   pOR  = (PMQOR)malloc( NumQueues * sizeof(MQOR));
   pPMR = (pPutMsgRec)malloc( NumQueues * sizeof(PutMsgRec));

   if((NULL == pRR) || (NULL == pOR) || (NULL == pPMR))
   {
     printf("%s(%d) malloc failed\n", __FILE__, __LINE__);
     exit(99);
   }

   /******************************************************************/
   /*                                                                */
   /*   Use parameters as the name of the target queues              */
   /*                                                                */
   /******************************************************************/
   for( Index = 0 ; Index < NumQueues ; Index ++)
   {
     strncpy( (pOR+Index)->ObjectName
            , argv[1+2*Index]
            , (size_t)MQ_Q_NAME_LENGTH);
     strncpy( (pOR+Index)->ObjectQMgrName
            , argv[2+2*Index]
            , (size_t)MQ_Q_MGR_NAME_LENGTH);
   }

   /******************************************************************/
   /*   Connect to queue manager                                     */
   /*                                                                */
   /*   Try to connect to the queue manager associated with the      */
   /*   first queue, if that fails then try each of the other        */
   /*   queue managers in turn.                                      */
   /*                                                                */
   /*  Note: It is not usual to use response records on MQCONN.      */
   /*        In this sample the use of response records facilitates  */
   /*        the error reporting mechanism. Similarily it is unusual */
   /*        to use object records at this time, but as we have      */
   /*        already copied the input arguments into this more       */
   /*        convienient form then it is easier to use the OR's      */
   /*        than the parameters addressed by argv.                  */
   /******************************************************************/
   for( Index = 0 ; Index < NumQueues ; Index ++)
   {
     MQCONN((pOR+Index)->ObjectQMgrName, /* queue manager            */
            &Hcon,                   /* connection handle            */
            &((pRR+Index)->CompCode),/* completion code              */
            &((pRR+Index)->Reason)); /* reason code                  */

     if ((pRR+Index)->CompCode == MQCC_FAILED)
     {
       continue;
     }
     if ((pRR+Index)->CompCode == MQCC_OK)
     {
       DisconnectRequired = True ;
     }
     Connected = True;
     break ;
   }

   /******************************************************************/
   /* Print any non zero responses                                   */
   /******************************************************************/
   print_responses("MQCONN", pRR, Index, pOR);

   /******************************************************************/
   /* If we failed to connect to any queue manager then exit.        */
   /******************************************************************/
   if( False == Connected  )
   {
     printf("unable to connect to any queue manager\n");
     exit(99) ;
   }


   /******************************************************************/
   /*                                                                */
   /*   Open the target message queue for output                     */
   /*                                                                */
   /******************************************************************/
   od.Version = MQOD_VERSION_2 ;
   od.RecsPresent = NumQueues ;      /* number of object/resp recs   */
   od.ObjectRecPtr = pOR;            /* address of object records    */
   od.ResponseRecPtr = pRR ;         /* Number of object records     */
   O_options = MQOO_OUTPUT           /* open queue for output        */
           + MQOO_FAIL_IF_QUIESCING; /* but not if MQM stopping      */
   MQOPEN(Hcon,                      /* connection handle            */
          &od,                       /* object descriptor for queue  */
          O_options,                 /* open options                 */
          &Hobj,                     /* object handle                */
          &OpenCode,                 /* MQOPEN completion code       */
          &Reason);                  /* reason code                  */

   /******************************************************************/
   /* report reason(s) if any; stop if failed.                       */
   /*                                                                */
   /* Note: The reasons in the response records are only valid if    */
   /*       the MQI Reason is MQRC_MULTIPLE_REASONS. If any other    */
   /*       reason is reported then all destinations in the list     */
   /*       completed/failed with the same reason.                   */
   /*       If the MQI CompCode is MQCC_FAILED then all of the       */
   /*       destinations in the list failed to open. If some         */
   /*       destinations opened and others failed to open then       */
   /*       the response will be set to MQCC_WARNING.                */
   /*                                                                */
   /******************************************************************/
   if (Reason == MQRC_MULTIPLE_REASONS)
   {
     print_responses("MQOPEN", pRR, NumQueues, pOR);
   }
   else
   {
     if (Reason != MQRC_NONE)
     {
       printf("MQOPEN  returned CompCode=%ld, Reason=%ld\n"
             , OpenCode
             , Reason);
     }
   }

   if (OpenCode == MQCC_FAILED)
   {
     printf("unable to open any queue for output\n");
   }

   /******************************************************************/
   /*                                                                */
   /*   Read lines from the file and put them to the message queue   */
   /*   Loop until null line or end of file, or there is a failure   */
   /*                                                                */
   /******************************************************************/
   CompCode = OpenCode;        /* use MQOPEN result for initial test */
   fp = stdin;

   pmo.Version = MQPMO_VERSION_2 ;
   pmo.RecsPresent = NumQueues ;
   pmo.PutMsgRecPtr = pPMR ;
   pmo.PutMsgRecFields = PutMsgRecFields ;
   pmo.ResponseRecPtr = pRR ;
   while (CompCode != MQCC_FAILED)
   {
     if (fgets(buffer, sizeof(buffer), fp) != NULL)
     {
       buflen = strlen(buffer);       /* length without null         */
       if (buffer[buflen-1] == '\n')  /* last char is a new-line     */
       {
         buffer[buflen-1]  = '\0';    /* replace new-line with null  */
         --buflen;                    /* reduce buffer length        */
       }
     }
     else buflen = 0;        /* treat EOF same as null line          */

     /****************************************************************/
     /*                                                              */
     /*   Put each buffer to the message queue                       */
     /*                                                              */
     /****************************************************************/
     if (buflen > 0)
     {
       for( Index = 0 ; Index < NumQueues ; Index ++)
       {
         memcpy( (pPMR+Index)->MsgId
               , MQMI_NONE
               , sizeof((pPMR+Index)->MsgId));
         memcpy( (pPMR+Index)->CorrelId
               , MQCI_NONE
               , sizeof((pPMR+Index)->CorrelId));
       }
       memcpy(md.Format,          /* character string format         */
              MQFMT_STRING, (size_t)MQ_FORMAT_LENGTH);
       MQPUT(Hcon,                /* connection handle               */
             Hobj,                /* object handle                   */
             &md,                 /* message descriptor              */
             &pmo,                /* default options (datagram)      */
             buflen,              /* buffer length                   */
             buffer,              /* message buffer                  */
             &CompCode,           /* completion code                 */
             &Reason);            /* reason code                     */
   /******************************************************************/
   /* report reason(s) if any; stop if failed.                       */
   /*                                                                */
   /* Note: The reasons in the response records are only valid if    */
   /*       the MQI Reason is MQRC_MULTIPLE_REASONS. If any other    */
   /*       reason is reported then all destinations in the list     */
   /*       completed/failed with the same reason.                   */
   /*       If the MQI CompCode is MQCC_FAILED then the MQPUT failed */
   /*       for all of the destinations in the list. If the MQPUT    */
   /*       Rsuccedded for some destinations and failed for others   */
   /*       then the response will be set to MQCC_WARNING.           */
   /*                                                                */
   /******************************************************************/
       if (Reason == MQRC_MULTIPLE_REASONS)
       {
         print_responses("MQPUT", pRR, NumQueues, pOR);
       }
       else
       {
         if (Reason != MQRC_NONE)
         {
           printf("MQPUT  returned CompCode=%ld, Reason=%ld\n"
                 , OpenCode
                 , Reason);
         }
       }
     }
     else   /* satisfy end condition when empty line is read */
       CompCode = MQCC_FAILED;
   }

   /******************************************************************/
   /*                                                                */
   /*   Close the target queue (if it was opened)                    */
   /*                                                                */
   /******************************************************************/
   if (OpenCode != MQCC_FAILED)
   {
     C_options = 0;                  /* no close options             */
     MQCLOSE(Hcon,                   /* connection handle            */
             &Hobj,                   /* object handle               */
             C_options,
             &CompCode,               /* completion code             */
             &Reason);                /* reason code                 */

     /* report reason, if any     */
     if (Reason != MQRC_NONE)
     {
       printf("MQCLOSE ended with reason code %ld\n", Reason);
     }
   }

   /******************************************************************/
   /*                                                                */
   /*   Disconnect from MQM if not already connected                 */
   /*                                                                */
   /******************************************************************/
   if (DisconnectRequired==True)
   {
     MQDISC(&Hcon,                   /* connection handle            */
            &CompCode,               /* completion code              */
            &Reason);                /* reason code                  */

     /* report reason, if any     */
     if (Reason != MQRC_NONE)
     {
       printf("MQDISC ended with reason code %ld\n", Reason);
     }
   }

   /******************************************************************/
   /*                                                                */
   /* END OF AMQSPTL4                                                */
   /*                                                                */
   /******************************************************************/
   if( NULL != pOR )
   {
     free( pOR ) ;
   }
   if( NULL != pRR )
   {
     free( pRR ) ;
   }
   if( NULL != pPMR )
   {
     free( pPMR ) ;
   }
   printf("Sample AMQSPTL4 end\n");
   return(0);
 }




/********************************************************************/
/*                                                                  */
/* print_usage:                                                     */
/*                                                                  */
/* Function: Display the correct signature for AMQSPTL4             */
/*                                                                  */
/* Notes:    This function is called when it is detected that the   */
/*           input parameters to AMQSPTL4 are not in the correct    */
/*           format.                                                */
/*                                                                  */
/********************************************************************/
static void print_usage(void)
{
 printf("AMQSPTL4 correct usage is:\n\n");
 printf("CALL PGM(AMQSPTL4) PARM(Queue1 QMgr1 ... QueueN QMgrN)\n\n");
}



/********************************************************************/
/*                                                                  */
/* print_responses                                                  */
/*                                                                  */
/* Function: Print MQI responses from the ResponseRecord array.     */
/*                                                                  */
/* Notes:    This function is typically called when a reason of     */
/*           MQRC_MULTIPLE_REASONS is received.                     */
/*           The reasons relate to the queue at the equivalent      */
/*           ordinal position in the MQOR array.                    */
/********************************************************************/
static void print_responses( char * comment
                    , PMQRR  pRR
                    , MQLONG NumQueues
                    , PMQOR  pOR)
{
  MQLONG Index;
  for( Index = 0 ; Index < NumQueues ; Index ++ )
  {
    if( MQCC_OK != (pRR+Index)->CompCode )
    {
      printf("%s for %.48s( %.48s) returned CompCode=%ld, Reason=%ld\n"
            , comment
            , (pOR+Index)->ObjectName
            , (pOR+Index)->ObjectQMgrName
            , (pRR+Index)->CompCode
            , (pRR+Index)->Reason);
    }
  }
}
