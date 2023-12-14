/* %Z% %W% %I% %E% %U% */
 /********************************************************************/
 /*                                                                  */
 /* Program name: AMQSSTMA                                           */
 /*                                                                  */
 /* Description: Sample C program that sets properties of a message  */
 /*              handle using MQSETMP and puts it to a message queue.*/
 /*   <copyright                                                     */
 /*   notice="lm-source-program"                                     */
 /*   pids="5724-H72,"                                               */
 /*   years="1994,2012"                                              */
 /*   crc="2107890334" >                                             */
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
 /********************************************************************/
 /*                                                                  */
 /* Function:                                                        */
 /*                                                                  */
 /*                                                                  */
 /*   AMQSSTMA is a sample C program to set properties of a message  */
 /*   handle and puts it to a message queue, and is an example of    */
 /*   the use of MQSETMP.                                            */
 /*                                                                  */
 /* Note: This program uses the message descriptor within the        */
 /*       message handle to set the Format of the message. To use    */
 /*       the message descriptor parameter of the MQPUT call see     */
 /*       lines marked @@@@.                                         */
 /*                                                                  */
 /********************************************************************/
 /*                                                                  */
 /*   AMQSSTMA has the following parameters                          */
 /*       required:                                                  */
 /*                 (1) The name of the target queue                 */
 /*       optional:                                                  */
 /*                 (2) Queue manager name                           */
 /*                 (3) The open options                             */
 /*                 (4) The close options                            */
 /*                 (5) The name of the target queue manager         */
 /*                 (6) The name of the dynamic queue                */
 /*                                                                  */
 /********************************************************************/
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
    /* includes for MQI */
 #include <cmqc.h>

 int main(int argc, char **argv)
 {
   /*  Declare file and character for sample input                   */
   FILE *fp;

   /*   Declare MQI structures needed                                */
   MQOD     od = {MQOD_DEFAULT};    /* Object Descriptor             */
   MQCMHO cmho = {MQCMHO_DEFAULT};  /* create message handle options */
   MQSMPO smpo = {MQSMPO_DEFAULT};  /* set message property options  */
   MQPD     pd = {MQPD_DEFAULT};    /* property descriptor           */
   MQCHARV  vs = {MQCHARV_DEFAULT}; /* variable length string        */
   MQMD     md = {MQMD_DEFAULT};    /* Message Descriptor            */
   MQPMO   pmo = {MQPMO_DEFAULT};   /* put message options           */
   MQDMHO dmho = {MQDMHO_DEFAULT};  /* delete message handle options */
      /** note, sample uses defaults where it can **/

   MQHCONN  Hcon;                   /* connection handle             */
   MQHOBJ   Hobj;                   /* object handle                 */
   MQHMSG   Hmsg;                   /* message handle                */
   MQLONG   O_options;              /* MQOPEN options                */
   MQLONG   C_options;              /* MQCLOSE options               */
   MQLONG   CompCode;               /* completion code               */
   MQLONG   OpenCode;               /* MQOPEN completion code        */
   MQLONG   CreateCode = MQCC_FAILED; /* MQCRTMH completion code     */
   MQLONG   Reason;                 /* reason code                   */
   MQLONG   CReason;                /* reason code for MQCONN        */
   char     name[1024];             /* property name                 */
   MQLONG   valuelen;               /* property value length         */
   char     value[1024];            /* property value                */
   MQLONG   messlen;                /* message length                */
   char     buffer[100];            /* message buffer                */
   char     QMName[50];             /* queue manager name            */

   printf("Sample AMQSSTMA start\n");
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
     printf("MQCONN ended with reason code %d\n", CReason);
     exit( (int)CReason );
   }

   /******************************************************************/
   /*                                                                */
   /*   Use parameter as the name of the target queue                */
   /*                                                                */
   /******************************************************************/
   strncpy(od.ObjectName, argv[1], (size_t)MQ_Q_NAME_LENGTH);
   printf("target queue is %s\n", od.ObjectName);

   if (argc > 5)
   {
     strncpy(od.ObjectQMgrName, argv[5], (size_t) MQ_Q_MGR_NAME_LENGTH);
     printf("target queue manager is %s\n", od.ObjectQMgrName);
   }

   if (argc > 6)
   {
     strncpy(od.DynamicQName, argv[6], (size_t) MQ_Q_NAME_LENGTH);
     printf("dynamic queue name is %s\n", od.DynamicQName);
   }

   /******************************************************************/
   /*                                                                */
   /*   Open the target message queue for output                     */
   /*                                                                */
   /******************************************************************/
   if (argc > 3)
   {
     O_options = atoi( argv[3] );
     printf("open  options are %d\n", O_options);
   }
   else
   {
     O_options = MQOO_OUTPUT            /* open queue for output     */
               | MQOO_FAIL_IF_QUIESCING /* but not if MQM stopping   */
               ;                        /* = 0x2010 = 8208 decimal   */
   }

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
   }

   /******************************************************************/
   /*                                                                */
   /*   Create a message handle to set properties on.                */
   /*                                                                */
   /******************************************************************/
   if(OpenCode != MQCC_FAILED)
   {
     cmho.Options = MQCMHO_VALIDATE; /* validate property name       */

     MQCRTMH(Hcon,                  /* connection handle             */
             &cmho,                 /* create message handle options */
             &Hmsg,                 /* message handle                */
             &CreateCode,           /* MQCRTMH completion code       */
             &Reason);              /* reason code                   */

     /* report reason, if any; stop if failed      */
     if (Reason != MQRC_NONE)
     {
       printf("MQCRTMH ended with reason code %d\n", Reason);
     }

     if (CreateCode == MQCC_FAILED)
     {
       printf("unable to create message handle\n");
     }

     /****************************************************************/
     /*                                                              */
     /*   Read lines from the file and set properties of the message */
     /*   handle accordingly.                                        */
     /*   Loop until null line or end of file, or there is a failure */
     /*                                                              */
     /****************************************************************/
     CompCode = CreateCode;   /* use MQCRTMH result for initial test */
     fp = stdin;

     while (CompCode != MQCC_FAILED)
     {
       vs.VSPtr     = name;
       vs.VSOffset  = 0;
       vs.VSBufSize = 0;
       vs.VSCCSID   = MQCCSI_APPL;

       printf("Enter property name\n");
       if (fgets(name, sizeof(name), fp) != NULL)
       {
         vs.VSLength = (MQLONG)strlen(name);  /* length without null */
         if (name[vs.VSLength-1] == '\n') /* last char is a new-line */
         {
           --vs.VSLength;              /* reduce name length         */
         }
       }
       else vs.VSLength = 0;  /* treat EOF same as null line         */

       if (vs.VSLength > 0)
       {
         printf("Enter property value\n");
         if (fgets(value, sizeof(value), fp) != NULL)
         {
           valuelen = (MQLONG)strlen(value);  /* length without null */
           if (value[valuelen-1] == '\n') /* last char is a new-line */
           {
             --valuelen;               /* reduce value length        */
           }
         }
         else valuelen = 0;

         MQSETMP(Hcon,            /* connection handle               */
                 Hmsg,            /* message handle                  */
                 &smpo,           /* set message property options    */
                 &vs,             /* property name                   */
                 &pd,             /* property descriptor             */
                 MQTYPE_STRING,   /* property data type              */
                 valuelen,        /* value length                    */
                 value,           /* value buffer                    */
                 &CompCode,       /* completion code                 */
                 &Reason);        /* reason code                     */

         /* report reason, if any */
         if (Reason != MQRC_NONE)
         {
           printf("MQSETMP ended with reason code %d\n", Reason);
           if (CompCode == MQCC_FAILED)
           {
             printf("Failed to set property\n");
           }
           CompCode = MQCC_OK;
         }
       }
       else    /* satisfy end condition when empty line is read */
         CompCode = MQCC_FAILED;
     }

     /****************************************************************/
     /*                                                              */
     /*   Put the message to the queue                               */
     /*                                                              */
     /****************************************************************/
     if(CreateCode != MQCC_FAILED)
     {
       pmo.Version = MQPMO_VERSION_3; /* message handles were added  */
                                      /* to version 3 of the pmo     */
       pmo.Options = MQPMO_NO_SYNCPOINT; /* No Syncpoint             */
       pmo.OriginalMsgHandle = Hmsg;  /* set the message properties  */

       /* Set the format in the message descriptor to MQFMT_STRING   */
       /* (character string format).                                 */

       vs.VSPtr     = "Root.MQMD.Format";
       vs.VSOffset  = 0;
       vs.VSBufSize = 0;
       vs.VSLength  = MQVS_NULL_TERMINATED;
       vs.VSCCSID   = MQCCSI_APPL;

       MQSETMP(Hcon,             /* connection handle                */
               Hmsg,             /* message handle                   */
               &smpo,            /* set message property options     */
               &vs,              /* property name                    */
               &pd,              /* property descriptor              */
               MQTYPE_STRING,    /* property data type               */
               MQ_FORMAT_LENGTH, /* value length                     */
               MQFMT_STRING,     /* value buffer                     */
               &CompCode,        /* completion code                  */
               &Reason);         /* reason code                      */

       /* report reason, if any */
       if (Reason != MQRC_NONE)
       {
         printf("MQSETMP ended with reason code %d\n", Reason);
         if (CompCode == MQCC_FAILED)
         {
           printf("Failed to set Root.MQMD.Format property\n");
         }
       }

       /* @@@@ Use the following line to set the Format field of the */
       /*      stack variable message descriptor. That variable will */
       /*      then need to be passed into the MQPUT call below.     */
       /* memcpy(md.Format, MQFMT_STRING, (size_t)MQ_FORMAT_LENGTH); */

       if(CompCode != MQCC_FAILED)
       {
         printf("Enter message text\n");
         if (fgets(buffer, sizeof(buffer), fp) != NULL)
         {
           messlen = (MQLONG)strlen(buffer); /* length without null    */
           if (buffer[messlen-1] == '\n') /* last char is a new-line   */
           {
             buffer[messlen-1]  = '\0';  /* replace new-line with null */
             --messlen;                  /* reduce buffer length       */
           }
         }
         else messlen = 0;           /* treat EOF same as null line    */

         MQPUT(Hcon,                  /* connection handle             */
               Hobj,                  /* object handle                 */
               NULL,                  /* message descriptor       @@@@ */
                                      /* change "NULL" to "&md" to use */
                                      /* the stack variable rather     */
                                      /* than the message handle's     */
                                      /* message descriptor            */
               &pmo,                  /* default options (datagram)    */
               messlen,               /* message length                */
               buffer,                /* message buffer                */
               &CompCode,             /* completion code               */
               &Reason);              /* reason code                   */

         /* report reason, if any */
         if (Reason != MQRC_NONE)
         {
           printf("MQPUT ended with reason code %d\n", Reason);
         }
       }
     }
   }

   /******************************************************************/
   /*                                                                */
   /*   Delete the message handle (if it was created). This releases */
   /*   the memory associated with the message handle.               */
   /*                                                                */
   /******************************************************************/
   if (CreateCode != MQCC_FAILED)
   {
     MQDLTMH(Hcon,                  /* connection handle             */
             &Hmsg,                 /* object handle                 */
             &dmho,                 /* delete message handle options */
             &CompCode,             /* completion code               */
             &Reason);              /* reason code                   */

     /* report reason, if any     */
     if (Reason != MQRC_NONE)
     {
       printf("MQDLTMH ended with reason code %d\n", Reason);
     }
   }

   /******************************************************************/
   /*                                                                */
   /*   Close the target queue (if it was opened)                    */
   /*                                                                */
   /******************************************************************/
   if (OpenCode != MQCC_FAILED)
   {
     if (argc > 4)
     {
       C_options = atoi( argv[4] );
       printf("close options are %d\n", C_options);
     }
     else
     {
       C_options = MQCO_NONE;        /* no close options             */
     }

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
       printf("MQDISC ended with reason code %d\n", Reason);
     }
   }

   /******************************************************************/
   /*                                                                */
   /* END OF AMQSSTMA                                                */
   /*                                                                */
   /******************************************************************/
   printf("Sample AMQSSTMA end\n");
   return(0);
 }
