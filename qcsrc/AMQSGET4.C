/* %Z% %W% %I% %E% %U% */
 /********************************************************************/
 /*                                                                  */
 /* Program name: AMQSGET4                                           */
 /*                                                                  */
 /* Description: Sample C program that gets messages from            */
 /*              a message queue (example using MQGET)               */
 /*                                                                  */
 /*   <copyright                                                     */
 /*   notice="lm-source-program"                                     */
 /*   pids="5724-H72,"                                               */
 /*   years="1993,2012"                                              */
 /*   crc="1935260464" >                                             */
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
 /*   AMQSGET4 is a sample C program to get messages from a          */
 /*   message queue, and is an example of MQGET.                     */
 /*                                                                  */
 /*      -- sample reads from message queue named in the parameter   */
 /*                                                                  */
 /*      -- displays the contents of the message queue,              */
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
 /*      Take name of input queue from the parameter                 */
 /*      MQOPEN queue for INPUT                                      */
 /*      while no MQI failures,                                      */
 /*      .  MQGET next message, remove from queue                    */
 /*      .  print the result                                         */
 /*      .  (no message available counts as failure, and loop ends)  */
 /*      MQCLOSE the subject queue                                   */
 /*                                                                  */
 /*                                                                  */
 /********************************************************************/
 /*                                                                  */
 /*   AMQSGET4 has 2 parameters -                                    */
 /*                  - the name of the message queue (required)      */
 /*                  - the queue manager name (optional)             */
 /*                                                                  */
 /********************************************************************/
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
     /* includes for MQI  */
 #include <cmqc.h>

 int main(int argc, char **argv)
 {

   /*   Declare MQI structures needed                                */
   MQOD     od = {MQOD_DEFAULT};    /* Object Descriptor             */
   MQMD     md = {MQMD_DEFAULT};    /* Message Descriptor            */
   MQGMO   gmo = {MQGMO_DEFAULT};   /* get message options           */
      /** note, sample uses defaults where it can **/

   MQHCONN  Hcon;                   /* connection handle             */
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

   printf("Sample AMQSGET4 start\n");
   if (argc < 2)
   {
     printf("Required parameter missing - queue name\n");
     exit(99);
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
   MQCONN(QMName,                  /* queue manager                  */
          &Hcon,                   /* connection handle              */
          &CompCode,               /* completion code                */
          &CReason);               /* reason code                    */

   /* report reason and stop if it failed     */
   if (CompCode == MQCC_FAILED)
   {
     printf("MQCONN ended with reason code %ld\n", CReason);
     exit( (int)CReason );
   }

   /******************************************************************/
   /*                                                                */
   /*   Open the named message queue for input; exclusive or shared  */
   /*   use of the queue is controlled by the queue definition here  */
   /*                                                                */
   /******************************************************************/
   O_options = MQOO_INPUT_AS_Q_DEF   /* open queue for input         */
         + MQOO_FAIL_IF_QUIESCING;   /* but not if MQM stopping      */
   MQOPEN(Hcon,                      /* connection handle            */
          &od,                       /* object descriptor for queue  */
          O_options,                 /* open options                 */
          &Hobj,                     /* object handle                */
          &OpenCode,                 /* completion code              */
          &Reason);                  /* reason code                  */

   /* report reason, if any; stop if failed      */
   if (Reason != MQRC_NONE)
   {
     printf("MQOPEN ended with reason code %ld\n", Reason);
   }

   if (OpenCode == MQCC_FAILED)
   {
     printf("unable to open queue for input\n");
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
   gmo.Options = MQGMO_WAIT       /* wait for new messages           */
               | MQGMO_CONVERT    /* convert if necessary            */
               | MQGMO_NO_SYNCPOINT;   /* No syncpoint               */
   gmo.WaitInterval = 15000;      /* 15 second limit for waiting     */

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
       if (Reason == MQRC_NO_MSG_AVAILABLE)
       {                         /* special report for normal end    */
         printf("no more messages\n");
       }
       else                      /* general report for other reasons */
       {
         printf("MQGET ended with reason code %ld\n", Reason);

         /*   treat truncated message as a failure for this sample   */
         if (Reason == MQRC_TRUNCATED_MSG_FAILED)
         {
           CompCode = MQCC_FAILED;
         }
       }
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
     C_options = 0;                   /* no close options            */
     MQCLOSE(Hcon,                    /* connection handle           */
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
   if (CReason != MQRC_ALREADY_CONNECTED )
   {
     MQDISC(&Hcon,                     /* connection handle          */
            &CompCode,                 /* completion code            */
            &Reason);                  /* reason code                */

     /* report reason, if any     */
     if (Reason != MQRC_NONE)
     {
       printf("MQDISC ended with reason code %ld\n", Reason);
     }
   }

   /******************************************************************/
   /*                                                                */
   /* END OF AMQSGET4                                                */
   /*                                                                */
   /******************************************************************/
   printf("Sample AMQSGET4 end\n");
   return(0);
 }
