/* %Z% %W% %I% %E% %U% */
 /********************************************************************/
 /*                                                                  */
 /* Program name: AMQSCBF0                                           */
 /*                                                                  */
 /* Description: Sample C program that gets messages from            */
 /*              message queues using MQCB                           */
 /*   <copyright                                                     */
 /*   notice="lm-source-program"                                     */
 /*   pids="5724-H72"                                                */
 /*   years="2007,2016"                                              */
 /*   crc="706078058" >                                              */
 /*   Licensed Materials - Property of IBM                           */
 /*                                                                  */
 /*   5724-H72                                                       */
 /*                                                                  */
 /*   (C) Copyright IBM Corp. 2007, 2016 All Rights Reserved.        */
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
 /*   AMQSCBF0 is a sample C program to get messages from            */
 /*   message queues, and is an example of MQCB.                     */
 /*                                                                  */
 /*      -- sample reads from message queues named in the parameters */
 /*                                                                  */
 /*      -- displays the contents of the message queues,             */
 /*         assuming each message data to represent a line of        */
 /*         text to be written                                       */
 /*                                                                  */
 /*         messages are removed from the queue                      */
 /*                                                                  */
 /*      -- writes a message for each MQI reason other than          */
 /*         MQRC_NONE; stops if there is a MQI completion code       */
 /*         of MQCC_FAILED                                           */
 /*                                                                  */
 /*                                                                  */
 /*   Program logic:                                                 */
 /*      Take name of input queues from the parameter list           */
 /*      MQOPEN queues for INPUT                                     */
 /*      MQCB   register a callback function to receive messages     */
 /*      MQCTL  start consumption of messages                        */
 /*      wait for use to press enter                                 */
 /*      MQCTL  stop consumptions of messages                        */
 /*      MQCLOSE the subject queues                                  */
 /*      MQDISC  disconnect from queue manager                       */
 /*                                                                  */
 /*                                                                  */
 /********************************************************************/
 /*                                                                  */
 /*   AMQSCBF0 has the following parameters                          */
 /*       required:                                                  */
 /*                 (1) The name of the source queue or queues       */
 /*       optional:                                                  */
 /*                 (2) -m <name>     Queue manager name             */
 /*                 (3) -o <options>  The open options               */
 /*                 (4) -r <type>     The reconnect type             */
 /*                 (5) -c <options>  The close options              */
 /*                                                                  */
 /********************************************************************/
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <ctype.h>
                                                /* includes for MQI  */
 #include <cmqc.h>
                                                /* Constants         */
 #define MAX_QUEUES  10
                                                /* Statics           */
 static int Parm_Index = 1;
                                                /* Prototypes        */

 int getparm(int       argc,
             char   ** argv,
             char   ** pFlag,
             char   ** pParm);

 void Usage();

 /********************************************************************/
 /* FUNCTION: MessageConsumer                                        */
 /* PURPOSE : Callback function called when messages arrive          */
 /********************************************************************/
 void MessageConsumer(MQHCONN   hConn,
                      MQMD    * pMsgDesc,
                      MQGMO   * pGetMsgOpts,
                      MQBYTE  * Buffer,
                      MQCBC   * pContext)
 {
   MQLONG i,max;
   MQLONG Length;

   switch(pContext->CallType)
   {
     case MQCBCT_MSG_REMOVED:
     case MQCBCT_MSG_NOT_REMOVED:
          Length = pGetMsgOpts -> ReturnedLength;
          if (pContext->Reason)
            printf("Message Call (%d Bytes) : Reason = %d\n",
                   Length,pContext->Reason);
          else
            printf("Message Call (%d Bytes) :\n",
                   Length);

          /***********************************************************/
          /* Print out only the first printable bytes                */
          /***********************************************************/
          max = Length;
          if (max > 200) max = 200;
          for (i=0; i<max; i++)
          {
            if (isprint(Buffer[i])) fputc(Buffer[i],stdout);
                               else fputc('.',stdout);
          }
          fputc('\n',stdout);
          if (max < Length)
            printf("......plus %d bytes.\n",Length-max);
          break;

     case MQCBCT_EVENT_CALL:
          printf("Event Call : Reason = %d\n",pContext->Reason);
          break;

   default:
          printf("Calltype = %d\n",pContext->CallType);
          break;
   }
 }

 /********************************************************************/
 /* FUNCTION: main                                                   */
 /* PURPOSE : Main program entry point                               */
 /********************************************************************/
 int main(int argc, char **argv)
 {

   /*   Declare MQI structures needed                                */
   MQCNO   cno = {MQCNO_DEFAULT};   /* Connect Options               */
   MQOD     od = {MQOD_DEFAULT};    /* Object Descriptor             */
   MQMD     md = {MQMD_DEFAULT};    /* Message Descriptor            */
   MQGMO   gmo = {MQGMO_DEFAULT};   /* get message options           */
   MQCBD   cbd = {MQCBD_DEFAULT};   /* Callback Descriptor           */
   MQCTLO  ctlo= {MQCTLO_DEFAULT};  /* Control Options               */
      /** note, sample uses defaults where it can **/

   MQHCONN  Hcon = MQHC_UNUSABLE_HCONN;       /* connection handle   */
   MQLONG   O_options;              /* MQOPEN options                */
   MQLONG   C_options = MQCO_NONE;  /* MQCLOSE options               */
   MQLONG   CompCode;               /* completion code               */
   MQLONG   OpenCode;               /* MQOPEN completion code        */
   MQLONG   Reason = 999;           /* reason code                   */
   MQLONG   CReason;                /* reason code for MQCONN        */
   char     QMName[50] = "";        /* queue manager name            */
   struct  {                        /* queue array                   */
     MQHOBJ   Hobj;
     MQCHAR48 ObjectName;
   } Queues[MAX_QUEUES];
   int      QCount = 0;
   int      QIndex;
   char   * pFlag,* pParm;

   printf("Sample AMQSCBF0 start\n\n");

   O_options = MQOO_INPUT_AS_Q_DEF      /* open queue for input      */
             | MQOO_FAIL_IF_QUIESCING;  /* but not if MQM stopping   */

   /******************************************************************/
   /* Parse the parameters                                           */
   /******************************************************************/
   while (getparm(argc,argv,&pFlag,&pParm))
   {
     if (pFlag && *pFlag)
     {
                                       /* What have we been passed ? */
       if (!pParm)
       {
         Usage();
         goto MOD_EXIT;
       }
                                       /* All flags must have parm   */
       switch(*pFlag)
       {
                                       /* Queue Manager Name         */
         case 'm':
              strncpy(QMName, pParm, MQ_Q_MGR_NAME_LENGTH);
              break;
                                       /* Open options               */
         case 'o':
              if (!pParm)
              {
                Usage();
                goto MOD_EXIT;
              }
              O_options = atoi( pParm );
              break;
                                       /* Reconnect Options          */
         case 'r':
              switch (*pParm)
              {
                case 'd':
                     cno.Options |= MQCNO_RECONNECT_DISABLED;
                     break;

                case 'm':
                     cno.Options |= MQCNO_RECONNECT_Q_MGR;
                     break;

                case 'r':
                     cno.Options |= MQCNO_RECONNECT;
                     break;

                default:
                     Usage();
                     goto MOD_EXIT;
                     break;
              }
              break;
                                       /* Close options              */
         case 'c':
              if (!pParm)
              {
                Usage();
                goto MOD_EXIT;
              }
              C_options = atoi( pParm );
              break;

         default:
              Usage();
              goto MOD_EXIT;
       }
     }
     else
     {
       /**************************************************************/
       /* No flag given.....must be a queue name                     */
       /**************************************************************/
       if (QCount == MAX_QUEUES)
       {
         printf("Sorry, only a maximum of %d queues is supported\n",
                MAX_QUEUES);
         goto MOD_EXIT;
       }

       if (!pParm)

       {
         Usage();
         goto MOD_EXIT;
       }
       strncpy((char *)&Queues[QCount].ObjectName, pParm, MQ_Q_NAME_LENGTH);
       Queues[QCount].Hobj = MQHO_UNUSABLE_HOBJ;
       QCount++;
     }
   }
   /******************************************************************/
   /* Must get at least one queue name                               */
   /******************************************************************/
   if (QCount == 0)
   {
     Usage();
     goto MOD_EXIT;
   }
   /******************************************************************/
   /*                                                                */
   /*   Connect to queue manager                                     */
   /*                                                                */
   /******************************************************************/
   if(cno.Options == MQCNO_NONE)
   {
     MQCONN(QMName,                  /* queue manager                */
            &Hcon,                   /* connection handle            */
            &CompCode,               /* completion code              */
            &CReason);               /* reason code                  */
   }
   else
   {
     MQCONNX(QMName,                 /* queue manager                */
             &cno,                   /* connect options              */
             &Hcon,                  /* connection handle            */
             &CompCode,              /* completion code              */
             &CReason);              /* reason code                  */
   }
                          /* report reason and stop if it failed     */
   if (CompCode == MQCC_FAILED)
   {
     printf("MQCONN ended with reason code %d\n", CReason);
     exit( (int)CReason );
   }
   /******************************************************************/
   /*                                                                */
   /*   Loop round and open and register the consumers               */
   /*                                                                */
   /******************************************************************/
   QIndex = QCount;
   while (QIndex--)
   {
     strcpy(od.ObjectName, (char *)&Queues[QIndex].ObjectName);
     /****************************************************************/
     /*                                                              */
     /*   Open the queue                                             */
     /*                                                              */
     /****************************************************************/
     MQOPEN(Hcon,                    /* connection handle            */
            &od,                     /* object descriptor for queue  */
            O_options,               /* open options                 */
            &Queues[QIndex].Hobj,    /* object handle                */
            &OpenCode,               /* completion code              */
            &Reason);                /* reason code                  */

     if (OpenCode == MQCC_FAILED)
     {
       printf("MQOPEN of '%.48s' ended with reason code %d\n",
              &Queues[QIndex].ObjectName,Reason);
       goto MOD_EXIT;
     }
     /****************************************************************/
     /*                                                              */
     /*   Register a consumer                                        */
     /*                                                              */
     /****************************************************************/
     cbd.CallbackFunction = MessageConsumer;
     gmo.Options = MQGMO_NO_SYNCPOINT;

     MQCB(Hcon,
          MQOP_REGISTER,
          &cbd,
          Queues[QIndex].Hobj,
          &md,
          &gmo,
          &CompCode,
          &Reason);
     if (CompCode == MQCC_FAILED)
     {
       printf("MQCB ended with reason code %d\n", Reason);
       goto MOD_EXIT;
     }
   }
   /******************************************************************/
   /*                                                                */
   /*  Start consumption of messages                                 */
   /*                                                                */
   /******************************************************************/
   MQCTL(Hcon,
         MQOP_START,
         &ctlo,
         &CompCode,
         &Reason);
   if (CompCode == MQCC_FAILED)
   {
     printf("MQCTL ended with reason code %d\n", Reason);
     goto MOD_EXIT;
   }
   /******************************************************************/
   /*                                                                */
   /*  Wait for the user to press enter                              */
   /*                                                                */
   /******************************************************************/
   {
     char Buffer[10];
     printf("Press enter to end\n");
     fgets(Buffer,sizeof(Buffer),stdin);
   }
   /******************************************************************/
   /*                                                                */
   /*  Stop consumption of messages                                  */
   /*                                                                */
   /******************************************************************/
   MQCTL(Hcon,
         MQOP_STOP,
         &ctlo,
         &CompCode,
         &Reason);
   if (CompCode == MQCC_FAILED)
   {
     printf("MQCTL ended with reason code %d\n", Reason);
     goto MOD_EXIT;
   }

MOD_EXIT:
   /******************************************************************/
   /*                                                                */
   /*   Close the source queues (if any were opened)                 */
   /*                                                                */
   /******************************************************************/
   QIndex = QCount;
   while(QIndex--)
   {
     if (Queues[QIndex].Hobj != MQHO_UNUSABLE_HOBJ)
     {
       MQCLOSE(Hcon,                     /* connection handle           */
               &Queues[QIndex].Hobj,     /* object handle               */
               C_options,                /* close options               */
               &CompCode,                /* completion code             */
               &Reason);                 /* reason code                 */

       /* report reason, if any     */
       if (Reason != MQRC_NONE)
       {
         printf("MQCLOSE ended with reason code %d\n", Reason);
       }
     }
   }
   /******************************************************************/
   /*                                                                */
   /*   Disconnect from MQM if not already connected                 */
   /*                                                                */
   /******************************************************************/
   if (Hcon != MQHC_UNUSABLE_HCONN)
   {
     if (CReason != MQRC_ALREADY_CONNECTED )
     {
       MQDISC(&Hcon,                   /* connection handle          */
              &CompCode,               /* completion code            */
              &Reason);                /* reason code                */

       /* report reason, if any     */
       if (Reason != MQRC_NONE)
       {
         printf("MQDISC ended with reason code %d\n", Reason);
       }
     }
   }
   /******************************************************************/
   /*                                                                */
   /* END OF AMQSCBF0                                                */
   /*                                                                */
   /******************************************************************/
   printf("\nSample AMQSCBF0 end\n");
   return((int)Reason);
 }

 /********************************************************************/
 /* FUNCTION: Usage                                                  */
 /* PURPOSE : Print out the usage for the program                    */
 /********************************************************************/
 void Usage()
 {
   printf("Usage: [Options] <Queue Name> { <Queue Name> }\n");
   printf("  where Options are:\n");
   printf("    -m <Queue Manager Name>\n");
   printf("    -o <Open options>\n");
   printf("    -r <Reconnect Type>\n");
   printf("       d Reconnect Disabled\n");
   printf("       r Reconnect\n");
   printf("       m Reconnect Queue Manager\n");
   printf("    -c <Close options>\n");

 }
 /********************************************************************/
 /* FUNCTION: getparm                                                */
 /* PURPOSE : Return parameters from the command line                */
 /********************************************************************/
 int getparm(int       argc,
             char   ** argv,
             char   ** pFlag,
             char   ** pParm)
 {
   int    found = 0;
   char * p     = NULL;
   *pFlag = *pParm = NULL;
   if (Parm_Index >= argc) goto MOD_EXIT;
   found = 1;
   p = argv[Parm_Index++];
   if (*p == '-')
   {
     *pFlag = ++p;                    /* This is a flagged parm      */
     if (!**pFlag) goto MOD_EXIT;     /* No actual flag specified    */
     p++;                             /* Advance to actual parameter */
     if (!*p)                         /* Is it there ?               */
     {
       if (Parm_Index >= argc) goto MOD_EXIT;
       if (*argv[Parm_Index] != '-') *pParm = argv[Parm_Index++];
     }
     else *pParm = p;
   }
   else *pParm = p;
 MOD_EXIT:
   return found;
 }
