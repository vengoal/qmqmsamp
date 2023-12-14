/* %Z% %W% %I% %E% %U% */
 /********************************************************************/
 /*                                                                  */
 /* Program name: AMQSGRM4                                           */
 /*                                                                  */
 /* Description: Sample C program that gets reference messages from  */
 /*              a specified queue.                                  */
 /*                                                                  */
 /*   <copyright                                                     */
 /*   notice="lm-source-program"                                     */
 /*   pids="5724-H72,"                                               */
 /*   years="1994,2012"                                              */
 /*   crc="173133819" >                                              */
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
 /*   AMQSGRM4 is a sample C program that gets reference messages    */
 /*   from a queue and checks that the objects, identified in the    */
 /*   messages, exist.                                               */
 /*   Non-reference messages are ignored and discarded.              */
 /*   It is assumed that the program has been invoked by a trigger   */
 /*   monitor and has been passed a trigger message.                 */
 /*                                                                  */
 /*    Program logic:                                                */
 /*         Extract the queue and queue manager names from the       */
 /*         input trigger message.                                   */
 /*         MQCONN to specified queue manager                        */
 /*         MQOPEN the specified queue                               */
 /*         While MQGET WAIT returns a message                       */
 /*           If the message is a reference message                  */
 /*             Check existence of object                            */
 /*         MQCLOSE the queue.                                       */
 /*         MQDISC from queue manager                                */
 /*                                                                  */
 /*   The reference message headers are assumed to be not longer     */
 /*   than 1000 bytes.                                               */
 /*   The fully qualified names of the objects, referred to in the   */
 /*   reference messages, are assumed to be not longer than 256      */
 /*   bytes.                                                         */
 /*                                                                  */
 /********************************************************************/
 /*                                                                  */
 /*   AMQSGRM4 has 1 parameter - a string (MQTMC2) based on the      */
 /*       initiation trigger message; only the queue and queue       */
 /*       manager name fields are used in this example               */
 /*                                                                  */
 /********************************************************************/
 #include <stdio.h>
 #include <stdlib.h>
 #include <stddef.h>
 #include <string.h>
 #include <ctype.h>

    /* includes for MQI */
 #include <cmqc.h>

 /********************************************************************/
 /* Constants                                                        */
 /********************************************************************/

 #define NULL_POINTER (void*)0
 #define MAX_FILENAME_LENGTH 256
 #define MAX_MQRMH_LENGTH 1000

 /********************************************************************/
 /* Error and information messages                                   */
 /********************************************************************/

 #define STARTMSG            "AMQSGRM starting\n"
 #define ENDMSG              "AMQSGRM ending\n"
 #define NOTRIGGERMSG        "No input trigger message provided\n"
 #define MQCONNREASONMSG     "MQCONN ended with reason %d\n"
 #define MQOPENREASONMSG     "MQOPEN of queue ended with reason %d\n"
 #define MQGETREASONMSG      "MQGET ended with reason %d\n"
 #define MQCLOSEREASONMSG    "MQCLOSE ended with reason %d\n"
 #define MQDISCREASONMSG     "MQDISC ended with reason %d\n"
 #define NOOBJECTMSG         "File %s of type %s could not be found\n"
 #define NOTREFMESSAGEMSG  \
                  "Message is not a reference message. Format is %s\n"
 #define QUEUEMSG            "Queue name is %.48s\n"
 #define QMGRMSG             "Queue manager name is %.48s\n"
 #define OBJECTMSG           "File %s of type %s does exist\n"

 /********************************************************************/
 /* Global variables                                                 */
 /********************************************************************/

 int main(int argc, char **argv)
 {
   /*   Declare MQI structures needed                                */
   MQOD     od = {MQOD_DEFAULT};    /* Object Descriptor             */
   MQMD     md = {MQMD_DEFAULT};    /* Message Descriptor            */
   MQGMO    gmo = {MQGMO_DEFAULT};  /* get message options           */

      /** note, sample uses defaults where it can **/

   MQHCONN  Hcon;                   /* connection handle             */
   MQHOBJ   Hobj;                   /* object handle                 */
   MQRMH   *pMQRMH;                 /* Pointer to MQRMH structure    */
   MQLONG   CompCode = MQCC_OK;     /* completion code               */
   MQLONG   ConnCode = MQCC_FAILED; /* MQCONN completion code        */
   MQLONG   co = MQCO_NONE;         /* MQCLOSE options               */
   MQLONG   Reason;                 /* reason code                   */
   MQLONG   CReason;                /* reason code for MQCONN        */
   MQLONG   OpenCode = MQCC_FAILED; /* MQOPEN completion code        */
   MQLONG   oo;                     /* MQOPEN options                */
   MQLONG   DataLen;                /* length of message             */
   MQLONG   WaitInterval = 15000;   /* wait interval of 15 seconds   */
   char     Filename[MAX_FILENAME_LENGTH];
                                    /* file referred to in reference */
   char     Buffer[MAX_MQRMH_LENGTH];
                                    /* MQGET buffer                  */
   MQTMC2  *Trig;                   /* trigger message structure     */
   FILE    *File;                   /* file structure                */
   char    *pObjectName;            /* Object name                   */
   char     ObjectType[sizeof(MQCHAR8)+1];
                                    /* Object type                   */

   printf(STARTMSG);

   /******************************************************************/
   /*                                                                */
   /*  Check that an input parameter has been provided               */
   /*                                                                */
   /******************************************************************/
   if (argc < 2)
   {
     printf(NOTRIGGERMSG);
     CompCode = MQCC_FAILED;
   }

   /******************************************************************/
   /*                                                                */
   /* Get pointer to input trigger message and extract the queue     */
   /* and queue manager names.                                       */
   /*                                                                */
   /******************************************************************/
   if (CompCode != MQCC_FAILED)
   {
     Trig = (MQTMC2*)argv[1];        /* -> trigger message           */
     printf(QMGRMSG,Trig->QMgrName);
     printf(QUEUEMSG,Trig->QName);
   }

   /******************************************************************/
   /*                                                                */
   /*   Connect to queue manager                                     */
   /*                                                                */
   /******************************************************************/
   if (CompCode != MQCC_FAILED)
   {
     MQCONN(Trig -> QMgrName       /* queue manager                  */
           ,&Hcon                  /* connection handle              */
           ,&ConnCode              /* completion code                */
           ,&CReason               /* reason code                    */
           );

     CompCode = ConnCode;

     if (CompCode == MQCC_FAILED)
     {
       printf(MQCONNREASONMSG, CReason);
     }
   }

   /******************************************************************/
   /*                                                                */
   /*  Open the specified queue                                      */
   /*                                                                */
   /******************************************************************/
   if (CompCode != MQCC_FAILED)
   {
     /****************************************************************/
     /* Set up the object descriptor.                                */
     /****************************************************************/
     strncpy(od.ObjectName
            ,Trig -> QName
            ,(size_t)MQ_Q_NAME_LENGTH
            );

     oo = MQOO_FAIL_IF_QUIESCING +
          MQOO_INPUT_AS_Q_DEF;

     MQOPEN(Hcon                     /* connection handle            */
           ,&od                      /* object descriptor for queue  */
           ,oo                       /* options                      */
           ,&Hobj                    /* object handle                */
           ,&OpenCode                /* MQOPEN completion code       */
           ,&Reason);                /* reason code                  */


     if (Reason != MQRC_NONE)
     {
        printf(MQOPENREASONMSG, Reason);
     }

     CompCode = OpenCode;
   }

   /******************************************************************/
   /*                                                                */
   /* Loop, getting messages from the queue.                         */
   /* If a message is a reference message, extract the object(file)  */
   /* name and check that the file exists.                           */
   /* Keep going until no message is found within the wait interval. */
   /*                                                                */
   /******************************************************************/
   if (CompCode != MQCC_FAILED)
   {
     gmo.Options = MQGMO_WAIT |
                   MQGMO_CONVERT |
                   MQGMO_NO_SYNCPOINT |
                   MQGMO_ACCEPT_TRUNCATED_MSG;
                                       /* messages got may not be    */
                                       /* reference messages         */
     gmo.WaitInterval = WaitInterval;

     while(CompCode != MQCC_FAILED)
     {
       memcpy(md.MsgId
             ,MQMI_NONE
             ,sizeof(md.MsgId)
             );

       memcpy(md.CorrelId
             ,MQCI_NONE
             ,sizeof(md.CorrelId)
             );

       /**************************************************************/
       /*                                                            */
       /*   MQGET sets Encoding and CodedCharSetId to the values in  */
       /*   the message returned, so these fields should be reset to */
       /*   the default values before every call, as MQGMO_CONVERT   */
       /*   is specified.                                            */
       /*                                                            */
       /**************************************************************/
       md.Encoding       = MQENC_NATIVE;
       md.CodedCharSetId = MQCCSI_Q_MGR;

       MQGET(Hcon
            ,Hobj
            ,&md
            ,&gmo
            ,sizeof(Buffer)
            ,&Buffer
            ,&DataLen
            ,&CompCode
            ,&Reason
            );

       if (CompCode != MQCC_FAILED)
       {
         if (memcmp(md.Format
                   ,MQFMT_REF_MSG_HEADER
                   ,(size_t)MQ_FORMAT_LENGTH
                   )
            )
         {
           /**********************************************************/
           /* Not a reference message                                */
           /**********************************************************/
           printf(NOTREFMESSAGEMSG,md.Format);
         }
         else
         {
           pMQRMH = (MQRMH*)&Buffer;/* overlay MQRMH on MQGET buffer */

           /**********************************************************/
           /* Extract fully qualified name from MQRMH structure.     */
           /**********************************************************/
           pObjectName = (char*)&Buffer + pMQRMH -> DestNameOffset;
           memset(Filename,0,sizeof(Filename));
           strncpy(Filename
                  ,pObjectName
                  ,((size_t)(pMQRMH->DestNameLength) >= sizeof(Filename))
                   ? (size_t)(sizeof(Filename) -1)
                   : (size_t)(pMQRMH -> DestNameLength)
                  );

           /**********************************************************/
           /* Extract object type from MQRMH structure.              */
           /**********************************************************/
           memset(ObjectType,0,sizeof(ObjectType));
           strncpy(ObjectType
                  ,pMQRMH -> ObjectType
                  ,sizeof(pMQRMH -> ObjectType)
                  );

           /**********************************************************/
           /* Check file exists.                                     */
           /**********************************************************/
           File = fopen(Filename,"r");

           if (File == NULL)
           {
             printf(NOOBJECTMSG,Filename,ObjectType);
           }
           else
           {
             printf(OBJECTMSG,Filename,ObjectType);
             fclose(File);
           }
         }
       }
       else
       if (Reason != MQRC_NO_MSG_AVAILABLE)
       {
         printf(MQGETREASONMSG,Reason);
       }
     }
   }

   /******************************************************************/
   /* Close the queue (if open)                                      */
   /******************************************************************/
   if (OpenCode != MQCC_FAILED)
   {
     MQCLOSE(Hcon
            ,&Hobj
            ,co
            ,&CompCode
            ,&Reason
            );

     if (Reason != MQRC_NONE)
     {
       printf(MQCLOSEREASONMSG,Reason);
     }
   }

   /******************************************************************/
   /*                                                                */
   /*   Disconnect from MQM if not already connected                 */
   /*                                                                */
   /******************************************************************/
   if (ConnCode != MQCC_FAILED &&
       CReason != MQRC_ALREADY_CONNECTED)
   {
     MQDISC(&Hcon                    /* connection handle            */
           ,&CompCode                /* completion code              */
           ,&Reason);                /* reason code                  */


     if (Reason != MQRC_NONE)
     {
       printf(MQDISCREASONMSG, Reason);
     }
   }

   /******************************************************************/
   /*                                                                */
   /* END OF AMQSGRM4                                               */
   /*                                                                */
   /******************************************************************/
   printf(ENDMSG);

   switch (CompCode)
   {
     case MQCC_OK :
       CompCode = 0;
       break;
     case MQCC_WARNING :
       CompCode = 10;
       break;
     case MQCC_FAILED :
       CompCode = 20;
       break;
     default :
       CompCode = 20;
   }
   return((int)CompCode);
 }


