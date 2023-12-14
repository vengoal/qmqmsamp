const static char sccsid[] = "%Z% %W% %I% %E% %U%";
 /********************************************************************/
 /*                                                                  */
 /* Program name: AMQSPHAC                                           */
 /*                                                                  */
 /* Description: Sample C program that puts messages to              */
 /*              a message queue using a reconnectable connection.   */
 /*              It is a highly available client.                    */
 /*                                                                  */
 /*   <copyright                                                     */
 /*   notice="lm-source-program"                                     */
 /*   pids="5724-H72"                                                */
 /*   years="1994,2014"                                              */
 /*   crc="3211323105" >                                             */
 /*   Licensed Materials - Property of IBM                           */
 /*                                                                  */
 /*   5724-H72                                                       */
 /*                                                                  */
 /*   (C) Copyright IBM Corp. 1994, 2014 All Rights Reserved.        */
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
 /*   AMQSPHAC is a sample C program to put messages on a message    */
 /*   queue using a reconnectable connection. It is an example of    */
 /*   a program which is tolerant of temporary unavailabilty of      */
 /*   a queue manager.                                               */
 /*                                                                  */
 /*    Program logic:                                                */
 /*         MQCONNX queue manager                                    */
 /*         MQOPEN target queue for OUTPUT                           */
 /*         while no MQI failures,                                   */
 /*         .  MQPUT datagram message                                */
 /*         MQCLOSE target queue                                     */
 /*         MQDISC queue manager                                     */
 /*                                                                  */
 /*                                                                  */
 /********************************************************************/
 /*                                                                  */
 /*   AMQSPHAC has the following parameters                          */
 /*       required:                                                  */
 /*                 (1) The name of the target queue                 */
 /*       optional:                                                  */
 /*                 (2) Queue manager name                           */
 /*                                                                  */
 /********************************************************************/
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <time.h>
    /* includes for MQI */
 #include <cmqc.h>

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


 int main(int argc, char **argv)
 {
   /*   Declare MQI structures needed                                */
   MQCNO   cno = {MQCNO_DEFAULT};   /* connect options               */
   MQOD     od = {MQOD_DEFAULT};    /* Object Descriptor             */
   MQMD     md = {MQMD_DEFAULT};    /* Message Descriptor            */
   MQPMO   pmo = {MQPMO_DEFAULT};   /* put message options           */
   MQCBD   cbd = {MQCBD_DEFAULT};   /* Callback Descriptor           */
      /** note, sample uses defaults where it can **/

   MQHCONN  Hcon = MQHC_UNUSABLE_HCONN;         /* connection handle */
   MQHOBJ   Hobj;                   /* object handle                 */
   MQLONG   O_options;              /* MQOPEN options                */
   MQLONG   C_options;              /* MQCLOSE options               */
   MQLONG   CompCode;               /* completion code               */
   MQLONG   OpenCode;               /* MQOPEN completion code        */
   MQLONG   Reason;                 /* reason code                   */
   MQLONG   CReason;                /* reason code for MQCONN        */
   MQLONG   messlen;                /* message length                */
   char     buffer[50];             /* message buffer                */
   char     QMName[50];             /* queue manager name            */
   MQLONG   messnum = 0;            /* message number                */
   int      ExitRc  = 0;            /* program exit reason code      */

   printf("Sample AMQSPHAC start\n");
   if (argc < 2)
   {
     printf("Required parameter missing - queue name\n");
     ExitRc = 99;
     goto MOD_EXIT;
   }

   /******************************************************************/
   /*                                                                */
   /*   Connect to queue manager                                     */
   /*                                                                */
   /******************************************************************/
   QMName[0] = 0;    /* default */
   if (argc > 2)
     strncpy(QMName, argv[2], MQ_Q_MGR_NAME_LENGTH);

   cno.Options = MQCNO_RECONNECT;  /* reconnectable connection       */

   MQCONNX(QMName,                 /* queue manager                  */
           &cno,                   /* connect options                */
           &Hcon,                  /* connection handle              */
           &CompCode,              /* completion code                */
           &CReason);              /* reason code                    */

   /* report reason and stop if it failed     */
   if (CompCode == MQCC_FAILED)
   {
     printf("MQCONNX ended with reason code %d\n", CReason);
     ExitRc = (int)CReason;
     goto MOD_EXIT;
   }
   /******************************************************************/
   /*                                                                */
   /*   Register an Event Handler                                    */
   /*                                                                */
   /******************************************************************/
   cbd.CallbackType     = MQCBT_EVENT_HANDLER;
   cbd.CallbackFunction = EventHandler;

   MQCB(Hcon,
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
     ExitRc = (int)CReason;
     goto MOD_EXIT;
   }

   /******************************************************************/
   /*                                                                */
   /*   Use parameter as the name of the target queue                */
   /*                                                                */
   /******************************************************************/
   strncpy(od.ObjectName, argv[1], (size_t)MQ_Q_NAME_LENGTH);
   printf("target queue is %s\n", od.ObjectName);

   /******************************************************************/
   /*                                                                */
   /*   Open the target message queue for output                     */
   /*                                                                */
   /******************************************************************/
   O_options = MQOO_OUTPUT              /* open queue for output     */
             | MQOO_FAIL_IF_QUIESCING   /* but not if MQM stopping   */
             ;                          /* = 0x2010 = 8208 decimal   */

   MQOPEN(Hcon,                      /* connection handle            */
          &od,                       /* object descriptor for queue  */
          O_options,                 /* open options                 */
          &Hobj,                     /* object handle                */
          &OpenCode,                 /* MQOPEN completion code       */
          &Reason);                  /* reason code                  */

   /* report reason, if any; stop if failed      */
   if (Reason != MQRC_NONE)
   {
     printf("MQOPEN ended with reason code %d\n", Reason);
   }

   if (OpenCode == MQCC_FAILED)
   {
     printf("unable to open queue for output\n");
     ExitRc = (int) Reason;
     goto MOD_EXIT;
   }

   /******************************************************************/
   /*                                                                */
   /*   Loop putting messages to the message queue until there is    */
   /*   a failure.                                                   */
   /*                                                                */
   /******************************************************************/
   CompCode = OpenCode;        /* use MQOPEN result for initial test */

   memcpy(md.Format,           /* character string format            */
          MQFMT_STRING, (size_t)MQ_FORMAT_LENGTH);

                               /* Use non-persistent messages        */
   md.Persistence = MQPER_NOT_PERSISTENT;

   pmo.Options = MQPMO_NO_SYNCPOINT
               | MQPMO_NEW_MSG_ID
               | MQPMO_NEW_CORREL_ID
               | MQPMO_FAIL_IF_QUIESCING;

   while (CompCode != MQCC_FAILED)
   {
     sprintf(buffer, "Message %d", ++messnum);
     messlen = (MQLONG)strlen(buffer);

     /****************************************************************/
     /*                                                              */
     /*   Put a message to the message queue                         */
     /*                                                              */
     /****************************************************************/
     MQPUT(Hcon,                  /* connection handle               */
           Hobj,                  /* object handle                   */
           &md,                   /* message descriptor              */
           &pmo,                  /* default options (datagram)      */
           messlen,               /* message length                  */
           buffer,                /* message buffer                  */
           &CompCode,             /* completion code                 */
           &Reason);              /* reason code                     */

     /* report reason, if any */
     if (Reason == MQRC_NONE)
     {
       printf("message <%s>\n", buffer);
       msSleep(2000);
     }
     else
     {
       if (Reason == MQRC_CALL_INTERRUPTED)
       {
         /************************************************************/
         /* If we get MQRC_CALL_INTERRUPTED it means that we lost    */
         /* connection in the middle of the MQPUT call and we don't  */
         /* know for certain whether the message was put or not.     */
         /* The nature of the message will dictate what the          */
         /* application should do now; often all that is required is */
         /* to re-issue the MQPUT.  However, if the system cannot    */
         /* detect and handle the duplicate messages that could      */
         /* result with such a strategy, then it would be necessary  */
         /* for the application to find out whether the message was  */
         /* successfully put.  If the application cannot determine   */
         /* this, then probably the only option is to write out an   */
         /* exception.                                               */
         /*                                                          */
         /* In this simple example, we'll just re-issue the MQPUT    */
         /************************************************************/
         printf("MQPUT call interrupted, possible duplicate message %d\n",
                messnum);
         messnum--;
         CompCode = MQCC_OK;
       }
       else
       {
         printf("MQPUT ended with reason code %d\n", Reason);
       }
     }
   }

   /******************************************************************/
   /*                                                                */
   /*   Close the target queue (if it was opened)                    */
   /*                                                                */
   /******************************************************************/
   if (OpenCode != MQCC_FAILED)
   {
     C_options = MQCO_NONE;        /* no close options             */

     MQCLOSE(Hcon,                   /* connection handle            */
             &Hobj,                  /* object handle                */
             C_options,
             &CompCode,              /* completion code              */
             &Reason);               /* reason code                  */

     /* report reason, if any     */
     if (Reason != MQRC_NONE)
     {
       printf("MQCLOSE ended with reason code %d\n", Reason);
     }
   }

MOD_EXIT:
   /******************************************************************/
   /*                                                                */
   /*   Disconnect from MQM if not already connected                 */
   /*                                                                */
   /******************************************************************/
   if (Hcon    != MQHC_UNUSABLE_HCONN     &&
       CReason != MQRC_ALREADY_CONNECTED)
   {
     MQDISC(&Hcon,                   /* connection handle            */
            &CompCode,               /* completion code              */
            &Reason);                /* reason code                  */

     /* report reason, if any     */
     if (Reason != MQRC_NONE)
     {
       printf("MQDISC ended with reason code %d\n", Reason);
     }
   }

   /******************************************************************/
   /*                                                                */
   /* END OF AMQSPHAC                                                */
   /*                                                                */
   /******************************************************************/
   printf("Sample AMQSPHAC end\n");
   return(ExitRc);
 }
