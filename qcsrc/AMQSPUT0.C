/* %Z% %W% %I% %E% %U% */
 /********************************************************************/
 /*                                                                  */
 /* Program name: AMQSPUT0                                           */
 /*                                                                  */
 /* Description: Sample C program that puts messages to              */
 /*              a message queue (example using MQPUT)               */
 /*                                                                  */
 /*   <copyright                                                     */
 /*   notice="lm-source-program"                                     */
 /*   pids="5724-H72,"                                               */
 /*   years="1993,2012"                                              */
 /*   crc="3433096771" >                                             */
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
 /*   AMQSPUT0 is a sample C program to put messages on a message    */
 /*   queue, and is an example of the use of MQPUT.                  */
 /*                                                                  */
 /*      -- messages are sent to the queue named by the parameter    */
 /*                                                                  */
 /*      -- gets lines from StdIn, and adds each to target           */
 /*         queue, taking each line of text as the content           */
 /*         of a datagram message; the sample stops when a null      */
 /*         line (or EOF) is read.                                   */
 /*         New-line characters are removed.                         */
 /*         If a line is longer than 65534 characters it is broken   */
 /*         up into 65534-character pieces. Each piece becomes the   */
 /*         content of a datagram message.                           */
 /*         If the length of a line is a multiple of 65534 plus 1    */
 /*         e.g. 131069, the last piece will only contain a new-line */
 /*         character so will terminate the input.                   */
 /*                                                                  */
 /*      -- writes a message for each MQI reason other than          */
 /*         MQRC_NONE; stops if there is a MQI completion code       */
 /*         of MQCC_FAILED                                           */
 /*                                                                  */
 /*    Program logic:                                                */
 /*         MQOPEN target queue for OUTPUT                           */
 /*         while end of input file not reached,                     */
 /*         .  read next line of text                                */
 /*         .  MQPUT datagram message with text line as data         */
 /*         MQCLOSE target queue                                     */
 /*                                                                  */
 /*                                                                  */
 /********************************************************************/
 /*                                                                  */
 /*   AMQSPUT0 has 2 parameters                                      */
 /*                 - the name of the target queue (required)        */
 /*                 - queue manager name (optional)                  */
 /*                                                                  */
 /********************************************************************/
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
    /* includes for MQI */
 #include <cmqc.h>

 int main(int argc, char **argv)
 {
    /*  Declare file and character for sample input                  */
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
   MQLONG   CReason;                /* reason code for MQCONN        */
   MQLONG   messlen;                /* message length                */
   char     buffer[65535];          /* message buffer                */
   char     QMName[50];             /* queue manager name            */

   printf("Sample AMQSPUT0 start\n");
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
     strncpy(QMName, argv[2], (size_t)MQ_Q_MGR_NAME_LENGTH);
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
   O_options = MQOO_OUTPUT           /* open queue for output        */
           + MQOO_FAIL_IF_QUIESCING; /* but not if MQM stopping      */
   MQOPEN(Hcon,                      /* connection handle            */
          &od,                       /* object descriptor for queue  */
          O_options,                 /* open options                 */
          &Hobj,                     /* object handle                */
          &OpenCode,                 /* MQOPEN completion code       */
          &Reason);                  /* reason code                  */

   /* report reason, if any; stop if failed      */
   if (Reason != MQRC_NONE)
   {
     printf("MQOPEN ended with reason code %ld\n", Reason);
   }

   if (OpenCode == MQCC_FAILED)
   {
     printf("unable to open queue for output\n");
   }

   /******************************************************************/
   /*                                                                */
   /*   Read lines from the file and put them to the message queue   */
   /*   Loop until null line or end of file, or there is a failure   */
   /*                                                                */
   /******************************************************************/
   CompCode = OpenCode;        /* use MQOPEN result for initial test */
   fp = stdin;

   memcpy(md.Format,          /* character string format         */
          MQFMT_STRING, (size_t)MQ_FORMAT_LENGTH);

   pmo.Options |= MQPMO_NEW_MSG_ID;

   while (CompCode != MQCC_FAILED)
   {
     if (fgets(buffer, sizeof(buffer), fp) != NULL)
     {
       messlen = strlen(buffer);       /* length without null         */
       if (buffer[messlen-1] == '\n')  /* last char is a new-line     */
       {
         buffer[messlen-1]  = '\0';    /* replace new-line with null  */
         --messlen;                    /* reduce buffer length        */
       }
     }
     else messlen = 0;        /* treat EOF same as null line          */

     /****************************************************************/
     /*                                                              */
     /*   Put each buffer to the message queue                       */
     /*                                                              */
     /****************************************************************/
     if (messlen > 0)
     {
       MQPUT(Hcon,                /* connection handle               */
             Hobj,                /* object handle                   */
             &md,                 /* message descriptor              */
             &pmo,                /* default options (datagram)      */
             messlen,             /* message length                  */
             buffer,              /* message buffer                  */
             &CompCode,           /* completion code                 */
             &Reason);            /* reason code                     */

       /* report reason, if any */
       if (Reason != MQRC_NONE)
       {
         printf("MQPUT ended with reason code %ld\n", Reason);
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
   /* END OF AMQSPUT0                                                */
   /*                                                                */
   /******************************************************************/
   printf("Sample AMQSPUT0 end\n");
   return(0);
 }
