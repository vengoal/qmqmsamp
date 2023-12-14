/* %Z% %W% %I% %E% %U% */
 /********************************************************************/
 /*                                                                  */
 /* Program name: AMQSERV4                                           */
 /*                                                                  */
 /* Description: Sample C program that acts as a trigger server      */
 /*                                                                  */
 /*   <copyright                                                     */
 /*   notice="lm-source-program"                                     */
 /*   pids="5724-H72,"                                               */
 /*   years="1993,2012"                                              */
 /*   crc="4280709147" >                                             */
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
 /*   AMQSERV4 is a sample C program that acts as a trigger          */
 /*   server - reads an initiation queue, then itself performs       */
 /*   the OS/400 program or CICS transaction associated with         */
 /*   each trigger message.                                          */
 /*                                                                  */
 /*      -- reads messages from a trigger queue named in the         */
 /*         parameter to the trigger server                          */
 /*                                                                  */
 /*      -- performs command for each valid trigger message          */
 /*                                                                  */
 /*         -- command runs process named in ApplId                  */
 /*            -- CALLs OS400 program                                */
 /*         -- parm = character version of trigger message           */
 /*         -- EnvData is not used by the trigger server             */
 /*                                                                  */
 /*      -- writes a message for each MQI reason other than          */
 /*         MQRC_NONE; stops if there is a MQI completion code       */
 /*         of MQCC_FAILED                                           */
 /*                                                                  */
 /*                                                                  */
 /*   Program logic:                                                 */
 /*      Use program parameter as the initiation queue name          */
 /*      MCONN to the queue manager                                  */
 /*      MQOPEN queue for INPUT                                      */
 /*      while no MQI failures,                                      */
 /*      .  MQGET next message, remove from queue                    */
 /*      .  invoke command based on trigger messages                 */
 /*      .  .  ApplId   is name of program to call                   */
 /*      .  .  MQTMC    is parameter                                 */
 /*      MQCLOSE the trigger queue                                   */
 /*                                                                  */
 /*                                                                  */
 /********************************************************************/
 /*                                                                  */
 /*                                                                  */
 /*                                                                  */
 /*   Exceptions signaled:  none                                     */
 /*   Exceptions monitored: none                                     */
 /*                                                                  */
 /*   AMQSERV4 has 2 parameters -                                    */
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
   int  i;  /* auxiliary counter */

 /*   Declare MQI structures needed                                */
   MQOD     od = {MQOD_DEFAULT};    /* Object Descriptor           */
   MQMD     md = {MQMD_DEFAULT};    /* Message Descriptor          */
   MQGMO   gmo = {MQGMO_DEFAULT};   /* get message options         */
      /** note, sample uses defaults where it can **/
   MQTMC2   trig;                   /* trigger message buffer      */

   MQHCONN  Hcon;                   /* connection handle           */
   MQHOBJ   Hobj;                   /* object handle               */
   MQLONG   O_options;              /* MQOPEN options              */
   MQLONG   C_options;              /* MQCLOSE options             */
   MQLONG   CompCode;               /* completion code             */
   MQLONG   OpenCode;               /* MQOPEN completion code      */
   MQLONG   Reason;                 /* reason code                 */
   MQLONG   CReason;                /* reason code for MQCONN      */
   MQLONG   buflen;                 /* buffer length               */
   MQLONG   triglen;                /* message length received     */
   char     QMName[MQ_Q_MGR_NAME_LENGTH+1]; /* queue manager name    */

   MQCHAR   command[1100];          /* call command string ...     */
   MQCHAR   p1[256];                /* ApplId insert               */
   MQCHAR   p2[800];                /* trigger insert      SA93506 */

 /********************************************************************/
 /*                                                                  */
 /*   Initialize object descriptor for subject queue                 */
 /*                                                                  */
 /********************************************************************/

   printf("Sample AMQSERV4 started using queue %s\n", argv[0]);
   if (argc < 2)
   {
     printf("Required parameter missing - initiation queue name\n");
     exit(99);     /* stop if no parameter */
   }

   strncpy(od.ObjectName, argv[1], MQ_Q_NAME_LENGTH);
   memset(QMName, 0, MQ_Q_MGR_NAME_LENGTH+1); /* default(space padded) */
   if (argc > 2)
     {
     strncpy(QMName, argv[2], MQ_Q_MGR_NAME_LENGTH);
     printf("Sample AMQSERV4 started for Queue Manager %s\n", QMName);
     printf("and using queue %s\n", argv[1]);
   }
   else
   {
     printf("Sample AMQSERV4 started (for the default Queue Manager)\n");
     printf("and using queue %s\n", argv[1]);
   }

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
  /*  Open specified initiation queue for input; exclusive or       */
  /*  shared use of the queue is controlled by the queue definition */
  /*                                                                */
  /******************************************************************/
   O_options = MQOO_INPUT_AS_Q_DEF /* open queue for input         */
         + MQOO_FAIL_IF_QUIESCING; /* but not if MQM stopping      */
   MQOPEN(Hcon,                    /* connection handle            */
          &od,                     /* object descriptor for queue  */
          O_options,               /* open options                 */
          &Hobj,                   /* object handle                */
          &CompCode,               /* completion code              */
          &Reason);                /* reason code                  */

   /* report reason, if any; stop if failed      */
    if (Reason != MQRC_NONE)
    {
      printf("MQOPEN (%s) ==> %ld\n", od.ObjectName, Reason);
    }

    OpenCode = CompCode;               /* keep for conditional close */

   /******************************************************************/
   /*                                                                */
   /*   Get messages from the message queue                          */
   /*   Loop until there is a failure, ask to fail if the queue      */
   /*   manager is quiescing                                         */
   /*                                                                */
   /******************************************************************/
   buflen = sizeof(MQTM);       /* size of all MQTM trigger messages */
   gmo.Version = MQGMO_VERSION_2;     /* Avoid need to reset Message */
   gmo.MatchOptions = MQMO_NONE;      /* ID and Correlation ID after */
                                      /* every MQGET                 */
   gmo.Options = MQGMO_WAIT             /* wait for new messages ... */
      | MQGMO_FAIL_IF_QUIESCING         /* or until MQM stopping     */
      | MQGMO_ACCEPT_TRUNCATED_MSG      /* remove all long messages  */
      | MQGMO_NO_SYNCPOINT;             /* No syncpoint              */
  gmo.WaitInterval = MQWI_UNLIMITED;    /* no time limit             */

  while (CompCode != MQCC_FAILED)
  {
    /****************************************************************/
    /*                                                              */
    /*   Wait for a trigger                                         */
    /*                                                              */
    /****************************************************************/
     printf("...>\n");
     MQGET(Hcon,                 /* connection handle            */
            Hobj,                /* object handle                */
            &md,                 /* message descriptor           */
            &gmo,                /* get message options          */
            buflen,              /* buffer length                */
            &trig,               /* trigger message buffer       */
            &triglen,            /* message length               */
            &CompCode,           /* completion code              */
            &Reason);            /* reason code                  */

   /* report reason, if any     */
     if (Reason != MQRC_NONE)
     {
       printf("MQGET ==> %ld\n", Reason);
     }

    /****************************************************************/
    /*                                                              */
    /*   Process each message received                              */
    /*                                                              */
    /****************************************************************/
     if (CompCode != MQCC_FAILED)
     {
       if (triglen != buflen)
         printf("DataLength = %ld?\n", triglen);
       else
       {
        /************************************************************/
        /*                                                          */
        /*   Copy appropriate parts of trigger message into the     */
        /*   call command string. Increase size of triglen to allow */
        /*   QMgrName to be passed using MQTMC2 structure.          */
        /*                                                          */
        /************************************************************/
         triglen += MQ_Q_MGR_NAME_LENGTH;              /* SA93506 */
         memcpy(p1, trig.ApplId, sizeof(trig.ApplId));
         memcpy(&trig.Version,  "   2", 4);   /* replace integers */
         memcpy(&trig.ApplType, "    ", 4);
         memcpy(&trig.QMgrName, QMName, MQ_Q_MGR_NAME_LENGTH);
         memcpy(p2, &trig, triglen);     /* copy modified trigger */
         p2[triglen] = '\0';

        /* strip trailing blanks */
         for (i=sizeof(trig.ApplId)-1; i>=0; i--)
           if (p1[i] != ' ')
             break;
         p1[i+1] = '\0';


        /************************************************************/
        /*                                                          */
        /*   Call Program                                           */
        /*                                                          */
        /************************************************************/
         sprintf(command,
                 "QSYS/CALL PGM(%s) PARM('%s')", p1, p2);
         /* A CICS program could be called by replacing this line */
         /* with :                                                */
         /* sprintf(command, "STRCICSUSR TRANID(%s) DATA('%s')",  */
         /*    p1, p2);                                           */
         printf("%s;\n", command);
         system(command);
       }   /* end trigger processing         */
     }     /* end process for successful GET */
   }       /* end message processing loop    */

  /******************************************************************/
  /*                                                                */
  /*   Close the initiation queue - if it was opened                */
  /*                                                                */
  /******************************************************************/
   if (OpenCode != MQCC_FAILED)
   {
     C_options = 0;                /* no close options             */
     MQCLOSE(Hcon,                 /* connection handle            */
            &Hobj,                 /* object handle                */
            C_options,
            &CompCode,             /* completion code              */
            &Reason);              /* reason code                  */

     /* report reason, if any     */
     if (Reason != MQRC_NONE)
     {
       printf("MQCLOSE ==> %ld\n", Reason);
     }
   }

   /******************************************************************/
   /*                                                                */
   /*   Disconnect from MQM  (unless previously connected)           */
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
  /* END OF AMQSERV4                                                */
  /*                                                                */
  /******************************************************************/
  printf("Sample AMQSERV4 end\n");
  return(0);
 }
