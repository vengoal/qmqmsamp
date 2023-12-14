/* %Z% %W% %I% %E% %U% */
 /********************************************************************/
 /*                                                                  */
 /* Program name: AMQSXRM4                                           */
 /*                                                                  */
 /* Description: Sample C channel message exit program that          */
 /*              processes reference messages.                       */
 /*                                                                  */
 /*   <copyright                                                     */
 /*   notice="lm-source-program"                                     */
 /*   pids="5724-H72"                                                */
 /*   years="1994,2014"                                              */
 /*   crc="3666749488" >                                             */
 /*   Licensed Materials - Property of IBM                           */
 /*                                                                  */
 /*   5724-H72                                                       */
 /*                                                                  */
 /*   (C) Copyright IBM Corp. 1994, 2014 All Rights Reserved.        */
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
 /*   AMQSXRM4 is a sample C program that is intended to be used as  */
 /*   a channel message exit on the AS/400, the sample C program     */
 /*   AMQSXRMA may be used on the other MQ platforms                 */
 /*                                                                  */
 /*   It recognises reference messages with an object type that      */
 /*   matches the object type in the message exit user data field of */
 /*   the channel definition.                                        */
 /*                                                                  */
 /*   For these messages it does the following (other messages are   */
 /*   ignored) -                                                     */
 /*                                                                  */
 /*     Sender and Server channels  -                                */
 /*       The specified length of data is copied from the specified  */
 /*       offset of the specified file into the space remaining in   */
 /*       the agent buffer after the reference message.              */
 /*       If the end of the file is not reached the reference        */
 /*       message is put back on the transmission queue after        */
 /*       updating the DataLogicalOffset field.                      */
 /*                                                                  */
 /*     Requestor and Receiver channels  -                           */
 /*       If the DataLogicalOffset field is zero and the specified   */
 /*       file does not exist, it is created.                        */
 /*       The data following the reference message is added to the   */
 /*       end of the specified file.                                 */
 /*       If the reference message is not the last one for the       */
 /*       specified file, the reference message is discarded.        */
 /*       Otherwise, the reference message, without the appended     */
 /*       data, is returned to the channel program and will be put   */
 /*       to the target queue.                                       */
 /*                                                                  */
 /*   For Sender and Server channels, if the DataLogicalLength field */
 /*   in the input reference message is zero it is assumed that the  */
 /*   remaining part of the file, from DataLogicalOffset to the end  */
 /*   of the file, is to be sent along the channel.                  */
 /*   If it is non-zero, this indicates that only the length         */
 /*   specified is to be sent.                                       */
 /*                                                                  */
 /*   If an error occurs (e.g. unable to open a file),               */
 /*   MQCXP.ExitResponse is set to MQXCC_SUPPRESS_FUNCTION so that   */
 /*   the message being processed is put to the dead letter queue    */
 /*   instead of continuing to the destination queue.                */
 /*   A feedback code is returned in MQCXP.Feedback which will be    */
 /*   returned to the application that put the message, in the       */
 /*   MQMD.Feedback field, if the putting application requested      */
 /*   exception reports by setting MQRO_EXCEPTION in the             */
 /*   MQMD.Report field.                                             */
 /*                                                                  */
 /*   If the encoding or CodedCharacterSetId (CCSID) of the          */
 /*   reference message is different to that of the queue manager,   */
 /*   the reference message is converted to the local encoding and   */
 /*   CCSID.                                                         */
 /*   If the format of the object data is MQFMT_STRING, the object   */
 /*   data is converted to the local CCSID at the receiving end      */
 /*   before the data is written to the file.                        */
 /*                                                                  */
 /*   Program logic:                                                 */
 /*       Access the input parameters.                               */
 /*       Convert the reference message to the local encoding and    */
 /*        CCSID                                                     */
 /*       Validate the fields in the reference message               */
 /*       Extract the object type from the message exit user data    */
 /*       field in the channel definition.                           */
 /*       If the message is a reference message and                  */
 /*          the object type matches the one from the MQCD           */
 /*         If the channel is a Sender or Server                     */
 /*           Check that the reference message is not followed by    */
 /*            data.                                                 */
 /*           Replace the input reference message with the converted */
 /*            one.                                                  */
 /*           Set the read length to the minimum of                  */
 /*            MQRMH.DataLogicalLength and the space remaining in    */
 /*            the agent buffer.                                     */
 /*           Copy data from specified offset of the specified file  */
 /*           into the space remaining at the end of the agent       */
 /*           buffer (up to the MaxMsgLength value for the channel). */
 /*           Update the DataLength parameter.                       */
 /*           If the end of the file has been reached or             */
 /*            the requested length has been read                    */
 /*             Set the MQRMHF_LAST flag in the reference message    */
 /*           Else                                                   */
 /*             Reset the MQRMHF_LAST flag in the message.           */
 /*             Update the DataLogicalOffset field in the message.   */
 /*             MQPUT1 the message to the transmission queue.        */
 /*         Else if the channel is a Receiver or Requestor           */
 /*           Convert the object data, appended to the reference     */
 /*            message, to the local encoding and CCSID.             */
 /*           If DataLogicalOffset is zero and                       */
 /*              the specified file does not exist                   */
 /*             Create the file.                                     */
 /*           If DataLogicalOffset matches the current size of the   */
 /*           file                                                   */
 /*             Add the data to the end of the file.                 */
 /*           If the MQRMHF_LAST flag is set                         */
 /*             Replace the input reference message with the         */
 /*              converted one.                                      */
 /*             Decrement the DataLength parameter by the length of  */
 /*             the data.                                            */
 /*             Set DataLogicalOffset field in the MQRMH structure   */
 /*              to zero.                                            */
 /*             Set the MQRMH.DataLogicalLength to the size of the   */
 /*              file.                                               */
 /*           Else                                                   */
 /*             Set ExitResponse to MQXCC_SUPPRESS_FUNCTION          */
 /*             Set the MQMD.Report field to MQRO_DISCARD_MSG        */
 /*             Turn off the exception request flag in MQMD.Report   */
 /*                                                                  */
 /*   The fully qualified names of the objects, referred to in the   */
 /*   reference messages, are assumed to be not longer than 256      */
 /*   bytes.                                                         */
 /*                                                                  */
 /********************************************************************/
 /*                                                                  */
 /*   AMQSXRM4 has the usual parameters defined for channel exit     */
 /*       programs.                                                  */
 /*                                                                  */
 /********************************************************************/
 /*                                                                  */
 /*   If a function call within AMQSXRM4 fails, a feedback code is   */
 /*   set to indicate the error. This is in addition to executing    */
 /*   a printf to output an error message.                           */
 /*   The feedback code will be used as the Reason in the DLH header */
 /*   when the reference message is put to the DeadLetterQueue by    */
 /*   the channel.                                                   */
 /*   Also if the application that put the message to the remote     */
 /*   queue requested an exception report (AMQSPRM does), the        */
 /*   feedback code is returned in the exception report.             */
 /*   The feedback codes take the following form                     */
 /*     0x00ffeeee                                                   */
 /*   where ff is one of the XRMA_FB_xxx constants defined below and */
 /*   identifies the error e.g. fopen failed.                        */
 /*   eeee identifies the reason for the failure. This will be the   */
 /*   value of Reason in the case of MQ operations e.g. MQOPEN, or   */
 /*   the value of errno in the case of C functions like malloc and  */
 /*   fopen.                                                         */
 /*                                                                  */
 /********************************************************************/
 #include <errno.h>
 #include <stdio.h>
 #include <stdlib.h>
 #include <stddef.h>
 #include <string.h>
 #include <ctype.h>

    /* includes for MQI */
 #include <cmqc.h>
 #include <cmqxc.h>
 #include <cmqec.h>
 #include <amqsvmha.h>    /* Conversion macros                       */


 /********************************************************************/
 /* Typedefs.                                                        */
 /* The ExitUserArea within MQCXP is used to save the following      */
 /* values.                                                          */
 /********************************************************************/
 typedef struct tagSAVEDATA
 {
   MQHCONN HConn;       /* Connection handle       */
   MQLONG  ConnReason;  /* Reason code from MQCONN */
   MQLONG  QMgrCCSID;   /* Queue manager CCSID     */
 } SAVEDATA, *PSAVEDATA;

 /********************************************************************/
 /* Function prototypes                                              */
 /********************************************************************/
 int ConvertMQRMH  (PMQCXP    pExitParms
                   ,PSAVEDATA pSaveData
                   ,PMQCD     pChannelDef
                   ,PMQXQH    pMQXQH
                   ,PMQRMH    pMQRMH
                   ,PMQRMH   *ppConvertedMQRMH
                   ,PMQLONG   pConvertedMQRMHLen
                   ,PMQLONG   pOrigMQRMHLength
                   ,PMQLONG   pFeedbackCode
                   );

 int ConvertBulkData (PMQCXP    pExitParms
                     ,PSAVEDATA pSaveData
                     ,PMQCD     pChannelDef
                     ,PMQRMH    pMQRMH
                     ,PMQCHAR   pBulkData
                     ,PMQCHAR  *ppConvertedBulkData
                     ,MQLONG    BulkDataLen
                     ,PMQLONG   pConvertedbulkDataLen
                     ,PMQLONG   pFeedbackCode
                     );

 int GetQMgrCCSID (PMQCXP    pExitParms
                  ,PSAVEDATA pSaveData
                  ,PMQCD     pChannelDef
                  ,PMQLONG   pFeedbackCode
                  );

 MQLONG ConverttagMQRMH(PMQCXP  pExitParms,
                        PMQBYTE *in_cursor,
                        PMQBYTE *out_cursor,
                        PMQBYTE in_lastbyte,
                        PMQBYTE out_lastbyte,
                        MQHCONN hConn,
                        MQLONG  opts,
                        MQLONG  MsgEncoding,
                        MQLONG  ReqEncoding,
                        MQLONG  MsgCCSID,
                        MQLONG  ReqCCSID,
                        MQLONG  CompCode,
                        MQLONG  Reason,
                        PMQLONG pOrigMQRMHLength);

 /********************************************************************/
 /* Constants                                                        */
 /********************************************************************/
 #define MAX_FILENAME_LENGTH 256
 #define NULL_POINTER        (void*)0
 #define OK                  0
 #define FAILED              1
 #define MAX_MQRMH_LENGTH    1000

 /********************************************************************/
 /* Feedback codes                                                   */
 /********************************************************************/

 #define XRMA_FB_INVALIDEXITID       0x00110000
 #define XRMA_FB_INVALIDEXITREASON   0x00120000
 #define XRMA_FB_FOPENFILEFAILED     0x00130000
 #define XRMA_FB_FSEEKFILEFAILED     0x00140000
 #define XRMA_FB_FTELLFILEFAILED     0x00150000
 #define XRMA_FB_FREADFILEFAILED     0x00160000
 #define XRMA_FB_FWRITEFILEFAILED    0x00170000
 #define XRMA_FB_MQCONNFAILED        0x00180000
 #define XRMA_FB_MQOPENFAILED        0x00190000
 #define XRMA_FB_MQPUT1FAILED        0x001A0000
 #define XRMA_FB_MQGETFAILED         0x001B0000
 #define XRMA_FB_MQCLOSEFAILED       0x001C0000
 #define XRMA_FB_INVALIDFILEOFFSET   0x001D0000
 #define XRMA_FB_INVALIDCHANNELTYPE  0x001E0000
 #define XRMA_FB_INVALIDSTRUCLENGTH  0x001F0000
 #define XRMA_FB_MALLOCFAILED        0x00200000
 #define XRMA_FB_CONVERTFAILED       0x00210000
 #define XRMA_FB_INVALIDDATALENGTH   0x00220000

 /********************************************************************/
 /* Error and information messages                                   */
 /********************************************************************/
 #define STARTMSG             "AMQSXRM starting\n"
 #define ENDMSG               "AMQSXRM ending\n"
 #define REASONMSG            "Reason for calling exit is %s\n"
 #define INVALIDREASONMSG     "Reason %s is not valid for this exit\n"
 #define INVALIDEXITIDMSG     "Exitid %s is not valid for this exit\n"
 #define INVALIDMSGMSG      \
        "Message format is '%.8s'. Expecting '%.8s'. Message ignored\n"
 #define INVALIDOBJTYPEMSG  \
        "Object type is %s. Expecting %.8s. Message ignored\n"
 #define INVALIDOFFSETMSG   \
    "Data offset %d does not match file size %d for file %s."\
    " Message ignored\n"
 #define SRCENVERRORMSG   \
    "SrcEnv field not contained within MQRMH structure. "\
    "SrcEnvLength = %d. SrcEnvOffset = %d. StrucLength = %d."\
    " Message ignored\n"
 #define SRCNAMEERRORMSG   \
    "SrcName field not contained within MQRMH structure. "\
    "SrcNameLength = %d. SrcNameOffset = %d. StrucLength = %d."\
    " Message ignored\n"
 #define DESTENVERRORMSG   \
    "DestEnv field not contained within MQRMH structure. "\
    "DestEnvLength = %d. DestEnvOffset = %d. StrucLength = %d."\
    " Message ignored\n"
 #define DESTNAMEERRORMSG   \
    "DestName field not contained within MQRMH structure. "\
    "DestNameLength = %d. DestNameOffset = %d. StrucLength = %d."\
    " Message ignored\n"
 #define STRUCLENERRORMSG   \
    "Message does not contain a complete MQRMH structure. "\
    "StrucLength = %d. Message length = %d."\
    " Message ignored\n"
 #define DATALENERRORMSG   \
    "Message at Sender end contains data following the MQRMH structure. "\
    "Total length of headers = %d. Message length = %d."\
    " Message ignored\n"
 #define INVALIDCHLTYPEMSG    "Channel type is %d. Message ignored\n"
 #define UNKNOWNFILEMSG       "Cannot find file %s\n"
 #define CREATEFILEMSG        "File %s being created\n"
 #define UPDATEFILEMSG        "File %s being updated\n"
 #define READFILEMSG          "File %s being read\n"
 #define FILECOMPLETEMSG      "File %s is complete\n"
 #define EOFMSG               "End of file %s reached\n"
 #define OPENFAILEDMSG        "fopen to file %s failed. "
 #define SEEKFAILEDMSG        "fseek to file %s failed. "
 #define READFAILEDMSG        "fread to file %s failed. "
 #define TELLFAILEDMSG        "ftell to file %s failed. "
 #define WRITEFAILEDMSG       "fwrite to file %s failed. "
 #define MQCONNFAILEDMSG    \
            "MQCONN to queue manager %.48s failed with reason %d\n"
 #define MQOPENFAILEDMSG    \
            "MQOPEN to queue manager %.48s failed with reason %d\n"
 #define MQINQFAILEDMSG    \
            "MQINQ to queue manager %.48s failed with reason %d\n"
 #define MQPUT1FAILEDMSG    \
            "MQPUT1 to queue %.48s failed with reason %d\n"
 #define CONVERTMQRMHFAILEDMSG    \
            "ConvertMQRMH failed with return code %d\n"
 #define CONVERTDATAFAILEDMSG    \
            "ConvertBulkData failed with return code %d\n"
 #define ALLOCATEMQRMHFAILEDMSG    \
            "Unable to allocate a buffer to hold the converted MQRMH\n"
 #define ALLOCATEDATAHFAILEDMSG    \
  "Unable to allocate a buffer to hold the converted object data\n"


 int  main  (int argc, char * argv[])
{
  PMQCXP   pExitParms;
  PMQCD    pChannelDef;
  PMQLONG  pDataLength;
  PMQLONG  pAgentBufferLength;
  PMQCHAR  pAgentBuffer;
  PMQLONG  pExitBufferLength;
  PMQCHAR *pExitBuffer;
  PMQXQH    pMQXQH;
  PMQRMH    pMQRMH;
  MQLONG    FileDataOffset;  /* offset of data within file           */
  MQLONG    FeedbackCode = 0;/* feedback code                        */
  char    * pMsgData;        /* pointer to data within reference msg */
  char      DestFileName[MAX_FILENAME_LENGTH+1];
                             /* Name of file at receiving end        */
  char      SourceFileName[MAX_FILENAME_LENGTH+1];
                             /* Name of file at sending end          */
  FILE    * fd = NULL_POINTER; /* file descriptor                    */
  char      mode[3];         /* file open mode                       */
  long      position;        /* position within file.                */
  int       rc = OK;         /* return code                          */
  size_t    itemswritten;    /* returned by fwrite                   */
  size_t    bytesread;       /* returned by fread                    */
  char      dummy;           /* work area                            */
  MQOD      od = {MQOD_DEFAULT};    /* Object Descriptor             */
  MQMD      md = {MQMD_DEFAULT};    /* Message Descriptor            */
  MQPMO     pmo = {MQPMO_DEFAULT};  /* put message options           */
  MQLONG    CompCode;               /* completion code               */
  MQLONG    Reason;                 /* reason code                   */
  MQLONG    MsgSpace;        /* Space remaining in input buffer      */
  MQLONG    ReadLength;      /* Number of bytes to read from file    */
  MQLONG    WriteLength;     /* Number of bytes to write to file     */
  int       WriteData=1;     /* Do we write the data                 */
  MQLONG    InputDataLength; /* Length requested in ref msg          */
  PMQRMH    pConvertedMQRMH = NULL_POINTER;
                             /* MQRMH converted to another CCSID and */
                             /* encoding. Maybe in dbcs              */
  PMQCHAR   pConvertedBulkData = NULL_POINTER;
                             /* Bulk data converted to another CCSID */
  MQLONG    ConvertedMQRMHLen; /* Length of converted  MQRMH         */
  MQLONG    ConvertedBulkDataLen; /* Length of converted bulk data   */
  PSAVEDATA pSaveData;
                             /* Save area for MQHCONN etc.           */
  MQLONG    OrigMQRMHLength; /* Length of MQRMH before conversion    */
  char      MsgBuffer[500];  /* message with inserts                 */

  char  ExitIds[][28] = {"MQXT_CHANNEL_SEC_EXIT"
                        ,"MQXT_CHANNEL_MSG_EXIT"
                        ,"MQXT_CHANNEL_SEND_EXIT"
                        ,"MQXT_CHANNEL_RCV_EXIT"
                        ,"MQXT_CHANNEL_MSG_RETRY_EXIT"
                        ,"MQXT_CHANNEL_AUTO_DEF_EXIT"
                        };
  char  ExitReasons[][14] = {"MQXR_INIT"
                            ,"MQXR_TERM"
                            ,"MQXR_MSG"
                            ,"MQXR_XMIT"
                            ,"MQXR_SEC_MSG"
                            ,"MQXR_INIT_SEC"
                            ,"MQXR_RETRY"
                            };

  pExitParms          =  (PMQCXP )    argv[1];
  pChannelDef         =  (PMQCD  )    argv[2];
  pDataLength         =  (PMQLONG)    argv[3];
  pAgentBufferLength  =  (PMQLONG)    argv[4];
  pAgentBuffer        =  (PMQCHAR)    argv[5];
  pExitBufferLength   =  (PMQLONG)    argv[6];
  pExitBuffer         =  (PMQCHAR *)  argv[7];

  pExitParms -> ExitResponse = MQXCC_OK;
  pSaveData = (PSAVEDATA)&(pExitParms -> ExitUserArea);

  switch(pExitParms -> ExitId)
  {
    case MQXT_CHANNEL_MSG_EXIT:
         break;
    default:
         printf(INVALIDEXITIDMSG,ExitIds[pExitParms->ExitId-
                                         MQXT_CHANNEL_SEC_EXIT]);
         FeedbackCode = XRMA_FB_INVALIDEXITID +
                        pExitParms->ExitId;
         goto MOD_EXIT;
         break;
  }

  switch(pExitParms -> ExitReason)
  {
    case MQXR_INIT:
         printf(REASONMSG,ExitReasons[pExitParms->ExitReason-
                                         MQXR_INIT]);
         /************************************************************/
         /* Initialise the MQHCONN in ExitUserArea to show that we   */
         /* have not connected to the queue manager yet.             */
         /* Initialise QMgrCCSID to show that we have not got the    */
         /* real CCSID yet (MQCCSI_Q_MGR has the value 0).           */
         /************************************************************/
         pSaveData -> HConn     = MQHO_UNUSABLE_HOBJ;
         pSaveData -> QMgrCCSID = MQCCSI_Q_MGR;
         goto MOD_EXIT;
         break;
    case MQXR_TERM:
         printf(REASONMSG,ExitReasons[pExitParms->ExitReason-
                                         MQXR_INIT]);
         /************************************************************/
         /* If we have connected to a queue manager then disconnect. */
         /************************************************************/
         if (pSaveData -> HConn != MQHO_UNUSABLE_HOBJ &&
             pSaveData -> ConnReason != MQRC_ALREADY_CONNECTED)
         {
           pExitParms->pEntryPoints->MQDISC_Call(&(pSaveData -> HConn)
                                                ,&CompCode
                                                ,&Reason
                                                );
         }
         goto MOD_EXIT;
         break;
    case MQXR_MSG:
         printf(REASONMSG,ExitReasons[pExitParms->ExitReason-
                                         MQXR_INIT]);
         break;
    default:
         printf(INVALIDREASONMSG,ExitReasons[pExitParms->ExitReason-
                                         MQXR_INIT]);
         FeedbackCode = XRMA_FB_INVALIDEXITREASON +
                        pExitParms->ExitReason;
         goto MOD_EXIT;
         break;
  }

  /*******************************************************************/
  /* Get addresses of transmission header and message itself.        */
  /* The message should start with a reference message header.       */
  /*******************************************************************/

  pMQXQH  = (PMQXQH) pAgentBuffer;
  pMQRMH  = (PMQRMH) (pMQXQH + 1);

  /*******************************************************************/
  /* If the message format does not indicate a reference message     */
  /* then ignore the message.                                        */
  /*******************************************************************/
  if (memcmp(pMQXQH -> MsgDesc.Format
            ,MQFMT_REF_MSG_HEADER
            ,MQ_FORMAT_LENGTH
            ))
  {
    printf(INVALIDMSGMSG,pMQXQH-> MsgDesc.Format,MQFMT_REF_MSG_HEADER);
    goto MOD_EXIT;
  }

  /*******************************************************************/
  /* Find the length of the original MQRMH.                          */
  /* The converted MQRMH may be shorter as any gaps between the      */
  /* name fields (e.g. SrcName) will be missing from the             */
  /* converted header.                                               */
  /* If the MQRMH is in the wrong encoding then the following        */
  /* statement will produce the wrong result but the call to         */
  /* ConvertMQRMH will return the correct value.                     */
  /*******************************************************************/
  OrigMQRMHLength = pMQRMH -> StrucLength;

  /*******************************************************************/
  /* Convert the MQRMH structure to the queue managers codepage and  */
  /* the local encoding, if necessary.                               */
  /* The call of ConvertMQRMH can be deleted if you know that the    */
  /* reference message does not need conversion.                     */
  /*******************************************************************/
  rc = ConvertMQRMH  (pExitParms
                     ,pSaveData
                     ,pChannelDef
                     ,pMQXQH
                     ,pMQRMH
                     ,&pConvertedMQRMH
                     ,&ConvertedMQRMHLen
                     ,&OrigMQRMHLength
                     ,&FeedbackCode
                     );
  if (rc == OK)
  {
    if (pConvertedMQRMH != NULL_POINTER)
    {
      pMQRMH = pConvertedMQRMH; /* Use converted MQRMH               */
    }
  }
  else
  {
    printf(CONVERTMQRMHFAILEDMSG,rc);
    goto MOD_EXIT;
  }

  /*******************************************************************/
  /* If the object type does not match the type specified in the     */
  /* channel definition then ignore the message.                     */
  /*******************************************************************/
  if (memcmp(pMQRMH -> ObjectType
            ,pExitParms -> ExitData
            ,sizeof(pMQRMH -> ObjectType)
            ))
  {
    printf(INVALIDOBJTYPEMSG
          ,pMQRMH -> ObjectType
          ,pExitParms -> ExitData
          );
    goto MOD_EXIT;
  }

  /*******************************************************************/
  /* Check that the SrcEnv field lies within the structure           */
  /*******************************************************************/
  if (pMQRMH -> SrcEnvLength < 0 ||
      (pMQRMH -> SrcEnvLength > 0 &&
       (pMQRMH -> SrcEnvLength + pMQRMH -> SrcEnvOffset) >
       pMQRMH -> StrucLength
     ))
  {
     printf(SRCENVERRORMSG,pMQRMH -> SrcEnvLength,
            pMQRMH -> SrcEnvOffset, pMQRMH -> StrucLength);
     FeedbackCode = MQRC_SRC_ENV_ERROR;
     goto MOD_EXIT;
  }

  /*******************************************************************/
  /* Check that the SrcName field lies within the structure          */
  /*******************************************************************/
  if (pMQRMH -> SrcNameLength < 0 ||
      (pMQRMH -> SrcNameLength > 0 &&
       (pMQRMH -> SrcNameLength + pMQRMH -> SrcNameOffset) >
       pMQRMH -> StrucLength
     ))
  {
     printf(SRCNAMEERRORMSG,pMQRMH -> SrcNameLength,
            pMQRMH -> SrcNameOffset, pMQRMH -> StrucLength);
     FeedbackCode = MQRC_SRC_NAME_ERROR;
     goto MOD_EXIT;
  }

  /*******************************************************************/
  /* Check that the DestEnv field lies within the structure          */
  /*******************************************************************/
  if (pMQRMH -> DestEnvLength < 0 ||
      (pMQRMH -> DestEnvLength > 0 &&
       (pMQRMH -> DestEnvLength + pMQRMH -> DestEnvOffset) >
       pMQRMH -> StrucLength
     ))
  {
     printf(DESTENVERRORMSG,pMQRMH -> DestEnvLength,
            pMQRMH -> DestEnvOffset, pMQRMH -> StrucLength);
     FeedbackCode = MQRC_DEST_ENV_ERROR;
     goto MOD_EXIT;
  }

  /*******************************************************************/
  /* Check that the DestName field lies within the structure         */
  /*******************************************************************/
  if (pMQRMH -> DestNameLength < 0 ||
      (pMQRMH -> DestNameLength > 0 &&
       (pMQRMH -> DestNameLength + pMQRMH -> DestNameOffset) >
       pMQRMH -> StrucLength
     ))
  {
     printf(DESTNAMEERRORMSG,pMQRMH -> DestNameLength,
            pMQRMH -> DestNameOffset, pMQRMH -> StrucLength);
     FeedbackCode = MQRC_DEST_NAME_ERROR;
     goto MOD_EXIT;
  }

  /*******************************************************************/
  /* Check that the message contains a complete MQRMH structure      */
  /*******************************************************************/
  if (pMQRMH -> StrucLength > (MQLONG)(*pDataLength - sizeof(MQXQH)))
  {
     printf(STRUCLENERRORMSG,pMQRMH -> StrucLength,*pDataLength);
     FeedbackCode = XRMA_FB_INVALIDSTRUCLENGTH;
     goto MOD_EXIT;
  }

  /*******************************************************************/
  /* Extract the following values from the reference message.        */
  /*  Offset of data within the file.                                */
  /*  Length of data to be read or written                           */
  /*  Name of file on destination system.                            */
  /*  Name of file on source system.                                 */
  /*******************************************************************/
  FileDataOffset  = pMQRMH -> DataLogicalOffset;
  InputDataLength = pMQRMH -> DataLogicalLength;
  memset(DestFileName,0,sizeof(DestFileName));
  memset(SourceFileName,0,sizeof(SourceFileName));
  memcpy(DestFileName
        ,(char*)pMQRMH + pMQRMH -> DestNameOffset
        ,pMQRMH -> DestNameLength
        );
  memcpy(SourceFileName
        ,(char*)pMQRMH + pMQRMH -> SrcNameOffset
        ,pMQRMH -> SrcNameLength
        );

  /*******************************************************************/
  /* Move the data to or from the file.                              */
  /*******************************************************************/
  switch (pChannelDef -> ChannelType)
  {
    /*****************************************************************/
    /* For server and sender channels append data from the file to   */
    /* the reference message and return it to the caller.            */
    /* If the end of the file is not reached then put the reference  */
    /* back on the transmission queue.                               */
    /*****************************************************************/
    case MQCHT_SENDER:
    case MQCHT_SERVER:

      /***************************************************************/
      /* Make sure there is no data following the MQRMH header.      */
      /* If there is, it may mean that a message with file data      */
      /* appended to it has been routed to a Sender channel instead  */
      /* of a Receiver channel.                                      */
      /***************************************************************/
      if (*pDataLength >
          (MQLONG)(sizeof(MQXQH) + OrigMQRMHLength)
         )
      {
         printf(DATALENERRORMSG
               ,sizeof(MQXQH) + OrigMQRMHLength
               ,*pDataLength);
         FeedbackCode = XRMA_FB_INVALIDDATALENGTH;
         goto MOD_EXIT;
      }

      /***************************************************************/
      /* If the MQRMH has been converted then copy the converted     */
      /* version into AgentBuffer before copying in the object data  */
      /* and reset pMQRMH to point back to this area.                */
      /***************************************************************/
      if (pConvertedMQRMH != NULL_POINTER)
      {
        memcpy((PMQCHAR)(pMQXQH+1)
              ,(PMQCHAR)pConvertedMQRMH
              ,ConvertedMQRMHLen
              );

        pMQRMH = (PMQRMH)(pMQXQH +1);

        /*************************************************************/
        /* The Encoding and CCSID of the reference message in the    */
        /* transmission header should be correct but in case they    */
        /* are not, set them.                                        */
        /*************************************************************/
        pMQXQH -> MsgDesc.Encoding       = MQENC_NATIVE;
        pMQXQH -> MsgDesc.CodedCharSetId = pSaveData -> QMgrCCSID;
      }

      /***************************************************************/
      /* Calculate space remaining at end of input buffer and        */
      /* pointer to start of the space.                              */
      /* If the length of the agent buffer is greater than the       */
      /* MaxMsglength for the channel plus the transmission header   */
      /* size, then use the MaxMsgLength in the calculation.         */
      /* If InputDataLength > 0 this means that only that length     */
      /* is required. In this case, if the length specified is less  */
      /* than the space in the buffer then read just this length.    */
      /***************************************************************/
      MsgSpace  = ((*pAgentBufferLength >
                    (MQLONG)(pChannelDef->MaxMsgLength + sizeof(MQXQH))
                   )
                   ? (pChannelDef -> MaxMsgLength)
                   : (MQLONG)(*pAgentBufferLength - sizeof(MQXQH))
                  )
                  - pMQRMH -> StrucLength;
      pMsgData  = (PMQCHAR)pMQRMH + pMQRMH -> StrucLength;
      ReadLength = (InputDataLength > 0 &&
                    InputDataLength < MsgSpace)
                   ? InputDataLength
                   : MsgSpace;

      /***************************************************************/
      /* Open the file for reading                                   */
      /***************************************************************/
      fd = fopen(SourceFileName,"rb");

      if (fd == NULL_POINTER)
      {
        FeedbackCode = XRMA_FB_FOPENFILEFAILED + errno;
        sprintf(MsgBuffer,OPENFAILEDMSG,SourceFileName);
        perror(MsgBuffer);
        goto MOD_EXIT;
      }

      /***************************************************************/
      /* Position the file to the specified offset.                  */
      /***************************************************************/
      rc = fseek(fd
                ,FileDataOffset
                ,SEEK_SET
                );

      if (rc)
      {
        FeedbackCode = XRMA_FB_FSEEKFILEFAILED + errno;
        sprintf(MsgBuffer,SEEKFAILEDMSG,SourceFileName);
        perror(MsgBuffer);
        goto MOD_EXIT;
      }

      /***************************************************************/
      /* Read the data from the specified offset into the buffer     */
      /***************************************************************/
      bytesread = fread(pMsgData
                       ,1
                       ,ReadLength
                       ,fd
                       );

      if (ferror(fd))
      {
        FeedbackCode = XRMA_FB_FREADFILEFAILED + errno;
        sprintf(MsgBuffer,READFAILEDMSG,SourceFileName);
        perror(MsgBuffer);
        goto MOD_EXIT;
      }
      else
      if (feof(fd) ||
          (InputDataLength > 0  &&
           InputDataLength == (MQLONG)bytesread)
         )
      {
        /*************************************************************/
        /* End of file reached or all the requested data has been    */
        /* read.                                                     */
        /* Set MQRMHF_LAST flag.                                     */
        /*************************************************************/
        pMQRMH -> Flags  |= MQRMHF_LAST;
      }
      else
      {
        /*************************************************************/
        /* May have read last byte of the file (EOF is only set when */
        /* attempting to read beyond the last byte).                 */
        /* Try reading one more byte.                                */
        /*************************************************************/

        fread(&dummy,1,1,fd);

        if (ferror(fd))
        {
          FeedbackCode = XRMA_FB_FREADFILEFAILED + errno;
          sprintf(MsgBuffer,READFAILEDMSG,SourceFileName);
          perror(MsgBuffer);
          goto MOD_EXIT;
        }
        else
        if (feof(fd))
        {
          /***********************************************************/
          /* End of file reached before buffer was filled.           */
          /* Set MQRMHF_LAST flag.                                   */
          /***********************************************************/
          pMQRMH -> Flags |= MQRMHF_LAST;
        }
        else
        {
          /***********************************************************/
          /* End of file not reached.                                */
          /* Connect to the local queue manager if not already       */
          /* connected.                                              */
          /* Update the MQRMH.DataLogicalOffset field to identify    */
          /* the next piece of the file that is required.            */
          /* Decrement MQRMH.DataLength, if it is non-zero, by the   */
          /* number of bytes read.                                   */
          /* Turn off MQRMHF_LAST flag.                              */
          /* MQPUT1 the reference message back on the transmission   */
          /* queue.                                                  */
          /* If the message is persistent or the channel is not      */
          /* defined to be fast, then the message is put in          */
          /* syncpoint. Otherwise it is put out of syncpoint.        */
          /* The message is put with the same MQMD as was used in    */
          /* original MQPUT to the remote queue but the format is    */
          /* set to MQFMT_XMIT_Q_HEADER and Report set to none.      */
          /* Restore the current value of DataLogicalOffset before   */
          /* the message is returned to the caller.                  */
          /***********************************************************/

          if (pSaveData -> HConn == MQHO_UNUSABLE_HOBJ)
          {
            pExitParms->pEntryPoints->MQCONN_Call(pChannelDef -> QMgrName
                                                 ,&(pSaveData -> HConn)
                                                 ,&CompCode
                                                 ,&(pSaveData -> ConnReason));

            if (CompCode == MQCC_FAILED)
            {
              FeedbackCode = XRMA_FB_MQCONNFAILED +
                             pSaveData -> ConnReason;
              printf(MQCONNFAILEDMSG, pChannelDef -> QMgrName,
                     pSaveData -> ConnReason);
              goto MOD_EXIT;
            }
          }

          strncpy(od.ObjectName
                 ,pChannelDef -> XmitQName
                 ,sizeof(od.ObjectName)
                 );
          strncpy(od.ObjectQMgrName
                 ,pChannelDef -> QMgrName
                 ,sizeof(od.ObjectQMgrName)
                 );

          memcpy((PMQCHAR)&md
                ,(PMQCHAR)&(pMQXQH -> MsgDesc)
                ,sizeof(pMQXQH -> MsgDesc)
                );
          memcpy(md.Format,MQFMT_XMIT_Q_HEADER,MQ_FORMAT_LENGTH);
          memcpy(md.MsgId   , MQMI_NONE, sizeof(md.MsgId));
          memcpy(md.CorrelId, MQCI_NONE, sizeof(md.CorrelId));

          md.Report =  MQRO_NONE;

          pmo.Options = ((pChannelDef -> NonPersistentMsgSpeed ==
                          MQNPMS_NORMAL) ||
                         (pMQXQH -> MsgDesc.Persistence ==
                          MQPER_PERSISTENT))
                        ? MQPMO_FAIL_IF_QUIESCING |
                          MQPMO_SYNCPOINT
                        : MQPMO_FAIL_IF_QUIESCING |
                          MQPMO_NO_SYNCPOINT;

          pMQRMH -> Flags             &= ~MQRMHF_LAST;
          pMQRMH -> DataLogicalOffset += (MQLONG)bytesread;

          if (pMQRMH -> DataLogicalLength > 0)
          {
            pMQRMH -> DataLogicalLength -= (MQLONG)bytesread;
          }

          pExitParms->pEntryPoints->MQPUT1_Call(pSaveData -> HConn  /* connection handle            */
                                               ,&od                 /* object descriptor for queue  */
                                               ,&md                 /* message descriptor           */
                                               ,&pmo                /* options                      */
                                               ,sizeof(MQXQH) +     /* message length               */
                                                pMQRMH -> StrucLength
                                               ,pAgentBuffer        /* buffer                       */
                                               ,&CompCode           /* MQPUT1 completion code       */
                                               ,&Reason             /* reason code                  */
                                               );

          /***********************************************************/
          /* Restore original value of DataLogicalOffset.            */
          /***********************************************************/
          pMQRMH -> DataLogicalOffset -= (MQLONG)bytesread;

          if (CompCode != MQCC_OK)
          {
            printf(MQPUT1FAILEDMSG, od.ObjectName, Reason);
            FeedbackCode = XRMA_FB_MQPUT1FAILED + Reason;
            goto MOD_EXIT;
          }
        }
      }

      /***************************************************************/
      /* Set the MQRMH.DataLogicalLength field to the number of      */
      /* bytes read.                                                 */
      /* Set DataLength to the increased size of the message.        */
      /* The size of the message has increased by the number of      */
      /* bytes read from the file plus the difference in the sizes   */
      /* of the input and converted reference messages.              */
      /***************************************************************/
      pMQRMH -> DataLogicalLength  =  (MQLONG)bytesread;
      *pDataLength                 += (MQLONG)(bytesread - OrigMQRMHLength +
                                      pMQRMH -> StrucLength);

      break;
    /*****************************************************************/
    /* For requestor and receiver channels, copy data from the       */
    /* reference message into the file.                              */
    /* If this is the first part of the file then (re)create the     */
    /* file first.                                                   */
    /* If this is the last part of the file then return the          */
    /* reference message to the caller, otherwise discard it.        */
    /*****************************************************************/
    case MQCHT_REQUESTER:
    case MQCHT_RECEIVER:

      /***************************************************************/
      /* Calculate address of data appended to MQRMH header.         */
      /* Calculate length of data to be written to the file.         */
      /***************************************************************/
      pMsgData  = (PMQCHAR)(pMQXQH +1) + OrigMQRMHLength;
      WriteLength = *pDataLength - sizeof(MQXQH) -
                    OrigMQRMHLength;

      /***************************************************************/
      /* If the format of the object data is MQFMT_STRING then       */
      /* convert it and correct the CCSID in the reference message.  */
      /* Even though the length of the converted data is returned,   */
      /* the following code assumes that the length is unchanged.    */
      /* The call of ConvertBulkData can be removed if you know that */
      /* the object data does not need converting.                   */
      /***************************************************************/
      if (memcmp(pMQRMH -> Format
                ,MQFMT_STRING
                ,MQ_FORMAT_LENGTH
                ) == 0)
      {
        rc = ConvertBulkData (pExitParms
                             ,pSaveData
                             ,pChannelDef
                             ,pMQRMH
                             ,pMsgData
                             ,&pConvertedBulkData
                             ,WriteLength
                             ,&ConvertedBulkDataLen
                             ,&FeedbackCode
                             );
        if (rc == OK)
        {
          if (pConvertedBulkData != NULL_POINTER)
          {
            pMsgData = pConvertedBulkData; /* Use converted data     */

            /*********************************************************/
            /* Update the CCSID of the object in the converted MQRMH */
            /*********************************************************/
            pMQRMH -> CodedCharSetId = pSaveData -> QMgrCCSID;
          }
        }
        else
        {
          printf(CONVERTDATAFAILEDMSG,rc);
          goto MOD_EXIT;
        }
      }

      /***************************************************************/
      /* If the data offset within the file is zero then the file is */
      /* created for writing. If it already exists, its contents are */
      /* destroyed.                                                  */
      /* Otherwise it is opened for appending.                       */
      /***************************************************************/
      strcpy(mode
            ,(FileDataOffset == 0) ? "wb" : "ab+"
            );

      fd = fopen(DestFileName,mode);

      if (fd == NULL_POINTER)
      {
        FeedbackCode = XRMA_FB_FOPENFILEFAILED + errno;
        sprintf(MsgBuffer,OPENFAILEDMSG,DestFileName);
        perror(MsgBuffer);
        goto MOD_EXIT;
      }

      /***************************************************************/
      /* If the data offset within the file is not zero, check the   */
      /* current size of the file.                                   */
      /* If it matches the data offset, then write the data to the   */
      /* file.                                                       */
      /* Otherwise ignore the message.                               */
      /* If the data offset is zero then write the data to the file. */
      /***************************************************************/
      if (FileDataOffset != 0)
      {
                                       /* Go to the end of the file  */
        fseek(fd, 0, SEEK_END);
                                       /* Now, how big is it ?       */
        position = ftell(fd);

        if (position < 0)
        {
          FeedbackCode = XRMA_FB_FTELLFILEFAILED + errno;
          sprintf(MsgBuffer,TELLFAILEDMSG,DestFileName);
          perror(MsgBuffer);
          goto MOD_EXIT;
        }

        if (position > FileDataOffset)
        {
          /***********************************************************/
          /* File is bigger than the offset of this segment. This    */
          /* can happen during recovery when a channel terminates    */
          /* half way through a file transfer.                       */
          /* Our only response is to not write the data              */
          /***********************************************************/
          WriteData = 0;
        }

        if (position < FileDataOffset)
        {
          /***********************************************************/
          /* File is smaller than the offset of this segment. We     */
          /* must have lost a segment, alert the user to the error   */
          /***********************************************************/
          printf(INVALIDOFFSETMSG,FileDataOffset,position,
                 DestFileName);
          FeedbackCode = XRMA_FB_INVALIDFILEOFFSET;
          goto MOD_EXIT;
        }
      }

      /***************************************************************/
      /* Write the data to the specified offset in the file.         */
      /* The amount of data to be written is the length of the       */
      /* message minus the transmission and reference message headers*/
      /***************************************************************/
      if (WriteData && (WriteLength > 0))
      {
        itemswritten = fwrite(pMsgData
                             ,WriteLength
                             ,1
                             ,fd
                             );

        if (!itemswritten)
        {
          FeedbackCode = XRMA_FB_FWRITEFILEFAILED + errno;
          sprintf(MsgBuffer,WRITEFAILEDMSG,DestFileName);
          perror(MsgBuffer);
          goto MOD_EXIT;
        }
      }

      if (pMQRMH -> Flags == MQRMHF_LAST)
      {
        /*************************************************************/
        /* This is the last part of the file so we return the        */
        /* reference message to the caller without the appended      */
        /* object data, so set DataLength to the sum of the sizes of */
        /* the MQXQH and the output MQRMH.                           */
        /* The converted MQRMH is returned to the caller.            */
        /* DataLogicalOffset is set to zero.                         */
        /* MQRMH.DataLogicalLength is set to the total size of the   */
        /* file.                                                     */
        /* ExitResponse is left at MQXCC_OK which means that the     */
        /* reference message (minus the appended data) will be put   */
        /* to the target queue.                                      */
        /*************************************************************/
        if (pConvertedMQRMH != NULL_POINTER)
        {
          memcpy((PMQCHAR)(pMQXQH+1)
                ,(PMQCHAR)pConvertedMQRMH
                ,ConvertedMQRMHLen
                );

          /***********************************************************/
          /* The Encoding and CCSID of the reference message in the  */
          /* transmission header should be correct but in case they  */
          /* are not, set them.                                      */
          /***********************************************************/
          pMQXQH -> MsgDesc.Encoding       = MQENC_NATIVE;
          pMQXQH -> MsgDesc.CodedCharSetId = pSaveData -> QMgrCCSID;
        }

        *pDataLength                 = sizeof(MQXQH) +
                                       pMQRMH -> StrucLength;
        pMQRMH -> DataLogicalLength  = FileDataOffset + WriteLength;
        pMQRMH -> DataLogicalOffset  = 0;
      }
      else
      {
        /*************************************************************/
        /* This is not the last part of the file so discard the      */
        /* message.                                                  */
        /*************************************************************/
        pExitParms -> ExitResponse =  MQXCC_SUPPRESS_FUNCTION;
        pMQXQH -> MsgDesc.Report   |= MQRO_DISCARD_MSG;
        pMQXQH -> MsgDesc.Report   &= ~MQRO_EXCEPTION;
      }

      break;
    /*****************************************************************/
    /* For other channel types, ignore the message.                  */
    /*****************************************************************/
    default:
      printf(INVALIDCHLTYPEMSG
            ,pChannelDef -> ChannelType
            );
      goto MOD_EXIT;
  }

  MOD_EXIT:

  /*******************************************************************/
  /* Close the file if necessary.                                    */
  /*******************************************************************/
  if (fd)
  {
    fclose(fd);
  }

  /*******************************************************************/
  /* If the MQRMH was converted, free the storage                    */
  /*******************************************************************/
  if (pConvertedMQRMH != NULL_POINTER)
  {
    free(pConvertedMQRMH);
  }

  /*******************************************************************/
  /* If the object data was converted, free the storage              */
  /*******************************************************************/
  if (pConvertedBulkData != NULL_POINTER)
  {
    free(pConvertedBulkData);
  }

  /*******************************************************************/
  /* If an error has occurred, tell the caller to DLQ the message.   */
  /* Set the feedback code to be returned in an exception report     */
  /* (if requested).                                                 */
  /*******************************************************************/
  if (FeedbackCode)
  {
    pExitParms -> ExitResponse =  MQXCC_SUPPRESS_FUNCTION;
    pExitParms -> Feedback     =  FeedbackCode;
  }

  return ;
}

/*********************************************************************/
/* ConvertMQRMH                                                      */
/*-------------------------------------------------------------------*/
/* If the encoding or CCSID of the source MQRMH are different to     */
/* those of the queue manager, then get a block of storage and       */
/* convert the MQRMH into this storage and return it to the caller.  */
/* The caller is responsible for freeing the storage.                */
/*********************************************************************/
int ConvertMQRMH  (PMQCXP    pExitParms
                  ,PSAVEDATA pSaveData
                  ,PMQCD     pChannelDef
                  ,PMQXQH    pMQXQH
                  ,PMQRMH    pMQRMH
                  ,PMQRMH   *ppConvertedMQRMH
                  ,PMQLONG   pConvertedMQRMHLen
                  ,PMQLONG   pOrigMQRMHLength
                  ,PMQLONG   pFeedbackCode
                  )
{
  int rc = OK;                   /* function return code             */

  /*******************************************************************/
  /* Get the queue manager CCSID and save it, if necessary           */
  /*******************************************************************/
  if (pSaveData -> QMgrCCSID == MQCCSI_Q_MGR)
  {
    rc = GetQMgrCCSID(pExitParms,pSaveData,pChannelDef,pFeedbackCode);
    if (rc != OK)
    {
      goto MOD_EXIT;
    }
  }

  /*******************************************************************/
  /* At this point we should be connected to the local qmgr.         */
  /*******************************************************************/

  /*******************************************************************/
  /* If the Encoding of the MQRMH is not MQENC_NATIVE or the CCSID   */
  /* is not the same as that of the queue manager, then the MQRMH    */
  /* needs converting.                                               */
  /*******************************************************************/
  if (pMQXQH -> MsgDesc.Encoding != MQENC_NATIVE ||
      pMQXQH -> MsgDesc.CodedCharSetId != pSaveData -> QMgrCCSID)
  {
    /*****************************************************************/
    /* Allocate a buffer to hold the converted header.               */
    /* The caller will free it.                                      */
    /*****************************************************************/
    *ppConvertedMQRMH = malloc(MAX_MQRMH_LENGTH);

    if (*ppConvertedMQRMH == NULL_POINTER)
    {
      printf(ALLOCATEMQRMHFAILEDMSG);
      *pFeedbackCode = XRMA_FB_MALLOCFAILED + errno;
      rc = FAILED;
      goto MOD_EXIT;
    }

    /*****************************************************************/
    /* Convert the MQRMH                                             */
    /*****************************************************************/
    rc = ConverttagMQRMH(
                pExitParms                   /* Exit parameters      */
               ,(PMQBYTE*)&pMQRMH            /* Input MQRMH          */
               ,(PMQBYTE*)ppConvertedMQRMH   /* Output MQRMH         */
               ,(PMQBYTE)pMQRMH + MAX_MQRMH_LENGTH -1
                                             /* Input last byte      */
               ,(PMQBYTE)*ppConvertedMQRMH + MAX_MQRMH_LENGTH -1
                                             /* Output last byte     */
               ,pSaveData -> HConn           /* HConn                */
               ,MQGMO_NONE                   /* Application options  */
               ,pMQXQH -> MsgDesc.Encoding   /* Input encoding       */
               ,MQENC_NATIVE                 /* Output encoding      */
               ,pMQXQH -> MsgDesc.CodedCharSetId
                                             /* Input CodedCharSetId */
               ,pSaveData -> QMgrCCSID       /* Output CodedCharSetId*/
               ,MQCC_OK
               ,MQRC_NONE
               ,pOrigMQRMHLength
               );

    if (rc != MQRC_NONE)
    {
      *pFeedbackCode = XRMA_FB_CONVERTFAILED + rc;
      rc = FAILED;
    }

    /*****************************************************************/
    /* Return the length of the converted MQRMH                      */
    /*****************************************************************/
    *pConvertedMQRMHLen = (*ppConvertedMQRMH) -> StrucLength;
  }

  MOD_EXIT:

  return rc;
}

/*********************************************************************/
/* ConvertBulkData                                                   */
/*-------------------------------------------------------------------*/
/* If the CCSID of the object data is different to that of the       */
/* queue manager, then get a block of storage and convert the object */
/* data into this storage and return it to the caller.               */
/* The caller is responsible for freeing the storage.                */
/*********************************************************************/
int ConvertBulkData (PMQCXP    pExitParms
                    ,PSAVEDATA pSaveData
                    ,PMQCD     pChannelDef
                    ,PMQRMH    pMQRMH
                    ,PMQCHAR   pBulkData
                    ,PMQCHAR  *ppConvertedBulkData
                    ,MQLONG    BulkDataLen
                    ,PMQLONG   pConvertedBulkDataLen
                    ,PMQLONG   pFeedbackCode
                    )
{
  int rc = OK;                   /* function return code             */

  /*******************************************************************/
  /* The following variables are used by the ConvertCharIEP macro    */
  /* and other macros that it invokes                                */
  /*******************************************************************/
  MQLONG ReturnCode = MQRC_NONE; /* ConvertCharIEP return code       */
  MQLONG Reason = MQRC_NONE;
  MQLONG opts = MQGMO_NONE;
  MQHOBJ hConn;
  PMQBYTE pInData = (PMQBYTE)pBulkData;
                                 /* Local copy of ptr io input data  */
  PMQBYTE pOutData;              /* local copy of ptr to output data */
  PMQBYTE *in_cursor  = &pInData;
  PMQBYTE *out_cursor = &pOutData;
  PMQBYTE in_lastbyte = (PMQBYTE)pBulkData + BulkDataLen -1;
  PMQBYTE out_lastbyte;
  MQLONG  MsgCCSID = pMQRMH -> CodedCharSetId;
                                 /* CodedCharSetId of input data     */
  MQLONG  ReqCCSID;              /* CodedCharSetId of output data    */
  MQLONG  MsgEncoding = pMQRMH -> Encoding;
                                 /* Encoding of input data           */
  MQLONG  ReqEncoding = MQENC_NATIVE;
                         /* Local Encoding (in case source is UCS-2) */

  /*******************************************************************/
  /* Get the queue manager CCSID and save it, if necessary           */
  /*******************************************************************/
  if (pSaveData -> QMgrCCSID == MQCCSI_Q_MGR)
  {
    rc = GetQMgrCCSID(pExitParms,pSaveData,pChannelDef,pFeedbackCode);
    if (rc != OK)
    {
      goto MOD_EXIT;
    }
  }
  ReqCCSID = pSaveData -> QMgrCCSID;

  /*******************************************************************/
  /* At this point we should be connected to the local qmgr.         */
  /*******************************************************************/
  hConn = pSaveData -> HConn;

  /*******************************************************************/
  /* If the CCSID is not the same as that of the queue manager, then */
  /* the object data needs converting.                               */
  /*******************************************************************/
  if (MsgCCSID != ReqCCSID)
  {
    /*****************************************************************/
    /* Allocate a buffer to hold the converted data.                 */
    /* The caller will free it.                                      */
    /*****************************************************************/
    *ppConvertedBulkData = malloc(BulkDataLen);

    if (*ppConvertedBulkData == NULL_POINTER)
    {
      printf(ALLOCATEDATAHFAILEDMSG);
      *pFeedbackCode = XRMA_FB_MALLOCFAILED + errno;
      rc = FAILED;
      goto MOD_EXIT;
    }

    pOutData     = (PMQBYTE)*ppConvertedBulkData;
    out_lastbyte = *out_cursor + BulkDataLen -1;

    /*****************************************************************/
    /* Convert the data                                              */
    /*****************************************************************/
    ConvertCharIEP(pExitParms->pEntryPoints, BulkDataLen);

    Fail: rc = ReturnCode;
    if (rc != MQRC_NONE)
    {
      *pFeedbackCode = XRMA_FB_CONVERTFAILED + rc;
    }

    /*****************************************************************/
    /* Return the length of the converted data                       */
    /*****************************************************************/
    *pConvertedBulkDataLen = (MQLONG)((PMQLONG)*out_cursor -
                             (PMQLONG)*ppConvertedBulkData);
  }
  MOD_EXIT:

  return rc;
}

/*********************************************************************/
/* GetQMgrCCSID                                                      */
/*-------------------------------------------------------------------*/
/* If necessary, connect to the queue manager.                       */
/* Open the queue manager object and issue MQINQ to get the CCSID.   */
/* Save it.                                                          */
/*********************************************************************/
int GetQMgrCCSID (PMQCXP    pExitParms
                 ,PSAVEDATA pSaveData
                 ,PMQCD     pChannelDef
                 ,PMQLONG   pFeedbackCode
                 )
{
  int rc = OK;               /* Function return code                 */
  MQLONG   CompCode;
  MQLONG   Reason;
  MQOD     ObjDesc = {MQOD_DEFAULT}; /* Object descriptor            */
  MQLONG   Selectors[1];
  MQLONG   flags;
  MQHOBJ   Hobj;

  /*******************************************************************/
  /* If not already connected, connect to the local queue manager    */
  /*******************************************************************/
  if (pSaveData -> HConn == MQHO_UNUSABLE_HOBJ)
  {
     pExitParms->pEntryPoints->MQCONN_Call(pChannelDef -> QMgrName
                                          ,&(pSaveData -> HConn)
                                          ,&CompCode
                                          ,&(pSaveData -> ConnReason));

     if (CompCode == MQCC_FAILED)
     {
       printf(MQCONNFAILEDMSG, pChannelDef -> QMgrName,
              pSaveData -> ConnReason);
       *pFeedbackCode = XRMA_FB_MQCONNFAILED + pSaveData -> ConnReason;
       rc = FAILED;
       goto MOD_EXIT;
     }
  }

  /********************************************************************/
  /* Open the queue manager object                                    */
  /********************************************************************/
  ObjDesc.ObjectType = MQOT_Q_MGR;
  memcpy(ObjDesc.ObjectQMgrName
        ,pChannelDef -> QMgrName
        ,MQ_Q_MGR_NAME_LENGTH
        );
  flags = MQOO_INQUIRE;

  pExitParms->pEntryPoints->MQOPEN_Call(pSaveData -> HConn
                                       ,&ObjDesc
                                       ,flags
                                       ,&Hobj
                                       ,&CompCode
                                       ,&Reason
                                       );

  if (CompCode == MQCC_FAILED)
  {
    printf(MQOPENFAILEDMSG, pChannelDef -> QMgrName,
           Reason);
    *pFeedbackCode = XRMA_FB_MQOPENFAILED + Reason;
    rc = FAILED;
    goto MOD_EXIT;
  }

  /********************************************************************/
  /* Use MQINQ to get the queue manager CCSID                         */
  /********************************************************************/
  Selectors[0] = MQIA_CODED_CHAR_SET_ID;

  pExitParms->pEntryPoints->MQINQ_Call(pSaveData -> HConn
                                      ,Hobj
                                      ,1                    /* Number of selectors                   */
                                      ,Selectors
                                      ,1                    /* Number of integer selectors           */
                                      ,&(pSaveData -> QMgrCCSID)
                                      ,0                    /* length of character attributes        */
                                      ,NULL_POINTER         /* character attributes                  */
                                      ,&CompCode
                                      ,&Reason
                                      );

  if (CompCode == MQCC_FAILED)
  {
    printf(MQINQFAILEDMSG, pChannelDef -> QMgrName,
           Reason);
    *pFeedbackCode = XRMA_FB_MQOPENFAILED + Reason;
    rc = FAILED;
    goto MOD_EXIT;
  }

  MOD_EXIT:

  if (Hobj != MQHO_UNUSABLE_HOBJ)
  {
    pExitParms->pEntryPoints->MQCLOSE_Call(pSaveData -> HConn
                                          ,&Hobj
                                          ,MQCO_NONE
                                          ,&CompCode
                                          ,&Reason
                                          );
  }

  return rc;
}

/*********************************************************************/
/* ConverttagMQRMH                                                   */
/*-------------------------------------------------------------------*/
/* This routine is structured like the one generated by the          */
/* CRTMQCVX command to be inserted in a data conversion exit.        */
/*********************************************************************/
MQLONG ConverttagMQRMH(PMQCXP  pExitParms,
                       PMQBYTE *in_cursor,
                       PMQBYTE *out_cursor,
                       PMQBYTE in_lastbyte,
                       PMQBYTE out_lastbyte,
                       MQHCONN hConn,
                       MQLONG  opts,
                       MQLONG  MsgEncoding,
                       MQLONG  ReqEncoding,
                       MQLONG  MsgCCSID,
                       MQLONG  ReqCCSID,
                       MQLONG  CompCode,
                       MQLONG  Reason,
                       PMQLONG pOrigMQRMHLength
                      )
{
   MQLONG ReturnCode = MQRC_NONE;
   PMQBYTE pIMQRMH = *in_cursor;  /* save ptr to input MQRMH         */
   PMQRMH  pOMQRMH = (PMQRMH)*out_cursor;
                                  /* save ptr to output MQRMH        */

   ConvertCharIEP(pExitParms->pEntryPoints, 4); /* StrucId */

   AlignLong();
   ConvertLong(4); /* Version */
                   /* StrucLength */
                   /* Encoding */
                   /* CodedCharSetId */

   ConvertCharIEP(pExitParms->pEntryPoints, 8); /* Format */

   AlignLong();
   ConvertLong(1); /* Flags */

   ConvertCharIEP(pExitParms->pEntryPoints, 8); /* ObjectType */

   ConvertByte(24); /* ObjectInstanceId */

   AlignLong();
   ConvertLong(11); /* SrcEnvLength */
                    /* SrcEnvOffset */
                    /* SrcNameLength */
                    /* SrcNameOffset */
                    /* DestEnvLength */
                    /* DestEnvOffset */
                    /* DestNameLength */
                    /* DestNameOffset */
                    /* DataLogicalLength */
                    /* DataLogicalOffset */
                    /* DataLogicalOffset2 */

  /*******************************************************************/
  /* Convert the name and environment fields if necessary.           */
  /* Their positions are specified by offsets from the start of the  */
  /* MQRMH.                                                          */
  /*                                                                 */
  /* For each field set the in_cursor to the start of the input      */
  /* field and update the offset of the field in the output buffer   */
  /* to the current position in the buffer (the place where the      */
  /* field is about to be converted into).                           */
  /*                                                                 */
  /* Lastly, recalculate StrucLength as the input structure may      */
  /* contain bytes that are not referred to by any of the offset     */
  /* fields. These unreferenced bytes will not exist in the output   */
  /* message.                                                        */
  /* Also return the size of the input reference message.            */
  /*******************************************************************/

  if (pOMQRMH -> SrcEnvLength > 0)
  {
    *in_cursor = pIMQRMH + pOMQRMH -> SrcEnvOffset;
    pOMQRMH -> SrcEnvOffset = (MQLONG)(*out_cursor - (PMQBYTE)pOMQRMH);

    ConvertCharIEP(pExitParms->pEntryPoints, pOMQRMH -> SrcEnvLength);
  }

  if (pOMQRMH -> SrcNameLength > 0)
  {
    *in_cursor = pIMQRMH + pOMQRMH -> SrcNameOffset;
    pOMQRMH -> SrcNameOffset = (MQLONG)(*out_cursor - (PMQBYTE)pOMQRMH);

    ConvertCharIEP(pExitParms->pEntryPoints, pOMQRMH -> SrcNameLength);
  }

  if (pOMQRMH -> DestEnvLength > 0)
  {
    *in_cursor = pIMQRMH + pOMQRMH -> DestEnvOffset;
    pOMQRMH -> DestEnvOffset = (MQLONG)(*out_cursor - (PMQBYTE)pOMQRMH);

    ConvertCharIEP(pExitParms->pEntryPoints, pOMQRMH -> DestEnvLength);
  }

  if (pOMQRMH -> DestNameLength > 0)
  {
    *in_cursor = pIMQRMH + pOMQRMH -> DestNameOffset;
    pOMQRMH -> DestNameOffset = (MQLONG)(*out_cursor - (PMQBYTE)pOMQRMH);

    ConvertCharIEP(pExitParms->pEntryPoints, pOMQRMH -> DestNameLength);
  }

  *pOrigMQRMHLength = pOMQRMH -> StrucLength;
  pOMQRMH -> StrucLength = (MQLONG)(*out_cursor - (PMQBYTE)pOMQRMH);

  Fail:

  /*******************************************************************/
  /* Restore the input values of in_cursor and out_cursor            */
  /*******************************************************************/
  *in_cursor  = pIMQRMH;
  *out_cursor = (PMQBYTE)pOMQRMH;

  return(ReturnCode);
}

void MQStart(){;}
