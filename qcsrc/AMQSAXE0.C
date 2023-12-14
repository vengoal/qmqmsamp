/* %Z% %W% %I% %E% %U% */
 /********************************************************************/
 /*                                                                  */
 /* Program name: AMQSAXE0                                           */
 /*                                                                  */
 /* Description: Sample ApiExit which traces MQAPI calls             */
 /*   <copyright                                                     */
 /*   notice="lm-source-program"                                     */
 /*   pids="5724-H72"                                                */
 /*   years="1994,2016"                                              */
 /*   crc="47404793" >                                               */
 /*   Licensed Materials - Property of IBM                           */
 /*                                                                  */
 /*   5724-H72                                                       */
 /*                                                                  */
 /*   (C) Copyright IBM Corp. 1994, 2016 All Rights Reserved.        */
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
 /*   AMQSAXE is a sample C ApiExit which traces MQAPI calls         */
 /*                                                                  */
 /*      -- Configure the queue manager                              */
 /*                                                                  */
 /*         On UNIX add a stanza like the following to the           */
 /*         "qm.ini" file                                            */
 /*                                                                  */
 /*         ApiExitLocal:                                            */
 /*           Sequence=100                                           */
 /*           Function=EntryPoint                                    */
 /*           Module=<Module>                                        */
 /*           Name=SampleApiExit                                     */
 /*                                                                  */
 /*           ... where the <Module> is                              */
 /*                 AIX:         /usr/mqm/samp/bin/amqsaxe           */
 /*                 Other UNIXs: /opt/mqm/samp/bin/amqsaxe           */
 /*                 HP NonStop:  amqsaxe                             */
 /*                                                                  */
 /*         On Windows set the equivalent attributes in the registry */
 /*         using the MQ services snapin utility                     */
 /*                                                                  */
 /*      -- Make sure the Module is accessible to MQ                 */
 /*                                                                  */
 /*      -- Restart the Queue Manager to pick up these attributes    */
 /*                                                                  */
 /*      -- In the application process to be traced, describe where  */
 /*         the trace files should be written to.  For example:      */
 /*                                                                  */
 /*         On UNIX, make sure the directory "/var/mqm/trace" exists */
 /*         and export the following environment variable            */
 /*                                                                  */
 /*         "export MQAPI_TRACE_LOGFILE=/var/mqm/trace/ApiTrace"     */
 /*                                                                  */
 /*         On Windows, make sure the directory "C:\temp" exists     */
 /*         and set the following environment variable               */
 /*                                                                  */
 /*         "set MQAPI_TRACE_LOGFILE=C:\temp\ApiTrace"               */
 /*                                                                  */
 /*         On HP NonStop, export the following environment variable */
 /*                                                                  */
 /*         "export MQAPI_TRACE_LOGFILE=<ossdir>/apitrace" (OSS)     */
 /*         "param  MQAPITRACELOGFILE <ossdir>/apitrace"   (Guardian)*/
 /*                                                                  */
 /*      -- Optionally in the application process to be traced,      */
 /*         set the options by setting the following environment     */
 /*         variable                                                 */
 /*                                                                  */
 /*         On UNIX                                                  */
 /*                                                                  */
 /*         "export MQAPI_TRACE_OPTIONS=<value>"                     */
 /*                                                                  */
 /*         On Windows                                               */
 /*                                                                  */
 /*         "set MQAPI_TRACE_OPTIONS=<value>"                        */
 /*                                                                  */
 /*         On HP NonStop Server                                     */
 /*                                                                  */
 /*         "export MQAPI_TRACE_OPTIONS=<value>"   (OSS)             */
 /*         "param  MQAPITRACEOPTIONS <value>"     (Guardian)        */
 /*                                                                  */
 /*         Where <value> is a combination following:                */
 /*                                                                  */
 /*              1    Write context at the start of trace            */
 /*              2    Write context before each MQAPI call           */
 /*              4    Write context in all other cases               */
 /*              8    Write parms   at the start of trace            */
 /*             16    Write parms   before each MQAPI call           */
 /*             32    Write parms   in all other cases               */
 /*             64    Trace Before Dataconv on MQGET                 */
 /*            128    Dump ExitChainAreaPtr when dumping ExitParms   */
 /*                                                                  */
 /*         If MQAPI_TRACE_OPTIONS is not set and the ExitData is    */
 /*         not empty, the options will be set to the numeric value  */
 /*         of ExitData                                              */
 /*                                                                  */
 /*         Otherwise options default to "1"                         */
 /*                                                                  */
 /*   NOTE: On platforms that support separate threaded libraries    */
 /*   (ie AIX, HPUX and Linux) both a non-threaded and a threaded    */
 /*   version of an ApiExit module must be provided.  The threaded   */
 /*   version of the ApiExit module must have an "_r" suffix.  The   */
 /*   threaded version of the MQ Application Stub will implicitly    */
 /*   append "_r" to the given Module name before it is loaded.      */
 /*                                                                  */
 /********************************************************************/

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <limits.h>
 #include <ctype.h>
 #include <time.h>

 #include <cmqec.h>

 #if (MQAT_DEFAULT == MQAT_WINDOWS_NT)
 #include <windows.h>
 #include <winbase.h>
#else
  #if MQAT_DEFAULT == MQAT_MVS
    #define _XOPEN_SOURCE_EXTENDED 1
    #define _OPEN_MSGQ_EXT
 #endif
 #include <sys/time.h>
 #endif

 /********************************************************************/
 /* Macros to find the max or min of a pair of numbers               */
 /********************************************************************/
 #ifndef max
   #define max(a,b)        (((a) > (b)) ? (a) : (b))
 #endif

 #ifndef min
   #define min(a,b)        (((a) < (b)) ? (a) : (b))
 #endif

/*********************************************************************/
/* Definitions of data structures                                    */
/*********************************************************************/

typedef struct myExitUserArea
{
  FILE        * fp;
  MQLONG        Options;

#if (MQAT_DEFAULT == MQAT_WINDOWS_NT)
  LARGE_INTEGER PerformanceCounter;
  double        Frequency;
#endif

} MYEXITUSERAREA;

#define OPTIONS_DUMP_CONTEXT_AT_START    0x0001
#define OPTIONS_DUMP_CONTEXT_BEFORE      0x0002
#define OPTIONS_DUMP_CONTEXT_ALWAYS      0x0004
#define OPTIONS_DUMP_PARMS_AT_START      0x0008
#define OPTIONS_DUMP_PARMS_BEFORE        0x0010
#define OPTIONS_DUMP_PARMS_ALWAYS        0x0020
#define OPTIONS_DUMP_DATACONV            0x0040
#define OPTIONS_DUMP_EXITCHAINAREA       0x0080
#define OPTIONS_DEFAULT                  ( OPTIONS_DUMP_CONTEXT_AT_START  \
                                         )
#define TITLE_FORMAT        " %-25.25s %20.20s %26.26s\n"

/*********************************************************************/
/*                                                                   */
/* Standard MQ Entrypoint                                            */
/*                                                                   */
/*********************************************************************/

void MQStart()
{
  ;
}

/*********************************************************************/
/*                                                                   */
/* Convert an environment value into a string                        */
/*                                                                   */
/*********************************************************************/

char * strEnvironment ( MQLONG Environment, char *Buffer )
{
  char *p = NULL;

  switch (Environment)
  {
    case MQXE_OTHER:           p = "MQXE_OTHER";           break;
    case MQXE_MCA:             p = "MQXE_MCA";             break;
    case MQXE_MCA_SVRCONN:     p = "MQXE_MCA_SVRCONN";     break;
    case MQXE_COMMAND_SERVER:  p = "MQXE_COMMAND_SERVER";  break;
    case MQXE_MQSC:            p = "MQXE_MQSC";            break;
    default:
      sprintf(Buffer, "%d", Environment);
      p = Buffer;
  }

  return p;
}

/*********************************************************************/
/*                                                                   */
/* Convert an appl type value into a string                          */
/*                                                                   */
/*********************************************************************/

char * strApplType ( MQLONG ApplType, char *Buffer )
{
  char *p = NULL;

  switch (ApplType)
  {
    case MQAT_UNKNOWN:          p = "MQAT_UNKNOWN";          break;
    case MQAT_NO_CONTEXT:       p = "MQAT_NO_CONTEXT";       break;
    case MQAT_CICS:             p = "MQAT_CICS";             break;
    case MQAT_MVS:              p = "MQAT_MVS";              break;
    case MQAT_IMS:              p = "MQAT_IMS";              break;
    case MQAT_OS2:              p = "MQAT_OS2";              break;
    case MQAT_DOS:              p = "MQAT_DOS";              break;
    case MQAT_UNIX:             p = "MQAT_UNIX";             break;
    case MQAT_QMGR:             p = "MQAT_QMGR";             break;
    case MQAT_OS400:            p = "MQAT_OS400";            break;
    case MQAT_WINDOWS:          p = "MQAT_WINDOWS";          break;
    case MQAT_CICS_VSE:         p = "MQAT_CICS_VSE";         break;
    case MQAT_WINDOWS_NT:       p = "MQAT_WINDOWS_NT";       break;
    case MQAT_VMS:              p = "MQAT_VMS";              break;
    case MQAT_NSK:              p = "MQAT_NSK";              break;
    case MQAT_VOS:              p = "MQAT_VOS";              break;
    case MQAT_IMS_BRIDGE:       p = "MQAT_IMS_BRIDGE";       break;
    case MQAT_XCF:              p = "MQAT_XCF";              break;
    case MQAT_CICS_BRIDGE:      p = "MQAT_CICS_BRIDGE";      break;
    case MQAT_NOTES_AGENT:      p = "MQAT_NOTES_AGENT";      break;
    case MQAT_BROKER:           p = "MQAT_BROKER";           break;
    case MQAT_JAVA:             p = "MQAT_JAVA";             break;
    case MQAT_DQM:              p = "MQAT_DQM";              break;
    case MQAT_SYSTEM_EXTENSION: p = "MQAT_SYSTEM_EXTENSION"; break;
    default:
      sprintf(Buffer, "%d", ApplType);
      p = Buffer;
  }

  return p;
}

/*********************************************************************/
/*                                                                   */
/* Convert an ExitId value into a string                             */
/*                                                                   */
/*********************************************************************/

char * strExitId ( MQLONG ExitId, char *Buffer )
{
  char *p = NULL;

  switch (ExitId)
  {
    case MQXT_API_EXIT:               p = "MQXT_API_EXIT";               break;
    case MQXT_CHANNEL_SEC_EXIT:       p = "MQXT_CHANNEL_SEC_EXIT";       break;
    case MQXT_CHANNEL_MSG_EXIT:       p = "MQXT_CHANNEL_MSG_EXIT";       break;
    case MQXT_CHANNEL_SEND_EXIT:      p = "MQXT_CHANNEL_SEND_EXIT";      break;
    case MQXT_CHANNEL_RCV_EXIT:       p = "MQXT_CHANNEL_RCV_EXIT";       break;
    case MQXT_CHANNEL_MSG_RETRY_EXIT: p = "MQXT_CHANNEL_MSG_RETRY_EXIT"; break;
    case MQXT_CHANNEL_AUTO_DEF_EXIT:  p = "MQXT_CHANNEL_AUTO_DEF_EXIT";  break;
    case MQXT_CLUSTER_WORKLOAD_EXIT:  p = "MQXT_CLUSTER_WORKLOAD_EXIT";  break;
    case MQXT_PUBSUB_ROUTING_EXIT:    p = "MQXT_PUBSUB_ROUTING_EXIT";    break;
    default:
      sprintf(Buffer, "%d", ExitId);
      p = Buffer;
  }

  return p;
}

/*********************************************************************/
/*                                                                   */
/* Convert an ExitReason into a string                               */
/*                                                                   */
/*********************************************************************/

char * strExitReason ( MQLONG ExitReason, char *Buffer )
{
  char *p = NULL;

  switch (ExitReason)
  {
    case MQXR_BEFORE:           p = "MQXR_BEFORE";          break;
    case MQXR_AFTER:            p = "MQXR_AFTER";           break;
    case MQXR_CONNECTION:       p = "MQXR_CONNECTION";      break;
    case MQXR_INIT:             p = "MQXR_INIT";            break;
    case MQXR_TERM:             p = "MQXR_TERM";            break;
    case MQXR_MSG:              p = "MQXR_MSG";             break;
    case MQXR_XMIT:             p = "MQXR_XMIT";            break;
    case MQXR_SEC_MSG:          p = "MQXR_SEC_MSG";         break;
    case MQXR_INIT_SEC:         p = "MQXR_INIT_SEC";        break;
    case MQXR_RETRY:            p = "MQXR_RETRY";           break;
    case MQXR_AUTO_CLUSSDR:     p = "MQXR_AUTO_CLUSSDR";    break;
    case MQXR_AUTO_RECEIVER:    p = "MQXR_AUTO_RECEIVER";   break;
    case MQXR_CLWL_OPEN:        p = "MQXR_CLWL_OPEN";       break;
    case MQXR_CLWL_PUT:         p = "MQXR_CLWL_PUT";        break;
    case MQXR_CLWL_MOVE:        p = "MQXR_CLWL_MOVE";       break;
    case MQXR_CLWL_REPOS:       p = "MQXR_CLWL_REPOS";      break;
    case MQXR_CLWL_REPOS_MOVE:  p = "MQXR_CLWL_REPOS_MOVE"; break;
    case MQXR_END_BATCH:        p = "MQXR_END_BATCH";       break;
    case MQXR_ACK_RECEIVED:     p = "MQXR_ACK_RECEIVED";    break;
    case MQXR_AUTO_SVRCONN:     p = "MQXR_AUTO_SVRCONN";    break;
    case MQXR_AUTO_CLUSRCVR:    p = "MQXR_AUTO_CLUSRCVR";   break;
    default:
      sprintf(Buffer, "%d", ExitReason);
      p = Buffer;
  }

  return p;
}

/*********************************************************************/
/*                                                                   */
/* Convert an ExitResponse into a string                             */
/*                                                                   */
/*********************************************************************/

char * strExitResponse ( MQLONG ExitResponse, char *Buffer )
{
  char *p = NULL;

  switch (ExitResponse)
  {
    case MQXCC_OK:                       p = "MQXCC_OK";                       break;
    case MQXCC_SUPPRESS_FUNCTION:        p = "MQXCC_SUPPRESS_FUNCTION";        break;
    case MQXCC_SKIP_FUNCTION:            p = "MQXCC_SKIP_FUNCTION";            break;
    case MQXCC_SEND_AND_REQUEST_SEC_MSG: p = "MQXCC_SEND_AND_REQUEST_SEC_MSG"; break;
    case MQXCC_SEND_SEC_MSG:             p = "MQXCC_SEND_SEC_MSG";             break;
    case MQXCC_SUPPRESS_EXIT:            p = "MQXCC_SUPPRESS_EXIT";            break;
    case MQXCC_CLOSE_CHANNEL:            p = "MQXCC_CLOSE_CHANNEL";            break;
    case MQXCC_REQUEST_ACK:              p = "MQXCC_REQUEST_ACK";              break;
    case MQXCC_FAILED:                   p = "MQXCC_FAILED";                   break;
    default:
      sprintf(Buffer, "%d", ExitResponse);
      p = Buffer;
  }

  return p;
}

/*********************************************************************/
/*                                                                   */
/* Convert an ExitResponse2 into a string                            */
/*                                                                   */
/*********************************************************************/

char * strExitResponse2 ( MQLONG ExitResponse2, char *Buffer )
{
  char *p = NULL;

  switch (ExitResponse2)
  {
    case MQXR2_DEFAULT_CONTINUATION: p = "MQXR2_DEFAULT_CONTINUATION"; break;
    case MQXR2_CONTINUE_CHAIN:       p = "MQXR2_CONTINUE_CHAIN";       break;
    case MQXR2_SUPPRESS_CHAIN:       p = "MQXR2_SUPPRESS_CHAIN";       break;
    default:
      sprintf(Buffer, "%d", ExitResponse2);
      p = Buffer;
  }

  return p;
}

/*********************************************************************/
/*                                                                   */
/* Convert an APICallerType into a string                            */
/*                                                                   */
/*********************************************************************/

char * strAPICallerType ( MQLONG ApiCallerType, char *Buffer )
{
  char *p = NULL;

  switch (ApiCallerType)
  {
    case MQXACT_EXTERNAL:   p = "MQXACT_EXTERNAL";   break;
    case MQXACT_INTERNAL:   p = "MQXACT_INTERNAL";   break;
    default:
      sprintf(Buffer, "%d", ApiCallerType);
      p = Buffer;
  }

  return p;
}

/*********************************************************************/
/*                                                                   */
/* Convert an Function Id into a string                              */
/*                                                                   */
/*********************************************************************/

char * strFunctionId ( MQLONG Function, char *Buffer )
{
  char *p = NULL;

  switch (Function)
  {
    case MQXF_INIT:             p = "MQXF_INIT";             break;
    case MQXF_TERM:             p = "MQXF_TERM";             break;
    case MQXF_CONN:             p = "MQXF_CONN";             break;
    case MQXF_CONNX:            p = "MQXF_CONNX";            break;
    case MQXF_DISC:             p = "MQXF_DISC";             break;
    case MQXF_OPEN:             p = "MQXF_OPEN";             break;
    case MQXF_SUB:              p = "MQXF_SUB";              break;
    case MQXF_SUBRQ:            p = "MQXF_SUBRQ";            break;
    case MQXF_CB:               p = "MQXF_CB";               break;
    case MQXF_CTL:              p = "MQXF_CTL";              break;
    case MQXF_CALLBACK:         p = "MQXF_CALLBACK";         break;
    case MQXF_CLOSE:            p = "MQXF_CLOSE";            break;
    case MQXF_PUT1:             p = "MQXF_PUT1";             break;
    case MQXF_PUT:              p = "MQXF_PUT";              break;
    case MQXF_GET:              p = "MQXF_GET";              break;
    case MQXF_DATA_CONV_ON_GET: p = "MQXF_DATA_CONV_ON_GET"; break;
    case MQXF_INQ:              p = "MQXF_INQ";              break;
    case MQXF_SET:              p = "MQXF_SET";              break;
    case MQXF_BEGIN:            p = "MQXF_BEGIN";            break;
    case MQXF_CMIT:             p = "MQXF_CMIT";             break;
    case MQXF_BACK:             p = "MQXF_BACK";             break;
    case MQXF_STAT:             p = "MQXF_STAT";             break;
    case MQXF_XACLOSE:          p = "MQXF_XACLOSE";          break;
    case MQXF_XACOMMIT:         p = "MQXF_XACOMMIT";         break;
    case MQXF_XACOMPLETE:       p = "MQXF_XACOMPLETE";       break;
    case MQXF_XAEND:            p = "MQXF_XAEND";            break;
    case MQXF_XAFORGET:         p = "MQXF_XAFORGET";         break;
    case MQXF_XAOPEN:           p = "MQXF_XAOPEN";           break;
    case MQXF_XAPREPARE:        p = "MQXF_XAPREPARE";        break;
    case MQXF_XARECOVER:        p = "MQXF_XARECOVER";        break;
    case MQXF_XAROLLBACK:       p = "MQXF_XAROLLBACK";       break;
    case MQXF_XASTART:          p = "MQXF_XASTART";          break;
    case MQXF_AXREG:            p = "MQXF_AXREG";            break;
    case MQXF_AXUNREG:          p = "MQXF_AXUNREG";          break;
    default:
      sprintf(Buffer, "%d", Function);
      p = Buffer;
  }

  return p;
}

/*********************************************************************/
/*                                                                   */
/* Convert an Feedback value into a string                           */
/*                                                                   */
/*********************************************************************/

char * strFeedback ( MQLONG Feedback, char *Buffer )
{
  char *p = NULL;

  switch (Feedback)
  {
    case MQFB_NONE:                   p = "MQFB_NONE";                   break;
    case MQFB_QUIT:                   p = "MQFB_QUIT";                   break;
    case MQFB_EXPIRATION:             p = "MQFB_EXPIRATION";             break;
    case MQFB_COA:                    p = "MQFB_COA";                    break;
    case MQFB_COD:                    p = "MQFB_COD";                    break;
    case MQFB_CHANNEL_COMPLETED:      p = "MQFB_CHANNEL_COMPLETED";      break;
    case MQFB_CHANNEL_FAIL_RETRY:     p = "MQFB_CHANNEL_FAIL_RETRY";     break;
    case MQFB_CHANNEL_FAIL:           p = "MQFB_CHANNEL_FAIL";           break;
    case MQFB_APPL_CANNOT_BE_STARTED: p = "MQFB_APPL_CANNOT_BE_STARTED"; break;
    case MQFB_TM_ERROR:               p = "MQFB_TM_ERROR";               break;
    case MQFB_APPL_TYPE_ERROR:        p = "MQFB_APPL_TYPE_ERROR";        break;
    case MQFB_STOPPED_BY_MSG_EXIT:    p = "MQFB_STOPPED_BY_MSG_EXIT";    break;
    case MQFB_XMIT_Q_MSG_ERROR:       p = "MQFB_XMIT_Q_MSG_ERROR";       break;
    case MQFB_PAN:                    p = "MQFB_PAN";                    break;
    case MQFB_NAN:                    p = "MQFB_NAN";                    break;
    case MQFB_STOPPED_BY_CHAD_EXIT:   p = "MQFB_STOPPED_BY_CHAD_EXIT";   break;
    case MQFB_STOPPED_BY_PUBSUB_EXIT: p = "MQFB_STOPPED_BY_PUBSUB_EXIT"; break;
    case MQFB_NOT_A_REPOSITORY_MSG:   p = "MQFB_NOT_A_REPOSITORY_MSG";   break;
    case MQFB_BIND_OPEN_CLUSRCVR_DEL: p = "MQFB_BIND_OPEN_CLUSRCVR_DEL"; break;
    case MQFB_DATA_LENGTH_ZERO:       p = "MQFB_DATA_LENGTH_ZERO";       break;
    case MQFB_DATA_LENGTH_NEGATIVE:   p = "MQFB_DATA_LENGTH_NEGATIVE";   break;
    case MQFB_DATA_LENGTH_TOO_BIG:    p = "MQFB_DATA_LENGTH_TOO_BIG";    break;
    case MQFB_BUFFER_OVERFLOW:        p = "MQFB_BUFFER_OVERFLOW";        break;
    case MQFB_LENGTH_OFF_BY_ONE:      p = "MQFB_LENGTH_OFF_BY_ONE";      break;
    case MQFB_IIH_ERROR:              p = "MQFB_IIH_ERROR";              break;
    case MQFB_NOT_AUTHORIZED_FOR_IMS: p = "MQFB_NOT_AUTHORIZED_FOR_IMS"; break;
    case MQFB_IMS_ERROR:              p = "MQFB_IMS_ERROR";              break;
    case MQFB_IMS_FIRST:              p = "MQFB_IMS_FIRST";              break;
    case MQFB_IMS_LAST:               p = "MQFB_IMS_LAST";               break;
    case MQFB_CICS_INTERNAL_ERROR:    p = "MQFB_CICS_INTERNAL_ERROR";    break;
    case MQFB_CICS_NOT_AUTHORIZED:    p = "MQFB_CICS_NOT_AUTHORIZED";    break;
    case MQFB_CICS_BRIDGE_FAILURE:    p = "MQFB_CICS_BRIDGE_FAILURE";    break;
    case MQFB_CICS_CORREL_ID_ERROR:   p = "MQFB_CICS_CORREL_ID_ERROR";   break;
    case MQFB_CICS_CCSID_ERROR:       p = "MQFB_CICS_CCSID_ERROR";       break;
    case MQFB_CICS_ENCODING_ERROR:    p = "MQFB_CICS_ENCODING_ERROR";    break;
    case MQFB_CICS_CIH_ERROR:         p = "MQFB_CICS_CIH_ERROR";         break;
    case MQFB_CICS_UOW_ERROR:         p = "MQFB_CICS_UOW_ERROR";         break;
    case MQFB_CICS_COMMAREA_ERROR:    p = "MQFB_CICS_COMMAREA_ERROR";    break;
    case MQFB_CICS_APPL_NOT_STARTED:  p = "MQFB_CICS_APPL_NOT_STARTED";  break;
    case MQFB_CICS_APPL_ABENDED:      p = "MQFB_CICS_APPL_ABENDED";      break;
    case MQFB_CICS_DLQ_ERROR:         p = "MQFB_CICS_DLQ_ERROR";         break;
    case MQFB_CICS_UOW_BACKED_OUT:    p = "MQFB_CICS_UOW_BACKED_OUT";    break;
    case MQFB_ACTIVITY:               p = "MQFB_ACTIVITY";               break;
    case MQFB_MAX_ACTIVITIES:         p = "MQFB_MAX_ACTIVITIES";         break;
    case MQFB_NOT_FORWARDED:          p = "MQFB_NOT_FORWARDED";          break;
    case MQFB_NOT_DELIVERED:          p = "MQFB_NOT_DELIVERED";          break;
    case MQFB_UNSUPPORTED_FORWARDING: p = "MQFB_UNSUPPORTED_FORWARDING"; break;
    case MQFB_UNSUPPORTED_DELIVERY:   p = "MQFB_UNSUPPORTED_DELIVERY";   break;
    default:
      sprintf(Buffer, "%d", Feedback);
      p = Buffer;
  }

  return p;
}
/*********************************************************************/
/*                                                                   */
/* Convert a ChannelType into a string                               */
/*                                                                   */
/*********************************************************************/
char * strChannelType ( MQLONG Type, char *Buffer )
{
  char *p = NULL;

  switch (Type)
  {
    case MQCHT_SENDER:          p = "MQCHT_SENDER";      break;
    case MQCHT_SERVER:          p = "MQCHT_SERVER";      break;
    case MQCHT_RECEIVER:        p = "MQCHT_RECEIVER";    break;
    case MQCHT_REQUESTER:       p = "MQCHT_REQUESTER";   break;
    case MQCHT_ALL:             p = "MQCHT_ALL";         break;
    case MQCHT_CLNTCONN:        p = "MQCHT_CLNTCONN";    break;
    case MQCHT_SVRCONN:         p = "MQCHT_SVRCONN";     break;
    case MQCHT_CLUSRCVR:        p = "MQCHT_CLUSRCVR";    break;
    case MQCHT_CLUSSDR:         p = "MQCHT_CLUSSDR";     break;
    case MQCHT_MQTT:            p = "MQCHT_MQTT";        break;
    case MQCHT_AMQP:            p = "MQCHT_AMQP";        break;
    default:
      sprintf(Buffer, "%d", Type);
      p = Buffer;
  }

  return p;
}


/*********************************************************************/
/*                                                                   */
/* Check to see if a string is blank                                 */
/*                                                                   */
/*********************************************************************/

MQLONG myBlankCheck ( void * DataPointer, size_t DataLength )
{
  size_t i      = 0;
  MQLONG result = 1;
  MQLONG more   = 1;
  char * string = DataPointer;

  for (i = 0; more && (i < DataLength); i++)
  {
    switch (string[i])
    {
      case 0:
        more   = 0;
        break;

      case ' ':
        break;

      default:
        result = 0;
        more   = 0;
        break;
    }
  }

  return result;
}

/*********************************************************************/
/*                                                                   */
/* Check a pointer is valid                                          */
/*                                                                   */
/*********************************************************************/

MQLONG myCheckPointer ( void * DataPointer )
{
  if (DataPointer)
    return 1;
  else
    return 0;
}

/*********************************************************************/
/*                                                                   */
/* Return a string representation of a pointer                       */
/*                                                                   */
/*********************************************************************/

char * strptr ( void *Pointer, char *format, char * StringBuffer )
{
  if (myCheckPointer( Pointer ))
  {
    sprintf( StringBuffer, format, Pointer);
    return StringBuffer;
  }
  else
    return "(null)";
}

/*********************************************************************/
/*                                                                   */
/* Return a string representation of a pointer                       */
/*                                                                   */
/*********************************************************************/

char * strpptr ( void *Pointer, char *format, char * StringBuffer )
{
  if (myCheckPointer( Pointer ))
  {
    void **pPointer = Pointer;
    return strptr( *pPointer, format, StringBuffer );
  }
  else
    return "(null)";
}

/*********************************************************************/
/*                                                                   */
/* Initialise the Windows Performance counter                        */
/*                                                                   */
/*********************************************************************/

#if (MQAT_DEFAULT == MQAT_WINDOWS_NT)

int InitPerformanceCounter( MYEXITUSERAREA * pExitUserArea )
{
  LARGE_INTEGER PerformanceFrequency;
  QueryPerformanceFrequency( &PerformanceFrequency );
  pExitUserArea->Frequency = (double) PerformanceFrequency.QuadPart;

  QueryPerformanceCounter( &pExitUserArea->PerformanceCounter );

  return 0;
}

#endif

/*********************************************************************/
/*                                                                   */
/* Make a timestamp string using the current time                    */
/*                                                                   */
/*********************************************************************/

void myGetAbsoluteTime ( char * StringBuffer, size_t StringBufferLength )
{
  time_t long_time;
  struct tm *newtime;

  time( &long_time );
  newtime = localtime( &long_time );

  strftime( StringBuffer, StringBufferLength, "%Y-%m-%d  %H:%M:%S", newtime );
}

void myGetRelativeTime ( char           * StringBuffer
                       , MYEXITUSERAREA * pExitUserArea
                       )
{
#if (MQAT_DEFAULT == MQAT_WINDOWS_NT)
  LARGE_INTEGER PerformanceCounter;
  double        Counter;
  double        Period;

  QueryPerformanceCounter( &PerformanceCounter ) ;

  Counter   = (double) ( PerformanceCounter.QuadPart - pExitUserArea->PerformanceCounter.QuadPart );
  Period    = Counter * 1000000.0 / pExitUserArea->Frequency;

  sprintf( StringBuffer, "%.0lf", Period );
#else
  struct timeval t1;
  gettimeofday(&t1, NULL );
  sprintf( StringBuffer, "%ld.%06ld", t1.tv_sec, t1.tv_usec );
#endif
}

/*********************************************************************/
/*                                                                   */
/* Format a block of data into hex and dump to a file                */
/*                                                                   */
/* Offset    Data                                                    */
/* XXXXXXXX: XXXXXXXX XXXXXXXX XXXXXXXX XXXXXXXX   ................  */
/* 0         1         2         3         4         5         6         7   */
/* 012345678901234567890123456789012345678901234567890123456789012345678901  */
/*                                                                   */
/*********************************************************************/

char HEX[] = "0123456789ABCDEF";

void DumpHex( FILE   * fp
            , void   * DataPointer
            , size_t   DataLength
            )
{
  if (myCheckPointer( DataPointer ))
  {
    char             StringBuffer[100];
    unsigned char  * pd;
    unsigned char  * pa;
    unsigned char  * p           = DataPointer;
    size_t           PointerSize = sizeof(void*);
    size_t           DataOffset  = (PointerSize * 2) + 2;
    size_t           AsciiOffset = DataOffset  + 38;
    size_t           LineLength  = AsciiOffset + 16;
    size_t           index, i;
    int              n;

    sprintf( &StringBuffer[ LineLength ], "\n" );

    index = 0;
    while (index < DataLength)
    {
      pd = (unsigned char*) StringBuffer + DataOffset;
      pa = (unsigned char*) StringBuffer + AsciiOffset;

      memset( StringBuffer, ' ', LineLength);
      n = sprintf(StringBuffer, "%p", p);
      StringBuffer[ n ] = ':';

      for (i = 0; (i < 16) && (index < DataLength); i++)
      {
        if ((i % 4) == 0) pd++;

        *pd++ = HEX[ *p / 16 ];
        *pd++ = HEX[ *p % 16 ];
        *pa++ = isprint(*p) ? *p : '.';

        index++;
        p++;
      }

      fprintf(fp, "          %s", StringBuffer);
    }
  }
}

/*********************************************************************/
/*                                                                   */
/* Dump the ExitChainAreaPtr                                         */
/*                                                                   */
/*********************************************************************/

void DumpExitChainArea( FILE * fp, PMQAXP pExitParms )
{
  MYEXITUSERAREA ** ppExitUserArea   = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea    = *ppExitUserArea;
  MQACH           * ExitChainAreaPtr = pExitParms->ExitChainAreaPtr;

  if (  (pExitUserArea->Options & OPTIONS_DUMP_EXITCHAINAREA)
     && (ExitChainAreaPtr)
     )
  {
    fprintf( fp, "   --------------------------\n" );
    fprintf( fp, "   ExitChainAreaPtr:       %p\n",    ExitChainAreaPtr );

    while (ExitChainAreaPtr)
    {
      fprintf( fp, "    StrucId:               %.4s\n",  ExitChainAreaPtr->StrucId );

      if (memcmp( ExitChainAreaPtr->StrucId, MQACH_STRUC_ID, 4))
      {
        fprintf( fp, "ERROR *** BAD MQACH StrucId ***\n" );
        ExitChainAreaPtr = NULL;
      }
      else
      {
        fprintf( fp, "    Version:               %d\n",    ExitChainAreaPtr->Version          );
        fprintf( fp, "    StrucLength:           %d\n",    ExitChainAreaPtr->StrucLength      );
        fprintf( fp, "    ChainAreaLength:       %d\n",    ExitChainAreaPtr->ChainAreaLength  );
        fprintf( fp, "    ExitInfoName:          %.48s\n", ExitChainAreaPtr->ExitInfoName     );
        fprintf( fp, "    NextChainAreaPtr:      %p\n",    ExitChainAreaPtr->NextChainAreaPtr );

        ExitChainAreaPtr = ExitChainAreaPtr->NextChainAreaPtr;

        fprintf( fp, "   --------------------------\n" );
        fprintf( fp, "   ExitChainAreaPtr:       %p\n",    ExitChainAreaPtr );
      }
    }

    fprintf( fp, "   --------------------------\n" );
  }
  else
    fprintf( fp, "   ExitChainAreaPtr:       %p\n",    ExitChainAreaPtr );
}

/*********************************************************************/
/*                                                                   */
/* Dump the ApiExit Params                                           */
/*                                                                   */
/*********************************************************************/

void MQENTRY DumpParms ( FILE *fp, PMQAXP pExitParms )
{
  char buffer[50] = "";

  fprintf( fp, "  PARMS\n" );
  fprintf( fp, "   ExitId:                 %s\n",    strExitId( pExitParms->ExitId, buffer ) );
  fprintf( fp, "   ExitReason:             %s\n",    strExitReason( pExitParms->ExitReason, buffer ) );
  fprintf( fp, "   ExitResponse:           %s\n",    strExitResponse( pExitParms->ExitResponse, buffer ) );
  fprintf( fp, "   ExitResponse2:          %s\n",    strExitResponse2( pExitParms->ExitResponse2, buffer ) );
  fprintf( fp, "   Feedback:               %s\n",    strFeedback( pExitParms->Feedback, buffer ) );
  fprintf( fp, "   APICallerType:          %s\n",    strAPICallerType( pExitParms->APICallerType, buffer ) );
  fprintf( fp, "   ExitUserArea:           %p\n",    pExitParms->ExitUserArea );
  DumpHex(fp, pExitParms->ExitUserArea, MQ_EXIT_USER_AREA_LENGTH);
  fprintf( fp, "   ExitData:               %.32s\n", pExitParms->ExitData );
  fprintf( fp, "   ExitInfoName:           %.48s\n", pExitParms->ExitInfoName );
  fprintf( fp, "   ExitPDArea:             %p\n",    pExitParms->ExitPDArea );
  DumpHex(fp, pExitParms->ExitPDArea, MQ_EXIT_PD_AREA_LENGTH);
  fprintf( fp, "   QMgrName:               %.48s\n", pExitParms->QMgrName );
  DumpExitChainArea( fp, pExitParms );
  fprintf( fp, "   Hconfig:                %p\n",    pExitParms->Hconfig );
  fprintf( fp, "   Function:               %s\n",    strFunctionId( pExitParms->Function, buffer ) );
}

/*********************************************************************/
/*                                                                   */
/* Dump the ApiExit Context                                          */
/*                                                                   */
/*********************************************************************/

void MQENTRY DumpContext ( FILE *fp, PMQAXC pExitContext )
{
  char buffer[50] = "";

  fprintf( fp, "  CONTEXT\n" );
  fprintf( fp, "   Environment:            %s\n", strEnvironment( pExitContext->Environment, buffer ) );
  fprintf( fp, "   UserId:                 %.12s\n", pExitContext->UserId );
  fprintf( fp, "   SecurityId:\n" );
  DumpHex(fp, pExitContext->SecurityId, MQ_SECURITY_ID_LENGTH);
  fprintf( fp, "   ConnectionName:         %.264s\n", pExitContext->ConnectionName );
  fprintf( fp, "   LongMCAUserIdLength:    %d\n", pExitContext->LongMCAUserIdLength );
  DumpHex(fp, pExitContext->LongMCAUserIdPtr, pExitContext->LongMCAUserIdLength);
  fprintf( fp, "   LongRemoteUserIdLength: %d\n", pExitContext->LongRemoteUserIdLength );
  DumpHex(fp, pExitContext->LongRemoteUserIdPtr, pExitContext->LongRemoteUserIdLength);
  fprintf( fp, "   ApplName:               %.28s\n", pExitContext->ApplName );
  fprintf( fp, "   ApplType:               %s\n", strApplType( pExitContext->ApplType, buffer ) );
  fprintf( fp, "   ProcessId:              %d\n", pExitContext->ProcessId );
  fprintf( fp, "   ThreadId:               %d\n", pExitContext->ThreadId );

  if (pExitContext -> Version >= MQAXC_VERSION_2)
  {
    if (pExitContext->ChannelName[0])
    {
      fprintf( fp, "   ChannelName:            %.20s\n", pExitContext->ChannelName );
    }
    if (pExitContext->pChannelDefinition)
    {
      fprintf( fp, "   ChannelType:            %s\n",
               strChannelType(pExitContext->pChannelDefinition->ChannelType, buffer));
    }
  }
}

/*********************************************************************/
/*                                                                   */
/* Write out hex numnber                                             */
/*                                                                   */
/*********************************************************************/
void fprint_hex(FILE * fp, char * name, MQLONG * pValue)
{
  if (pValue) fprintf(fp, "  %14s: (%p) 0x%X\n", name, pValue, *pValue);
         else fprintf(fp, "  %14s: (NULL)\n", name);
}
/*********************************************************************/
/*                                                                   */
/* Write out hex numnber                                             */
/*                                                                   */
/*********************************************************************/
void fprint_phex(FILE * fp, char * name, MQLONG ** pValue)
{
  if (pValue) fprint_hex(fp,name,*pValue);
         else fprintf(fp, "  %14s: (NULL)\n", name);
}
/*********************************************************************/
/*                                                                   */
/* Write out int numnber                                             */
/*                                                                   */
/*********************************************************************/
void fprint_int(FILE * fp, char * name, MQLONG * pValue)
{
  if (pValue) fprintf(fp, "  %14s: (%p) %d\n", name, pValue, *pValue);
         else fprintf(fp, "  %14s: (NULL>\n", name);
}
/*********************************************************************/
/*                                                                   */
/* Write out int numnber                                             */
/*                                                                   */
/*********************************************************************/
void fprint_pint(FILE * fp, char * name, MQLONG ** pValue)
{
  if (pValue) fprint_int(fp, name, *pValue);
         else fprintf(fp, "  %14s: (NULL>\n", name);
}
/*********************************************************************/
/*                                                                   */
/* Before MQBACK Entrypoint                                          */
/*                                                                   */
/*********************************************************************/

MQ_BACK_EXIT BackBefore;

void MQENTRY BackBefore  ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , PMQHCONN  pHconn
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "MQBACK\n");
  fprintf(fp, TITLE_FORMAT, "BEFORE", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);

  return;
}

/*********************************************************************/
/*                                                                   */
/* Before MQBACK Entrypoint                                          */
/*                                                                   */
/*********************************************************************/

MQ_BACK_EXIT BackAfter;

void MQENTRY BackAfter  ( PMQAXP    pExitParms
                        , PMQAXC    pExitContext
                        , PMQHCONN  pHconn
                        , PMQLONG   pCompCode
                        , PMQLONG   pReason
                        )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "AFTER", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprint_int(fp, "CompCode", pCompCode);
  fprint_int(fp, "Reason"  , pReason);
  fprintf(fp, "--------------------------------------------------------------------------\n");

  return;
}

/*********************************************************************/
/*                                                                   */
/* Before MQBEGIN Entrypoint                                         */
/*                                                                   */
/*********************************************************************/

MQ_BEGIN_EXIT BeginBefore;

void MQENTRY BeginBefore ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , PMQHCONN  pHconn
                         , PPMQBO    ppBeginOptions
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "MQBEGIN\n");
  fprintf(fp, TITLE_FORMAT, "BEFORE", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprintf(fp, "  BeginOptions  : %p\n",   *ppBeginOptions );
  DumpHex(fp, *ppBeginOptions, sizeof(MQBO));

  return;
}

/*********************************************************************/
/*                                                                   */
/* Before MQBEGIN Entrypoint                                         */
/*                                                                   */
/*********************************************************************/

MQ_BEGIN_EXIT BeginAfter;

void MQENTRY BeginAfter  ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , PMQHCONN  pHconn
                         , PPMQBO    ppBeginOptions
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "AFTER", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprintf(fp, "  BeginOptions  : %p\n", *ppBeginOptions );
  DumpHex(fp, *ppBeginOptions, sizeof(MQBO));
  fprint_int(fp, "CompCode", pCompCode);
  fprint_int(fp, "Reason"  , pReason);
  fprintf(fp, "--------------------------------------------------------------------------\n");

  return;
}

/*********************************************************************/
/*                                                                   */
/* Before MQCLOSE Entrypoint                                         */
/*                                                                   */
/*********************************************************************/

MQ_CLOSE_EXIT CloseBefore;

void MQENTRY CloseBefore ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , PMQHCONN  pHconn
                         , PPMQHOBJ  ppHobj
                         , PMQLONG   pOptions
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "MQCLOSE\n");
  fprintf(fp, TITLE_FORMAT, "BEFORE", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprint_hex (fp, "Hconn", pHconn);
  fprint_phex(fp, "Hobj" , ppHobj);
  fprintf(fp, "  Options       : %d\n", *pOptions  );

  return;
}

/*********************************************************************/
/*                                                                   */
/* After MQCLOSE Entrypoint                                          */
/*                                                                   */
/*********************************************************************/

MQ_CLOSE_EXIT CloseAfter;

void MQENTRY CloseAfter  ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , PMQHCONN  pHconn
                         , PPMQHOBJ  ppHobj
                         , PMQLONG   pOptions
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "AFTER", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprint_phex(fp, "Hobj" , ppHobj);
  fprint_int(fp, "CompCode", pCompCode);
  fprint_int(fp, "Reason"  , pReason);
  fprintf(fp, "--------------------------------------------------------------------------\n");

  return;
}

/*********************************************************************/
/*                                                                   */
/* Before MQCMIT Entrypoint                                          */
/*                                                                   */
/*********************************************************************/

MQ_CMIT_EXIT CmitBefore;

void MQENTRY CmitBefore  ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , PMQHCONN  pHconn
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "MQCMIT\n");
  fprintf(fp, TITLE_FORMAT, "BEFORE", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);

  return;
}

/*********************************************************************/
/*                                                                   */
/* After MQCMIT Entrypoint                                           */
/*                                                                   */
/*********************************************************************/

MQ_CMIT_EXIT CmitAfter;

void MQENTRY CmitAfter   ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , PMQHCONN  pHconn
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "AFTER", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprint_int(fp, "CompCode", pCompCode);
  fprint_int(fp, "Reason"  , pReason);
  fprintf(fp, "--------------------------------------------------------------------------\n");

  return;
}

/*********************************************************************/
/*                                                                   */
/* Before MQCONN Entrypoint                                          */
/*                                                                   */
/*********************************************************************/

MQ_CONNX_EXIT ConnBefore;

void MQENTRY ConnBefore  ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , MQCHAR48  QMgrName
                         , PPMQCNO   ppConnectOpts
                         , PPMQHCONN ppHconn
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "MQCONN\n");
  fprintf(fp, TITLE_FORMAT, "BEFORE", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprintf(fp, "  QMgrName      : %.48s\n", QMgrName      );
  fprintf(fp, "  ConnectOpts   : 0x%p\n", *ppConnectOpts );
  DumpHex(fp, *ppConnectOpts, sizeof(MQCNO));

  return;
}

/*********************************************************************/
/*                                                                   */
/* After MQCONN Entrypoint                                           */
/*                                                                   */
/*********************************************************************/

MQ_CONNX_EXIT ConnAfter;

void MQENTRY ConnAfter   ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , MQCHAR48  QMgrName
                         , PPMQCNO   ppConnectOpts
                         , PPMQHCONN ppHconn
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "AFTER", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprint_phex(fp, "Hconn", ppHconn);
  fprint_int(fp, "CompCode", pCompCode);
  fprint_int(fp, "Reason"  , pReason);
  fprintf(fp, "--------------------------------------------------------------------------\n");

  return;
}

/*********************************************************************/
/*                                                                   */
/* Before MQCONNX Entrypoint                                         */
/*                                                                   */
/*********************************************************************/

MQ_CONNX_EXIT ConnxBefore;

void MQENTRY ConnxBefore ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , MQCHAR48  QMgrName
                         , PPMQCNO   ppConnectOpts
                         , PPMQHCONN ppHconn
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "MQCONNX\n");
  fprintf(fp, TITLE_FORMAT, "BEFORE", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprintf(fp, "  QMgrName      : %.48s\n", QMgrName      );
  fprintf(fp, "  ConnectOpts   : 0x%p\n", *ppConnectOpts );
  DumpHex(fp, *ppConnectOpts, sizeof(MQCNO));

  return;
}

/*********************************************************************/
/*                                                                   */
/* After MQCONNX Entrypoint                                          */
/*                                                                   */
/*********************************************************************/

MQ_CONNX_EXIT ConnxAfter;

void MQENTRY ConnxAfter  ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , MQCHAR48  QMgrName
                         , PPMQCNO   ppConnectOpts
                         , PPMQHCONN ppHconn
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "AFTER", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprint_phex(fp, "Hconn", ppHconn);
  fprint_int(fp, "CompCode", pCompCode);
  fprint_int(fp, "Reason"  , pReason);
  fprintf(fp, "--------------------------------------------------------------------------\n");

  return;
}

/*********************************************************************/
/*                                                                   */
/* Before MQDISC Entrypoint                                          */
/*                                                                   */
/*********************************************************************/

MQ_DISC_EXIT DiscBefore;

void MQENTRY DiscBefore  ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , PPMQHCONN ppHconn
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "MQDISC\n");
  fprintf(fp, TITLE_FORMAT, "BEFORE", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprint_phex(fp, "Hconn", ppHconn);

  return;
}

/*********************************************************************/
/*                                                                   */
/* After MQDISC Entrypoint                                           */
/*                                                                   */
/*********************************************************************/

MQ_DISC_EXIT DiscAfter;

void MQENTRY DiscAfter   ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , PPMQHCONN ppHconn
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "AFTER", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprint_phex(fp, "Hconn", ppHconn);
  fprint_int(fp, "CompCode", pCompCode);
  fprint_int(fp, "Reason"  , pReason);
  fprintf(fp, "--------------------------------------------------------------------------\n");

  return;
}

/*********************************************************************/
/*                                                                   */
/* Before MQGET Entrypoint                                           */
/*                                                                   */
/*********************************************************************/

MQ_GET_EXIT GetBefore;

void MQENTRY GetBefore   ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , PMQHCONN  pHconn
                         , PMQHOBJ   pHobj
                         , PPMQMD    ppMsgDesc
                         , PPMQGMO   ppGetMsgOpts
                         , PMQLONG   pBufferLength
                         , PPMQVOID  ppBuffer
                         , PPMQLONG  ppDataLength
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "MQGET\n");
  fprintf(fp, TITLE_FORMAT, "BEFORE", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprint_hex(fp, "Hobj" , pHobj);
  fprintf(fp, "  MsgDesc       : %s\n", strptr(ppMsgDesc,    "0x%p",   buffer1));
  DumpHex(fp, *ppMsgDesc, sizeof(MQMD));
  fprintf(fp, "  GetMsgOpts    : %s\n", strptr(ppGetMsgOpts, "0x%p",   buffer1));
  DumpHex(fp, *ppGetMsgOpts, sizeof(MQGMO));
  fprint_int(fp, "BufferLength", pBufferLength);

  return;
}

/*********************************************************************/
/*                                                                   */
/* Before Data Conversion on MQGET Entrypoint                        */
/*                                                                   */
/*********************************************************************/

MQ_GET_EXIT GetBeforeDataConv;

void MQENTRY GetBeforeDataConv( PMQAXP    pExitParms
                              , PMQAXC    pExitContext
                              , PMQHCONN  pHconn
                              , PMQHOBJ   pHobj
                              , PPMQMD    ppMsgDesc
                              , PPMQGMO   ppGetMsgOpts
                              , PMQLONG   pBufferLength
                              , PPMQVOID  ppBuffer
                              , PPMQLONG  ppDataLength
                              , PMQLONG   pCompCode
                              , PMQLONG   pReason
                              )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "BEFORE DATA CONVERSION", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprintf(fp, "  MsgDesc       : %s\n", strptr(ppMsgDesc,     "0x%p", buffer1));
  DumpHex(fp, *ppMsgDesc, sizeof(MQMD));
  fprintf(fp, "  GetMsgOpts    : %s\n", strptr(ppGetMsgOpts,  "0x%p", buffer1));
  DumpHex(fp, *ppGetMsgOpts, sizeof(MQGMO));
  fprint_int (fp, "BufferLength", pBufferLength);
  fprint_pint(fp, "DataLength"  , ppDataLength);
  fprintf(fp, "  Buffer        : %s\n", strptr(ppBuffer,  "0x%p", buffer1));
  DumpHex(fp, *ppBuffer, min(1024, **ppDataLength));
  fprint_int(fp, "CompCode", pCompCode);
  fprint_int(fp, "Reason"  , pReason);

  return;
}

/*********************************************************************/
/*                                                                   */
/* After MQGET Entrypoint                                            */
/*                                                                   */
/*********************************************************************/

MQ_GET_EXIT GetAfter;

void MQENTRY GetAfter    ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , PMQHCONN  pHconn
                         , PMQHOBJ   pHobj
                         , PPMQMD    ppMsgDesc
                         , PPMQGMO   ppGetMsgOpts
                         , PMQLONG   pBufferLength
                         , PPMQVOID  ppBuffer
                         , PPMQLONG  ppDataLength
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "AFTER", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprintf(fp, "  MsgDesc       : %s\n", strptr(ppMsgDesc,     "0x%p", buffer1));
  DumpHex(fp, *ppMsgDesc, sizeof(MQMD));
  fprintf(fp, "  GetMsgOpts    : %s\n", strptr(ppGetMsgOpts,  "0x%p", buffer1));
  DumpHex(fp, *ppGetMsgOpts, sizeof(MQGMO));
  fprintf(fp, "  Buffer        : %s\n", strptr(ppBuffer,  "0x%p", buffer1));

  if (  myCheckPointer( pCompCode )
     && ((*pCompCode == MQCC_OK) || (*pCompCode == MQCC_WARNING))
     )
  {
    DumpHex(fp, *ppBuffer, min(1024, **ppDataLength));
  }
  fprint_pint(fp, "DataLength"  , ppDataLength);

  fprint_int(fp, "CompCode", pCompCode);
  fprint_int(fp, "Reason"  , pReason);
  fprintf(fp, "--------------------------------------------------------------------------\n");

  return;
}

/*********************************************************************/
/*                                                                   */
/* Before MQINQ Entrypoint                                           */
/*                                                                   */
/*********************************************************************/

MQ_INQ_EXIT InqBefore;

void MQENTRY InqBefore   ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , PMQHCONN  pHconn
                         , PMQHOBJ   pHobj
                         , PMQLONG   pSelectorCount
                         , PPMQLONG  ppSelectors
                         , PMQLONG   pIntAttrCount
                         , PPMQLONG  ppIntAttrs
                         , PMQLONG   pCharAttrLength
                         , PPMQCHAR  ppCharAttrs
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "MQINQ\n");
  fprintf(fp, TITLE_FORMAT, "BEFORE", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprint_hex(fp, "Hobj" , pHobj);
  fprintf(fp, "  SelectorCount : %d\n", *pSelectorCount   );
  fprintf(fp, "  Selectors     : %s\n", strpptr(ppSelectors,    "0x%p", buffer1));
  DumpHex(fp, *ppSelectors, *pSelectorCount * sizeof(MQLONG) );
  fprintf(fp, "  IntAttrCount  : %s\n", strptr(pIntAttrCount,   "%d", buffer1));
  fprintf(fp, "  CharAttrLength: %s\n", strptr(pCharAttrLength, "%d", buffer1));

  return;
}

/*********************************************************************/
/*                                                                   */
/* After MQINQ Entrypoint                                            */
/*                                                                   */
/*********************************************************************/

MQ_INQ_EXIT InqAfter;

void MQENTRY InqAfter    ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , PMQHCONN  pHconn
                         , PMQHOBJ   pHobj
                         , PMQLONG   pSelectorCount
                         , PPMQLONG  ppSelectors
                         , PMQLONG   pIntAttrCount
                         , PPMQLONG  ppIntAttrs
                         , PMQLONG   pCharAttrLength
                         , PPMQCHAR  ppCharAttrs
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "AFTER", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprintf(fp, "  IntAttrs      : %s\n", strptr(ppIntAttrs,  "0x%p", buffer1));
  DumpHex(fp, *ppIntAttrs, *pIntAttrCount * sizeof(MQLONG) );
  fprintf(fp, "  CharAttrs     : %s\n", strptr(ppCharAttrs, "0x%p", buffer1));
  DumpHex(fp, *ppCharAttrs, *pCharAttrLength               );
  fprint_int(fp, "CompCode", pCompCode);
  fprint_int(fp, "Reason"  , pReason);
  fprintf(fp, "--------------------------------------------------------------------------\n");

  return;
}

/*********************************************************************/
/*                                                                   */
/* Before MQOPEN Entrypoint                                          */
/*                                                                   */
/*********************************************************************/

MQ_OPEN_EXIT OpenBefore;

void MQENTRY OpenBefore  ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , PMQHCONN  pHconn
                         , PPMQOD    ppObjDesc
                         , PMQLONG   pOptions
                         , PPMQHOBJ  ppHobj
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "MQOPEN\n");
  fprintf(fp, TITLE_FORMAT, "BEFORE", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprintf(fp, "  pObjDesc      : %s\n",   strptr(ppObjDesc, "0x%p",   buffer1));
  DumpHex(fp, *ppObjDesc, sizeof(MQOD));
  fprintf(fp, "  Options       : %s\n",   strptr(pOptions,  "0x%X", buffer1));
  fprintf(fp, "  pHobj         : %s\n",   strptr(ppHobj,    "0x%X",   buffer1));

  return;
}

/*********************************************************************/
/*                                                                   */
/* After MQOPEN Entrypoint                                           */
/*                                                                   */
/*********************************************************************/

MQ_OPEN_EXIT OpenAfter;

void MQENTRY OpenAfter   ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , PMQHCONN  pHconn
                         , PPMQOD    ppObjDesc
                         , PMQLONG   pOptions
                         , PPMQHOBJ  ppHobj
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "AFTER", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprintf(fp, "  pObjDesc      : %s\n", strptr(ppObjDesc, "0x%p",  buffer1));
  DumpHex(fp, *ppObjDesc, sizeof(MQOD));
  fprint_phex(fp, "Hobj" , ppHobj);
  fprint_int(fp, "CompCode", pCompCode);
  fprint_int(fp, "Reason"  , pReason);
  fprintf(fp, "--------------------------------------------------------------------------\n");

  return;
}

/*********************************************************************/
/*                                                                   */
/* Before MQSUB Entrypoint                                           */
/*                                                                   */
/*********************************************************************/

MQ_SUB_EXIT SubBefore;

void MQENTRY SubBefore   ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , PMQHCONN  pHconn
                         , PPMQSD    ppSubDesc
                         , PPMQHOBJ  ppHobj_in
                         , PPMQHOBJ  ppHsub_out
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "MQSUB\n");
  fprintf(fp, TITLE_FORMAT, "BEFORE", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprintf(fp, "  ppSubDesc     : %s\n",   strptr(ppSubDesc, "0x%p", buffer1));
  DumpHex(fp, *ppSubDesc, sizeof(MQSD));
  fprint_phex(fp, "Hobj In"  , ppHobj_in);
  fprint_phex(fp, "Hsub Out" , ppHsub_out);

  return;
}

/*********************************************************************/
/*                                                                   */
/* After MQSUB Entrypoint                                            */
/*                                                                   */
/*********************************************************************/

MQ_SUB_EXIT SubAfter;

void MQENTRY SubAfter    ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , PMQHCONN  pHconn
                         , PPMQSD    ppSubDesc
                         , PPMQHOBJ  ppHobj_in
                         , PPMQHOBJ  ppHsub_out
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "AFTER", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprintf(fp, "  pSubDesc      : %s\n", strptr(ppSubDesc,   "0x%p",  buffer1));
  DumpHex(fp, *ppSubDesc, sizeof(MQSD));
  fprint_phex(fp, "Hobj In"  , ppHobj_in);
  fprint_phex(fp, "Hsub Out" , ppHsub_out);;
  fprint_int(fp, "CompCode", pCompCode);
  fprint_int(fp, "Reason"  , pReason);
  fprintf(fp, "--------------------------------------------------------------------------\n");

  return;
}

/*********************************************************************/
/*                                                                   */
/* Before MQSUBRQ Entrypoint                                         */
/*                                                                   */
/*********************************************************************/

MQ_SUBRQ_EXIT SubrqBefore;

void MQENTRY SubrqBefore ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , PMQHCONN  pHconn
                         , PMQHOBJ   pHsub
                         , MQLONG  * pAction
                         , PPMQSRO   ppSubRqOpts
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "MQSUBRQ\n");
  fprintf(fp, TITLE_FORMAT, "BEFORE", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprintf(fp, "  Hsub          : %s\n", strptr(pHsub, "0x%X", buffer1));
  fprintf(fp, "  Action        : %d\n", *pAction );
  fprintf(fp, "  ppSubRqOpts   : %s\n", strptr(ppSubRqOpts, "0x%p", buffer1));
  DumpHex(fp, *ppSubRqOpts, sizeof(MQSRO));

  return;
}

/*********************************************************************/
/*                                                                   */
/* After MQSUBRQ Entrypoint                                          */
/*                                                                   */
/*********************************************************************/

MQ_SUBRQ_EXIT SubrqAfter;

void MQENTRY SubrqAfter  ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , PMQHCONN  pHconn
                         , PMQHOBJ   pHsub
                         , MQLONG  * pAction
                         , PPMQSRO   ppSubRqOpts
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "AFTER", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hsub"  , pHsub);
  fprintf(fp, "  Action        : %d\n", *pAction );
  fprintf(fp, "  ppSubRqOpts   : %s\n", strptr(ppSubRqOpts, "0x%p", buffer1));
  DumpHex(fp, *ppSubRqOpts, sizeof(MQSRO));
  fprint_int(fp, "CompCode", pCompCode);
  fprint_int(fp, "Reason"  , pReason);
  fprintf(fp, "--------------------------------------------------------------------------\n");

  return;
}

/*********************************************************************/
/*                                                                   */
/* Before MQCB Entrypoint                                            */
/*                                                                   */
/*********************************************************************/

MQ_CB_EXIT CbBefore;

void MQENTRY CbBefore    ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , PMQHCONN  pHconn
                         , PMQLONG   pOperation
                         , PPMQCBD   ppCallbackDesc
                         , PMQHOBJ   pHobj
                         , PPMQMD    ppMsgDesc
                         , PPMQGMO   ppGetMsgOpts
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "MQCB\n");
  fprintf(fp, TITLE_FORMAT, "BEFORE", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprintf(fp, "  pOperation    : %s\n",   strptr(pOperation,"0x%X", buffer1));
  fprintf(fp, "  Operation     : %ld\n", *pOperation);
  fprint_hex(fp, "Hobj" , pHobj);
  fprintf(fp, "  ppCallbackDesc: %s\n",   strptr(ppCallbackDesc, "0x%p", buffer1));
  DumpHex(fp, *ppCallbackDesc, sizeof(MQCBD));
  fprintf(fp, "  ppMsgDesc: %s\n",        strptr(ppMsgDesc, "0x%p", buffer1));
  DumpHex(fp, *ppMsgDesc, sizeof(MQMD));
  fprintf(fp, "  ppGetMsgOpts: %s\n",            strptr(ppGetMsgOpts,     "0x%p", buffer1));
  DumpHex(fp, *ppGetMsgOpts, sizeof(MQGMO));

  return;
}

/*********************************************************************/
/*                                                                   */
/* After MQCB Entrypoint                                             */
/*                                                                   */
/*********************************************************************/

MQ_CB_EXIT CbAfter;

void MQENTRY CbAfter     ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , PMQHCONN  pHconn
                         , PMQLONG   pOperation
                         , PPMQCBD   ppCallbackDesc
                         , PMQHOBJ   pHobj
                         , PPMQMD    ppMsgDesc
                         , PPMQGMO   ppGetMsgOpts
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "AFTER", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprintf(fp, "  ppCallbackDesc: %s\n",   strptr(ppCallbackDesc, "0x%p", buffer1));
  DumpHex(fp, *ppCallbackDesc, sizeof(MQCBD));
  fprintf(fp, "  ppMsgDesc: %s\n",        strptr(ppMsgDesc, "0x%p", buffer1));
  DumpHex(fp, *ppMsgDesc, sizeof(MQMD));
  fprintf(fp, "  ppGetMsgOpts: %s\n",            strptr(ppGetMsgOpts,     "0x%p", buffer1));
  DumpHex(fp, *ppGetMsgOpts, sizeof(MQGMO));
  fprintf(fp, "  pCompCode     : %s\n", strptr(pCompCode,   "0x%p", buffer1));
  fprint_int(fp, "CompCode", pCompCode);
  fprint_int(fp, "Reason"  , pReason);
  fprintf(fp, "--------------------------------------------------------------------------\n");

  return;
}

/*********************************************************************/
/*                                                                   */
/* Before MQCTL Entrypoint                                           */
/*                                                                   */
/*********************************************************************/

MQ_CTL_EXIT CtlBefore;

void MQENTRY CtlBefore   ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , PMQHCONN  pHconn
                         , PMQLONG   pOperation
                         , PPMQCTLO  ppControlOpts
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "MQCTL\n");
  fprintf(fp, TITLE_FORMAT, "BEFORE", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprintf(fp, "  pOperation    : %s\n",   strptr(pOperation,"0x%X", buffer1));
  fprintf(fp, "  Operation     : %ld\n", *pOperation);
  fprintf(fp, "  ppControlOpts : %s\n",   strptr(ppControlOpts, "0x%p", buffer1));
  DumpHex(fp, *ppControlOpts, sizeof(MQCTLO));

  return;
}

/*********************************************************************/
/*                                                                   */
/* After MQCTL Entrypoint                                            */
/*                                                                   */
/*********************************************************************/

MQ_CTL_EXIT CtlAfter;

void MQENTRY CtlAfter    ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , PMQHCONN  pHconn
                         , PMQLONG   pOperation
                         , PPMQCTLO  ppControlOpts
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "AFTER", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprintf(fp, "  ppControlOpts: %s\n",    strptr(ppControlOpts, "0x%p", buffer1));
  DumpHex(fp, *ppControlOpts, sizeof(MQCTLO));
  fprintf(fp, "  pCompCode      : %s\n", strptr(pCompCode,   "0x%p", buffer1));
  fprint_int(fp, "CompCode", pCompCode);
  fprint_int(fp, "Reason"  , pReason);
  fprintf(fp, "--------------------------------------------------------------------------\n");

  return;
}

/*********************************************************************/
/*                                                                   */
/* Before MQ Callback Entrypoint                                     */
/*                                                                   */
/*********************************************************************/

MQ_CALLBACK_EXIT CallbackBefore;

void MQENTRY CallbackBefore  ( PMQAXP    pExitParms
                             , PMQAXC    pExitContext
                             , PMQHCONN  pHconn
                             , PPMQMD    ppMsgDesc
                             , PPMQGMO   ppGetMsgOpts
                             , PPMQVOID  ppBuffer
                             , PPMQCBC   ppMQCBContext
                             )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  PMQCBC            pMQCBContext   = *ppMQCBContext;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "MQ Callback\n");
  fprintf(fp, TITLE_FORMAT, "BEFORE", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprintf(fp, "  ppMQCBContext: %s\n",   strptr(ppMQCBContext, "0x%p", buffer1));
  DumpHex(fp, *ppMQCBContext, sizeof(MQCBC));
  fprintf(fp, "  ppMsgDesc: %s\n",        strptr(ppMsgDesc, "0x%p", buffer1));
  DumpHex(fp, *ppMsgDesc, sizeof(MQMD));
  fprintf(fp, "  ppGetMsgOpts: %s\n",            strptr(ppGetMsgOpts,     "0x%p", buffer1));
  DumpHex(fp, *ppGetMsgOpts, sizeof(MQGMO));
  fprintf(fp, "  ppBuffer: %s\n",            strptr(ppBuffer,     "0x%p", buffer1));
  DumpHex(fp, *ppBuffer, min(pMQCBContext->DataLength,
                             pMQCBContext->BufferLength));

  return;
}

/*********************************************************************/
/*                                                                   */
/* After MQ Callback Entrypoint                                      */
/*                                                                   */
/*********************************************************************/

MQ_CALLBACK_EXIT CallbackAfter;

void MQENTRY CallbackAfter ( PMQAXP    pExitParms
                           , PMQAXC    pExitContext
                           , PMQHCONN  pHconn
                           , PPMQMD    ppMsgDesc
                           , PPMQGMO   ppGetMsgOpts
                           , PPMQVOID  ppBuffer
                           , PPMQCBC   ppMQCBContext
                           )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  PMQCBC            pMQCBContext   = *ppMQCBContext;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "AFTER", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprintf(fp, "  ppMQCBContext: %s\n",   strptr(ppMQCBContext, "0x%p", buffer1));
  DumpHex(fp, *ppMQCBContext, sizeof(MQCBC));
  fprintf(fp, "  ppMsgDesc: %s\n",        strptr(ppMsgDesc, "0x%p", buffer1));
  DumpHex(fp, *ppMsgDesc, sizeof(MQMD));
  fprintf(fp, "  ppGetMsgOpts: %s\n",            strptr(ppGetMsgOpts,     "0x%p", buffer1));
  DumpHex(fp, *ppGetMsgOpts, sizeof(MQGMO));
  fprintf(fp, "  ppBuffer: %s\n",            strptr(ppBuffer,     "0x%p", buffer1));
  DumpHex(fp, *ppBuffer, min(pMQCBContext->DataLength,
                             pMQCBContext->BufferLength));
  fprintf(fp, "--------------------------------------------------------------------------\n");

  return;
}

/*********************************************************************/
/*                                                                   */
/* Before MQSTAT Entrypoint                                          */
/*                                                                   */
/*********************************************************************/

MQ_STAT_EXIT StatBefore;

void MQENTRY StatBefore   ( PMQAXP    pExitParms
                          , PMQAXC    pExitContext
                          , PMQHCONN  pHconn
                          , PMQLONG   pType
                          , PPMQSTS   ppStatus
                          , PMQLONG   pCompCode
                          , PMQLONG   pReason
                          )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "MQSTAT\n");
  fprintf(fp, TITLE_FORMAT, "BEFORE", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprintf(fp, "  Type           : %s\n",   strptr(pType,   "%d", buffer1));
  fprintf(fp, "  pStatus(in/out): %s\n",   strptr(ppStatus, "0x%p", buffer1));
  DumpHex(fp, *ppStatus, sizeof(MQSTS));

  return;
}

/*********************************************************************/
/*                                                                   */
/* After MQSTAT Entrypoint                                           */
/*                                                                   */
/*********************************************************************/

MQ_STAT_EXIT StatAfter;

void MQENTRY StatAfter    ( PMQAXP    pExitParms
                          , PMQAXC    pExitContext
                          , PMQHCONN  pHconn
                          , PMQLONG   pType
                          , PPMQSTS   ppStatus
                          , PMQLONG   pCompCode
                          , PMQLONG   pReason
                          )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "AFTER", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprintf(fp, "  Type          : %s\n",   strptr(pType,   "%d", buffer1));
  fprintf(fp, "  pStatus(in/out): %s\n",   strptr(ppStatus, "0x%p", buffer1));
  DumpHex(fp, *ppStatus, sizeof(MQSTS));
  fprint_int(fp, "CompCode", pCompCode);
  fprint_int(fp, "Reason"  , pReason);
  fprintf(fp, "--------------------------------------------------------------------------\n");

  return;
}


/*********************************************************************/
/*                                                                   */
/* Before MQPUT Entrypoint                                           */
/*                                                                   */
/*********************************************************************/

MQ_PUT_EXIT PutBefore;

void MQENTRY PutBefore   ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , PMQHCONN  pHconn
                         , PMQHOBJ   pHobj
                         , PPMQMD    ppMsgDesc
                         , PPMQPMO   ppPutMsgOpts
                         , PMQLONG   pBufferLength
                         , PPMQVOID  ppBuffer
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "MQPUT\n");
  fprintf(fp, TITLE_FORMAT, "BEFORE", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprint_hex(fp, "Hobj" , pHobj);
  fprintf(fp, "  MsgDesc       : %s\n", strptr(ppMsgDesc,     "0x%p",   buffer1));
  DumpHex(fp, *ppMsgDesc, sizeof(MQMD));
  fprintf(fp, "  PutMsgOpts    : %s\n", strptr(ppPutMsgOpts,  "0x%p",   buffer1));
  DumpHex(fp, *ppPutMsgOpts, sizeof(MQPMO));
  fprintf(fp, "  BufferLength  : %s\n", strptr(pBufferLength, "%d",   buffer1));
  fprintf(fp, "  Buffer        : %s\n", strptr(ppBuffer,      "0x%p",   buffer1));

  if (myCheckPointer( pBufferLength ))
    DumpHex(fp, *ppBuffer, min(1024, *pBufferLength));

  return;
}

/*********************************************************************/
/*                                                                   */
/* After MQPUT Entrypoint                                            */
/*                                                                   */
/*********************************************************************/

MQ_PUT_EXIT PutAfter;

void MQENTRY PutAfter    ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , PMQHCONN  pHconn
                         , PMQHOBJ   pHobj
                         , PPMQMD    ppMsgDesc
                         , PPMQPMO   ppPutMsgOpts
                         , PMQLONG   pBufferLength
                         , PPMQVOID  ppBuffer
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "AFTER", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprintf(fp, "  MsgDesc       : %s\n", strptr(ppMsgDesc,    "0x%p", buffer1));
  DumpHex(fp, *ppMsgDesc, sizeof(MQMD));
  fprintf(fp, "  PutMsgOpts    : %s\n", strptr(ppPutMsgOpts, "0x%p", buffer1));
  DumpHex(fp, *ppPutMsgOpts, sizeof(MQPMO));
  fprint_int(fp, "CompCode", pCompCode);
  fprint_int(fp, "Reason"  , pReason);
  fprintf(fp, "--------------------------------------------------------------------------\n");

  return;
}

/*********************************************************************/
/*                                                                   */
/* Before MQPUT1 Entrypoint                                          */
/*                                                                   */
/*********************************************************************/

MQ_PUT1_EXIT Put1Before;

void MQENTRY Put1Before  ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , PMQHCONN  pHconn
                         , PPMQOD    ppObjDesc
                         , PPMQMD    ppMsgDesc
                         , PPMQPMO   ppPut1MsgOpts
                         , PMQLONG   pBufferLength
                         , PPMQVOID  ppBuffer
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "MQPUT1\n");
  fprintf(fp, TITLE_FORMAT, "BEFORE", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprintf(fp, "  pObjDesc      : %s\n", strptr(ppObjDesc,     "0x%p",   buffer1));
  DumpHex(fp, *ppObjDesc, sizeof(MQOD));
  fprintf(fp, "  MsgDesc       : %s\n", strptr(ppMsgDesc,     "0x%p",   buffer1));
  DumpHex(fp, *ppMsgDesc, sizeof(MQMD));
  fprintf(fp, "  Put1MsgOpts    : %s\n",strptr(ppPut1MsgOpts, "0x%p",   buffer1));
  DumpHex(fp, *ppPut1MsgOpts, sizeof(MQPMO));
  fprintf(fp, "  BufferLength  : %s\n", strptr(pBufferLength, "%d",   buffer1));
  fprintf(fp, "  Buffer        : %s\n", strptr(ppBuffer,      "0x%p",   buffer1));

  if (myCheckPointer( pBufferLength ))
    DumpHex(fp, *ppBuffer, min(1024, *pBufferLength));

  return;
}

/*********************************************************************/
/*                                                                   */
/* After MQPUT1 Entrypoint                                           */
/*                                                                   */
/*********************************************************************/

MQ_PUT1_EXIT Put1After;

void MQENTRY Put1After   ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , PMQHCONN  pHconn
                         , PPMQOD    ppObjDesc
                         , PPMQMD    ppMsgDesc
                         , PPMQPMO   ppPut1MsgOpts
                         , PMQLONG   pBufferLength
                         , PPMQVOID  ppBuffer
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "AFTER", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprintf(fp, "  pObjDesc      : 0x%p\n",   *ppObjDesc      );
  DumpHex(fp, *ppObjDesc, sizeof(MQOD));
  fprintf(fp, "  MsgDesc       : 0x%p\n",   *ppMsgDesc      );
  DumpHex(fp, *ppMsgDesc, sizeof(MQMD));
  fprintf(fp, "  Put1MsgOpts    : 0x%p\n",  *ppPut1MsgOpts );
  DumpHex(fp, *ppPut1MsgOpts, sizeof(MQPMO));
  fprint_int(fp, "CompCode", pCompCode);
  fprint_int(fp, "Reason"  , pReason);
  fprintf(fp, "--------------------------------------------------------------------------\n");

  return;
}

/*********************************************************************/
/*                                                                   */
/* Before MQSET Entrypoint                                           */
/*                                                                   */
/*********************************************************************/

MQ_SET_EXIT SetBefore;

void MQENTRY SetBefore   ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , PMQHCONN  pHconn
                         , PMQHOBJ   pHobj
                         , PMQLONG   pSelectorCount
                         , PPMQLONG  ppSelectors
                         , PMQLONG   pIntAttrCount
                         , PPMQLONG  ppIntAttrs
                         , PMQLONG   pCharAttrLength
                         , PPMQCHAR  ppCharAttrs
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "MQSET\n");
  fprintf(fp, TITLE_FORMAT, "BEFORE", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprint_hex(fp, "Hobj" , pHobj);
  fprintf(fp, "  SelectorCount : %s\n", strptr(pSelectorCount,  "%d",   buffer1));
  fprintf(fp, "  Selectors     : %s\n", strptr(ppSelectors,     "0x%p",   buffer1));
  DumpHex(fp, *ppSelectors, *pSelectorCount * sizeof(MQLONG));
  fprintf(fp, "  IntAttrCount  : %s\n", strptr(pIntAttrCount,   "%d",   buffer1));
  fprintf(fp, "  IntAttrs      : %s\n", strptr(ppIntAttrs,      "0x%p",   buffer1));
  fprintf(fp, "  CharAttrLength: %s\n", strptr(pCharAttrLength, "%d",   buffer1));
  DumpHex(fp, *ppIntAttrs, *pIntAttrCount * sizeof(MQLONG)   );
  fprintf(fp, "  CharAttrs     : %s\n", strptr(ppCharAttrs,     "0x%p",   buffer1));
  DumpHex(fp, *ppCharAttrs, *pCharAttrLength                 );

  return;
}

/*********************************************************************/
/*                                                                   */
/* After MQSET Entrypoint                                            */
/*                                                                   */
/*********************************************************************/

MQ_SET_EXIT SetAfter;

void MQENTRY SetAfter    ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , PMQHCONN  pHconn
                         , PMQHOBJ   pHobj
                         , PMQLONG   pSelectorCount
                         , PPMQLONG  ppSelectors
                         , PMQLONG   pIntAttrCount
                         , PPMQLONG  ppIntAttrs
                         , PMQLONG   pCharAttrLength
                         , PPMQCHAR  ppCharAttrs
                         , PMQLONG   pCompCode
                         , PMQLONG   pReason
                         )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "AFTER", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprint_int(fp, "CompCode", pCompCode);
  fprint_int(fp, "Reason"  , pReason);
  fprintf(fp, "--------------------------------------------------------------------------\n");

  return;
}


/*********************************************************************/
/*                                                                   */
/* Before xa_close Entrypoint                                        */
/*                                                                   */
/*********************************************************************/

XA_CLOSE_EXIT XACloseBefore;

void MQENTRY XACloseBefore   ( PMQAXP    pExitParms
                             , PMQAXC    pExitContext
                             , PMQHCONN  pHconn
                             , PPMQCHAR  ppXa_info
                             , PMQLONG   pRmid
                             , PMQLONG   pFlags
                             , PMQLONG   pXARetCode)
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "xa_close\n");
  fprintf(fp, TITLE_FORMAT, "BEFORE", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprintf(fp, "  ppXa_info     : %s\n",   strpptr(ppXa_info, "0x%p", buffer1));
  if (ppXa_info && *ppXa_info)
  {
    DumpHex(fp, *ppXa_info, strlen(*ppXa_info));
  }
  fprintf(fp, "  Rmid          : %d\n",   *pRmid);
  fprintf(fp, "  Flags         : %d\n",   *pFlags);
  return;
}

/*********************************************************************/
/*                                                                   */
/* After xa_close Entrypoint                                         */
/*                                                                   */
/*********************************************************************/

XA_CLOSE_EXIT XACloseAfter;

void MQENTRY XACloseAfter ( PMQAXP    pExitParms
                          , PMQAXC    pExitContext
                          , PMQHCONN  pHconn
                          , PPMQCHAR  ppXa_info
                          , PMQLONG   pRmid
                          , PMQLONG   pFlags
                          , PMQLONG   pXARetCode)
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "AFTER", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprintf(fp, "  ppXa_info     : %s\n",   strpptr(ppXa_info, "0x%p", buffer1));
  if (ppXa_info && *ppXa_info)
  {
    DumpHex(fp, *ppXa_info, strlen(*ppXa_info));
  }
  fprintf(fp, "  Rmid          : %d\n",   *pRmid);
  fprintf(fp, "  Flags         : %d\n",   *pFlags);
  fprintf(fp, "  XA Return code: %d\n",   *pXARetCode);

  fprintf(fp, "--------------------------------------------------------------------------\n");

  return;
}


/*********************************************************************/
/*                                                                   */
/* Before xa_commit Entrypoint                                       */
/*                                                                   */
/*********************************************************************/

XA_COMMIT_EXIT XACommitBefore;

void MQENTRY XACommitBefore   ( PMQAXP    pExitParms
                              , PMQAXC    pExitContext
                              , PMQHCONN  pHconn
                              , PMQPTR    ppXID
                              , PMQLONG   pRmid
                              , PMQLONG   pFlags
                              , PMQLONG   pXARetCode)
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "xa_commit\n");
  fprintf(fp, TITLE_FORMAT, "BEFORE", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprintf(fp, "  ppXID     : %s\n",   strpptr(ppXID, "0x%p", buffer1));
  fprintf(fp, "  Rmid          : %d\n",   *pRmid);
  fprintf(fp, "  Flags         : %d\n",   *pFlags);
  return;
}

/*********************************************************************/
/*                                                                   */
/* After xa_commit Entrypoint                                        */
/*                                                                   */
/*********************************************************************/

XA_COMMIT_EXIT XACommitAfter;

void MQENTRY XACommitAfter    ( PMQAXP    pExitParms
                              , PMQAXC    pExitContext
                              , PMQHCONN  pHconn
                              , PMQPTR    ppXID
                              , PMQLONG   pRmid
                              , PMQLONG   pFlags
                              , PMQLONG   pXARetCode)
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "AFTER", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprintf(fp, "  ppXID     : %s\n",   strpptr(ppXID, "0x%p", buffer1));
  fprintf(fp, "  Rmid          : %d\n",   *pRmid);
  fprintf(fp, "  Flags         : %d\n",   *pFlags);
  fprintf(fp, "  XA Return code: %d\n",   *pXARetCode);

  fprintf(fp, "--------------------------------------------------------------------------\n");

  return;
}

/*********************************************************************/
/*                                                                   */
/* Before xa_complete Entrypoint                                     */
/*                                                                   */
/*********************************************************************/

XA_COMPLETE_EXIT XACompleteBefore;

void MQENTRY XACompleteBefore   ( PMQAXP    pExitParms,
                                  PMQAXC    pExitContext,
                                  PMQHCONN  pHconn,
                                  PPMQLONG  ppHandle,
                                  PPMQLONG  ppRetval,
                                  PMQLONG   pRmid,
                                  PMQLONG   pFlags,
                                  PMQLONG   pXARetCode)
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";
  PMQLONG           pValue         = NULL;

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "xa_complete\n");
  fprintf(fp, TITLE_FORMAT, "BEFORE", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  pValue = *ppHandle;
  fprintf(fp, "  Handle        : %d\n",   *pValue);
  pValue = *ppRetval;
  fprintf(fp, "  Retval        : %d\n",   *pValue);
  fprintf(fp, "  Rmid          : %d\n",   *pRmid);
  fprintf(fp, "  Flags         : %d\n",   *pFlags);
  return;
}

/*********************************************************************/
/*                                                                   */
/* After xa_complete Entrypoint                                      */
/*                                                                   */
/*********************************************************************/

XA_COMPLETE_EXIT XACompleteAfter;

void MQENTRY XACompleteAfter    ( PMQAXP    pExitParms,
                                  PMQAXC    pExitContext,
                                  PMQHCONN  pHconn,
                                  PPMQLONG  ppHandle,
                                  PPMQLONG  ppRetval,
                                  PMQLONG   pRmid,
                                  PMQLONG   pFlags,
                                  PMQLONG   pXARetCode)
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";
  PMQLONG           pValue         = NULL;

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "AFTER", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  pValue = *ppHandle;
  fprintf(fp, "  Handle        : %d\n",   *pValue);
  pValue = *ppRetval;
  fprintf(fp, "  Retval        : %d\n",   *pValue);
  fprintf(fp, "  Rmid          : %d\n",   *pRmid);
  fprintf(fp, "  Flags         : %d\n",   *pFlags);
  fprintf(fp, "  XA Return code: %d\n",   *pXARetCode);

  fprintf(fp, "--------------------------------------------------------------------------\n");

  return;
}



/*********************************************************************/
/*                                                                   */
/* Before xa_end Entrypoint                                          */
/*                                                                   */
/*********************************************************************/

XA_END_EXIT XAEndBefore;

void MQENTRY XAEndBefore   ( PMQAXP    pExitParms
                           , PMQAXC    pExitContext
                           , PMQHCONN  pHconn
                           , PMQPTR    ppXID
                           , PMQLONG   pRmid
                           , PMQLONG   pFlags
                           , PMQLONG   pXARetCode)
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "xa_end\n");
  fprintf(fp, TITLE_FORMAT, "BEFORE", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprintf(fp, "  ppXID     : %s\n",   strpptr(ppXID, "0x%p", buffer1));
  fprintf(fp, "  Rmid          : %d\n",   *pRmid);
  fprintf(fp, "  Flags         : %d\n",   *pFlags);
  return;
}

/*********************************************************************/
/*                                                                   */
/* After xa_end Entrypoint                                           */
/*                                                                   */
/*********************************************************************/

XA_END_EXIT XAEndAfter;

void MQENTRY XAEndAfter    ( PMQAXP    pExitParms
                           , PMQAXC    pExitContext
                           , PMQHCONN  pHconn
                           , PMQPTR    ppXID
                           , PMQLONG   pRmid
                           , PMQLONG   pFlags
                           , PMQLONG   pXARetCode)
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "AFTER", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprintf(fp, "  ppXID     : %s\n",   strpptr(ppXID, "0x%p", buffer1));
  fprintf(fp, "  Rmid          : %d\n",   *pRmid);
  fprintf(fp, "  Flags         : %d\n",   *pFlags);
  fprintf(fp, "  XA Return code: %d\n",   *pXARetCode);

  fprintf(fp, "--------------------------------------------------------------------------\n");

  return;
}



/*********************************************************************/
/*                                                                   */
/* Before xa_forget Entrypoint                                       */
/*                                                                   */
/*********************************************************************/

XA_FORGET_EXIT XAForgetBefore;

void MQENTRY XAForgetBefore   ( PMQAXP    pExitParms
                              , PMQAXC    pExitContext
                              , PMQHCONN  pHconn
                              , PMQPTR    ppXID
                              , PMQLONG   pRmid
                              , PMQLONG   pFlags
                              , PMQLONG   pXARetCode)
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "xa_forget\n");
  fprintf(fp, TITLE_FORMAT, "BEFORE", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprintf(fp, "  ppXID     : %s\n",   strpptr(ppXID, "0x%p", buffer1));
  fprintf(fp, "  Rmid          : %d\n",   *pRmid);
  fprintf(fp, "  Flags         : %d\n",   *pFlags);
  return;
}

/*********************************************************************/
/*                                                                   */
/* After xa_forget Entrypoint                                        */
/*                                                                   */
/*********************************************************************/

XA_FORGET_EXIT XAForgetAfter;

void MQENTRY XAForgetAfter    ( PMQAXP    pExitParms
                              , PMQAXC    pExitContext
                              , PMQHCONN  pHconn
                              , PMQPTR    ppXID
                              , PMQLONG   pRmid
                              , PMQLONG   pFlags
                              , PMQLONG   pXARetCode)
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "AFTER", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprintf(fp, "  ppXID     : %s\n",   strpptr(ppXID, "0x%p", buffer1));
  fprintf(fp, "  Rmid          : %d\n",   *pRmid);
  fprintf(fp, "  Flags         : %d\n",   *pFlags);
  fprintf(fp, "  XA Return code: %d\n",   *pXARetCode);

  fprintf(fp, "--------------------------------------------------------------------------\n");

  return;
}



/*********************************************************************/
/*                                                                   */
/* Before xa_open Entrypoint                                         */
/*                                                                   */
/*********************************************************************/

XA_OPEN_EXIT XAOpenBefore;

void MQENTRY XAOpenBefore   ( PMQAXP    pExitParms
                            , PMQAXC    pExitContext
                            , PMQHCONN  pHconn
                            , PPMQCHAR  ppXa_info
                            , PMQLONG   pRmid
                            , PMQLONG   pFlags
                            , PMQLONG   pXARetCode)
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "xa_open\n");
  fprintf(fp, TITLE_FORMAT, "BEFORE", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprintf(fp, "  ppXa_info     : %s\n",   strpptr(ppXa_info, "0x%p", buffer1));
  if (ppXa_info && *ppXa_info)
  {
    DumpHex(fp, *ppXa_info, strlen(*ppXa_info));
  }
  fprintf(fp, "  Rmid          : %d\n",   *pRmid);
  fprintf(fp, "  Flags         : %d\n",   *pFlags);
  return;
}

/*********************************************************************/
/*                                                                   */
/* After xa_open Entrypoint                                          */
/*                                                                   */
/********************************************************************/

XA_OPEN_EXIT XAOpenAfter;

void MQENTRY XAOpenAfter ( PMQAXP    pExitParms
                         , PMQAXC    pExitContext
                         , PMQHCONN  pHconn
                         , PPMQCHAR  ppXa_info
                         , PMQLONG   pRmid
                         , PMQLONG   pFlags
                         , PMQLONG   pXARetCode)
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "AFTER", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprintf(fp, "  ppXa_info     : %s\n",   strpptr(ppXa_info, "0x%p", buffer1));
  if (ppXa_info && *ppXa_info)
  {
    DumpHex(fp, *ppXa_info, strlen(*ppXa_info));
  }
  fprintf(fp, "  Rmid          : %d\n",   *pRmid);
  fprintf(fp, "  Flags         : %d\n",   *pFlags);
  fprintf(fp, "  XA Return code: %d\n",   *pXARetCode);

  fprintf(fp, "--------------------------------------------------------------------------\n");

  return;
}

/*********************************************************************/
/*                                                                   */
/* Before xa_prepare Entrypoint                                      */
/*                                                                   */
/*********************************************************************/

XA_PREPARE_EXIT XAPrepareBefore;

void MQENTRY XAPrepareBefore   ( PMQAXP    pExitParms
                               , PMQAXC    pExitContext
                               , PMQHCONN  pHconn
                               , PMQPTR    ppXID
                               , PMQLONG   pRmid
                               , PMQLONG   pFlags
                               , PMQLONG   pXARetCode)
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "xa_prepare\n");
  fprintf(fp, TITLE_FORMAT, "BEFORE", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprintf(fp, "  ppXID     : %s\n",   strpptr(ppXID, "0x%p", buffer1));
  fprintf(fp, "  Rmid          : %d\n",   *pRmid);
  fprintf(fp, "  Flags         : %d\n",   *pFlags);
  return;
}

/*********************************************************************/
/*                                                                   */
/* After xa_prepare Entrypoint                                       */
/*                                                                   */
/*********************************************************************/

XA_PREPARE_EXIT XAPrepareAfter;

void MQENTRY XAPrepareAfter    ( PMQAXP    pExitParms
                               , PMQAXC    pExitContext
                               , PMQHCONN  pHconn
                               , PMQPTR    ppXID
                               , PMQLONG   pRmid
                               , PMQLONG   pFlags
                               , PMQLONG   pXARetCode)
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "AFTER", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprintf(fp, "  ppXID     : %s\n",   strpptr(ppXID, "0x%p", buffer1));
  fprintf(fp, "  Rmid          : %d\n",   *pRmid);
  fprintf(fp, "  Flags         : %d\n",   *pFlags);
  fprintf(fp, "  XA Return code: %d\n",   *pXARetCode);
  fprintf(fp, "--------------------------------------------------------------------------\n");

  return;
}


/*********************************************************************/
/*                                                                   */
/* Before xa_recover Entrypoint                                      */
/*                                                                   */
/*********************************************************************/

XA_RECOVER_EXIT XARecoverBefore;

void MQENTRY XARecoverBefore   ( PMQAXP    pExitParms
                               , PMQAXC    pExitContext
                               , PMQHCONN  pHconn
                               , PMQPTR    ppXID
                               , PMQLONG   pCount
                               , PMQLONG   pRmid
                               , PMQLONG   pFlags
                               , PMQLONG   pXARetCode)
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "xa_recover\n");
  fprintf(fp, TITLE_FORMAT, "BEFORE", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprintf(fp, "  ppXID     : %s\n",   strpptr(ppXID, "0x%p", buffer1));
  fprintf(fp, "  Count         : %d\n",   *pCount);
  fprintf(fp, "  Rmid          : %d\n",   *pRmid);
  fprintf(fp, "  Flags         : %d\n",   *pFlags);
  return;
}

/*********************************************************************/
/*                                                                   */
/* After xa_recover Entrypoint                                       */
/*                                                                   */
/*********************************************************************/

XA_RECOVER_EXIT XARecoverAfter;

void MQENTRY XARecoverAfter    ( PMQAXP    pExitParms
                               , PMQAXC    pExitContext
                               , PMQHCONN  pHconn
                               , PMQPTR    ppXID
                               , PMQLONG   pCount
                               , PMQLONG   pRmid
                               , PMQLONG   pFlags
                               , PMQLONG   pXARetCode)
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "AFTER", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprintf(fp, "  ppXID     : %s\n",   strpptr(ppXID, "0x%p", buffer1));
  fprintf(fp, "  Count         : %d\n",   *pCount);
  fprintf(fp, "  Rmid          : %d\n",   *pRmid);
  fprintf(fp, "  Flags         : %d\n",   *pFlags);
  fprintf(fp, "  XA Return code: %d\n",   *pXARetCode);
  fprintf(fp, "--------------------------------------------------------------------------\n");

  return;
}




/*********************************************************************/
/*                                                                   */
/* Before xa_rollback Entrypoint                                     */
/*                                                                   */
/*********************************************************************/

XA_ROLLBACK_EXIT XARollbackBefore;

void MQENTRY XARollbackBefore  ( PMQAXP    pExitParms
                               , PMQAXC    pExitContext
                               , PMQHCONN  pHconn
                               , PMQPTR    ppXID
                               , PMQLONG   pRmid
                               , PMQLONG   pFlags
                               , PMQLONG   pXARetCode)
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "xa_rollback\n");
  fprintf(fp, TITLE_FORMAT, "BEFORE", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprintf(fp, "  ppXID     : %s\n",   strpptr(ppXID, "0x%p", buffer1));
  fprintf(fp, "  Rmid          : %d\n",   *pRmid);
  fprintf(fp, "  Flags         : %d\n",   *pFlags);
  return;
}

/*********************************************************************/
/*                                                                   */
/* After xa_rollback Entrypoint                                      */
/*                                                                   */
/*********************************************************************/

XA_ROLLBACK_EXIT XARollbackAfter;

void MQENTRY XARollbackAfter   ( PMQAXP    pExitParms
                               , PMQAXC    pExitContext
                               , PMQHCONN  pHconn
                               , PMQPTR    ppXID
                               , PMQLONG   pRmid
                               , PMQLONG   pFlags
                               , PMQLONG   pXARetCode)
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "AFTER", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprintf(fp, "  ppXID     : %s\n",   strpptr(ppXID, "0x%p", buffer1));
  fprintf(fp, "  Rmid          : %d\n",   *pRmid);
  fprintf(fp, "  Flags         : %d\n",   *pFlags);
  fprintf(fp, "  XA Return code: %d\n",   *pXARetCode);
  fprintf(fp, "--------------------------------------------------------------------------\n");

  return;
}



/*********************************************************************/
/*                                                                   */
/* Before xa_start Entrypoint                                        */
/*                                                                   */
/*********************************************************************/

XA_START_EXIT XAStartBefore;

void MQENTRY XAStartBefore     ( PMQAXP    pExitParms
                               , PMQAXC    pExitContext
                               , PMQHCONN  pHconn
                               , PMQPTR    ppXID
                               , PMQLONG   pRmid
                               , PMQLONG   pFlags
                               , PMQLONG   pXARetCode)
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "xa_start\n");
  fprintf(fp, TITLE_FORMAT, "BEFORE", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprintf(fp, "  ppXID     : %s\n",   strpptr(ppXID, "0x%p", buffer1));
  fprintf(fp, "  Rmid          : %d\n",   *pRmid);
  fprintf(fp, "  Flags         : %d\n",   *pFlags);
  return;
}

/*********************************************************************/
/*                                                                   */
/* After xa_start Entrypoint                                         */
/*                                                                   */
/*********************************************************************/

XA_START_EXIT XAStartAfter;

void MQENTRY XAStartAfter      ( PMQAXP    pExitParms
                               , PMQAXC    pExitContext
                               , PMQHCONN  pHconn
                               , PMQPTR    ppXID
                               , PMQLONG   pRmid
                               , PMQLONG   pFlags
                               , PMQLONG   pXARetCode)
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "AFTER", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprint_hex(fp, "Hconn", pHconn);
  fprintf(fp, "  ppXID     : %s\n",   strpptr(ppXID, "0x%p", buffer1));
  fprintf(fp, "  Rmid          : %d\n",   *pRmid);
  fprintf(fp, "  Flags         : %d\n",   *pFlags);
  fprintf(fp, "  XA Return code: %d\n",   *pXARetCode);
  fprintf(fp, "--------------------------------------------------------------------------\n");

  return;
}

/*********************************************************************/
/*                                                                   */
/* Before ax_reg Entrypoint                                          */
/*                                                                   */
/*********************************************************************/

AX_REG_EXIT AXRegBefore;

void MQENTRY AXRegBefore   ( PMQAXP    pExitParms
                           , PMQAXC    pExitContext
                           , PMQPTR    ppXID
                           , PMQLONG   pRmid
                           , PMQLONG   pFlags
                           , PMQLONG   pXARetCode
                           )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "ax_reg\n");
  fprintf(fp, TITLE_FORMAT, "BEFORE", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprintf(fp, "  ppXID     : %s\n",   strpptr(ppXID, "0x%p", buffer1));
  fprintf(fp, "  Rmid          : %d\n",   *pRmid);
  fprintf(fp, "  Flags         : %d\n",   *pFlags);
  return;
}

/*********************************************************************/
/*                                                                   */
/* After ax_reg Entrypoint                                           */
/*                                                                   */
/*********************************************************************/

AX_REG_EXIT AXRegAfter;

void MQENTRY AXRegAfter       ( PMQAXP    pExitParms
                              , PMQAXC    pExitContext
                              , PMQPTR    ppXID
                              , PMQLONG   pRmid
                              , PMQLONG   pFlags
                              , PMQLONG   pXARetCode
                              )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "AFTER", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprintf(fp, "  ppXID     : %s\n",   strpptr(ppXID, "0x%p", buffer1));
  fprintf(fp, "  Rmid          : %d\n",   *pRmid);
  fprintf(fp, "  Flags         : %d\n",   *pFlags);
  fprintf(fp, "  XA Return code: %d\n",   *pXARetCode);
  fprintf(fp, "--------------------------------------------------------------------------\n");

  return;
}


/*********************************************************************/
/*                                                                   */
/* Before ax_unreg Entrypoint                                        */
/*                                                                   */
/*********************************************************************/

AX_UNREG_EXIT AXUnregBefore;

void MQENTRY AXUnregBefore   ( PMQAXP    pExitParms
                             , PMQAXC    pExitContext
                             , PMQLONG   pRmid
                             , PMQLONG   pFlags
                             , PMQLONG   pXARetCode
                             )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "ax_unreg\n");
  fprintf(fp, TITLE_FORMAT, "BEFORE", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_BEFORE)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_BEFORE)
    DumpContext( fp, pExitContext );

  fprintf(fp, "  Rmid          : %d\n",   *pRmid);
  fprintf(fp, "  Flags         : %d\n",   *pFlags);
  return;
}

/*********************************************************************/
/*                                                                   */
/* After ax_unreg Entrypoint                                         */
/*                                                                   */
/*********************************************************************/

AX_UNREG_EXIT AXUnregAfter;

void MQENTRY AXUnregAfter       ( PMQAXP    pExitParms
                                , PMQAXC    pExitContext
                                , PMQLONG   pRmid
                                , PMQLONG   pFlags
                                , PMQLONG   pXARetCode
                                )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, TITLE_FORMAT, "AFTER", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprintf(fp, "  Rmid          : %d\n",   *pRmid);
  fprintf(fp, "  Flags         : %d\n",   *pFlags);
  fprintf(fp, "  XA Return code: %d\n",   *pXARetCode);
  fprintf(fp, "--------------------------------------------------------------------------\n");

  return;
}

/*********************************************************************/
/*                                                                   */
/* TERM                                                              */
/*                                                                   */
/*********************************************************************/

MQ_TERM_EXIT Terminate;

void MQENTRY Terminate ( PMQAXP   pExitParms
                       , PMQAXC   pExitContext
                       , PMQLONG  pCompCode
                       , PMQLONG  pReason
                       )
{
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = *ppExitUserArea;
  FILE            * fp             = pExitUserArea->fp;
  char              buffer1[50]    = "";
  char              buffer2[50]    = "";

  myGetAbsoluteTime( buffer1, sizeof(buffer1) );
  myGetRelativeTime( buffer2, pExitUserArea );

  fprintf(fp, "MQAPI Trace\n");
  fprintf(fp, TITLE_FORMAT, "END", buffer1, buffer2);

  if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_ALWAYS)
    DumpParms( fp, pExitParms );

  if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_ALWAYS)
    DumpContext( fp, pExitContext );

  fprintf(fp, "--------------------------------------------------------------------------\n");

  fclose(fp);
  free( pExitUserArea );

  return;
}

/*********************************************************************/
/*                                                                   */
/* Initialisation function                                           */
/*                                                                   */
/*********************************************************************/

MQ_INIT_EXIT EntryPoint;

void MQENTRY EntryPoint ( PMQAXP   pExitParms
                        , PMQAXC   pExitContext
                        , PMQLONG  pCompCode
                        , PMQLONG  pReason
                        )
{
  MQLONG            rc             = MQRC_NONE;
  MYEXITUSERAREA ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
  MYEXITUSERAREA  * pExitUserArea  = NULL;
  FILE            * fp             = NULL;
  char            * env            = NULL;
  char              logfile[1024]  = "";
  MQPID             pid            = 0;
  MQPID             tid            = 0;

  /*******************************************************************/
  /* Make sure that the Hconfig supplied contains the interface      */
  /* entry points.                                                   */
  /*******************************************************************/
  if(memcmp(pExitParms->Hconfig->StrucId, MQIEP_STRUC_ID, 4))
  {
    rc = MQRC_API_EXIT_ERROR;
    pExitParms->ExitResponse = MQXCC_FAILED;
  }

  /*******************************************************************/
  /* Malloc storage for the ExitUserArea                             */
  /*******************************************************************/

  if (rc == MQRC_NONE)
  {
    pExitUserArea = calloc( 1, sizeof(MYEXITUSERAREA) );

    if (pExitUserArea)
      *ppExitUserArea = pExitUserArea;
    else
    {
      rc = MQRC_API_EXIT_ERROR;
      pExitParms->ExitResponse = MQXCC_FAILED;
    }
  }

  /*******************************************************************/
  /* Initialise the Windows Performance counter                      */
  /*******************************************************************/

#if (MQAT_DEFAULT == MQAT_WINDOWS_NT)
  if (rc == MQRC_NONE)  InitPerformanceCounter( pExitUserArea );
#endif

  /*******************************************************************/
  /* Get the options                                                 */
  /*******************************************************************/

  if (rc == MQRC_NONE)
  {
#if MQAT_DEFAULT == MQAT_NSK && defined(_GUARDIAN_TARGET)
    env = getenv( "MQAPITRACEOPTIONS" );
#else
    env = getenv( "MQAPI_TRACE_OPTIONS" );
#endif

    if (env)
      pExitUserArea->Options = atoi( env );

    else if (myBlankCheck( pExitParms->ExitData, MQ_EXIT_DATA_LENGTH ))
      pExitUserArea->Options = OPTIONS_DEFAULT;

    else
      pExitUserArea->Options = atoi(pExitParms->ExitData);
  }

  /*******************************************************************/
  /* Write to the logfile                                            */
  /*******************************************************************/

  if (rc == MQRC_NONE)
  {
#if MQAT_DEFAULT == MQAT_NSK && defined(_GUARDIAN_TARGET)
    env = getenv( "MQAPITRACELOGFILE" );
#else
    env = getenv( "MQAPI_TRACE_LOGFILE" );
#endif

    if (env)
    {
      /***************************************************************/
      /* Open the log file                                           */
      /***************************************************************/

      sprintf( logfile, "%s.%d.%d.log", env, pExitContext->ProcessId, pExitContext->ThreadId );
#if MQAT_DEFAULT == MQAT_NSK
      fp = fopen_oss( logfile, "w" );
#else
      fp = fopen( logfile, "w" );
#endif

      if (fp == NULL)
      {
        rc = MQRC_API_EXIT_ERROR;
        pExitParms->ExitResponse = MQXCC_FAILED;
      }
      else
      {
        char buffer1[50] = "";
        char buffer2[50] = "";

        myGetAbsoluteTime( buffer1, sizeof(buffer1) );
        myGetRelativeTime( buffer2, pExitUserArea );

        pExitUserArea->fp = fp;
        fprintf(fp, "MQAPI Trace\n");
        fprintf(fp, TITLE_FORMAT, "START", buffer1, buffer2);

        if (pExitUserArea->Options & OPTIONS_DUMP_PARMS_AT_START)
          DumpParms( fp, pExitParms );

        if (pExitUserArea->Options & OPTIONS_DUMP_CONTEXT_AT_START)
          DumpContext( fp, pExitContext );

        fprintf(fp, "--------------------------------------------------------------------------\n");
      }

      /***************************************************************/
      /* Register the MQBACK entrypoints                             */
      /***************************************************************/
      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_BACK
                                        , (PMQFUNC) BackBefore
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_AFTER
                                        , MQXF_BACK
                                        , (PMQFUNC) BackAfter
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      /***************************************************************/
      /* Register the MQBEGIN entrypoints                            */
      /***************************************************************/

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_BEGIN
                                        , (PMQFUNC) BeginBefore
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_AFTER
                                        , MQXF_BEGIN
                                        , (PMQFUNC) BeginAfter
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      /***************************************************************/
      /* Register the MQCLOSE entrypoints                            */
      /***************************************************************/

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_CLOSE
                                        , (PMQFUNC) CloseBefore
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_AFTER
                                        , MQXF_CLOSE
                                        , (PMQFUNC) CloseAfter
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      /***************************************************************/
      /* Register the MQCMIT entrypoints                             */
      /***************************************************************/

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_CMIT
                                        , (PMQFUNC) CmitBefore
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_AFTER
                                        , MQXF_CMIT
                                        , (PMQFUNC) CmitAfter
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      /***************************************************************/
      /* Register the MQCONN entrypoints                             */
      /***************************************************************/

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_CONN
                                        , (PMQFUNC) ConnBefore
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_AFTER
                                        , MQXF_CONN
                                        , (PMQFUNC) ConnAfter
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      /***************************************************************/
      /* Register the MQCONNX entrypoints                            */
      /***************************************************************/

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_CONNX
                                        , (PMQFUNC) ConnxBefore
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_AFTER
                                        , MQXF_CONNX
                                        , (PMQFUNC) ConnxAfter
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      /***************************************************************/
      /* Register the MQDISC entrypoints                             */
      /***************************************************************/

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_DISC
                                        , (PMQFUNC) DiscBefore
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_AFTER
                                        , MQXF_DISC
                                        , (PMQFUNC) DiscAfter
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      /***************************************************************/
      /* Register the MQSTAT entrypoints                             */
      /***************************************************************/

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_STAT
                                        , (PMQFUNC) StatBefore
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_AFTER
                                        , MQXF_STAT
                                        , (PMQFUNC) StatAfter
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }


      /***************************************************************/
      /* Register the MQGET entrypoints                              */
      /***************************************************************/

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_GET
                                        , (PMQFUNC) GetBefore
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if ((rc == MQRC_NONE) && (pExitUserArea->Options & OPTIONS_DUMP_DATACONV))
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_DATA_CONV_ON_GET
                                        , (PMQFUNC) GetBeforeDataConv
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_AFTER
                                        , MQXF_GET
                                        , (PMQFUNC) GetAfter
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      /***************************************************************/
      /* Register the MQINQ entrypoints                              */
      /***************************************************************/

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_INQ
                                        , (PMQFUNC) InqBefore
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_AFTER
                                        , MQXF_INQ
                                        , (PMQFUNC) InqAfter
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }
      /***************************************************************/
      /* Register the MQOPEN entrypoints                             */
      /***************************************************************/

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_OPEN
                                        , (PMQFUNC) OpenBefore
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_AFTER
                                        , MQXF_OPEN
                                        , (PMQFUNC) OpenAfter
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }
      /***************************************************************/
      /* Register the MQSUB entrypoints                              */
      /***************************************************************/

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_SUB
                                        , (PMQFUNC) SubBefore
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_AFTER
                                        , MQXF_SUB
                                        , (PMQFUNC) SubAfter
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      /***************************************************************/
      /* Register the MQSUBRQ entrypoints                            */
      /***************************************************************/

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_SUBRQ
                                        , (PMQFUNC) SubrqBefore
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_AFTER
                                        , MQXF_SUBRQ
                                        , (PMQFUNC) SubrqAfter
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      /***************************************************************/
      /* Register the MQCB entrypoints                               */
      /***************************************************************/

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_CB
                                        , (PMQFUNC) CbBefore
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_AFTER
                                        , MQXF_CB
                                        , (PMQFUNC) CbAfter
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      /***************************************************************/
      /* Register the MQCTL entrypoints                              */
      /***************************************************************/

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_CTL
                                        , (PMQFUNC) CtlBefore
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_AFTER
                                        , MQXF_CTL
                                        , (PMQFUNC) CtlAfter
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      /***************************************************************/
      /* Register the MQ Callback entrypoints                        */
      /***************************************************************/

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_CALLBACK
                                        , (PMQFUNC) CallbackBefore
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_AFTER
                                        , MQXF_CALLBACK
                                        , (PMQFUNC) CallbackAfter
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      /***************************************************************/
      /* Register the MQPUT entrypoints                              */
      /***************************************************************/

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_PUT
                                        , (PMQFUNC) PutBefore
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_AFTER
                                        , MQXF_PUT
                                        , (PMQFUNC) PutAfter
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      /***************************************************************/
      /* Register the MQPUT1 entrypoints                             */
      /***************************************************************/

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_PUT1
                                        , (PMQFUNC) Put1Before
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_AFTER
                                        , MQXF_PUT1
                                        , (PMQFUNC) Put1After
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      /***************************************************************/
      /* Register the MQSET entrypoints                              */
      /***************************************************************/

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_SET
                                        , (PMQFUNC) SetBefore
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_AFTER
                                        , MQXF_SET
                                        , (PMQFUNC) SetAfter
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      /***************************************************************/
      /* Register the xa_close entrypoints                           */
      /***************************************************************/

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_XACLOSE
                                        , (PMQFUNC) XACloseBefore
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_AFTER
                                        , MQXF_XACLOSE
                                        , (PMQFUNC) XACloseAfter
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }


      /***************************************************************/
      /* Register the xa_commit entrypoints                          */
      /***************************************************************/

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_XACOMMIT
                                        , (PMQFUNC) XACommitBefore
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_AFTER
                                        , MQXF_XACOMMIT
                                        , (PMQFUNC) XACommitAfter
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }


      /***************************************************************/
      /* Register the xa_complete entrypoints                        */
      /***************************************************************/

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_XACOMPLETE
                                        , (PMQFUNC) XACompleteBefore
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_AFTER
                                        , MQXF_XACOMPLETE
                                        , (PMQFUNC) XACompleteAfter
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      /***************************************************************/
      /* Register the xa_end entrypoints                             */
      /***************************************************************/

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_XAEND
                                        , (PMQFUNC) XAEndBefore
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_AFTER
                                        , MQXF_XAEND
                                        , (PMQFUNC) XAEndAfter
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      /***************************************************************/
      /* Register the xa_forget entrypoints                          */
      /***************************************************************/

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_XAFORGET
                                        , (PMQFUNC) XAForgetBefore
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_AFTER
                                        , MQXF_XAFORGET
                                        , (PMQFUNC) XAForgetAfter
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }


      /***************************************************************/
      /* Register the xa_open entrypoints                            */
      /***************************************************************/

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_XAOPEN
                                        , (PMQFUNC) XAOpenBefore
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_AFTER
                                        , MQXF_XAOPEN
                                        , (PMQFUNC) XAOpenAfter
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      /***************************************************************/
      /* Register the xa_prepare entrypoints                         */
      /***************************************************************/

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_XAPREPARE
                                        , (PMQFUNC) XAPrepareBefore
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_AFTER
                                        , MQXF_XAPREPARE
                                        , (PMQFUNC) XAPrepareAfter
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      /***************************************************************/
      /* Register the xa_recover entrypoints                         */
      /***************************************************************/

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_XARECOVER
                                        , (PMQFUNC) XARecoverBefore
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_AFTER
                                        , MQXF_XARECOVER
                                        , (PMQFUNC) XARecoverAfter
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }


      /***************************************************************/
      /* Register the xa_rollback entrypoints                        */
      /***************************************************************/

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_XAROLLBACK
                                        , (PMQFUNC) XARollbackBefore
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_AFTER
                                        , MQXF_XAROLLBACK
                                        , (PMQFUNC) XARollbackAfter
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      /***************************************************************/
      /* Register the xa_start entrypoints                           */
      /***************************************************************/

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_XASTART
                                        , (PMQFUNC) XAStartBefore
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_AFTER
                                        , MQXF_XASTART
                                        , (PMQFUNC) XAStartAfter
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      /***************************************************************/
      /* Register the ax_reg entrypoints                             */
      /***************************************************************/

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_AXREG
                                        , (PMQFUNC) AXRegBefore
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_AFTER
                                        , MQXF_AXREG
                                        , (PMQFUNC) AXRegAfter
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      /***************************************************************/
      /* Register the ax_unreg entrypoints                           */
      /***************************************************************/

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_BEFORE
                                        , MQXF_AXUNREG
                                        , (PMQFUNC) AXUnregBefore
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_AFTER
                                        , MQXF_AXUNREG
                                        , (PMQFUNC) AXUnregAfter
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }
      /***************************************************************/
      /* Register the termination entrypoint                         */
      /***************************************************************/

      if (rc == MQRC_NONE)
      {
        pExitParms->Hconfig->MQXEP_Call ( pExitParms->Hconfig
                                        , MQXR_CONNECTION
                                        , MQXF_TERM
                                        , (PMQFUNC) Terminate
                                        , NULL
                                        , pCompCode
                                        , pReason
                                        );

        if (*pReason != MQRC_NONE)
        {
          rc = *pReason;
          pExitParms->ExitResponse = MQXCC_FAILED;
        }
      }
    }
  }

  return;
}


