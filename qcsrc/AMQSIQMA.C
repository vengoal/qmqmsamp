/* %Z% %W% %I% %E% %U% */
 /********************************************************************/
 /*                                                                  */
 /* Program name: AMQSIQMA                                           */
 /*                                                                  */
 /* Description: Sample C program that inquires properties of a      */
 /*              message handle, using MQINQMP, from a message queue.*/
 /*   <copyright                                                     */
 /*   notice="lm-source-program"                                     */
 /*   pids="5724-H72,"                                               */
 /*   years="1994,2012"                                              */
 /*   crc="3360170835" >                                             */
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
 /*   AMQSIQMA is a sample C program to inquire properties of a      */
 /*   message handle from a message queue, and is an example of the  */
 /*   use of MQINQMP.                                                */
 /*                                                                  */
 /********************************************************************/
 /*                                                                  */
 /*   AMQSIQMA has the following parameters                          */
 /*       required:                                                  */
 /*                 (1) The name of the source queue                 */
 /*       optional:                                                  */
 /*                 (2) Queue manager name                           */
 /*                 (3) The open options                             */
 /*                 (4) The close options                            */
 /*                                                                  */
 /********************************************************************/
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
    /* includes for MQI */
 #include <cmqc.h>

 int main(int argc, char **argv)
 {
   /*   Declare MQI structures needed                                */
   MQOD     od = {MQOD_DEFAULT};    /* Object Descriptor             */
   MQCMHO cmho = {MQCMHO_DEFAULT};  /* create message handle options */
   MQMD     md = {MQMD_DEFAULT};    /* Message Descriptor            */
   MQGMO   gmo = {MQGMO_DEFAULT};   /* get message options           */
   MQIMPO impo = {MQIMPO_DEFAULT};  /* inquire msg property options  */
   MQPD     pd = {MQPD_DEFAULT};    /* Property descriptor           */
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
   MQBYTE   buffer[65535];          /* message buffer                */
   MQLONG   messlen;                /* message length received       */
   MQLONG   datalen;                /* value length received         */
   MQLONG   type;                   /* value data type               */
   char     name[1024];             /* property name                 */
   char     value[1024];            /* property value                */
   char     QMName[50];             /* queue manager name            */
   MQCHARV  inqnames = {MQPROP_INQUIRE_ALL}; /* name of properties   */
                                             /* to inquire           */

   printf("Sample AMQSIQMA start\n");
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
   QMName[0] = 0;   /* default */
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
   /*   Open the named message queue for input; exclusive or shared  */
   /*   use of the queue is controlled by the queue definition here  */
   /*                                                                */
   /******************************************************************/

   if (argc > 3)
   {
     O_options = atoi( argv[3] );
     printf("open  options are %d\n", O_options);
   }
   else
   {
     O_options = MQOO_INPUT_AS_Q_DEF    /* open queue for input      */
               | MQOO_FAIL_IF_QUIESCING /* but not if MQM stopping   */
               ;                        /* = 0x2001 = 8193 decimal   */
   }


   /******************************************************************/
   /*                                                                */
   /*   Create object descriptor for subject queue                   */
   /*                                                                */
   /******************************************************************/
   strncpy(od.ObjectName, argv[1], MQ_Q_NAME_LENGTH);

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
   }

   /******************************************************************/
   /*                                                                */
   /*   Create a message handle to inquire properties on.            */
   /*                                                                */
   /******************************************************************/
   if (OpenCode != MQCC_FAILED)
   {
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
     /*   Get a message from the message queue                       */
     /*                                                              */
     /****************************************************************/
     if (CreateCode != MQCC_FAILED)
     {
       gmo.Version = MQGMO_VERSION_4; /* the message handle was added to */
                                      /* version 4 of the gmo structure  */
       gmo.MsgHandle = Hmsg;          /* get the message properties      */
       gmo.Options = MQGMO_WAIT       /* wait for new messages           */
                   | MQGMO_CONVERT    /* convert if necessary            */
                   | MQGMO_PROPERTIES_IN_HANDLE; /* properties in handle */
       gmo.WaitInterval = 15000;      /* 15 second limit for waiting     */

       MQGET(Hcon,                /* connection handle               */
             Hobj,                /* object handle                   */
             &md,                 /* message descriptor              */
             &gmo,                /* get message options             */
             sizeof(buffer)-1,    /* buffer length                   */
             buffer,              /* message buffer                  */
             &messlen,            /* message length                  */
             &CompCode,           /* completion code                 */
             &Reason);            /* reason code                     */

       /* report reason, if any     */
       if (Reason != MQRC_NONE)
       {
         if (Reason == MQRC_NO_MSG_AVAILABLE)
         {                       /* special report for normal end    */
           printf("no message available\n");
         }
         else                    /* general report for other reasons */
         {
           printf("MQGET ended with reason code %d\n", Reason);

           /*   treat truncated message as a failure for this sample */
           if (Reason == MQRC_TRUNCATED_MSG_FAILED)
           {
             CompCode = MQCC_FAILED;
           }
         }
       }

       /**************************************************************/
       /*   Display each property of the message received            */
       /**************************************************************/
       if (CompCode != MQCC_FAILED)
       {
         impo.Options = MQIMPO_INQ_NEXT
                                 /* iterate over the properties      */
                      | MQIMPO_CONVERT_TYPE
                                 /* convert type if necessary        */
                      | MQIMPO_CONVERT_VALUE;
                                 /* convert values into native CCSID */

         impo.ReturnedName.VSPtr = name; /* supply a buffer for the  */
                                         /* returned property name   */
         impo.ReturnedName.VSBufSize = sizeof(name)-1; /* supply the */
                                      /* returned name buffer length */

         type = MQTYPE_STRING;        /* request all properties as a */
                                      /* string                      */

         while (CompCode != MQCC_FAILED)
         {
           MQINQMP(Hcon,          /* connection handle               */
                   Hmsg,          /* message handle                  */
                   &impo,         /* inquire message property options*/
                   &inqnames,     /* iterate over all properties     */
                   &pd,           /* property descriptor             */
                   &type,         /* property data type              */
                   sizeof(value)-1, /* length of value buffer        */
                   value,         /* value buffer                    */
                   &datalen,      /* value length                    */
                   &CompCode,     /* completion code                 */
                   &Reason );     /* reason code                     */

           /* report reason, if any     */
           if (Reason != MQRC_NONE)
           {
             if (Reason != MQRC_PROPERTY_NOT_AVAILABLE)
             {
               printf("MQINQMP ended with reason code %d\n", Reason);
               CompCode = MQCC_OK;
             }
           }

           if(CompCode != MQCC_FAILED)
           {
             /* print the property name & value  */
             name[impo.ReturnedName.VSLength] = '\0';
             value[datalen] = '\0';
             printf("property name <%s> value <%s>\n", name, value);
           }
         }

         /* Display the message text */
         buffer[messlen] = '\0';
         printf("message text <%s>\n", buffer);
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
   /*   Close the source queue (if it was opened)                    */
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
       printf("MQDISC ended with reason code %d\n", Reason);
     }
   }

   /******************************************************************/
   /*                                                                */
   /* END OF AMQSIQMA                                                */
   /*                                                                */
   /******************************************************************/
   printf("Sample AMQSIQMA end\n");
   return(0);
 }
