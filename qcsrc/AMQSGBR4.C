/* %Z% %W% %I% %E% %U% */
 /********************************************************************/
 /*                                                                  */
 /* Program name: AMQSGBR4                                           */
 /*                                                                  */
 /* Description: Sample C program that displays messages on          */
 /*              a message queue (example using Browse option        */
 /*              of MQGET)                                           */
 /*                                                                  */
 /*   <copyright                                                     */
 /*   notice="lm-source-program"                                     */
 /*   pids="5724-H72,"                                               */
 /*   years="1993,2012"                                              */
 /*   crc="1217746612" >                                             */
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
 /*   AMQSGBR4 is a sample C program to display messages on          */
 /*   a message queue, and is an example of Browse option of MQGET   */
 /*                                                                  */
 /*      -- sample gets messages from the queue named in the         */
 /*         parameter                                                */
 /*                                                                  */
 /*      -- displays the contents of the message queue,              */
 /*         assuming each message data to represent a line of        */
 /*         text to be written                                       */
 /*                                                                  */
 /*         only the first 65535 characters of each message are      */
 /*         shown; incomplete messages are noted                     */
 /*                                                                  */
 /*         leaves the messages on the queue                         */
 /*                                                                  */
 /*         the displayed name of the queue is the resolved          */
 /*         local queue name if the object name is an alias          */
 /*                                                                  */
 /*      -- writes a message for each MQI reason other than          */
 /*         MQRC_NONE; stops if there is a MQI completion code       */
 /*         of MQCC_FAILED                                           */
 /*                                                                  */
 /*                                                                  */
 /*   Program logic:                                                 */
 /*      Use the parameter to name the input queue                   */
 /*      MQOPEN queue for BROWSE                                     */
 /*      while no MQI failures,                                      */
 /*      .  MQGET next message (browse)                              */
 /*      .  print Resolved Queue Name first time                     */
 /*      .  print up to 65535 bytes of message data                  */
 /*      MQCLOSE the source queue                                    */
 /*                                                                  */
 /*                                                                  */
 /********************************************************************/
 /*                                                                  */
 /*   AMQSGBR4 has 2 parameters -                                    */
 /*                  - the name of the input queue (required)        */
 /*                  - queue manager name (optional)                 */
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
   MQLONG   CReason;                /* reason code from MQCONN       */
   MQBYTE   buffer[65536];          /* message buffer                */
   MQLONG   buflen;                 /* buffer length                 */
   MQLONG   messlen;                /* message length received       */
   int      i;                      /* auxiliary counter             */
   char     QMName[50];             /* queue manager name            */

   printf("Sample AMQSGBR4 (browse) start\n");
   if (argc < 2)
   {
     printf("Required parameter missing - queue name\n");
     exit(99);
   }

   /******************************************************************/
   /*                                                                */
   /*   Create object descriptor for fixed subject queue             */
   /*                                                                */
   /******************************************************************/
   strncpy(od.ObjectName, argv[1], MQ_Q_NAME_LENGTH);
   QMName[0] = 0;    /* default */
   if (argc > 2)
     strncpy(QMName, argv[2], MQ_Q_MGR_NAME_LENGTH);
   printf("%s\n", QMName);

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
     exit(CReason);
   }

   /******************************************************************/
   /*                                                                */
   /*   Open the message queue for Browse                            */
   /*                                                                */
   /******************************************************************/
   O_options = MQOO_BROWSE         /* open queue for browse,         */
         + MQOO_FAIL_IF_QUIESCING; /* but not if MQM stopping        */
   MQOPEN(Hcon,                    /* connection handle              */
          &od,                     /* object descriptor for queue    */
          O_options,               /* open options                   */
          &Hobj,                   /* object handle                  */
          &OpenCode,               /* completion code                */
          &Reason);                /* reason code                    */

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
   /* If a message has been put on a queue using any MD version 2    */
   /* feature then we need to use a V2 MD to prevent the message     */
   /* text from being prepended by an MD extension (MQMDE).          */
   /******************************************************************/
   md.Version = MQMD_VERSION_2 ;

   /******************************************************************/
   /*                                                                */
   /*   Get messages from the message queue                          */
   /*   Loop until there is a failure, continuing on warnings        */
   /*                                                                */
   /******************************************************************/
   CompCode = OpenCode;        /* use MQOPEN result for initial test */
   gmo.Version = MQGMO_VERSION_2;     /* Avoid need to reset Message */
   gmo.MatchOptions = MQMO_NONE;      /* ID and Correlation ID after */
                                      /* every MQGET                 */
   gmo.Options = MQGMO_NO_WAIT     /* don't wait for new messages    */
     + MQGMO_BROWSE_NEXT           /* browse messages in order       */
     + MQGMO_ACCEPT_TRUNCATED_MSG    /* truncate longer message      */
     + MQGMO_CONVERT;                    /* Convert Application Data */
   i = 0;                          /* start counting the messages    */

   while (CompCode != MQCC_FAILED)
   {
     buflen = sizeof(buffer) - 1;  /* get size of message buffer     */

     /****************************************************************/
     /*                                                              */
     /*   MQGET sets Encoding and CodedCharSetId to the values in    */
     /*   the message returned, so these fields should be reset to   */
     /*   the default values before every call, in case              */
     /*   MQGMO_CONVERT is specified.                                */
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

     /****************************************************************/
     /*                                                              */
     /*   Display each message received                              */
     /*                                                              */
     /****************************************************************/
     if (CompCode != MQCC_FAILED)
     {
       /**************************************************************/
       /*                                                            */
       /*   Display name of resolved queue before first message      */
       /*                                                            */
       /**************************************************************/
       if (i == 0)
       {
         printf("Messages for %-48.48s\n", gmo.ResolvedQName);
       }
       i ++;                           /* count messages on queue    */

       if (messlen < buflen)
         buffer[messlen] = '\0';       /* add terminator             */
       else
         buffer[buflen] = '\0';
       printf("%d <%s>\n", i, buffer);
     }

     /* report reason, if any     */
     switch(Reason)
     {
       case (MQRC_NONE):
         break;
       case (MQRC_NO_MSG_AVAILABLE):
         printf("no more messages\n");
         break;
       case (MQRC_TRUNCATED_MSG_ACCEPTED):
         /* if message truncated, flag it */
         printf("  --- truncated\n");
         break;
       default:
         /* list other reason codes    */
         printf("MQGET ended with reason code %ld\n", Reason);
         break;
     }
   }

   /******************************************************************/
   /*                                                                */
   /*   Close the source queue (if it was opened)                    */
   /*                                                                */
   /******************************************************************/
   if (OpenCode != MQCC_FAILED)
   {
     C_options = 0;                  /* no close options             */
     MQCLOSE(Hcon,                   /* connection handle            */
             &Hobj,                  /* object handle                */
             C_options,
             &CompCode,              /* completion code              */
             &Reason);               /* reason code                  */

     /* report reason, if any     */
     if (Reason != MQRC_NONE)
     {
       printf("MQCLOSE ended with reason code %ld\n", Reason);
     }
   }

   /******************************************************************/
   /*                                                                */
   /*   Disconnect from MQM  (unless previously connected)           */
   /*                                                                */
   /******************************************************************/
   if (CReason != MQRC_ALREADY_CONNECTED)
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
   /* END OF AMQSGBR4                                                */
   /*                                                                */
   /******************************************************************/
   printf("Sample AMQSGBR4 (browse) end\n");
   return(0);
 }
