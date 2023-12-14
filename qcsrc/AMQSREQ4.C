/* %Z% %W% %I% %E% %U% */
 /********************************************************************/
 /*                                                                  */
 /* Program name: AMQSREQ4                                           */
 /*                                                                  */
 /* Description: Sample C program that puts request messages to      */
 /*              a message queue and shows the replies (example      */
 /*              using REPLY queue)                                  */
 /*                                                                  */
 /*   <copyright                                                     */
 /*   notice="lm-source-program"                                     */
 /*   pids="5724-H72,"                                               */
 /*   years="1993,2012"                                              */
 /*   crc="2098015419" >                                             */
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
 /*   AMQSREQ4 is a sample C program to put request messages to a    */
 /*   message queue, and then show the reply messages                */
 /*                                                                  */
 /*   Note: AMQSREQ4 will only receive replies to its requests       */
 /*         if there is a program running, or triggered, to          */
 /*         respond to them.   Samples AMQSINQA, AMQSSETA or         */
 /*         AMQSECH4 could be used for this purpose.                 */
 /*                                                                  */
 /*      -- requests are sent to the queue named by parameter 1;     */
 /*         replies are to the queue named by parameter 3 (or a      */
 /*         queue named SYSTEM.SAMPLE.REPLY if not specified).       */
 /*         The ReplyToQ can be either a local queue or a model      */
 /*         queue from which a dynamic queue can be created          */
 /*                                                                  */
 /*      -- gets lines from StdIn, and adds each to target           */
 /*         queue, taking each line of text as the content           */
 /*         of a request message; sample stops when a null           */
 /*         line (or EOF) is read                                    */
 /*                                                                  */
 /*      -- writes the reply messages, assuming each message         */
 /*         to represent a line of text                              */
 /*                                                                  */
 /*      -- writes a message for each MQI reason other than          */
 /*         MQRC_NONE; stops if there is a MQI completion code       */
 /*         of MQCC_FAILED                                           */
 /*                                                                  */
 /*                                                                  */
 /*   Program logic:                                                 */
 /*      MQOPEN server queue for output                              */
 /*      MQOPEN the reply queue for exclusive input                  */
 /*      for each line in the input file,                            */
 /*      .  MQPUT request message containing text to server queue    */
 /*      while no MQI failures,                                      */
 /*      .  MQGET message from reply queue                           */
 /*      .  display its content                                      */
 /*      MQCLOSE both queues                                         */
 /*                                                                  */
 /********************************************************************/
 /*                                                                  */
 /*   AMQSREQ4 has 3 parameters                                      */
 /*                 - the name of the target queue (required)        */
 /*                 - queue manager name (optional)                  */
 /*                 - the name of the ReplyToQ (optional)            */
 /*                                                                  */
 /********************************************************************/
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
    /* includes for MQI  */
 #include <cmqc.h>

 int main(int argc, char **argv)
 {

   /*   Declare file for sample input                                */
   FILE *fp;

   /*   Declare MQI structures needed                                */
   MQOD     od = {MQOD_DEFAULT};    /* Object Descriptor             */
   MQOD    odr = {MQOD_DEFAULT};    /* Object Descriptor for reply   */
   MQMD     md = {MQMD_DEFAULT};    /* Message Descriptor            */
   MQGMO   gmo = {MQGMO_DEFAULT};   /* get message options           */
   MQPMO   pmo = {MQPMO_DEFAULT};   /* put message options           */
      /** note, sample uses defaults where it can **/

   MQHCONN  Hcon;                   /* connection handle             */
   MQHOBJ   Hobj;                   /* object handle (server)        */
   MQHOBJ   Hreply;                 /* object handle for reply       */
   MQLONG   O_options;              /* MQOPEN options                */
   MQLONG   C_options;              /* MQCLOSE options               */
   MQLONG   CompCode;               /* completion code               */
   MQLONG   OpenCode;               /* MQOPEN completion code        */
   MQLONG   Reason;                 /* reason code                   */
   MQLONG   CReason;                /* reason code for MQCONN        */
   char     buffer[100];            /* message buffer                */
   MQLONG   buflen;                 /* buffer length                 */
   MQLONG   replylen;               /* reply length                  */
   MQCHAR48 replyQ;                 /* reply queue name              */
   MQLONG   Selectors[1];           /* selectors                     */
   MQLONG   QueueType[1];           /* queue type used for close opts*/
   char     QMName[50];             /* queue manager name            */

   Selectors[0] = MQIA_DEFINITION_TYPE;
   printf("Sample AMQSREQ4 start\n");
   if (argc < 2)
   {
     printf("Required parameter missing - queue name\n");
     exit(99);
   }

   /******************************************************************/
   /*                                                                */
   /*   Connect to queue manager                                     */
   /*                                                                */
   /******************************************************************/
   QMName[0] = 0;    /* default */
   if (argc > 2)
     strncpy(QMName, argv[2], MQ_Q_MGR_NAME_LENGTH);
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
   /*   Use parameter as the name of the target queue                */
   /*                                                                */
   /******************************************************************/
   strncpy(od.ObjectName, argv[1], MQ_Q_NAME_LENGTH);
   printf("server queue is %s\n", od.ObjectName);

   /******************************************************************/
   /*                                                                */
   /*   Open the server message queue for output                     */
   /*                                                                */
   /******************************************************************/
   O_options = MQOO_OUTPUT         /* open queue for output          */
        + MQOO_FAIL_IF_QUIESCING;  /* but not if MQM stopping        */
   MQOPEN(Hcon,                    /* connection handle              */
          &od,                     /* object descriptor for queue    */
          O_options,               /* open options                   */
          &Hobj,                   /* object handle                  */
          &OpenCode,               /* completion code                */
          &Reason);                /* reason code                    */

   /* report reason, if any; stop if failed */
   if (Reason != MQRC_NONE)
   {
     printf("MQOPEN ended with reason code %ld\n", Reason);
   }

   if (OpenCode == MQCC_FAILED)
   {
     printf("unable to open server queue for output\n");
     exit(Reason);
   }

   /******************************************************************/
   /*                                                                */
   /*   Open the queue to receive the reply messages; allow a        */
   /*   dynamic queue to be created from a model queue               */
   /*                                                                */
   /******************************************************************/
   O_options = MQOO_INPUT_EXCLUSIVE /* open queue for input          */
          + MQOO_INQUIRE            /* open queue for inquiry        */
          + MQOO_FAIL_IF_QUIESCING; /* but not if MQM stopping       */

   if (argc > 3)                    /* specified reply queue name    */
     strncpy(odr.ObjectName, argv[3], MQ_Q_NAME_LENGTH);
   else                             /* default reply queue name      */
     strcpy(odr.ObjectName, "SYSTEM.SAMPLE.REPLY");

   strcpy(odr.DynamicQName, "*");  /* dynamic queue name             */

   MQOPEN(Hcon,                    /* connection handle              */
          &odr,                    /* object descriptor for queue    */
          O_options,               /* open options                   */
          &Hreply,                 /* reply object handle            */
          &OpenCode,               /* completion code                */
          &Reason);                /* reason code                    */
   /* report reason, if any; stop if failed      */
   if (Reason != MQRC_NONE)
   {
     printf("MQOPEN ended with reason code %ld\n", Reason);
   }

   if (OpenCode == MQCC_FAILED)
   {
     printf("unable to open reply queue\n");
   }
   else
   {
     /****************************************************************/
     /*                                                              */
     /*   Save reply queue name - ObjectName is either the specified */
     /*   local queue, or a generated dynamic queue name             */
     /*                                                              */
     /****************************************************************/
     strncpy(replyQ, odr.ObjectName, MQ_Q_NAME_LENGTH);
     printf("replies to %.48s\n", replyQ);
   }

   /******************************************************************/
   /*                                                                */
   /*   Read lines from the user and put them to the message queue   */
   /*   Loop until null line or if there is a failure                */
   /*                                                                */
   /******************************************************************/
   CompCode = OpenCode;       /* use MQOPEN result for initial test  */
   fp = stdin;

   md.MsgType  = MQMT_REQUEST; /* message is a request           */
   /* ask for exceptions to be reported with original text       */
   md.Report = MQRO_EXCEPTION_WITH_DATA;
   strncpy(md.ReplyToQ,        /* reply queue name               */
           replyQ, MQ_Q_NAME_LENGTH);
   memcpy(md.Format,           /* character string format        */
           MQFMT_STRING, MQ_FORMAT_LENGTH);

   pmo.Options |= MQPMO_NEW_MSG_ID | MQPMO_NO_SYNCPOINT;

   while (CompCode != MQCC_FAILED)
   {
     if (fgets(buffer, sizeof(buffer) - 1, fp)
                               != NULL)  /* read next line           */
     {
       buflen = strlen(buffer) - 1; /* length without end-line       */
       buffer[buflen] = '\0';       /* remove end-line               */
     }
     else buflen = 0;                /* treat EOF same as null line  */

     /****************************************************************/
     /*                                                              */
     /*   Put each buffer to the message queue                       */
     /*                                                              */
     /****************************************************************/
     if (buflen > 0)
     {
       MQPUT(Hcon,                /* connection handle               */
             Hobj,                /* object handle                   */
             &md,                 /* message descriptor              */
             &pmo,                /* default options                 */
             buflen,              /* buffer length                   */
             buffer,              /* message buffer                  */
             &CompCode,           /* completion code                 */
             &Reason);            /* reason code                     */

       /* report reason, if any */
       if (Reason != MQRC_NONE)
       {
         printf("MQPUT ended with reason code %ld\n", Reason);
       }

     }
     else        /* close the message loop if null line in file */
       CompCode = MQCC_FAILED;
   }

   /******************************************************************/
   /*                                                                */
   /*   Get and display the reply messages                           */
   /*                                                                */
   /******************************************************************/
   CompCode = OpenCode;          /* only if the reply queue is open  */
   gmo.Version = MQGMO_VERSION_2;    /* Avoid need to reset Message  */
   gmo.MatchOptions = MQMO_NONE;     /* ID and Correlation ID after  */
                                     /* every MQGET                  */
   gmo.WaitInterval = 60000;     /* 1 minute limit for first reply   */
   gmo.Options = MQGMO_WAIT        /* wait for replies               */
       | MQGMO_CONVERT             /* request conversion if needed   */
       | MQGMO_ACCEPT_TRUNCATED_MSG  /* can truncate if needed       */
       | MQGMO_NO_SYNCPOINT;         /* No syncpoint                 */

   while (CompCode != MQCC_FAILED)
   {
     /** specify representation that is required **/
     md.Encoding = MQENC_NATIVE;
     md.CodedCharSetId = MQCCSI_Q_MGR;

     /****************************************************************/
     /*                                                              */
     /*   Get reply message                                          */
     /*                                                              */
     /****************************************************************/
     buflen = 80;                 /* get first 80 bytes only         */
     MQGET(Hcon,                  /* connection handle               */
           Hreply,                /* object handle for reply         */
           &md,                   /* message descriptor              */
           &gmo,                  /* get options                     */
           buflen,                /* buffer length                   */
           buffer,                /* message buffer                  */
           &replylen,             /* reply length                    */
           &CompCode,             /* completion code                 */
           &Reason);              /* reason code                     */
     gmo.WaitInterval = 15000;    /* 15 second limit for others      */

     /* report reason, if any */
     switch(Reason)
     {
       case MQRC_NONE:
         break;
       case MQRC_NO_MSG_AVAILABLE:
         printf("no more replies\n");
         break;
       default:
         printf("MQGET ended with reason code %ld\n", Reason);
         break;
     }

     /****************************************************************/
     /*                                                              */
     /*   Display reply message                                      */
     /*                                                              */
     /****************************************************************/
     if (CompCode != MQCC_FAILED)
     {
       if (replylen < buflen)      /* terminate (truncated) string   */
         buffer[replylen] = '\0';
       else
         buffer[buflen] = '\0';

       printf("response <%s>\n", buffer); /* display the response    */
       if (md.MsgType == MQMT_REPORT)     /* display report feedback */
         printf("  report with feedback = %ld\n", md.Feedback);
     }
   }

   /******************************************************************/
   /*                                                                */
   /*   Close server queue - program terminated if open failed       */
   /*                                                                */
   /******************************************************************/
   C_options = 0;                  /* no close options               */

   MQCLOSE(Hcon,                   /* connection handle              */
           &Hobj,                   /* object handle                 */
           C_options,
           &CompCode,               /* completion code               */
           &Reason);                /* reason code                   */

   /* report reason, if any     */
   if (Reason != MQRC_NONE)
   {
     printf("MQCLOSE (server) ended with reason code %ld\n", Reason);
   }

   /******************************************************************/
   /*                                                                */
   /*   Close reply queue - if it was opened                         */
   /*                                                                */
   /******************************************************************/
   if (OpenCode != MQCC_FAILED)
   {
     MQINQ(Hcon,         /* Check to see if the reply queue requires */
           Hreply,       /* deletion after the MQCLOSE.              */
           1,           /* e.g. dynamic queue definitions           */
           Selectors,
           1,
           QueueType,
           0,
           NULL,
           &CompCode,
           &Reason
           );

     C_options = 0;
     if (QueueType[0] != MQQDT_PREDEFINED)
         C_options = MQCO_DELETE;        /* delete if dynamic queue  */

     MQCLOSE(Hcon,                   /* connection handle            */
             &Hreply,                 /* object handle               */
             C_options,
             &CompCode,               /* completion code             */
             &Reason);                /* reason code                 */

     /* report reason, if any     */
     if (Reason != MQRC_NONE)
     {
       printf("MQCLOSE (reply) ended with reason code %ld\n", Reason);
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

   /*******************************************************************/
   /*                                                                 */
   /* END OF AMQSREQ4                                                 */
   /*                                                                 */
   /*******************************************************************/
   printf("Sample AMQSREQ4 end\n");
   return(0);
 }
