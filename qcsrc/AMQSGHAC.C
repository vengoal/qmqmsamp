const static char sccsid[] = "%Z% %W% %I% %E% %U%";
 /********************************************************************/
 /*                                                                  */
 /* Program name: AMQSGHAC                                           */
 /*                                                                  */
 /* Description: Sample C program that gets messages from            */
 /*              a message queue using a reconnectable connection.   */
 /*              It is a highly available client.                    */
 /*                                                                  */
 /*   <copyright                                                     */
 /*   notice="lm-source-program"                                     */
 /*   pids="5724-H72"                                                */
 /*   years="1994,2014"                                              */
 /*   crc="3569207032" >                                             */
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
 /*   AMQSGHAC is a sample C program to get messages from a          */
 /*   message queue using a reconnectable connection. It is an       */
 /*   example of a program which is tolerant of temporary            */
 /*   unavailability of a queue manager.                             */
 /*                                                                  */
 /*   Program logic:                                                 */
 /*      MQCONNX queue manager                                       */
 /*      Take name of input queue from the parameter                 */
 /*      MQOPEN queue for INPUT                                      */
 /*      while no MQI failures,                                      */
 /*      .  MQGET next message, remove from queue                    */
 /*      .  print the result                                         */
 /*      MQCLOSE the source queue                                    */
 /*      MQDISC queue manager                                        */
 /*                                                                  */
 /*                                                                  */
 /********************************************************************/
 /*                                                                  */
 /*   AMQSGHAC has the following parameters                          */
 /*       required:                                                  */
 /*                 (1) The name of the source queue                 */
 /*       optional:                                                  */
 /*                 (2) Queue manager name                           */
 /*                                                                  */
 /********************************************************************/
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <time.h>
     /* includes for MQI  */
 #include <cmqc.h>

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
   MQGMO   gmo = {MQGMO_DEFAULT};   /* get message options           */
   MQCBD   cbd = {MQCBD_DEFAULT};   /* Callback Descriptor           */
      /** note, sample uses defaults where it can **/

   MQHCONN  Hcon = MQHC_UNUSABLE_HCONN;        /* connection handle  */
   MQHOBJ   Hobj;                   /* object handle                 */
   MQLONG   O_options;              /* MQOPEN options                */
   MQLONG   C_options;              /* MQCLOSE options               */
   MQLONG   CompCode;               /* completion code               */
   MQLONG   OpenCode;               /* MQOPEN completion code        */
   MQLONG   Reason;                 /* reason code                   */
   MQLONG   CReason;                /* reason code for MQCONN        */
   MQBYTE   buffer[65536];          /* message buffer                */
   MQLONG   buflen;                 /* buffer length                 */
   MQLONG   messlen;                /* message length received       */
   char     QMName[50];             /* queue manager name            */
   int      ExitRc  = 0;            /* program exit reason code      */

   printf("Sample AMQSGHAC start\n");
   if (argc < 2)
   {
     printf("Required parameter missing - queue name\n");
     ExitRc = 99;
     goto MOD_EXIT;
   }

   /******************************************************************/
   /*                                                                */
   /*   Create object descriptor for subject queue                   */
   /*                                                                */
   /******************************************************************/
   strncpy(od.ObjectName, argv[1], MQ_Q_NAME_LENGTH);
   QMName[0] = 0;   /* default */
   if (argc > 2)
     strncpy(QMName, argv[2], MQ_Q_MGR_NAME_LENGTH);

   /******************************************************************/
   /*                                                                */
   /*   Connect to queue manager                                     */
   /*                                                                */
   /******************************************************************/
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
   /*   Open the named message queue for input; exclusive or shared  */
   /*   use of the queue is controlled by the queue definition here  */
   /*                                                                */
   /******************************************************************/

   O_options = MQOO_INPUT_AS_Q_DEF    /* open queue for input      */
             | MQOO_FAIL_IF_QUIESCING /* but not if MQM stopping   */
             ;                        /* = 0x2001 = 8193 decimal   */


   MQOPEN(Hcon,                      /* connection handle            */
          &od,                       /* object descriptor for queue  */
          O_options,                 /* open options                 */
          &Hobj,                     /* object handle                */
          &OpenCode,                 /* completion code              */
          &Reason);                  /* reason code                  */

   /* report reason, if any; stop if failed      */
   if (Reason != MQRC_NONE)
   {
     printf("MQOPEN ended with reason code %d\n", Reason);
   }

   if (OpenCode == MQCC_FAILED)
   {
     printf("unable to open queue for input\n");
     ExitRc = (int) Reason;
     goto MOD_EXIT;
   }

   /******************************************************************/
   /*                                                                */
   /*   Get messages from the message queue                          */
   /*   Loop until there is a failure                                */
   /*                                                                */
   /******************************************************************/
   CompCode = OpenCode;       /* use MQOPEN result for initial test  */

   gmo.Version = MQGMO_VERSION_2;     /* Avoid need to reset Message */
   gmo.MatchOptions = MQMO_NONE;      /* ID and Correlation ID after */
                                      /* every MQGET                 */
   gmo.Options = MQGMO_WAIT           /* wait for new messages       */
               | MQGMO_NO_SYNCPOINT   /* no transaction              */
               | MQGMO_CONVERT;       /* convert if necessary        */
   gmo.WaitInterval = MQWI_UNLIMITED; /* Unlimited waiting           */

   while (CompCode != MQCC_FAILED)
   {
     buflen = sizeof(buffer) - 1; /* buffer size available for GET   */

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
       printf("MQGET ended with reason code %d\n", Reason);
     }

     /****************************************************************/
     /*   Display each message received                              */
     /****************************************************************/
     if (CompCode != MQCC_FAILED)
     {
       buffer[messlen] = '\0';            /* add terminator          */
       printf("message <%s>\n", buffer);
     }
   }

   /******************************************************************/
   /*                                                                */
   /*   Close the source queue (if it was opened)                    */
   /*                                                                */
   /******************************************************************/
   if (OpenCode != MQCC_FAILED)
   {
     C_options = MQCO_NONE;           /* no close options            */

     MQCLOSE(Hcon,                    /* connection handle           */
             &Hobj,                   /* object handle               */
             C_options,
             &CompCode,               /* completion code             */
             &Reason);                /* reason code                 */

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
     MQDISC(&Hcon,                     /* connection handle          */
            &CompCode,                 /* completion code            */
            &Reason);                  /* reason code                */

     /* report reason, if any     */
     if (Reason != MQRC_NONE)
     {
       printf("MQDISC ended with reason code %d\n", Reason);
     }
   }

   /******************************************************************/
   /*                                                                */
   /* END OF AMQSGHAC                                                */
   /*                                                                */
   /******************************************************************/
   printf("Sample AMQSGHAC end\n");
   return(0);
 }
