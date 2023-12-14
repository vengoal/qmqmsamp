/* %Z% %W% %I% %E% %U% */
 /********************************************************************/
 /*                                                                  */
 /* Program name: AMQSPRM4                                           */
 /*                                                                  */
 /* Description: Sample C program that creates a reference message,  */
 /*              referring to a file,and puts it to a queue.         */
 /*                                                                  */
 /*   <copyright                                                     */
 /*   notice="lm-source-program"                                     */
 /*   pids="5724-H72,"                                               */
 /*   years="1994,2012"                                              */
 /*   crc="1399475352" >                                             */
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
 /*   AMQSPRM4 is a sample C program that creates a reference        */
 /*   message that refers to a file and puts it to a specified queue.*/
 /*   It also waits (for 15 seconds) for exception and confirmation  */
 /*   of arrival messages.                                           */
 /*                                                                  */
 /*      -- The destination file can have a name different to        */
 /*         that of the source file.                                 */
 /*                                                                  */
 /*      -- The CCSID and Encoding of the data in the file are       */
 /*         assumed to be the same as those of the local queue       */
 /*         manager.                                                 */
 /*                                                                  */
 /*      -- The Format of the data in the file is assumed to be      */
 /*         MQFMT_STRING.                                            */
 /*                                                                  */
 /*      -- The reference message is non-persistent.                 */
 /*                                                                  */
 /*    Program logic:                                                */
 /*         Parse and validate the input parameters.                 */
 /*         MQCONN to local queue manager                            */
 /*         Determine the queue manager coded char set id            */
 /*         MQOPEN a model queue                                     */
 /*         Create the reference message.                            */
 /*         MQPUT1 the reference message to target queue             */
 /*         while MQGET WAIT returns a message and COA message not   */
 /*          received                                                */
 /*            Display contents of exception or COA message          */
 /*         MQCLOSE model(temporary dynamic) queue                   */
 /*         MQDISC from queue manager                                */
 /*                                                                  */
 /********************************************************************/
 /*                                                                  */
 /* Details on the queues and channels required to run the reference */
 /* message samples can be found in the Application Programming      */
 /* Guide.                                                           */
 /********************************************************************/
 /*                                                                  */
 /*   amqsprm has the following parameters                           */
 /*                                                                  */
 /*      /m queue-mgr-name     name of local queue manager           */
 /*                            (optional).                           */
 /*                            Default is the default queue manager. */
 /*                                                                  */
 /*      /i source-file        fully qualified name of source file   */
 /*                            to be transferred. (required)         */
 /*                            The name is limited to 256 characters */
 /*                            but this can easily be changed.       */
 /*                                                                  */
 /*      /o target-file        fully qualified name of file on the   */
 /*                            destination systems (optional).       */
 /*                            The name is limited to 256 characters */
 /*                            but this can easily be changed.       */
 /*                            Default is the source file name.      */
 /*                                                                  */
 /*      /q queue-name         destination queue to which the        */
 /*                            reference message is put (required).  */
 /*                                                                  */
 /*      /g queue-mgr-name     queue manager on which queue, named   */
 /*                            in /q parameter, exists (optional).   */
 /*                            Defaults to the queue manager         */
 /*                            specified by the /m parameter or the  */
 /*                            default queue manager.                */
 /*                                                                  */
 /*      /t object-type        Object type. (required).              */
 /*                            Limited to 8 characters.              */
 /*                                                                  */
 /*      /w wait-interval      time (in seconds) to wait for         */
 /*                            exception and COA reports.            */
 /*                            Default is 15 seconds.                */
 /*                            Minimum value is 1.                   */
 /*                                                                  */
 /********************************************************************/
 #include <stdio.h>
 #include <stddef.h>
 #include <stdlib.h>
 #include <string.h>
 #include <ctype.h>
    /* includes for MQI */
 #include <cmqc.h>

 /********************************************************************/
 /* Constants                                                        */
 /********************************************************************/

 #define PRM_OPTIONS "g:i:m:o:q:t:w:"
 #define NULL_POINTER (void*)0
 #define COLON (':')
 #define BADCH ('?')
 #define TRUE  1
 #define FALSE 0
 #define MAX_FILENAME_LENGTH 256
 #define MAX_MQRMH_LENGTH    1000
 #define OK     0
 #define FAILED 1
 #define DEFAULT_WAIT_INTERVAL 15

 /********************************************************************/
 /* typedefs                                                         */
 /********************************************************************/
 typedef struct tagMQRMHX{
  MQRMH  ref;
  MQCHAR SrcName[MAX_FILENAME_LENGTH];
  MQCHAR DestName[MAX_FILENAME_LENGTH];
 } MQRMHX;

 /********************************************************************/
 /* Error and information messages                                   */
 /********************************************************************/

 #define STARTMSG              "AMQSPRM starting\n"
 #define ENDMSG                "AMQSPRM ending\n"
 #define UNRECOGNISEDPARMMSG   "Unrecognisable parameter %c\n"
 #define FILENAMETOOLONGMSG    "File name %s is too long\n"
 #define QUEUENAMETOOLONGMSG   "Queue name %s is too long\n"
 #define OBJECTTYPETOOLONGMSG  \
                 "Object type %s is too long. It has been truncated\n"
 #define QMGRNAMETOOLONGMSG    "Queue manager name %s is too long\n"
 #define DESTINATIONQUEUEMSG   "Destination queue is %s\n"
 #define DESTINATIONQMGRMSG    "Destination queue manager is %s\n"
 #define INVALIDWAITINTERVALMSG "Wait interval '%s' not valid\n"
 #define NOSOURCEFILEMSG       "No source file name specified\n"
 #define NODESTINATIONMSG      "No destination queue name specified\n"
 #define NOOBJECTTYPEMSG       "No object type specified\n"
 #define MQCONNREASONMSG       "MQCONN failed with reason %d\n"
 #define MQOPENREASONMSG       "MQOPEN failed with reason %d\n"
 #define MQINQREASONMSG        "MQINQ failed with reason %d\n"
 #define MQGETREASONMSG        "MQGET failed with reason %d\n"
 #define MQPUT1REASONMSG       "MQPUT1 failed with reason %d\n"
 #define MQCLOSEREASONMSG      "MQCLOSE failed with reason %d\n"
 #define MQDISCREASONMSG       "MQDISC failed with reason %d\n"
 #define SOURCEFILEMSG         "Source file is %s\n"
 #define DESTFILEMSG           "Destination file is %s\n"
 #define OBJECTTYPEMSG         "Object type is %.8s\n"
 #define WAITINTERVALMSG       "Wait interval is %d seconds\n"
 #define OPENMODELQFAILEDMSG   \
  "Open of model queue failed for reason %d, " \
  "So no exception or COA messages\n"
 #define MSGARRIVEDMSG   \
 "Reference message has arrived on destination queue\n"
 #define EXCEPTIONMSG   \
 "Reference message failed to arrive on destination queue\n"
 #define EXCEPTIONVALUESMSG   \
 "Feedback code = %#.8X. DataLogicalOffset = %d. DataLogicalLength = %d\n"
 #define NOQMGRCCSIDMSG   \
 "Unable to determine the queue manager coded char set id\n"
 #define TIMEOUTMSG   \
 "COA not received from destination queue manager within %d seconds\n"

 /********************************************************************/
 /* Function prototypes                                              */
 /********************************************************************/
 int prmGetOpt(int    argc
              ,char **argv
              ,char  *ostr
              );

 int prmGetQMgrCCSID (MQCHAR48  QMgrName
                     ,MQHCONN   HConn
                     ,PMQLONG   pQMgrCCSID
                     );

 /********************************************************************/
 /* Global variables                                                 */
 /********************************************************************/
 int prmind = 1;          /* index into argv vector                  */
 int prmopt;              /* character checked for validity          */
 char * prmarg;           /* argument associated with option         */

 int main(int argc, char **argv)
 {
   /*   Declare MQI structures needed                                */
   MQOD     od = {MQOD_DEFAULT};    /* Object Descriptor             */
   MQMD     md = {MQMD_DEFAULT};    /* Message Descriptor            */
   MQPMO    pmo = {MQPMO_DEFAULT};  /* put message options           */
   MQGMO    gmo = {MQGMO_DEFAULT};  /* get message options           */
   MQRMHX   refx = {{MQRMH_DEFAULT}}; /* reference message           */
      /** note, sample uses defaults where it can **/

   MQHCONN  Hcon;                   /* connection handle             */
   MQHOBJ   Hobj = MQHO_UNUSABLE_HOBJ;
                                    /* object handle                 */
   MQLONG   CompCode = MQCC_OK;     /* completion code               */
   MQLONG   OpenCompCode;           /* MQOPEN completion code        */
   MQLONG   ConnCode = MQCC_FAILED; /* MQCONN completion code        */
   int      rc;                     /* Return code                   */
   MQLONG   Reason;                 /* reason code                   */
   MQLONG   CReason;                /* reason code for MQCONN        */
   MQLONG   oo;                     /* MQOPEN options                */
   MQLONG   co  = MQCO_NONE;        /* MQCLOSE options               */
   MQLONG   DataLength;             /* Length of report message      */
   MQLONG   WaitInterval = DEFAULT_WAIT_INTERVAL;
                                    /* Wait interval                 */
   MQLONG   QMgrCCSID;              /* QMgr CodedCharSetId           */
   PMQRMH   pMQRMH = NULL_POINTER;  /* -> MQRMH structure            */
   PMQXQH   pMQXQH;                 /* -> MQXQH structure            */
   char     QMName[MQ_Q_MGR_NAME_LENGTH+1] = "";
                                    /* local queue manager name      */
   char     DestQName[MQ_Q_NAME_LENGTH+1];
                                    /* destination queue name        */
   char     DestQMName[MQ_Q_MGR_NAME_LENGTH+1];
                                    /* destination qmgr name         */
   char     SourceFileName[MAX_FILENAME_LENGTH+1];
                                    /* source file name              */
   char     TargetFileName[MAX_FILENAME_LENGTH+1];
                                    /* target file name              */
   char     ObjectType[sizeof(refx.ref.ObjectType)];
                                    /* Object type                   */
   char     Buffer[sizeof(MQXQH)+MAX_MQRMH_LENGTH];
                                    /* Buffer to receive exception   */
                                    /* and COA reports               */
   int      c;                      /* input option                  */
   MQLONG   ioption = FALSE;        /* /i option specified           */
   MQLONG   ooption = FALSE;        /* /o option specified           */
   MQLONG   qoption = FALSE;        /* /q option specified           */
   MQLONG   moption = FALSE;        /* /m option specified           */
   MQLONG   goption = FALSE;        /* /g option specified           */
   MQLONG   toption = FALSE;        /* /t option specified           */
   MQLONG   woption = FALSE;        /* /w option specified           */
   MQLONG   Arrived = FALSE;        /* Message has arrived at dest.  */

   /******************************************************************/
   /*                                                                */
   /* Initialisation                                                 */
   /*                                                                */
   /******************************************************************/

   printf(STARTMSG);

   /******************************************************************/
   /*                                                                */
   /*   Parse input parameters                                       */
   /*                                                                */
   /******************************************************************/
   while ((c = prmGetOpt(argc,argv,PRM_OPTIONS)) != EOF)
   {
     switch (c)
     {
       case 'g':
         if (strlen(prmarg) > MQ_Q_MGR_NAME_LENGTH)
         {
           printf(QMGRNAMETOOLONGMSG,prmarg);
           CompCode = MQCC_FAILED;
         }
         memset(DestQMName,0,sizeof(DestQMName));
         strncpy(DestQMName,prmarg
                ,(size_t)MQ_Q_MGR_NAME_LENGTH);
         printf(DESTINATIONQMGRMSG,prmarg);
         goption = TRUE;
         break;
       case 'i':
         if (strlen(prmarg) > MAX_FILENAME_LENGTH)
         {
           printf(FILENAMETOOLONGMSG,prmarg);
           CompCode = MQCC_FAILED;
         }
         memset(SourceFileName,0,sizeof(SourceFileName));
         strncpy(SourceFileName,prmarg,MAX_FILENAME_LENGTH);
         printf(SOURCEFILEMSG,prmarg);
         ioption = TRUE;
         break;
       case 'm':
         if (strlen(prmarg) > MQ_Q_MGR_NAME_LENGTH)
         {
           printf(QMGRNAMETOOLONGMSG,prmarg);
           CompCode = MQCC_FAILED;
         }
         memset(QMName,0,sizeof(QMName));
         strncpy(QMName,prmarg
                ,(size_t)MQ_Q_MGR_NAME_LENGTH);
         moption = TRUE;
         break;
       case 'q':
         if (strlen(prmarg) > MQ_Q_NAME_LENGTH)
         {
           printf(QUEUENAMETOOLONGMSG,prmarg);
           CompCode = MQCC_FAILED;
         }
         memset(DestQName,0,sizeof(DestQName));
         strncpy(DestQName,prmarg
                ,(size_t)MQ_Q_NAME_LENGTH);
         printf(DESTINATIONQUEUEMSG,prmarg);
         qoption = TRUE;
         break;
       case 'o':
         if (strlen(prmarg) > MAX_FILENAME_LENGTH)
         {
           printf(FILENAMETOOLONGMSG,prmarg);
           CompCode = MQCC_FAILED;
         }
         memset(TargetFileName,0,sizeof(TargetFileName));
         strncpy(TargetFileName,prmarg,MAX_FILENAME_LENGTH);
         printf(DESTFILEMSG,prmarg);
         ooption = TRUE;
         break;
       case 't':
         if (strlen(prmarg) > sizeof(ObjectType))
         {
           printf(OBJECTTYPETOOLONGMSG,prmarg);
         }
         memset(ObjectType,' ',sizeof(ObjectType));
         memcpy(ObjectType
               ,prmarg
               ,(strlen(prmarg) > sizeof(ObjectType))
                ? sizeof(ObjectType)
                : strlen(prmarg)
               );
         printf(OBJECTTYPEMSG,ObjectType);
         toption = TRUE;
         break;
       case 'w':
         WaitInterval = atol(prmarg);
         if (WaitInterval <= 0)
         {
           printf(INVALIDWAITINTERVALMSG,prmarg);
           CompCode = MQCC_FAILED;
         }
         woption = TRUE;
         break;
       default:
         printf(UNRECOGNISEDPARMMSG,prmopt);
         CompCode = MQCC_WARNING;
         break;
      }
   }

   if (!ioption)
   {
     /****************************************************************/
     /* Source filename (option /i) must be specified                */
     /****************************************************************/
     printf(NOSOURCEFILEMSG);
     CompCode = MQCC_FAILED;
   }

   if (!goption)
   {
     /****************************************************************/
     /* Destination queue manager name (option /g) not specified.    */
     /* Default to local queue manager.                              */
     /****************************************************************/
     memcpy(DestQMName,QMName,sizeof(QMName));
     printf(DESTINATIONQMGRMSG,DestQMName);
   }

   if (!ooption)
   {
     /****************************************************************/
     /* Destination filename (option /o) not specified.              */
     /* Default to source name.                                      */
     /****************************************************************/
     strncpy(TargetFileName,SourceFileName,strlen(SourceFileName));
     printf(DESTFILEMSG,TargetFileName);
   }
   if (!qoption)
   {
     /****************************************************************/
     /* Destination queue name (option /q) must be specified.        */
     /****************************************************************/
     printf(NODESTINATIONMSG);
     CompCode = MQCC_FAILED;
   }

   if (!toption)
   {
     /****************************************************************/
     /* Object type (option /t) must be specified                    */
     /****************************************************************/
     printf(NOOBJECTTYPEMSG);
     CompCode = MQCC_FAILED;
   }

   /******************************************************************/
   /* If no errors, display wait interval                            */
   /******************************************************************/
   if (CompCode != MQCC_FAILED)
   {
     printf(WAITINTERVALMSG,WaitInterval);
   }

   /******************************************************************/
   /*                                                                */
   /*   Connect to queue manager                                     */
   /*                                                                */
   /******************************************************************/
   if (CompCode != MQCC_FAILED)
   {
     MQCONN(QMName                 /* queue manager                  */
           ,&Hcon                  /* connection handle              */
           ,&ConnCode              /* completion code                */
           ,&CReason);             /* reason code                    */

     CompCode = ConnCode;

     if (CompCode == MQCC_FAILED)
     {
       printf(MQCONNREASONMSG, CReason);
     }
   }

   /******************************************************************/
   /* Get the queue manager CodedCharSetId (CCSID)                   */
   /******************************************************************/
   if (CompCode != MQCC_FAILED)
   {
     rc = prmGetQMgrCCSID(QMName
                         ,Hcon
                         ,&QMgrCCSID
                         );

     if (rc != 0)
     {
       printf(NOQMGRCCSIDMSG);
       CompCode = MQCC_FAILED;
     }
   }

   /******************************************************************/
   /* Open a model queue thus creating and opening a temporary       */
   /* dynamic queue.                                                 */
   /* This queue is used to receive report messages.                 */
   /******************************************************************/
   if (CompCode != MQCC_FAILED)
   {
     /****************************************************************/
     /* Set up the object descriptor.                                */
     /****************************************************************/
     strncpy(od.ObjectName
            ,"SYSTEM.DEFAULT.MODEL.QUEUE"
            ,(size_t)MQ_Q_NAME_LENGTH
            );

     oo = MQOO_FAIL_IF_QUIESCING +
          MQOO_INPUT_AS_Q_DEF;

     MQOPEN(Hcon                     /* connection handle            */
           ,&od                      /* object descriptor for queue  */
           ,oo                       /* options                      */
           ,&Hobj                    /* object handle                */
           ,&OpenCompCode            /* MQOPEN completion code       */
           ,&Reason);                /* reason code                  */


     if (Reason != MQRC_NONE)
     {
       printf(OPENMODELQFAILEDMSG, Reason);
     }
   }

   /******************************************************************/
   /*                                                                */
   /*   Build reference message.                                     */
   /*   Set Encoding and CodedCharSetId to defaults.                 */
   /*   Set Format to MQFMT_STRING.                                  */
   /*                                                                */
   /******************************************************************/
   if (CompCode != MQCC_FAILED)
   {
     refx.ref.StrucLength         = sizeof(refx);
     refx.ref.Encoding            = MQENC_NATIVE;
     refx.ref.CodedCharSetId      = QMgrCCSID;
     memcpy(refx.ref.Format,MQFMT_STRING
           ,(size_t)MQ_FORMAT_LENGTH);
     refx.ref.Flags               = MQRMHF_LAST;
     memcpy(refx.ref.ObjectType,ObjectType,
            sizeof(refx.ref.ObjectType));

     memset(refx.SrcName
           ,' '
           ,sizeof(refx.SrcName)+sizeof(refx.DestName));

     memcpy(refx.SrcName
           ,SourceFileName
           ,strlen(SourceFileName)
           );
     memcpy(refx.DestName
           ,TargetFileName
           ,strlen(TargetFileName)
           );

     refx.ref.SrcNameLength = strlen(SourceFileName);
     refx.ref.SrcNameOffset = offsetof(MQRMHX,SrcName);

     refx.ref.DestNameLength = strlen(TargetFileName);
     refx.ref.DestNameOffset = offsetof(MQRMHX,DestName);
   }

   /******************************************************************/
   /*                                                                */
   /*  Put the reference message to the destination queue.           */
   /*                                                                */
   /******************************************************************/
   if (CompCode != MQCC_FAILED)
   {
     /****************************************************************/
     /* Set up the object descriptor.                                */
     /****************************************************************/
     memcpy(md.ReplyToQ           /* ReplyToQ is the dynamic queue   */
           ,od.ObjectName
           ,sizeof(od.ObjectName)
           );
     strncpy(od.ObjectName
            ,DestQName
            ,sizeof(od.ObjectName)
            );
     strncpy(od.ObjectQMgrName
            ,DestQMName
            ,sizeof(od.ObjectQMgrName)
            );
     strncpy(md.ReplyToQMgr
            ,DestQMName
            ,sizeof(od.ObjectQMgrName)
            );

     pmo.Options = MQPMO_FAIL_IF_QUIESCING
                 | MQPMO_NO_SYNCPOINT;
     memcpy(md.Format,MQFMT_REF_MSG_HEADER
           ,(size_t)MQ_FORMAT_LENGTH);
     md.Report = MQRO_COA + MQRO_EXCEPTION;

     MQPUT1(Hcon,                    /* connection handle            */
            &od,                     /* object descriptor for queue  */
            &md,                     /* message descriptor           */
            &pmo,                    /* options                      */
            sizeof(refx),            /* buffer length                */
            &refx,                   /* buffer                       */
            &CompCode,               /* MQPUT1 completion code       */
            &Reason);                /* reason code                  */


     if (Reason != MQRC_NONE)
     {
        printf(MQPUT1REASONMSG, Reason);
     }
   }

   /******************************************************************/
   /*                                                                */
   /* Loop, getting messages from the reply to queue.                */
   /* End the loop if a COA report is received.                      */
   /*                                                                */
   /******************************************************************/
   if (CompCode     != MQCC_FAILED &&
       OpenCompCode != MQCC_FAILED)
   {
     gmo.Options = MQGMO_WAIT
                 | MQGMO_NO_SYNCPOINT
                 | MQGMO_ACCEPT_TRUNCATED_MSG;
                                       /* messages got may not be    */
                                       /* reference messages         */
     gmo.WaitInterval = WaitInterval * 1000;

     while(CompCode != MQCC_FAILED &&
           !Arrived)
     {
       memcpy(md.MsgId
             ,MQMI_NONE
             ,sizeof(md.MsgId)
             );

       memcpy(md.CorrelId
             ,MQCI_NONE
             ,sizeof(md.CorrelId)
             );

       MQGET(Hcon
            ,Hobj
            ,&md
            ,&gmo
            ,sizeof(Buffer)
            ,&Buffer
            ,&DataLength
            ,&CompCode
            ,&Reason
            );

       if (CompCode != MQCC_FAILED)
       {
         if (md.MsgType == MQMT_REPORT)
         {
           /*********************************************************/
           /* If the message has arrived at the destination then    */
           /* end the loop.                                         */
           /* Otherwise report the exception.                       */
           /*********************************************************/
           if (md.Feedback == MQFB_COA)
           {
             printf(MSGARRIVEDMSG);
             Arrived = TRUE;
           }
           else
           {
             printf(EXCEPTIONMSG);

             if (DataLength > 0)
             {
               /*****************************************************/
               /* Find the MQRMH structure.                         */
               /* It may be the first thing in the buffer but it    */
               /* may follow a transmission header                  */
               /*****************************************************/
               if (memcmp(md.Format
                         ,MQFMT_REF_MSG_HEADER
                         ,sizeof(md.Format)
                         ) == 0)
               {
                 pMQRMH = (PMQRMH)&Buffer;
               }
               else
               if (memcmp(md.Format
                         ,MQFMT_XMIT_Q_HEADER
                         ,sizeof(md.Format)
                         ) == 0)
               {
                 pMQXQH = (PMQXQH)&Buffer;
                 if (pMQXQH -> MsgDesc.Format == MQFMT_REF_MSG_HEADER)
                 {
                   pMQRMH = (PMQRMH)(pMQXQH + 1);
                 }
               }
             }

             /*******************************************************/
             /* Report some useful values                           */
             /*******************************************************/
             printf(EXCEPTIONVALUESMSG
                   ,md.Feedback
                   ,(pMQRMH == NULL_POINTER)
                    ? 0
                    : pMQRMH -> DataLogicalOffset
                   ,(pMQRMH == NULL_POINTER)
                    ? 0
                    : pMQRMH -> DataLogicalLength
                   );
           }
         }
       }
       else
       if (Reason == MQRC_NO_MSG_AVAILABLE)
       {
         printf(TIMEOUTMSG,WaitInterval);
       }
       else
       {
         printf(MQGETREASONMSG,Reason);
       }
     }
   }

   /******************************************************************/
   /* Close the queue (if open)                                      */
   /******************************************************************/
   if (Hobj != MQHO_UNUSABLE_HOBJ)
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
     MQDISC(&Hcon,                   /* connection handle            */
            &CompCode,               /* completion code              */
            &Reason);                /* reason code                  */


     if (Reason != MQRC_NONE)
     {
       printf(MQDISCREASONMSG, Reason);
     }
   }

   /******************************************************************/
   /*                                                                */
   /* END OF amqsprma                                                */
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
 /********************************************************************/
 /* prmGetOpt                                                        */
 /*------------------------------------------------------------------*/
 /* Returns option found in argv, or EOF or BADCH(invalid option).   */
 /* prmopt is the current option which is equal to the returned      */
 /*        value except when BADCH or EOF are returned.              */
 /* prmarg is the value of the current option or NULL if the option  */
 /*        has no value                                              */
 /* prmind is the current index into the argv vector                 */
 /********************************************************************/
 int prmGetOpt(int argc, char **argv, char *ostr)
 {
   static char *place = "";              /* option letter processing */
   char *oli = NULL_POINTER;             /* option letter list index */
   long   i  ;                           /* loop index               */
   int   found = 0;                      /* found flag               */


   if (!*place)
   {                                      /* update scanning pointer */
     place = argv[prmind];
     /****************************************************************/
     /* Check for end of input                                       */
     /****************************************************************/

     if ((prmind >= argc) ||
         ((*place != '-') && (*place != '/')) ||
         (!*++place))
     {
        return EOF;
     }
   }

   /******************************************************************/
   /* option letter okay?                                            */
   /******************************************************************/

   prmopt = (int)*place++;            /* Current option              */

   for (i=0; ((size_t)i < strlen(ostr)) && !found; i++)
   {                            /* check in option string for option */
     if ((prmopt == (int)ostr[i]) ||
         (prmopt == (int)toupper(ostr[i])))      /* case insensitive */
     {
       found = 1;                     /* option is present           */
       oli = &ostr[i];                /* set oli to point to it      */
     }
   } /* End check in option string for option */

   if (prmopt == (int)COLON || !oli)

   {                                  /* Current option not valid    */
      if (!*place)
      {
        ++prmind;
      }

      return BADCH;
   } /* End current option not valid */

   /******************************************************************/
   /* Check if this option requires an argument                      */
   /******************************************************************/

   if (*++oli != COLON)
   {                           /* this option doesn't require an arg */
     prmarg = NULL_POINTER;
     if (!*place)
     {
        ++prmind;
     }
   }
   else
   {                            /* this option needs an argument     */
     if (*place)
     {
        prmarg = place;            /* parm follows arg directly      */
        if (*prmarg == COLON)
          prmarg++;                /* Remove colon if present        */
     }
     else
     {
       if (argc <= ++prmind)
       {                          /* No argument found               */
         place = "";
         return BADCH;
       }
       else
       {
         prmarg = argv[prmind];   /* white space                     */
         if (*prmarg == COLON)
           prmarg++;              /* Remove colon if present         */
       }
     }

     place = "";
     ++prmind;
   } /* End this option needs an argument */

   prmopt = tolower(prmopt);

   return prmopt;                 /* return option letter            */
 }

/*********************************************************************/
/* prmGetQMgrCCSID                                                   */
/*-------------------------------------------------------------------*/
/* Open the queue manager object and issue MQINQ to get the CCSID.   */
/* Save it.                                                          */
/*********************************************************************/
int prmGetQMgrCCSID (MQCHAR48 QMgrName
                    ,MQHCONN  HConn
                    ,PMQLONG  pQMgrCCSID
                    )
{
  int rc = OK;               /* Function return code                 */
  MQLONG   CompCode;
  MQLONG   Reason;
  MQOD     ObjDesc = {MQOD_DEFAULT}; /* Object descriptor            */
  MQLONG   Selectors[1];
  MQLONG   flags;
  MQHOBJ   Hobj;

  /********************************************************************/
  /* Open the queue manager object                                    */
  /********************************************************************/
  ObjDesc.ObjectType = MQOT_Q_MGR;
  memcpy(ObjDesc.ObjectQMgrName
        ,QMgrName
        ,MQ_Q_MGR_NAME_LENGTH
        );
  flags = MQOO_INQUIRE;

  MQOPEN(HConn
        ,&ObjDesc
        ,flags
        ,&Hobj
        ,&CompCode
        ,&Reason
        );

  if (CompCode == MQCC_FAILED)
  {
    printf(MQOPENREASONMSG,Reason);
    rc = FAILED;
    goto MOD_EXIT;
  }

  /********************************************************************/
  /* Use MQINQ to get the queue manager CCSID                         */
  /********************************************************************/
  Selectors[0] = MQIA_CODED_CHAR_SET_ID;

  MQINQ(HConn
       ,Hobj
       ,1                    /* Number of selectors                   */
       ,Selectors
       ,1                    /* Number of integer selectors           */
       ,pQMgrCCSID
       ,0                    /* length of character attributes        */
       ,NULL_POINTER         /* character attributes                  */
       ,&CompCode
       ,&Reason
       );

  if (CompCode == MQCC_FAILED)
  {
    printf(MQINQREASONMSG, QMgrName, Reason);
    rc = FAILED;
    goto MOD_EXIT;
  }

  MOD_EXIT:

  if (Hobj != MQHO_UNUSABLE_HOBJ)
  {
    MQCLOSE(HConn
           ,&Hobj
           ,MQCO_NONE
           ,&CompCode
           ,&Reason
           );
  }

  return rc;
}


