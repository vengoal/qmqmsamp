/* %Z% %W% %I% %E% %U% */
 /********************************************************************/
 /*                                                                  */
 /* Program name: AMQSSET4                                           */
 /*                                                                  */
 /* Description: Sample C program using MQSET                        */
 /*                                                                  */
 /*   <copyright                                                     */
 /*   notice="lm-source-program"                                     */
 /*   pids="5724-H72,"                                               */
 /*   years="1994,2016"                                              */
 /*   crc="296461934" >                                              */
 /*   Licensed Materials - Property of IBM                           */
 /*                                                                  */
 /*   5724-H72,                                                      */
 /*                                                                  */
 /*   (C) Copyright IBM Corp. 1994, 2016 All Rights Reserved.        */
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
 /*   AMQSSET4 is a sample C program that inhibits PUTs to a         */
 /*   message queue, and is an example of MQSET.  It is intended     */
 /*   to run as a triggered program, and so receives input in a      */
 /*   trigger parameter.                                             */
 /*                                                                  */
 /*   Note: one way to run AMQSSET4 is to be running any             */
 /*         trigger monitor, and to use AMQSREQ* to send             */
 /*         request messages (containing queue names) to a           */
 /*         queue (eg SYSTEM.SAMPLE.SET) which must have been        */
 /*         defined to trigger AMQSSET4.                             */
 /*                                                                  */
 /*      -- gets request messages from a queue named in the          */
 /*         trigger message parameter, taking the content of each    */
 /*         message to be the name of a message queue                */
 /*                                                                  */
 /*      -- inhibits PUTs to the queue named in the request, and     */
 /*         sends a message to the named reply queue to confirm      */
 /*         it was done                                              */
 /*                                                                  */
 /*         alternatively, sends a report message if the queue       */
 /*         can't be inhibited - eg, if the queue to be inhibited    */
 /*         does not exist                                           */
 /*                                                                  */
 /*      -- stops when the input queue becomes empty                 */
 /*                                                                  */
 /*      -- writes a message for each MQI reason other than          */
 /*         MQRC_NONE; stops if there is a MQI completion code       */
 /*         of MQCC_FAILED                                           */
 /*                                                                  */
 /*                                                                  */
 /*   Program logic:                                                 */
 /*      MQCONNect to queue manager, specified in trigger message    */
 /*      MQOPEN message queue (A) for input                          */
 /*      while no MQI failures,                                      */
 /*      .  MQGET next message from queue A                          */
 /*      .  if request message received,                             */
 /*      .  .  OPEN queue (B) named in request for SET               */
 /*      .  .  MQSET, inhibit PUTs to queue B                        */
 /*      .  .  prepare reply message if PUT was inhibited            */
 /*      .  .  MQCLOSE queue B                                       */
 /*      .  .  prepare report message if reply not available         */
 /*      .  .  MQPUT1, send reply or report to named reply queue     */
 /*      MQCLOSE queue A                                             */
 /*      MQDISConnect from queue manager                             */
 /*                                                                  */
 /*                                                                  */
 /********************************************************************/
 /*                                                                  */
 /*   AMQSSET4 has 1 parameter - a string (MQTMC2) based on the      */
 /*          initiation trigger message; only the QName and queue    */
 /*          manager name fields are used in this example            */
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
   MQOD    odG = {MQOD_DEFAULT};    /* Object Descriptor for GET     */
   MQOD    odR = {MQOD_DEFAULT};    /* Object Descriptor for reply   */
   MQOD    odS = {MQOD_DEFAULT};    /* Object Descriptor for SET     */
   MQMD     md = {MQMD_DEFAULT};    /* Message Descriptor            */
   MQGMO   gmo = {MQGMO_DEFAULT};   /* get message options           */
   MQPMO   pmo = {MQPMO_DEFAULT};   /* put message options           */
      /** note, sample uses defaults where it can **/
   MQTMC2  *trig;                   /* trigger message structure     */

   MQHCONN  Hcon;                   /* connection handle             */
   MQHOBJ   Hobj;                   /* object handle, server queue   */
   MQHOBJ   Hset;                   /* handle for MQSET              */
   MQLONG   O_options;              /* MQOPEN options                */
   MQLONG   C_options;              /* MQCLOSE options               */
   MQLONG   CompCode;               /* completion code               */
   MQLONG   Reason;                 /* reason code                   */
   MQLONG   CReason;                /* reason codeon MQCONN          */
   char     buffer[101];            /* message buffer                */
   MQLONG   buflen;                 /* buffer length                 */
   MQLONG   messlen;                /* message length received       */
   MQLONG   Select[1];              /* attribute selector(s)         */
   MQLONG   IAV[1];                 /* integer attribute value(s)    */

   printf("Sample AMQSSET4 start\n");
   if (argc < 2)
   {
     printf("Missing parameter - start program by MQI trigger\n");
     exit(1);
   }

   /******************************************************************/
   /*                                                                */
   /*   Set the program argument into the trigger message            */
   /*                                                                */
   /******************************************************************/
   trig = (MQTMC2*) argv[1];                  /* -> trigger message  */
   if(strncmp(trig->StrucId, MQTMC_STRUC_ID, 2))
   {
     printf("Parameter supplied is not a trigger message\n");
     printf("This program must be started by MQI trigger\n");
     exit(2);
   }

   printf("%s\n", trig->QMgrName);

   /******************************************************************/
   /*                                                                */
   /*   This sample includes an explicit connect (MQCONN),           */
   /*   which isn't required on all MQ platforms, because            */
   /*   it's intended to be portable                                 */
   /*                                                                */
   /******************************************************************/
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
          trig->QName, MQ_Q_NAME_LENGTH);
   O_options = MQOO_INPUT_SHARED   /* open queue for shared input    */
         + MQOO_FAIL_IF_QUIESCING; /* but not if MQM stopping        */
   MQOPEN(Hcon,                    /* connection handle              */
          &odG,                    /* object descriptor for queue    */
          O_options,               /* open options                   */
          &Hobj,                   /* object handle                  */
          &CompCode,               /* completion code                */
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
                   | MQGMO_WAIT    /* wait for new messages          */
                   | MQGMO_NO_SYNCPOINT;   /* No syncpoint           */
     gmo.WaitInterval = 5000;      /* 5 second limit for waiting     */

     /****************************************************************/
     /*                                                              */
     /*   In order to read the messages in sequence, MsgId and       */
     /*   CorrelID must have the default value.  MQGET sets them     */
     /*   to the values in for message it returns, so re-initialise  */
     /*   them before every call                                     */
     /*                                                              */
     /****************************************************************/
     memcpy(md.MsgId, MQMI_NONE, sizeof(md.MsgId));
     memcpy(md.CorrelId, MQCI_NONE, sizeof(md.MsgId));

     MQGET(Hcon,                /* connection handle                 */
           Hobj,                /* object handle                     */
           &md,                 /* message descriptor                */
           &gmo,                /* GET options                       */
           buflen,              /* buffer length                     */
           buffer,              /* message buffer                    */
           &messlen,            /* message length                    */
           &CompCode,           /* completion code                   */
           &Reason);            /* reason code                       */

     /* report reason if any (loop ends if it failed)   */
     if (Reason != MQRC_NONE)
     {
       printf("MQGET ended with reason code %ld\n", Reason);
     }

     /****************************************************************/
     /*                                                              */
     /*    Only process REQUEST messages                             */
     /*                                                              */
     /****************************************************************/
     if (CompCode == MQCC_OK)
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
       /*   Open named queue for SET                                 */
       /*                                                            */
       /**************************************************************/
       strncpy(odS.ObjectName,     /* name of queue from message     */
               buffer, MQ_Q_NAME_LENGTH);
       O_options = MQOO_SET        /* open to set attributes         */
                         + MQOO_FAIL_IF_QUIESCING;
       MQOPEN(Hcon,                /* connection handle              */
              &odS,                /* object descriptor for queue    */
              O_options,           /* open options                   */
              &Hset,               /* object handle for MQSET        */
              &CompCode,           /* completion code                */
              &Reason);            /* reason code                    */

       /* prepare to report error if it failed */
       if (CompCode != MQCC_OK)
       {
         md.MsgType = MQMT_REPORT;  /* flag that report is needed    */
         md.Feedback = Reason;      /* use Reason as the cause       */
       }

       /**************************************************************/
       /*                                                            */
       /*   Inhibit Put if the queue was opened successfully         */
       /*                                                            */
       /**************************************************************/
       else
       {
         Select[0] = MQIA_INHIBIT_PUT;   /* attribute selector       */
         IAV[0]    = MQQA_PUT_INHIBITED; /* attribute value          */
         MQSET(Hcon,            /* connection handle                 */
               Hset,            /* object handle                     */
               1,               /* Selector count                    */
               Select,          /* Selector array                    */
               1,               /* integer attribute count           */
               IAV,             /* integer attribute array           */
               0,               /* character attribute count         */
               NULL,            /* character attribute array         */
               /*  note - can use NULL because count is zero         */
               &CompCode,       /* completion code                   */
               &Reason);        /* reason code                       */

         /************************************************************/
         /*                                                          */
         /*   Prepare reply if Inhibit (MQSET) worked, or            */
         /*   report if it failed                                    */
         /*                                                          */
         /************************************************************/
         if (CompCode == MQCC_OK)
         {
           strcat(buffer, " PUT inhibited");
           messlen  = strlen(buffer);             /* length of reply */
           md.MsgType = MQMT_REPLY;
         }
         else
         {
           md.MsgType = MQMT_REPORT;
           md.Feedback = Reason;                /* result of MQSET   */
         }

         /************************************************************/
         /*                                                          */
         /*   Close the queue opened for SET                         */
         /*   Result not checked in this example                     */
         /*                                                          */
         /************************************************************/
         C_options = 0;             /* no close options              */
         MQCLOSE(Hcon,              /* connection handle             */
                 &Hset,             /* object handle                 */
                 C_options,
                 &CompCode,         /* completion code               */
                 &Reason);          /* reason code                   */

       } /* end of sequence to inhibit PUT */

       /**************************************************************/
       /*                                                            */
       /*   If Inhibit (MQSET) was unsuccessful, prepare a report    */
       /*   message, suppressing data if not requested               */
       /*                                                            */
       /**************************************************************/
       if (md.MsgType & MQMT_REPORT)
       {                        /* if flagged as report needed ...   */
         if ((md.Report & MQRO_EXCEPTION_WITH_DATA)
                               != MQRO_EXCEPTION_WITH_DATA)
           messlen = 0;      /* suppress data if not wanted */
       }

       /**************************************************************/
       /*                                                            */
       /*   Send reply or report using MQPUT1                        */
       /*   (report is only to be sent if exceptions were requested) */
       /*                                                            */
       /**************************************************************/
       if ((md.MsgType == MQMT_REPLY)     /* if reply is available   */
          ||(md.Report & MQRO_EXCEPTION)) /* or exception requested  */
       {
         /************************************************************/
         /*                                                          */
         /*   Copy the ReplyTo queue name to the object descriptor   */
         /*                                                          */
         /************************************************************/
         strncpy(odR.ObjectName, md.ReplyToQ, MQ_Q_NAME_LENGTH);
         strncpy(odR.ObjectQMgrName,
                 md.ReplyToQMgr, MQ_Q_MGR_NAME_LENGTH);

         /************************************************************/
         /*                                                          */
         /*   MsgId and CorrelId are currently the values of the     */
         /*   got message.  Reset them if requested, then            */
         /*   stop further reports                                   */
         /*                                                          */
         /************************************************************/
         if ( !(md.Report & MQRO_PASS_CORREL_ID) )
         {
           memcpy(md.CorrelId, md.MsgId, sizeof(md.MsgId));
         }

         if ( !(md.Report & MQRO_PASS_MSG_ID) )
         {
           memcpy(md.MsgId, MQMI_NONE, sizeof(md.MsgId));
         }

         /************************************************************/
         /*                                                          */
         /* If MQRO_PASS_DISCARD_AND_EXPIRY was set in the original  */
         /* message, then reply or report should inherit             */
         /* MQRO_DISCARD_MSG if it was also set                      */
         /*                                                          */
         /************************************************************/
         if (md.Report & MQRO_PASS_DISCARD_AND_EXPIRY)
         {
           if (md.Report & MQRO_DISCARD_MSG)
           {
             md.Report = MQRO_DISCARD_MSG;
           }
           else
           {
             md.Report = MQRO_NONE;       /*    stop further reports */
           }
         }
         else
         {
           md.Report = MQRO_NONE;         /*    stop further reports */
         }

         pmo.Options |= MQPMO_NO_SYNCPOINT;  /* No syncpoint         */

         /************************************************************/
         /*                                                          */
         /*   Put the message                                        */
         /*                                                          */
         /************************************************************/
         MQPUT1(Hcon,            /* connection handle                */
                &odR,            /* object descriptor                */
                &md,             /* object descriptor for queue      */
                &pmo,            /* default options                  */
                messlen,         /* message length                   */
                buffer,          /* message buffer                   */
                &CompCode,       /* completion code                  */
                &Reason);        /* reason code                      */

         /* report reason if any  */
         if (Reason != MQRC_NONE)
         {
           printf("MQPUT1 ended with reason code %ld\n", Reason);
         }

       }  /* end sequence to send reply or report */
     }  /* end processing for message received */
   }  /* end while (CompCode == MQCC_OK) - message loop */

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
   /*   Disconnect from MQM (unless previously connected)            */
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
   /* END OF AMQSSET4                                                */
   /*                                                                */
   /******************************************************************/
   printf("Sample AMQSSET4 end\n");
   return(0);
 }
