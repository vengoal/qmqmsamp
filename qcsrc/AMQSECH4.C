/* %Z% %W% %I% %E% %U% */
 /********************************************************************/
 /*                                                                  */
 /* Program name: AMQSECH4                                           */
 /*                                                                  */
 /* Description: Sample C program - echo messages to reply to queue  */
 /*                                                                  */
 /*   <copyright                                                     */
 /*   notice="lm-source-program"                                     */
 /*   pids="5724-H72,"                                               */
 /*   years="1994,2012"                                              */
 /*   crc="2018148919" >                                             */
 /*   Licensed Materials - Property of IBM                           */
 /*                                                                  */
 /*   5724-H72,                                                      */
 /*                                                                  */
 /*   (C) Copyright IBM Corp. 1994, 2012 All Rights Reserved.        */
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
 /*   AMQSECH4 is a sample C program that echos a message            */
 /*   from a message queue, to the reply to queue          .         */
 /*   It is intended to run as a triggered program, and so           */
 /*   receives input in a trigger parameter.                         */
 /*                                                                  */
 /*   NOTE: One way to run AMQSECH4 is to be running any             */
 /*         trigger monitor, and to use AMQSREQ* to send             */
 /*         request messages (containing any text) to a              */
 /*         queue (eg SYSTEM.SAMPLE.ECHO) which must have been       */
 /*         defined to trigger AMQSECH(4).                           */
 /*                                                                  */
 /*      -- gets request messages from a queue whose name is         */
 /*         in the trigger parameter; the content of each            */
 /*         message is the message to be echoed                      */
 /*                                                                  */
 /*      -- sends a reply message to the named reply queue which     */
 /*         contains message                                         */
 /*                                                                  */
 /*                                                                  */
 /*      -- stops when the input queue becomes empty                 */
 /*                                                                  */
 /*      -- writes a message for each MQI reason other than          */
 /*         MQRC_NONE; stops if there is a MQI completion code       */
 /*         of MQCC_FAILED                                           */
 /*                                                                  */
 /*                                                                  */
 /*  Program logic:                                                  */
 /*     MQCONNect to message queue manager                           */
 /*     MQOPEN message queue for shared input                        */
 /*     while no MQI failures,                                       */
 /*     .  MQGET next message from input queue                       */
 /*     .  Prepare reply message if MQGET was successful             */
 /*     .  Prepare a report message if MQGET failed                  */
 /*     .  MQPUT1, send reply or report to named reply queue         */
 /*     MQCLOSE queue A                                              */
 /*     MQDISConnect from queue manager                              */
 /*                                                                  */
 /*                                                                  */
 /********************************************************************/
 /*                                                                  */
 /*   AMQSECH4 has 1 parameter - a string (MQTMC2) based on the      */
 /*       initiation trigger message; only the QName and queue       */
 /*       manager name fields are used in this example               */
 /*                                                                  */
 /********************************************************************/
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
     /*  includes for MQI  */
 #include <cmqc.h>

 int main(int argc, char **argv)
 {

   /*   Declare MQI structures needed                                */
   MQOD    odG = {MQOD_DEFAULT};    /* Object Descriptor for GET     */
   MQOD    odR = {MQOD_DEFAULT};    /* Object Descriptor for reply   */
   MQMD     md = {MQMD_DEFAULT};    /* Message Descriptor            */
   MQGMO   gmo = {MQGMO_DEFAULT};   /* get message options           */
   MQPMO   pmo = {MQPMO_DEFAULT};   /* put message options           */
      /** note, sample uses defaults where it can **/
   MQTMC2   *trig;                  /* trigger message structure     */

   MQHCONN  Hcon;                   /* connection handle             */
   MQHOBJ   Hobj;                   /* object handle, server queue   */
   MQLONG   O_options;              /* MQOPEN options                */
   MQLONG   C_options;              /* MQCLOSE options               */
   MQLONG   CompCode;               /* completion code               */
   MQLONG   Reason;                 /* reason code                   */
   MQLONG   CReason;                /* reason code (MQCONN)          */
   MQBYTE   buffer[100];            /* message buffer                */
   MQLONG   buflen;                 /* buffer length                 */
   MQLONG   messlen;                /* message length received       */

   printf("Sample AMQSECH4 start\n");
   if (argc < 2)
   {
     printf("Missing parameter - start program by MQI trigger\n");
     exit(99);
   }

   /******************************************************************/
   /*                                                                */
   /*   Set the program argument into the trigger message            */
   /*                                                                */
   /******************************************************************/
   trig = (MQTMC2*)argv[1];        /* -> trigger message             */

   if(strncmp(trig->StrucId, MQTMC_STRUC_ID, 2))
   {
     printf("Parameter supplied is not a trigger message\n");
     printf("This program must be started by MQI trigger\n");
     exit(2);
   }

   printf("%s\n", trig->QMgrName);

   MQCONN(trig->QMgrName,          /* queue manager                  */
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
   /*   Open the message queue for shared input                      */
   /*                                                                */
   /******************************************************************/
   memcpy(odG.ObjectName,          /* name of input queue            */
          trig -> QName, MQ_Q_NAME_LENGTH);
   O_options = MQOO_INPUT_SHARED   /* open queue for shared input    */
             + MQOO_FAIL_IF_QUIESCING; /* but not if MQM stopping    */
   MQOPEN(Hcon,                    /* connection handle              */
          &odG,                    /* object descriptor for queue    */
          O_options,               /* open options                   */
          &Hobj,                   /* object handle                  */
          &CompCode,               /* MQOPEN completion code         */
          &Reason);                /* reason code                    */

   /* report reason if any; stop if it failed     */
   if (Reason != MQRC_NONE)
   {
     printf("MQOPEN (input) ended with reason code %ld\n", Reason);
   }

   if (CompCode == MQCC_FAILED)
   {
     exit(Reason);
   }

   /******************************************************************/
   /*                                                                */
   /*   Get messages from the message queue                          */
   /*   Loop until there is a warning or failure                     */
   /*                                                                */
   /******************************************************************/
   buflen = sizeof(buffer) - 1;
   while (CompCode == MQCC_OK)
   {
     gmo.Options = MQGMO_ACCEPT_TRUNCATED_MSG
                 | MQGMO_CONVERT  /* receive converted messages      */
                 | MQGMO_WAIT     /* wait for new messages           */
                 | MQGMO_NO_SYNCPOINT;    /* No syncpoint            */
     gmo.WaitInterval = 5000;     /* 5 second limit for waiting      */
                                  /* specify representation required */
     md.Encoding = MQENC_NATIVE;
     md.CodedCharSetId = MQCCSI_Q_MGR;

     /****************************************************************/
     /*                                                              */
     /*   In order to read the messages in sequence, MsgId and       */
     /*   CorrelID must have the default value.  MQGET sets them     */
     /*   to the values in for message it returns, so re-initialise  */
     /*   them before every call                                     */
     /*                                                              */
     /****************************************************************/
     memcpy(md.MsgId, MQMI_NONE, sizeof(md.MsgId));
     memcpy(md.CorrelId, MQCI_NONE, sizeof(md.CorrelId));

     MQGET(Hcon,                /* connection handle                 */
           Hobj,                /* object handle                     */
           &md,                 /* message descriptor                */
           &gmo,                /* GET options                       */
           buflen,              /* buffer length                     */
           buffer,              /* message buffer                    */
           &messlen,            /* message length                    */
           &CompCode,           /* completion code                   */
           &Reason);            /* reason code                       */

     /* report reason if any  (loop ends if it failed)    */
     if (Reason != MQRC_NONE)
     {
       printf("MQGET ended with reason code %ld\n", Reason);
     }

     /****************************************************************/
     /*                                                              */
     /*    Only process REQUEST messages                             */
     /*                                                              */
     /****************************************************************/
     if ((CompCode == MQCC_OK)  ||  (CompCode == MQCC_WARNING))
     {
       buffer[messlen] = '\0';  /* end string ready to use */
       printf("%s\n", buffer);

       if (md.MsgType != MQMT_REQUEST)
       {
         printf("  -- not a request and discarded\n");
         continue;
       }

       /**************************************************************/
       /*                                                            */
       /*   Send reply using MQPUT1                                  */
       /*                                                            */
       /**************************************************************/
       md.MsgType = MQMT_REPLY;

       /**************************************************************/
       /*                                                            */
       /*   Copy the ReplyTo queue name to the object descriptor     */
       /*                                                            */
       /**************************************************************/
       strncpy(odR.ObjectName, md.ReplyToQ, MQ_Q_NAME_LENGTH);
       strncpy(odR.ObjectQMgrName,
               md.ReplyToQMgr, MQ_Q_MGR_NAME_LENGTH);

       /**************************************************************/
       /*                                                            */
       /*   MsgId and CorrelId are currently the values of the       */
       /*   got message.  Reset them if requested, then              */
       /*   stop further reports                                     */
       /*                                                            */
       /**************************************************************/
       if ( !(md.Report & MQRO_PASS_CORREL_ID) )
       {
         memcpy(md.CorrelId, md.MsgId, sizeof(md.MsgId));
       }

       if ( !(md.Report & MQRO_PASS_MSG_ID) )
       {
         memcpy(md.MsgId, MQMI_NONE, sizeof(md.MsgId));
       }

       /**************************************************************/
       /*                                                            */
       /* If MQRO_PASS_DISCARD_AND_EXPIRY was set in the original    */
       /* message, then the reply should inherit MQRO_DISCARD_MSG    */
       /* if it was also set                                         */
       /*                                                            */
       /**************************************************************/
       if (md.Report & MQRO_PASS_DISCARD_AND_EXPIRY)
       {
         if (md.Report & MQRO_DISCARD_MSG)
         {
           md.Report = MQRO_DISCARD_MSG;
         }
         else
         {
           md.Report = MQRO_NONE;         /*    stop further reports */
         }
       }
       else
       {
         md.Report = MQRO_NONE;           /*    stop further reports */
       }

       pmo.Options |= MQPMO_NO_SYNCPOINT; /*            no syncpoint */

       /**************************************************************/
       /*                                                            */
       /*   Put the message                                          */
       /*                                                            */
       /**************************************************************/
       MQPUT1(Hcon,            /* connection handle                  */
              &odR,            /* object descriptor                  */
              &md,             /* message descriptor                 */
              &pmo,            /* default options                    */
              messlen,         /* message length                     */
              buffer,          /* message buffer                     */
              &CompCode,       /* completion code                    */
              &Reason);        /* reason code                        */

       /* report reason if any  (loop ends if it failed)    */
       if (Reason != MQRC_NONE)
       {
         printf("MQPUT1 ended with reason code %ld\n", Reason);
       }
     }     /* end message for reply                */
   }     /* end Get message loop                 */

   /******************************************************************/
   /*                                                                */
   /*   Close server queue when no messages left                     */
   /*                                                                */
   /******************************************************************/
   C_options = 0;                   /* no close options              */
   MQCLOSE(Hcon,                    /* connection handle             */
           &Hobj,                   /* object handle                 */
           C_options,
           &CompCode,               /* completion code               */
           &Reason);                /* reason code                   */

   /* report reason, if any     */
   if (Reason != MQRC_NONE)
   {
     printf("MQCLOSE ended with reason code %ld\n", Reason);
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
   /* END OF AMQSECH4                                                */
   /*                                                                */
   /******************************************************************/
   printf("Sample AMQSECH4 end\n");
   return(0);
 }
