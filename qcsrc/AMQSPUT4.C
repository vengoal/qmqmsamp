/* %Z% %W% %I% %E% %U% */
/********************************************************************/
 /*                                                                  */
 /* Program name: AMQSPUT4                                           */
 /*                                                                  */
 /* Description: Sample C program that puts messages to              */
 /*              a message queue (example using MQPUT)               */
 /*                                                                  */
 /*   <copyright                                                     */
 /*   notice="lm-source-program"                                     */
 /*   pids="5724-H72,"                                               */
 /*   years="1993,2012"                                              */
 /*   crc="197011674" >                                              */
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
 /*   AMQSPUT4 is a sample C program to put messages on a message    */
 /*   queue, and is an example of the use of MQPUT.                  */
 /*                                                                  */
 /*      -- input is from a file named in the parameter;             */
 /*         first line of the file names the target queue            */
 /*                                                                  */
 /*      -- gets further text lines, and adds each to message        */
 /*         queue Q, taking each line of text as the content         */
 /*         of a datagram message; the sample stops when a null      */
 /*         line is read.                                            */
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
 /*         Open input file, and read first line as a queue name     */
 /*         MQOPEN target queue for OUTPUT                           */
 /*         while end of input file not reached,                     */
 /*         .  read next line of text                                */
 /*         .  MQPUT datagram message with text line as data         */
 /*         MQCLOSE target queue                                     */
 /*                                                                  */
 /*                                                                  */
 /********************************************************************/
 /*                                                                  */
 /*                                                                  */
 /*                                                                  */
 /*   Exceptions signaled:  none                                     */
 /*   Exceptions monitored: none                                     */
 /*                                                                  */
 /*   AMQSPUT4 has 2 parameters                                      */
 /*                 - the name of the input file (required)          */
 /*                 - the queue manager name (optional)              */
 /*                                                                  */
 /********************************************************************/
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
    /* includes for MQI */
 #include <cmqc.h>

 int main(int argc, char *argv[])
 {

 /*   Declare file and character for sample input                    */
   FILE *fp;
   int    i;  /* auxiliary counter  */

 /*   Declare MQI structures needed                                */
   MQOD     od = {MQOD_DEFAULT};    /* Object Descriptor           */
   MQMD     md = {MQMD_DEFAULT};    /* Message Descriptor          */
   MQPMO   pmo = {MQPMO_DEFAULT};   /* put message options         */
      /** note, sample uses defaults where it can **/

   MQHCONN  Hcon;                   /* connection handle           */
   MQHOBJ   Hobj;                   /* object handle               */
   MQLONG   O_options;              /* MQOPEN options              */
   MQLONG   C_options;              /* MQCLOSE options             */
   MQLONG   CompCode;               /* completion code             */
   MQLONG   OpenCode;               /* MQOPEN completion code      */
   MQLONG   Reason;                 /* reason code                 */
   MQLONG   CReason;                /* reason code for MQCONN      */
   MQLONG   buflen;                 /* buffer length               */
   MQBYTE   buffer[65535];          /* message buffer              */
   char     QMName[50];             /* queue manager name          */

 /********************************************************************/
 /*                                                                  */
 /*   Open file; read the name of the target queue                   */
 /*                                                                  */
 /********************************************************************/
 /*   Open the sample input specified as parameter                   */
   printf("Sample AMQSPUT4 start\n");
   if ((fp=fopen(argv[1], "r")) == NULL)
   {
     printf("Cannot open the file\n");
     exit(1);
   }

 /*   Read first line into the queue name                           */
   fgets(buffer, 50, fp);      /* read first line (to end-line)     */
   i = strlen(buffer) - 1;     /* length of queue name              */
   buffer[i] = '\0';           /* remove end-line (or truncate)     */
   if (i > MQ_Q_NAME_LENGTH)   /* warning if name too long          */
     printf("given queue name too long\n");
   strncpy(od.ObjectName, buffer, MQ_Q_NAME_LENGTH); /* queue name  */

   printf("target queue is %s\n", od.ObjectName);

   /******************************************************************/
   /*                                                                */
   /*   Connect to queue manager                                     */
   /*                                                                */
   /******************************************************************/
   if (argc > 2)
     strncpy(QMName, argv[2], MQ_Q_MGR_NAME_LENGTH);
   else
     QMName[0] = 0;    /* default */
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

 /********************************************************************/
 /*                                                                  */
 /*   Open the target message queue for output                       */
 /*                                                                  */
 /********************************************************************/
   O_options = MQOO_OUTPUT         /* open queue for output        */
         + MQOO_FAIL_IF_QUIESCING; /* but not if MQM stopping      */
   MQOPEN(Hcon,                    /* connection handle            */
          &od,                     /* object descriptor for queue  */
          O_options,               /* open options                 */
          &Hobj,                   /* object handle                */
          &OpenCode,               /* MQOPEN completion code       */
          &Reason);                /* reason code                  */

   /* report reason, if any; stop if failed      */
    if (Reason != MQRC_NONE)
    {
      printf("MQOPEN ended with reason code %ld\n", Reason);
    }

    if (OpenCode == MQCC_FAILED)
    {
      printf("unable to open queue for output\n");
    }

 /********************************************************************/
 /*                                                                  */
 /*   Put messages to the message queue                              */
 /*                                                                  */
 /********************************************************************/
   CompCode = OpenCode;        /* initial condition result of MQOPEN */
 /*   Read lines into message buffer . . .                           */
   while (CompCode != MQCC_FAILED)
   {
        /* read next line          */
     if (fgets(buffer, sizeof(buffer), fp) != NULL)
     {
       buflen = strlen(buffer);       /* length without null         */
       if (buffer[buflen-1] == '\n')  /* last char is a new-line     */
       {
         buffer[buflen-1]  = '\0';    /* replace new-line with null  */
         --buflen;                    /* reduce buffer length        */
       }
     }
     else buflen = 0;        /* treat EOF same as null line */

 /*   . . . put each buffer to message queue                         */
     if (buflen > 0)
     {
       memcpy(md.Format,           /* character string format      */
              MQFMT_STRING, MQ_FORMAT_LENGTH);


       MQPUT(Hcon,                 /* connection handle (default)  */
              Hobj,                /* object handle                */
              &md,                 /* message descriptor           */
              &pmo,                /* default options (datagram)   */
              buflen,              /* buffer length                */
              buffer,              /* message buffer               */
              &CompCode,           /* completion code              */
              &Reason);            /* reason code                  */

   /* report reason, if any; stop if failed      */
        if (Reason != MQRC_NONE)
        {
          printf("MQPUT ended with reason code %ld\n", Reason);
        }
     }
     else   /* satisfy end condition when empty line is read */
       CompCode = MQCC_FAILED;
   }

 /********************************************************************/
 /*                                                                  */
 /*   Close the target queue (if it was opened)                      */
 /*                                                                  */
 /********************************************************************/
   if (OpenCode != MQCC_FAILED)
   {
     C_options = 0;                  /* no close options             */
     MQCLOSE(Hcon,                   /* connection handle            */
            &Hobj,                   /* object handle                */
            C_options,
            &CompCode,               /* completion code              */
            &Reason);                /* reason code                  */

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
 /********************************************************************/
 /*                                                                  */
 /* END OF AMQSPUT4                                                  */
 /*                                                                  */
 /********************************************************************/
   printf("Sample AMQSPUT4 end\n");
 }
