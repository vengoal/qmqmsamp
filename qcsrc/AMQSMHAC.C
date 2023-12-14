static const char sccsid[] = "%Z% %W% %I% %E% %U%";
 /********************************************************************/
 /*                                                                  */
 /* Program name: AMQSMHAC                                           */
 /*                                                                  */
 /* Description: Sample C program that transfers messages from       */
 /*              one queue to another using a reconnectable          */
 /*              connection.                                         */
 /*   <copyright                                                     */
 /*   notice="lm-source-program"                                     */
 /*   pids="5724-H72,"                                               */
 /*   years="2007,2012"                                              */
 /*   crc="3039121396" >                                             */
 /*   Licensed Materials - Property of IBM                           */
 /*                                                                  */
 /*   5724-H72,                                                      */
 /*                                                                  */
 /*   (C) Copyright IBM Corp. 2007, 2012 All Rights Reserved.        */
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
 /*   AMQSMHAC is a sample C program to get messages from one        */
 /*   message queue and put them to another. It demonstrates         */
 /*   reliable message transfer over a reconnectable connection.     */
 /*                                                                  */
 /*   Program logic:                                                 */
 /*      Parse input parameters                                      */
 /*      MQCONN to connect to the Queue Manager                      */
 /*      MQOPEN queue for INPUT                                      */
 /*      MQOPEN queue for OUTPUT                                     */
 /*      <loop transferring messages>                                */
 /*      MQDISC  disconnect from queue manager                       */
 /*                                                                  */
 /********************************************************************/
 /*                                                                  */
 /*   AMQSMHAC has the following parameters                          */
 /*       required:                                                  */
 /*                 -s  The name of the source queue                 */
 /*                 -t  The name of the target queue                 */
 /*       optional:                                                  */
 /*                 -m  Queue manager name                           */
 /*                 -w  Wait Interval                                */
 /*                                                                  */
 /********************************************************************/
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <time.h>
 #include <ctype.h>
                                                /* includes for MQI  */
 #include <cmqc.h>
                                                /* Statics           */
 static int Parm_Index = 1;
                                                /* Prototypes        */
 int getparm(int       argc,
             char   ** argv,
             char   ** pFlag,
             char   ** pParm);

 void Usage();

 /********************************************************************/
 /* FUNCTION: EventHandler                                           */
 /* PURPOSE : Callback function called when an event happens         */
 /********************************************************************/
 MQLONG EventHandler   (MQHCONN   hConn,
                        MQMD    * pMsgDesc,
                        MQGMO   * pGetMsgOpts,
                        MQBYTE  * Buffer,
                        MQCBC   * pContext)
 {
   time_t Now;
   char   TimeStamp[20];
   char * pTime;

   time(&Now);
   pTime = ctime(&Now);

   sprintf(TimeStamp,"%8.8s : ",pTime+11);

   switch(pContext->Reason)
   {
     case MQRC_CONNECTION_BROKEN:
          printf("%sEVENT : Connection Broken\n",TimeStamp);
          break;

     case MQRC_RECONNECTING:
          printf("%sEVENT : Connection Reconnecting (Delay: %dms)\n",TimeStamp,
                 pContext->ReconnectDelay);
          break;

     case MQRC_RECONNECTED:
          printf("%sEVENT : Connection Reconnected\n",TimeStamp);
          break;

     case MQRC_RECONNECT_FAILED:
          printf("%sEVENT : Reconnection failed\n",TimeStamp);
          break;

   default:
          printf("%sEVENT : Reason(%d)\n",TimeStamp,pContext->Reason);
          break;
   }
   return 0;
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
   MQPMO   pmo = {MQPMO_DEFAULT};   /* put message options           */
   MQCBD   cbd = {MQCBD_DEFAULT};   /* Callback Descriptor           */
      /** note, sample uses defaults where it can **/

   MQHCONN  hQm     = MQHC_UNUSABLE_HCONN;    /* connection handle   */
   MQHOBJ   hSource = MQHO_UNUSABLE_HOBJ;     /* object handle       */
   MQHOBJ   hTarget = MQHO_UNUSABLE_HOBJ;     /* object handle       */
   MQLONG   CompCode;               /* completion code               */
   MQLONG   Reason = 999;           /* reason code                   */
   MQLONG   CReason;                /* reason code for MQCONN        */
   char     QMName[50] = "";        /* queue manager name            */
   char     Source[50] = "";        /* source queue                  */
   char     Target[50] = "";        /* target queue                  */
   char   * pFlag,* pParm;
   char   * pMsg         = NULL;
   MQLONG   MsgSize      = 0;
   MQLONG   MsgLen       = 4096;
   MQLONG   WaitInterval = 15 * 60 * 1000;

   printf("Sample AMQSMHAC start\n\n");

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
                                       /* Source Queue               */
         case 's':
              strncpy(Source, pParm, MQ_Q_NAME_LENGTH);
              break;
                                       /* Target Queue               */
         case 't':
              strncpy(Target, pParm, MQ_Q_NAME_LENGTH);
              break;
                                       /* Wait Interval              */
         case 'w':
              WaitInterval = atoi(pParm) * 1000;
              break;

         default:
              Usage();
              goto MOD_EXIT;
       }
     }
     else
     {
       /**************************************************************/
       /* Nothing else expected                                      */
       /**************************************************************/
       Usage();
       goto MOD_EXIT;
     }
   }
   /******************************************************************/
   /* Check we have a source queue name                              */
   /******************************************************************/
   if (!Source[0])
   {
     printf("Please enter a source queue name\n");
     goto MOD_EXIT;
   }

   /******************************************************************/
   /* Check we have a target queue name                              */
   /******************************************************************/
   if (!Target[0])
   {
     printf("Please enter a target queue name\n");
     goto MOD_EXIT;
   }

   /******************************************************************/
   /*                                                                */
   /*   Connect to queue manager                                     */
   /*                                                                */
   /******************************************************************/
   cno.Options = MQCNO_RECONNECT_Q_MGR;

   MQCONNX(QMName,                 /* queue manager                  */
           &cno,                   /* connect options                */
           &hQm,                   /* connection handle              */
           &CompCode,              /* completion code                */
           &CReason);              /* reason code                    */

   if (CompCode == MQCC_FAILED)
   {
     printf("MQCONN ended with reason code %d\n", CReason);
     Reason = CReason;
     goto MOD_EXIT;
   }

   /******************************************************************/
   /*                                                                */
   /*   Register an Event Handler                                    */
   /*                                                                */
   /******************************************************************/
   cbd.CallbackType     = MQCBT_EVENT_HANDLER;
   cbd.CallbackFunction = EventHandler;

   MQCB(hQm,
        MQOP_REGISTER,
        &cbd,
        MQHO_UNUSABLE_HOBJ,
        NULL,
        NULL,
        &CompCode,
        &Reason);
   if (CompCode == MQCC_FAILED)
   {
     printf("MQCB ended with reason code %d\n", Reason);
     goto MOD_EXIT;
   }

   /******************************************************************/
   /*                                                                */
   /*   Open the source queue                                        */
   /*                                                                */
   /******************************************************************/
   strncpy(od.ObjectName, Source, MQ_Q_NAME_LENGTH);
   MQOPEN(hQm,                       /* connection handle            */
          &od,                       /* object descriptor for queue  */
          MQOO_INPUT_SHARED |        /* open options                 */
          MQOO_FAIL_IF_QUIESCING,
          &hSource,                  /* object handle                */
          &CompCode,                 /* completion code              */
          &Reason);                  /* reason code                  */

   if (CompCode == MQCC_FAILED)
   {
     printf("MQOPEN of '%.48s' ended with reason code %d\n",
            Source,Reason);
     goto MOD_EXIT;
   }

   /******************************************************************/
   /*                                                                */
   /*   Open the target queue                                        */
   /*                                                                */
   /******************************************************************/
   strncpy(od.ObjectName, Target, MQ_Q_NAME_LENGTH);
   MQOPEN(hQm,                       /* connection handle            */
          &od,                       /* object descriptor for queue  */
          MQOO_OUTPUT       |        /* open options                 */
          MQOO_FAIL_IF_QUIESCING,
          &hTarget,                  /* object handle                */
          &CompCode,                 /* completion code              */
          &Reason);                  /* reason code                  */

   if (CompCode == MQCC_FAILED)
   {
     printf("MQOPEN of '%.48s' ended with reason code %d\n",
            Source,Reason);
     goto MOD_EXIT;
   }
   /******************************************************************/
   /*   Get message loop                                             */
   /******************************************************************/
   while (1)
   {
     /****************************************************************/
     /* Ensure we have a big enough buffer                           */
     /****************************************************************/
     if (MsgLen > MsgSize)
     {
       if (pMsg) free(pMsg);

       pMsg = malloc(MsgLen);
       if (!pMsg)
       {
         printf("Can not malloc() %d bytes\n",MsgLen);
         goto MOD_EXIT;
       }
       MsgSize = MsgLen;
     }

     /****************************************************************/
     /* Get any message                                              */
     /****************************************************************/
     memcpy(md.MsgId, MQMI_NONE, sizeof(md.MsgId));
     memcpy(md.CorrelId, MQCI_NONE, sizeof(md.CorrelId));

     md.Encoding       = MQENC_NATIVE;
     md.CodedCharSetId = MQCCSI_Q_MGR;

     gmo.Options = MQGMO_SYNCPOINT         |
                   MQGMO_FAIL_IF_QUIESCING |
                   MQGMO_WAIT;

     gmo.WaitInterval = WaitInterval;

     MQGET(hQm,                 /* connection handle                 */
           hSource,             /* object handle                     */
           &md,                 /* message descriptor                */
           &gmo,                /* get message options               */
           MsgSize,             /* buffer length                     */
           pMsg,                /* message buffer                    */
           &MsgLen,             /* message length                    */
           &CompCode,           /* completion code                   */
           &Reason);            /* reason code                       */

     switch(Reason)
     {

       case 0:
            pmo.Options = MQPMO_SYNCPOINT |
                          MQPMO_FAIL_IF_QUIESCING;

            MQPUT(hQm,          /* connection handle                 */
                  hTarget,      /* object handle                     */
                  &md,          /* message descriptor                */
                  &pmo,         /* default options (datagram)        */
                  MsgLen,       /* message length                    */
                  pMsg,         /* message buffer                    */
                  &CompCode,    /* completion code                   */
                  &Reason);     /* reason code                       */

            switch(Reason)
            {
              case 0:
                   MQCMIT(hQm,
                          &CompCode,
                          &Reason);
                   switch(Reason)
                   {
                     case 0:
                                       /* All is well                */
                          break;
                                /* Not a problem, go round again     */
                     case MQRC_BACKED_OUT:
                          printf("MQCMIT ended with reason code %d (MQRC_BACKED_OUT)\n",
                                 Reason);
                          break;

                     case MQRC_CALL_INTERRUPTED:
                          printf("MQCMIT ended with reason code %d (MQRC_CALL_INTERRUPTED)\n",
                                 Reason);
                          break;
                     default:
                          printf("MQCMIT ended with reason code %d\n", Reason);
                          goto MOD_EXIT;
                          break;
                   }

                   break;
                                /* Transaction has backed out        */
              case MQRC_BACKED_OUT:
                   printf("MQPUT ended with reason code %d (MQRC_BACKED_OUT)\n",
                          Reason);
                   MQBACK(hQm,
                          &CompCode,
                          &Reason);
                   if (Reason != MQRC_NONE)
                   {
                     printf("MQBACK ended with reason code %d\n", Reason);
                     goto MOD_EXIT;
                   }
                   break;

              default:
                   printf("MQPUT ended with reason code %d\n", Reason);
                   goto MOD_EXIT;
                   break;

            }
            break;
                                /* Message was truncated             */
       case MQRC_TRUNCATED_MSG_FAILED:
            break;
                                /* Transaction has backed out        */
       case MQRC_BACKED_OUT:
            printf("MQGET ended with reason code %d (MQRC_BACKED_OUT)\n",
                   Reason);
            MQBACK(hQm,
                   &CompCode,
                   &Reason);
            if (Reason != MQRC_NONE)
            {
              printf("MQBACK ended with reason code %d\n", Reason);
              goto MOD_EXIT;
            }
            break;

       case MQRC_NO_MSG_AVAILABLE:
            printf("No more messages.\n");
            goto MOD_EXIT;
            break;

       default:
            printf("MQGET ended with reason code %d\n", Reason);
            goto MOD_EXIT;
            break;
     }
   }


MOD_EXIT:
   /******************************************************************/
   /*                                                                */
   /*   Disconnect from MQ if not already connected                  */
   /*                                                                */
   /******************************************************************/
   if (hQm != MQHC_UNUSABLE_HCONN)
   {
                                       /* Private Reason code        */
     MQLONG Reason;
     /****************************************************************/
     /* Ensure we don't have a partially processed transaction       */
     /****************************************************************/
     MQBACK(hQm,
            &CompCode,
            &Reason);
     if (Reason != MQRC_NONE)
     {
       printf("MQBACK ended with reason code %d\n", Reason);
     }

     if (CReason != MQRC_ALREADY_CONNECTED )
     {
       MQDISC(&hQm,                    /* connection handle          */
              &CompCode,               /* completion code            */
              &Reason);                /* reason code                */

       if (Reason != MQRC_NONE)
       {
         printf("MQDISC ended with reason code %d\n", Reason);
       }
     }
   }
   /******************************************************************/
   /*                                                                */
   /* END OF AMQSMHAC                                                */
   /*                                                                */
   /******************************************************************/
   printf("\nSample AMQSMHAC end\n");
   return((int)Reason);
 }

 /********************************************************************/
 /* FUNCTION: Usage                                                  */
 /* PURPOSE : Print out the usage for the program                    */
 /********************************************************************/
 void Usage()
 {
   printf("Usage: [Options] }\n");
   printf("  where Options are:\n");
   printf("    -s <Source Queue>\n");
   printf("    -t <Target Queue>\n");
   printf("    -m <Queue Manager Name>\n");
   printf("    -w <Wait Interval (seconds)>\n");
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
