/* %Z% %W% %I% %E% %U% */
/***********************************************************************/
/*                                                                     */
/* Program name: AMQSCLM                                               */
/*                                                                     */
/* Description: Active Cluster Queue Message Distribution Service      */
/*                                                                     */
/*   <copyright                                                        */
/*   notice="lm-source-program"                                        */
/*   pids="5724-H72"                                                   */
/*   years="2011,2014"                                                 */
/*   crc="3120257962" >                                                */
/*   Licensed Materials - Property of IBM                              */
/*                                                                     */
/*   5724-H72                                                          */
/*                                                                     */
/*   (C) Copyright IBM Corp. 2011, 2014 All Rights Reserved.           */
/*                                                                     */
/*   US Government Users Restricted Rights - Use, duplication or       */
/*   disclosure restricted by GSA ADP Schedule Contract with           */
/*   IBM Corp.                                                         */
/*   </copyright>                                                      */
/*                                                                     */
/***********************************************************************/
/*                                                                     */
/* This program is intended to be used in a cluster queue manager      */
/* environment where multiple instances of one or more clustered       */
/* queues exist across the cluster. In this environment this program   */
/* will monitor those queues and ensure that messages put to the       */
/* queues are directed to the instances of the queues that have        */
/* attached consuming applications.                                    */
/*                                                                     */
/* This tool is designed for the scenario where consuming applications */
/* are typically always connected, the cases where they become         */
/* disconnected from one or more instances of the queue are an         */
/* infrequent event. It is also based on the principle that the number */
/* of queue managers that are interested in any one monitored queue is */
/* relatively small. The more changes in consumer connections and      */
/* the number of interested queue managers will impose a greater       */
/* level of load on the MQ cluster infrastructure.                     */
/*                                                                     */
/* An instance of this program must be running against every queue     */
/* manager in the cluster that owns an instance of a queue being       */
/* monitored.                                                          */
/*                                                                     */
/* This program checks the status of a set of local cluster queues     */
/* to determine if each one has one or more consuming applications or  */
/* not. A queue with attached consumers is known as an 'active' queue. */
/*                                                                     */
/* Active queues are then configured within the cluster to have a      */
/* higher cluster workload priority than inactive queues. This         */
/* configuration is automatically propagated across the cluster using  */
/* standard MQ clustering capabilities.                                */
/*                                                                     */
/* Queue managers putting messages to these cluster queues will        */
/* automatically send messages to the instances of a queue with the    */
/* highest cluster workload priority. This ensures that new messages   */
/* are directed to the active instances of a queue.                    */
/*                                                                     */
/* The program can also requeue messages that are on inactive queues   */
/* to an active instance(s) of the cluster queue.                      */
/*                                                                     */
/* The basic logic to achieve this is as follows:                      */
/*                                                                     */
/* (1)  Obtain list of names of local cluster queues                   */
/*      that are subject to monitoring by this program.                */
/* (2)  For each queue in the list:                                    */
/*     (2a)  Determine its current state.                              */
/*     (2b)  Alter the queue's CLWLPRTY if necessary to achieve        */
/*           its desired state.                                        */
/*     (2c)  If inactive & CURDEPTH >0 and if there is another active  */
/*           Q instance in the cluster, move messages off (requeue)    */
/*                                                                     */
/* A running instance of this tool may be stopped in a number of ways: */
/*  - Terminate/quiesce the connected queue manager                    */
/*  - Get inhibit the reply queue dedicated to the tool                */
/*  - Send a message to the dedicated queue using a specific correlId  */
/*    ("STOP CLUSTER MONITOR")                                         */
/*                                                                     */
/***********************************************************************/
/*                                                                     */
/* Usage:   AMQSCLM -m QMgrName -c ClusterName                         */
/*                  (-q QNameMask | -f QListFile) -r MonitorQName      */
/*                  -l ReportDir [-i Interval] [-t]                    */
/*                  [-u ActiveVal] [-d] [-s] [-v]                      */
/*                                                                     */
/*  where:  -m QMgrName     Queue manager to monitor                   */
/*          -c ClusterName  Cluster containing the queues to monitor   */
/*          -q QNameMask    Queue(s) to monitor, where a trailing '*'  */
/*                          will monitor all matching queues           */
/*          -f QListFile    File containing list of queue(s) or queue  */
/*                          masks to monitor. Contains one queue name  */
/*                          per line.                                  */
/*                          (not valid if -q argument provided)        */
/*          -r MonitorQName Local queue to be used exclusively by the  */
/*                          monitor                                    */
/*          -l ReportDir    Directory path to store logged             */
/*                          informational messages                     */
/*          -i Interval     (optional) Interval in seconds at which    */
/*                          the monitor checks the queues.             */
/*                          Defaults to 300 (5 minutes)                */
/*          -t              (optional) Transfer messages from inactive */
/*                          queues to active instances in the cluster  */
/*                          (No transfer by default)                   */
/*          -u ActiveVal    (optional) Automatically switch the        */
/*                          CLWLUSEQ value of a queue from the         */
/*                          'ActiveVal' to 'ANY' while the queue is    */
/*                          inactive                                   */
/*                          (ActiveVal may be 'LOCAL' or 'QMGR')       */
/*                          (not modified by default)                  */
/*          -d              (optional) Enable additional diagnostic    */
/*                          output                                     */
/*                          (No diagnostic output by default)          */
/*          -s              (optional) Enable minimal statistics       */
/*                          output per interval                        */
/*                          (No per-iteration statistics output by     */
/*                          default)                                   */
/*          -v              (optional) Log report information to       */
/*                          standard out                               */
/*                                                                     */
/* Example: AMQSCLM -m QM1 -c CLUS1 -f /QList.txt -r MONQ              */
/*                  -l /mon_reports -t -u QMGR -s                      */
/*                                                                     */
/*          Monitors queue manager QM1's local cluster queues in       */
/*          cluster CLUST1, specifically the queues listed in file     */
/*          /QList.txt.                                                */
/*          The local queue MONQ is used as the monitor's working      */
/*          queue.                                                     */
/*          This example stores reports and informational messages     */
/*          in files created in the existing directory /mon_reports    */
/*          Any messages queued on inactive queues will be transferred */
/*          to alternative, active, instances of the queue in the      */
/*          cluster. The CLWLUSEQ property of the queues will be       */
/*          modified to allow message transer.                         */
/*          Minimal statistical information is reported.               */
/*                                                                     */
/***********************************************************************/

/* Include standard headers */
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Include MQSeries headers */
#include <cmqc.h>
#include <cmqcfc.h>
#include <cmqxc.h>

#if (MQAT_DEFAULT == MQAT_WINDOWS_NT)
  #include <windows.h>
#else
  #include <unistd.h>
  #include <limits.h>
  #include <string.h>
  #if (MQAT_DEFAULT == MQAT_MVS)
    #define _XOPEN_SOURCE_EXTENDED 1
    #define _OPEN_MSGQ_EXT
  #endif
  #include <sys/time.h>
#endif
#include <stdarg.h>
#include <sys/stat.h>

/***********************************************************************/
/*                               Constants                             */
/***********************************************************************/

#if !defined(FALSE)
#define FALSE 0
#endif

#if !defined(TRUE)
#define TRUE (!FALSE)
#endif

#if (MQAT_DEFAULT == MQAT_WINDOWS_NT)
  #define MAX_FILE_PATH   _MAX_FNAME
#else
  #define MAX_FILE_PATH   PATH_MAX
#endif

/***********************************************************************/
/* States of a queue                                                   */
/*                                                                     */
/* Each monitored queue can be active or inactive. An active queue is  */
/* one that currently has consuming applications attached. An inactive */
/* queue is one without consuming applications.                        */
/*                                                                     */
/* The monitoring application 'advertises' this state to other         */
/* members of the cluster by setting the cluster workload priority for */
/* the queue to be a positive value. An advertises an inactive one     */
/* using a priority of zero.                                           */
/*                                                                     */
/* Queue managers with applications putting to the cluster queue will  */
/* use such cluster workload priorities to decide where to send a      */
/* message, preferring to send to the higher priority queues.          */
/*                                                                     */
/* This tool checks the state of each monitored queue to determine     */
/* their current state, active or inactive, and whether their state    */
/* needs to be changed from its currently defined state.               */
/*                                                                     */
/* Current                                         Desired             */
/* State           State definition                State               */
/* ---------       -----------------------------   ------------        */
/* ACTIVE          IPPROCS > 0 and CLWLPRTY > 0    ACTIVE              */
/* INACTIVE        IPPROCS = 0 and CLWLPRTY = 0    INACTIVE            */
/* BECOME_ACTIVE   IPPROCS > 0 and CLWLPRTY = 0    ACTIVE              */
/* BECOME_INACTIVE IPPROCS = 0 and CLWLPRTY > 0    INACTIVE            */
/***********************************************************************/
#define INACTIVE          1
#define ACTIVE            2
#define BECOME_INACTIVE   3
#define BECOME_ACTIVE     4

/***********************************************************************/
/* Cluster workload priority value set against monitored queues        */
/***********************************************************************/
#define CLWLPRTY_ACTIVE   1          /* CLWLPRTY for an active queue   */
#define CLWLPRTY_INACTIVE 0          /* CLWLPRTY for an inactive queue */

/***********************************************************************/
/* General constants                                                   */
/***********************************************************************/
/* Number of messages requeued per transaction */
#define REQUEUE_BATCH        50
/* Default polling interval 300 seconds = 5 minutes */
#define DEFAULT_POLLING_INT  300
/* 10 seconds max PCF wait interval in milliseconds */
#define MAX_PCF_WAIT_INT     10000
/* 2 second min PCF wait interval in milliseconds */
#define MIN_PCF_WAIT_INT     2000
/* 30 minute measurement interval in seconds */
#define MEASUREMENT_INT_MAX  1800
/* Maximum report file size in bytes */
#define MAX_RPT_SIZE         1000000
/* Correlid of stop monitor msg */
#define STOP_MON_ID          "STOP CLUSTER MONITOR\0\0\0\0"
/* Initial size of buffer for transferring messages */
#define INITIAL_PCF_MSG_SIZE 2048
/* Initial size of buffer for transferring messages */
#define INITIAL_MSG_SIZE     4096
/* Time to wait before actively monitoring. This allows applications   */
/* to attach to the queues after a restart before we actively start    */
/* altering the cluster and moving messages (in seconds) */
#define START_DELAY_TIME     10

/***********************************************************************/
/* Constants for error reporting                                       */
/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Message code prefixes                                               */
#define RPT_PREFIX     "CLM"
#define W              'W'                          /* Warning message */
#define E              'E'                            /* Error message */
#define I              'I'                    /* Informational message */
#define D              'D'                            /* Debug message */

/*---------------------------------------------------------------------*/
/* Report log message codes                                            */
#define DEBUGMSGD      0
#define STARTMSGI      1
#define TERMINATEMSGI  2
#define MQCONNERR      10
#define MQOPENERR      11
#define MQOPENREQIERR  12
#define MQOPENREQOERR  13
#define MQPUTCHGQERR   14
#define MQGETCHGQERR   15
#define MQCLOSEREQIERR 16
#define MQCLOSEREQOERR 17
#define MQDISCERR      18
#define MQPUTPCFERR    19
#define MQGETPCFERR    20
#define MQPUTREQERR    21
#define MQGETREQERR    22
#define MQCMITREQERR   23
#define MQBACKREQERR   24
#define MQCLOSEERR     25
#define PCFINQQERR     26
#define PCFCHGQERR     27
#define PCFINQCLQERR   28
#define CHGQI          29
#define REQNOALTW      30
#define REQI           31
#define NOALTINSTD     32
#define RPTOPENERR     33
#define RPTRENAMEERR   34
#define QLISTOPENERR   35
#define QLISTCLOSEERR  36
#define QLISTOPEND     37
#define QLISTREADRD    38
#define QLISTRETURND   39
#define QLISTEMPTYRECD 40
#define QLISTREADE     41
#define QLISTEOFD      42
#define STOPMONI       43
#define QMGRTERMI      44
#define MEASUREI       45
#define NEWREPORTI     46
#define CHGUSEQI       47
#define STATSMSGI      48
#define MONACTI        49
#define STARTDELAYI    50
#define NOQERR         51
#define USEQERR        52
#define MQINQERR       53
#define MSGLENERR      54

/*---------------------------------------------------------------------*/
/* Non-report log message codes (stderr messages)                      */
#define RPTPATHERR     80
#define RPTWRITEERR    81
#define ARGERR         82
#define ARGMISSINGERR  83
#define ARGCONFLICTERR 84
#define ARGLENERR      85

/***********************************************************************/
/*                                Globals                              */
/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Report file declarations                                            */
FILE           *pReport;                     /* pointer to report file */
/* Report file names:  path\QmgrName.ClusterName.RPT0n.LOG\0 */
char           RptFilename[MAX_FILE_PATH+1];
char           RptFilename02[MAX_FILE_PATH+1];
char           RptFilename03[MAX_FILE_PATH+1];
char           dbs[500];                     /* Formatted debug output */

#if defined(AMQ_PROD_BUILD)
const static char version[] = "%I% (product)";
#else
const static char version[] = "%I% (custom)";
#endif

/*---------------------------------------------------------------------*/
/* Arguments & user inputs to the program                              */
/* Parameter:  Queue name(s) to monitor */
char           QNameMask[MQ_Q_NAME_LENGTH+1];
/* Parameter:  qmgr name */
char           QMgrName[MQ_Q_MGR_NAME_LENGTH+1];
/* Parameter:  Q Cluster name to monitor*/
char           QClusterName[MQ_CLUSTER_NAME_LENGTH+1];
/* Parameter:  Input file containing Qs to monitor */
char           QListFileName[MAX_FILE_PATH+1];
/* Parameter:  Reply queue name */
char           ReplyQ[MQ_Q_NAME_LENGTH+1];
/* Parameter:  Report file path */
char           ReportPath[MAX_FILE_PATH+1];
/* Parameter:  Log to standard out as well as the report file */
unsigned short LogToStdout;
/* Parameter:  Polling interval in seconds */
long           PollInterval;
/* Parameter:  debug flag */
unsigned short DebugFlag;
/* Parameter:  Message transfer flag */
unsigned short TransferFlag;
/* Parameter:  Switch the CLWLUSEQ value flag */
unsigned short SwitchUseQ;
MQLONG         UseQActiveVal;
/* Parameter:  Minimal statistics data */
unsigned short StatsFlag;

/*---------------------------------------------------------------------*/
/* Global control variables                                            */
unsigned short StopMon = FALSE;            /* Flag to stop the monitor */
const char     stopmonmqclq[] = STOP_MON_ID; /* CorrelId to stop on    */

/*---------------------------------------------------------------------*/
/* Queue List input file declarations                                  */
FILE           *pQListFile;    /* pointer to Q list file               */
unsigned short useQListFile;   /* 1 = read names/masks from input file */
                               /* 0 = use single mask passed as parm   */
char           pid[12];        /* Current process ID written to log    */

/*---------------------------------------------------------------------*/
/* Global variables used in PCF conversations                          */
unsigned PCFwaitInterval;    /* Time to wait for PCF response messages */
                             /* (in milliseconds)                      */
unsigned PCFexpiry;          /* Expiry time on PCF command messages    */
                             /* (in 10ths of a second)                 */

/*---------------------------------------------------------------------*/
/* Structure type for Queue attributes coming back in PCF response     */
/* Note: If the sample is modified to monitor different metrics for    */
/*       modifying cluster workload management, this structure may     */
/*       require modifying.                                            */
typedef struct LocalQParms
{
  char        QName[MQ_Q_NAME_LENGTH+1];
  char        QClusterName[MQ_CLUSTER_NAME_LENGTH+1];
  char        QClusterQmgrName[MQ_Q_MGR_NAME_LENGTH+1];
  MQLONG      IPPROCS;
  MQLONG      CLWLPRTY;
  MQLONG      CLWLUSEQ;
  MQLONG      CURDEPTH;
} LocalQParms;

/***********************************************************************/
/*                         Function prototypes                         */
/***********************************************************************/

void ProcessStringParm(MQCFST      *pPCFString,
                       LocalQParms *DefnLQ);

void ProcessIntegerParm(MQCFIN      *pPCFInteger,
                        LocalQParms *DefnLQ);

void checkState(LocalQParms     DefnLQ,
                unsigned short *currentState);

void MQParmCpy(char *target,
               char *source,
               int   tlength,
               int   slength);

void PutPCFMsg(MQHCONN   hConn,
               MQHOBJ    hQName,
               char      ReplyQName[MQ_Q_NAME_LENGTH+1],
               MQBYTE   *MsgData,
               MQLONG    UserMsgLen,
               MQBYTE24 *RequestMsgId,
               MQLONG   *pCompCode,
               MQLONG   *pReason);

void GetPCFMsg(MQHCONN   hConn,
               MQHOBJ    hQName,
               MQBYTE  **MsgData,
               MQLONG   *ReadBufferLen,
               MQBYTE24  CorrelId,
               unsigned  waitTime,
               MQLONG   *pCompCode,
               MQLONG   *pReason);

MQHOBJ OpenQ(MQHCONN     hConn,
             char        QName[MQ_Q_NAME_LENGTH+1],
             MQLONG      OpenOpts,
             MQLONG     *pCompCode,
             MQLONG     *pReason);

long ReQueue(MQHCONN  hConn,
             char    *QName,
             MQLONG   CurDepth);

void CheckReason(MQLONG Reason);

void WriteToRpt(unsigned  errid,
                int       numSInserts,
                int       numLInserts,
                char     *insert,
                ...);

int readNextQMask(char     *singleQMask,
                  unsigned  qMaskLen);

MQLONG ParseArguments(int   argc,
                      char *argv[]);

MQLONG buildInquireQPCF(MQBYTE *pAdminMsg,
                        MQLONG  AdminMsgLen,
                        char   *QNameMask,
                        char   *QCLusterName);

MQLONG buildChangeQPCF(MQBYTE *pAdminMsg,
                       MQLONG  AdminMsgLen,
                       char   *QName,
                       MQLONG  TargetCLWLPRTY,
                       MQLONG  TargetCLWLUSEQ);

MQLONG buildInquireClusterQPCF(MQBYTE *pAdminMsg,
                               MQLONG  AdminMsgLen,
                               char   *QName,
                               char   *QClusterName);

int parseInqQRespPCF(MQBYTE      *pAdminMsg,
                     LocalQParms *pDefnLQ,
                     MQLONG      *pLastQResponse);

unsigned short checkForAlternativeQueue(MQBYTE      *pAdminMsg,
                                        MQLONG      *pAdminMsgLen,
                                        MQHCONN      hConn,
                                        MQHOBJ       hCommandQ,
                                        MQHOBJ       hQLReplyQ,
                                        LocalQParms *pDefnLQ,
                                        MQCHAR      *QClusterName);

long getCurrentMilliseconds();

/***********************************************************************/
/*                               Functions                             */
/***********************************************************************/

/***********************************************************************/
/* The main processing flow of this tool is contained in the main      */
/* function below. The following steps are performed:                  */
/*                                                                     */
/*     1. Connect to queue manager                                     */
/*     2. Loop while the Qmgr is available and no errors reported      */
/*     3.   Open PCF system command queue if not yet opened            */
/*     4.   Open reply queue if not yet opened                         */
/*          Read queue names/masks from queue list file.               */
/*          For each queue name/mask read:                             */
/*     5.     Build "Inquire Queue" PCF command                        */
/*     6.     Send cmd to PCF command queue                            */
/*     7.     Loop until LAST PCF response received:                   */
/*     8.       Read response msg from REPLY Q with CorrelId           */
/*     9.       Parse the PCF Response for required metrics            */
/*    10.       Compare the previous state to the queue's state        */
/*    11.       If a state change is needed:                           */
/*    12.         Build "Change Queue" PCF command                     */
/*    13.         Send to PCF Command Q                                */
/*    14.         Read reply with CorrelId                             */
/*    15.         Verify that PCF return code is OK                    */
/*    16.       If queue is in inactive state and CURDEPTH > 0         */
/*    17.         Locate an alternate active instance(s) of this queue */
/*    18.         If an alternate active instance(s) is found:         */
/*    19.           Move CURDEPTH number of msgs to active instance(s) */
/*    20.   Sleep for the polling interval (via MQGET on stop msg)     */
/*    21. End of loop while-Qmgr-available and stop not rec'd          */
/*    22. Close the reply queue                                        */
/*    23. Close the PCF system command queue                           */
/*    24. Disconnect from queue manager                                */
/*                                                                     */
/*  All MQAPI calls (except MQCLOSE and MQDISC) are made with the      */
/*  "fail if quiescing" option.  Also, all reason codes are checked    */
/*  for a stopped, stopping, quiescing, and connection-broken-to qmgr; */
/*  And if one of those is detected, StopMon flag is set to TRUE       */
/*  so that the rest of the program knows to cease making MQAPI calls, */
/*  and prepare to terminate.                                          */
/*                                                                     */
/*  This monitoring application will continue to run indefinitely      */
/*  while it has a valid connection to the queue manager, the          */
/*  report logs are accessible and the reply queue used by the monitor */
/*  is not get inhibited.                                              */
/*                                                                     */
/*  The application will continue to run even if it does not have      */
/*  access to the reply queue. This allows a single instance to this   */
/*  application to be actively monitoring the queues with one or       */
/*  more instances of the application waiting to take over in the      */
/*  event of that instance terminating.                                */
/*                                                                     */
/***********************************************************************/
int main( int argc, char *argv[] )
{
  /* MQ handles & options */
  MQHCONN        hConn = MQHC_UNUSABLE_HCONN;    /* QMgr connection    */
  MQHOBJ         hQMgrObj = MQHO_UNUSABLE_HOBJ;
  MQHOBJ         hCommandQ = MQHO_UNUSABLE_HOBJ; /* PCF command queue  */
  MQHOBJ         hQLReplyQ = MQHO_UNUSABLE_HOBJ; /* Reply queue        */
  MQLONG         OpenOpts;                       /* MQOPEN options     */
  MQLONG         CloseOpts;                      /* MQCLOSE options    */
  MQOD           ObjDesc = {MQOD_DEFAULT};
  MQLONG         Selectors[1];


  /* MQ Completion & Reason code from most recently issued MQ API */
  MQLONG         CompCode;
  MQLONG         Reason;

  /* Variables associated with PCF commands and responses */
  MQLONG         AdminMsgLen;      /* Length of PCF message buffer     */
  MQBYTE        *pAdminMsg = NULL; /* Ptr to PCF message buffer        */
  MQCFH         *pPCFHeader;       /* Ptr to PCF header structure      */
  MQLONG         lastQResponse;    /* Last PCF response msg flag       */
  MQBYTE24       RequestMsgId;     /* MQMD msgid from PCF inquireQ cmd */
  MQBYTE24       ChgQRequestMsgId; /* MQMD msgid from PCF chgQ cmd     */
  LocalQParms    DefnLQ;           /* Queue detail from PCF response   */
  MQLONG         PCFMsgLen;        /* length of change Q PCF message   */

  /* Variables associated with Queue State */
  unsigned short currentState;     /* Current state of a queue         */
  unsigned short foundAltInstance; /* Alternate Q instance found flag  */
  unsigned short errorReported = FALSE;/* Actively monitoring          */
  MQLONG         TargetCLWLPRTY;   /* Value of CLWLPRTY to change to   */
  unsigned short firstTimeHere = TRUE;    /* First time round the loop */
  MQLONG         qMgrCLWLUSEQ;

  /* Variables associated with queue name masks */
  int            qmaskrc;          /* Return code from readNext        */

  /* Variables assocated with message transfer processing */
  char           TransferQName[MQ_Q_NAME_LENGTH+1];
  MQLONG         TransferCurDepth;

  MQLONG         TargetCLWLUSEQ;/* Value to udpate CLWLUSEQ to when -u */
                                /* is set.                             */

  /*-------------------------------------------------------------------*/
  /* Periodically performance stats are written to the log, once every */
  /* 'interval'. Stats are collected during the interval, reported and */
  /* then reset for the next interval.                                 */
  /* Interval start time */
  long           intStartTime;
  /* Start time of individual queue check process */
  long           starttime;
  /* End time of individual queue check process */
  long           endtime;
  /* Time since start of measurement interval  */
  long           intElapsedTime;
  /* Minimum check process time this interval */
  long           minTime;
  /* Maximum check process time this interval */
  long           maxTime;
  /* Total processing time during this measurement interval */
  long           totalProcTime;
  /* Processing time during this check process */
  long           procTime;
  /* Number of polling intervals completed in this measurement interval */
  long           pollIntCount = 0;
  /* Number of queues checked this check process */
  long           queuesChecked = 0;
  /* Number of queues checked this interval */
  long           totalQueuesChecked = 0;
  /* Number of queue active/inactive changes this check process */
  long           activeChanges = 0;
  /* Number of queue active/inactive changes this interval */
  long           totalActiveChanges = 0;
  /* Number of messages transferred this check process */
  long           msgsRequeued = 0;
  /* Number of messages transferred this interval */
  long           totalMsgsRequeued = 0;

  /* Get the current process ID to write to the report files */
#if (MQAT_DEFAULT == MQAT_WINDOWS_NT)
  sprintf(pid, "%d", GetCurrentProcessId());
#else
  sprintf(pid, "%d", getpid());
#endif

  /*********************************************************************/
  /* Mainline section  0:        Parse input arguments                 */
  /*********************************************************************/
  CompCode = ParseArguments(argc, argv);
  if (CompCode != MQCC_OK)
    exit(-1);                    /* If the arguments are invalid, exit */

  WriteToRpt( STARTMSGI, 1, 0, "" );        /* Report info startup msg */
  /* If the report file could not be opened, terminate immediately */
  if(StopMon)
    goto MOD_EXIT;

  /*-------------------------------------------------------------------*/
  /* The majority of the monitoring is achieved through sending        */
  /* request PCF messages to the queue manager's command queue and     */
  /* waiting for a response. Useful monitoring is only possible while  */
  /* these commands are being processed in a timely manner, so set the */
  /* duration that we are willing to wait for a response message       */
  /* accordingly.                                                      */
  /* (We use the poll interval as an indication of how loaded the      */
  /* system is and thus how long we should consider waiting)           */
  PCFwaitInterval = PollInterval * 1000;  /* convert to milliseconds   */
  if (PCFwaitInterval > MAX_PCF_WAIT_INT)
    PCFwaitInterval = MAX_PCF_WAIT_INT;
  if(PCFwaitInterval < MIN_PCF_WAIT_INT)
    PCFwaitInterval = MIN_PCF_WAIT_INT;

  /*-------------------------------------------------------------------*/
  /* As we are only going to wait for a finite time for response PCF   */
  /* messages we can also set an expiry on the messages to prevent     */
  /* a build up of messages if they fail to be processed.              */
  /* Set expiry time for PCF command messages = 2 x PCF response.      */
  /* In DEBUG mode, set higher, to 10 minutes to aid problem diagnosis.*/
  /* Minimum value is MIN_EXPIRY.                                      */
  if (DebugFlag)
    PCFexpiry = 10*60*10;  /* 10 minutes in units of 10ths of a second */
  else
    PCFexpiry = (PCFwaitInterval * 2 / 100);             /*10ths of sec*/

  /*-------------------------------------------------------------------*/
  /* Initialize file pointer to queue list file.                       */
  if (useQListFile)
    pQListFile = NULL;

  /*********************************************************************/
  /* Mainline section  1:        Connect to Queue Manager              */
  /*********************************************************************/
  /* Connect to the queue manager that the tool is monitoring queues   */
  /* on, exit if that fails.                                           */
  MQCONN( QMgrName, &hConn, &CompCode, &Reason);
  if ( CompCode != MQCC_OK )
  {
    WriteToRpt( MQCONNERR, 1, 2, QMgrName, CompCode, Reason );
    /* Print out an appropriate msg indicating we are terminating.     */
    CheckReason(Reason);
    /* If there is no connection to the queue manager, we terminate. */
    goto MOD_EXIT;
  }

  /*-------------------------------------------------------------------*/
  /* If message transfer has been enabled but not the ability to       */
  /* modify the CLWLUSEQ of the queuee then we should check each time  */
  /* that the queue's setting would correctly allow message transfer   */
  /* (i.e. be set to 'ANY'). If the queue has a setting of 'QMGR' it   */
  /* is the actual queue manager's CLWLUSEQ value that is required,    */
  /* therefore we need to inquire that value when needed.              */
  if (TransferFlag && !SwitchUseQ)
  {
    ObjDesc.ObjectType = MQOT_Q_MGR;
    strncpy(ObjDesc.ObjectQMgrName,
            QMgrName,
            sizeof(ObjDesc.ObjectQMgrName));
    OpenOpts = MQOO_INQUIRE;

    MQOPEN(hConn,
           &ObjDesc,
           OpenOpts,
           &hQMgrObj,
           &CompCode,
           &Reason);

    /* For any sort of failure we should terminate. */
    if ( CompCode != MQCC_OK )
    {
      WriteToRpt( MQOPENERR, 1, 2, QMgrName, CompCode, Reason );
      CheckReason(Reason);
      goto MOD_EXIT;
    }
  }

  /* ----------------------------------------------------------------- */
  /* A single block of memory is used for all PCF administration       */
  /* messages that are used by the monitor, both command requests and  */
  /* responses. We choose a reasonable size to accomodate all the      */
  /* request messages and the responses.                               */
  /* If we ever receive a response bigger than this block we'll        */
  /* reallocate a larger block, so picking the initial correct size is */
  /* not essential.                                                    */
  AdminMsgLen = INITIAL_PCF_MSG_SIZE;
  pAdminMsg = (MQBYTE *)malloc( AdminMsgLen );

  /* ----------------------------------------------------------------- */
  /* Initialize timestamps for measurement intervals.                  */
  intStartTime = getCurrentMilliseconds(); /* interval start time      */
  pollIntCount = 0;                        /* polling interval counter */
  minTime = LONG_MAX;                      /* minimum proc time        */
  maxTime = 0;                             /* maximum proc time        */
  totalProcTime = 0;                       /* total proc time          */

  if (DebugFlag)
  {
    sprintf(dbs,"Start of measurement interval\n");
    WriteToRpt(DEBUGMSGD, 1, 0, dbs);
  }

  /*********************************************************************/
  /* Mainline section 2:       Main loop                               */
  /*                           Exit when queue manager terminates,     */
  /*                           reply queue is get inhibited or when an */
  /*                           error occurs.                           */
  /*********************************************************************/
  /* Loop while Qmgr available, waking up once per interval            */
  /* to monitor all specified local cluster queues.                    */
  /* If the queue manager is quiesced or stopped the tool will stop    */
  while (!StopMon)
  {
    /* Reset the cached queue manager CLWLUSEQ value to ensure we */
    /* check against the current setting */
    qMgrCLWLUSEQ = -1;

    if (DebugFlag)
    {
      sprintf(dbs,"Start of polling interval\n");
      WriteToRpt(DEBUGMSGD, 1, 0, dbs);
    }

    /*******************************************************************/
    /* Mainline sections 3 - 4:   Open PCF command queue & reply queue */
    /*                            if not already opened                */
    /*******************************************************************/
    /* This is performed within the main loop to allow this monitor to */
    /* run even when it does not have exclusive access to its reply    */
    /* queue. This allows 'standby' instances of this application to   */
    /* be running and able to automatically take over from a failed    */
    /* instance if necessary.                                          */

    if (hQLReplyQ == MQHO_UNUSABLE_HOBJ)  /* ReplyQ needs to be opened */
    {
      /* ------------------------------------------------------------- */
      /* Open Reply queue - exclusive so as to prevent others from     */
      /* reading. As long as the same reply queue is used by all       */
      /* instances of the monitoring tool against a single queue       */
      /* manager we know that only a single instance can have this     */
      /* queue open.                                                   */
      OpenOpts = MQOO_INPUT_EXCLUSIVE       /* open queue for input    */
                 | MQOO_FAIL_IF_QUIESCING;  /* but not if MQM stopping */
      hQLReplyQ = OpenQ( hConn, ReplyQ,
                         OpenOpts, &CompCode, &Reason );

      if (Reason != MQRC_NONE)
        CheckReason(Reason);

      /* On first failure, report problem, but not after that */
      if(!errorReported)
      {
        /* report reason, if any; stop if failed */
        if (Reason != MQRC_NONE)
        {
          WriteToRpt( MQOPENERR, 1, 2, ReplyQ, CompCode, Reason );
        }
        else if (DebugFlag)
        {
          sprintf(dbs,"Successfully opened queue %s\n", ReplyQ);
          WriteToRpt(DEBUGMSGD, 1, 0, dbs);
        }
        if(!firstTimeHere)
          errorReported = TRUE;
      }
    }

    if ((CompCode == MQCC_OK) &&
        (hCommandQ == MQHO_UNUSABLE_HOBJ))  /* PCF CmdQ must be opened */
    {
      /* Open PCF command queue                                        */
      OpenOpts = MQOO_OUTPUT               /* open queue for output    */
                 | MQOO_FAIL_IF_QUIESCING; /* but not if MQM stopping  */
      hCommandQ = OpenQ( hConn, "SYSTEM.ADMIN.COMMAND.QUEUE",
                         OpenOpts, &CompCode, &Reason );

      if (Reason != MQRC_NONE)
        CheckReason(Reason);

      /* On first failure, report problem, but not after that */
      if(!errorReported)
      {
        /* report reason, if any; stop if failed */
        if (Reason != MQRC_NONE)
        {
          WriteToRpt( MQOPENERR, 1, 2, "SYSTEM.ADMIN.COMMAND.QUEUE",
                      CompCode, Reason );
        }
        else if (DebugFlag)
        {
          sprintf(dbs,"Successfully opened queue %s\n",
                  "SYSTEM.ADMIN.COMMAND.QUEUE");
          WriteToRpt(DEBUGMSGD, 1, 0, dbs);
        }
        if(!firstTimeHere)
          errorReported = TRUE;
      }
    }

    /*-----------------------------------------------------------------*/
    /* First time round the loop we sleep before actively monitoring.  */
    /* If we're started as soon as the queue manager starts we don't   */
    /* want to check the queues immediately as there will be no        */
    /* attached consuming applications on the monitored queues (as     */
    /* they may take a little while to start) and we would therefore   */
    /* make the queues inactive and try to move any queued messages.   */
    /* It is better to delay this monitoring for a few secons to allow */
    /* time for the consuming application to attach.                   */
    if (firstTimeHere)
    {
      WriteToRpt( STARTDELAYI, 1, 1, "", START_DELAY_TIME);

#if (MQAT_DEFAULT == MQAT_WINDOWS_NT)
        Sleep(START_DELAY_TIME*1000);
#else
        sleep(START_DELAY_TIME);
#endif
    }

    /* Start time of work */
    starttime = getCurrentMilliseconds();

    /*-----------------------------------------------------------------*/
    /* If we have access to all the required MQ resources we can start */
    /* the main monitoring processing.                                 */
    if ( !StopMon &&
         (hCommandQ != MQHO_UNUSABLE_HOBJ) &&
         (hQLReplyQ != MQHO_UNUSABLE_HOBJ) )
    {
      /* When we first start monitoring, log it */
      if( firstTimeHere || errorReported)
      {
        WriteToRpt( MONACTI, 1, 0, "");
        errorReported = FALSE;
        firstTimeHere = FALSE;
      }

      /*---------------------------------------------------------------*/
      /* For each time round the processing loop we re-read any file   */
      /* that was provided that holds queues or queue masks. This      */
      /* allows the contents of the file to change while the monitor   */
      /* is running, allowing dynamic changes to the set of monitored  */
      /* queues.                                                       */
      if (useQListFile)  /* If using the queue list file as the source */
      {
        QNameMask[0] = '\0';
        /* Read next mask */
        qmaskrc = readNextQMask(QNameMask,sizeof(QNameMask));
      }
      else                   /* else, using the single mask provided   */
        qmaskrc = 0;         /* indicates we have the mask             */

      while (!StopMon && (qmaskrc == 0))
      {
        /***************************************************************/
        /* Mainline section 5:      Build PCF "Inquire Queue" command  */
        /* The "Inquire Queue" command will inquire on all local       */
        /* queues with names that match the specified mask/string, and */
        /* which are in the specified cluster.                         */
        /* *************************************************************/

        /* Create the PCF message */
        PCFMsgLen = buildInquireQPCF(pAdminMsg,     /*      msg buffer */
                                     AdminMsgLen,  /* Length of buffer */
                                     QNameMask,     /*   Name of queue */
                                     QClusterName); /* Name of cluster */

        /***************************************************************/
        /* Mainline section 6:  Put "Inquire Queue" msg to the PCF     */
        /*                      command Q                              */
        /*                                                             */
        /* The PCF command message is put to the queue manager's       */
        /* SYSTEM.ADMIN.COMMAND.QUEUE, the queue manager's command     */
        /* server will process each message put to this queue and      */
        /* send a reply message with the results of the command (in    */
        /* this case a list of matching queues).                       */
        /***************************************************************/
        PutPCFMsg(hConn,               /* Connection handle to qmgr    */
                  hCommandQ,           /* Handle to command queue      */
                  ReplyQ,              /* Handle to Reply queue        */
                  (MQBYTE *)pAdminMsg, /* Message body (PCF command)   */
                  PCFMsgLen,           /* Command message length       */
                  &RequestMsgId,       /* MsgID returned               */
                  &CompCode,           /* Completion code              */
                  &Reason);            /* Reason code                  */

        /* Check return/reason code */
        if (CompCode == MQCC_OK)
        {
          /*************************************************************/
          /* Mainline section 7:  Loop until all PCF responses are     */
          /*                      received from the  "Inquire Queue"   */
          /*                      request                              */
          /*************************************************************/

          /*-----------------------------------------------------------*/
          /*     Start of DO-WHILE loop                                */
          do
          {
            /***********************************************************/
            /* Mainline section 8:  Read response from Reply Queue.    */
            /*                      Use Correlation ID to retrieve the */
            /*                      response.                          */
            /*                                                         */
            /* There will be one response PCF message per local queue  */
            /* matched. The last message will have the Control field   */
            /* of the PCF header set to MQCFC_LAST. All others will be */
            /* MQCFC_NOT_LAST.                                         */
            /*                                                         */
            /* An individual Reply message consists of a header        */
            /* followed by a number a parameters, the exact number,    */
            /* type and order will depend upon the type of request.    */
            /*                                                         */
            /* The message is retrieved into a buffer pointed to by    */
            /* pAdminMsg. The buffer should be large enough but will   */
            /* grow if necessary.                                      */
            /* ******************************************************* */

            /*---------------------------------------------------------*/
            /* Set the loop control variable to MQCFC_LAST as the      */
            /* pessimistic default. If the PCF command is successful   */
            /* it will be reset to the Control value.                  */
            /* If MQGET fails for any reason, the loop will terminate  */
            /* and we are done reading queue detail responses for this */
            /* interval.                                               */
            lastQResponse = MQCFC_LAST;
            GetPCFMsg(hConn,                  /* Queue manager handle  */
                      hQLReplyQ,              /* Queue handle          */
                      &pAdminMsg,             /* returned msg          */
                      &AdminMsgLen,           /* length of buffer      */
                      RequestMsgId,           /* CorrelId of response  */
                      PCFwaitInterval,        /* MQGET wait time       */
                      &CompCode,              /* Completion code       */
                      &Reason);               /* Reason code           */

            /* Successfully read a response message */
            if (CompCode == MQCC_OK)
            {
              /*********************************************************/
              /* Mainline section 9:  Parse the PCF response message.  */
              /*                      Store queue information in       */
              /*                      DefnLQ structure.                */
              /*********************************************************/
              if(parseInqQRespPCF(pAdminMsg, &DefnLQ, &lastQResponse))
              {
                /* Count the number of queues checked (for stats) */
                queuesChecked++;

                /*******************************************************/
                /* Mainline section 10: Call checkState to determine   */
                /*                      the queue's current state.     */
                /*******************************************************/
                checkState( DefnLQ, &currentState);

                /*******************************************************/
                /* Mainline section 11:  Check for state changes       */
                /*******************************************************/

                /* Assume no configuration changes needed */
                TargetCLWLPRTY = -1;
                TargetCLWLUSEQ = -1;

                /*-----------------------------------------------------*/
                /* If the activity state has switched, update the      */
                /* queue's CLWLPRTY property. */
                if ( (currentState == BECOME_ACTIVE) ||
                     (currentState == BECOME_INACTIVE) )
                {
                  /* Count the number of changes made (for stats) */
                  activeChanges++;

                  /* Set CLWLPRTY in accordance with the desired state */
                  if (currentState == BECOME_ACTIVE)
                    /* Queue now has consumers, activate queue */
                    TargetCLWLPRTY = CLWLPRTY_ACTIVE;
                  else
                    /* Queue now has no consumers, deactivate queue */
                    TargetCLWLPRTY = CLWLPRTY_INACTIVE;
                }

                /*-----------------------------------------------------*/
                /* If we're configured to automatically change the     */
                /* CLWLUSEQ property check to see if it needs changing.*/
                /* While a queue is inactive we do not want new        */
                /* messages put by applications connected to this      */
                /* queue manager to be sent to the local inactive      */
                /* queue, therefore CLWLUSEQ must be set to ANY. In    */
                /* addition if there are any messages already queued   */
                /* on an inactive queue and we are configured to       */
                /* transfer them then again we need CLWLUSEQ to be set */
                /* to ANY.                                             */
                /* This test is necessary everytime round (rather than */
                /* only when a queue goes inactive) because this tool  */
                /* may previously have modified the value to the       */
                /* inactive state.                                     */
                if ( SwitchUseQ )
                {
                  if ( ( (currentState == INACTIVE) ||
                         (currentState == BECOME_INACTIVE) ) &&
                       (DefnLQ.CLWLUSEQ != MQCLWL_USEQ_ANY) )
                  {
                    TargetCLWLUSEQ = MQCLWL_USEQ_ANY;
                  }
                  else if ( ( (currentState == ACTIVE) ||
                              (currentState == BECOME_ACTIVE) ) &&
                            (DefnLQ.CLWLUSEQ != UseQActiveVal) )
                  {
                    TargetCLWLUSEQ = UseQActiveVal;
                  }
                }
                /*-----------------------------------------------------*/
                /* If we're not modifying the queue's CLWLUSEQ value   */
                /* but we're still expected to transfer messasges we   */
                /* need to check that the queue's existing CLWLUSEQ    */
                /* value will allow us to transfer messages.           */
                else if (TransferFlag)
                {
                  switch(DefnLQ.CLWLUSEQ)
                  {
                    case MQCLWL_USEQ_ANY:
                      /* 'ANY' will allow transfers */
                      break;
                    case MQCLWL_USEQ_AS_Q_MGR:
                      /* If inheriting from the queue manager then the */
                      /* queue manager's value must be set to 'ANY'    */
                      /* for transfers to succeed. */

                      /* If we haven't retrieved the queue manager's   */
                      /* setting in this poll interval, inquire it now */
                      if(qMgrCLWLUSEQ == -1)
                      {
                        Selectors[0] = MQIA_CLWL_USEQ;

                        MQINQ(hConn,
                              hQMgrObj,
                              1,
                              Selectors,
                              1,
                              &qMgrCLWLUSEQ,
                              0,
                              NULL,
                              &CompCode,
                              &Reason);

                        if (CompCode != MQCC_OK)
                          WriteToRpt( MQINQERR, 1, 2, "",
                                      CompCode, Reason);
                        else if (DebugFlag)
                        {
                          sprintf(dbs,"Queue manager CLWLUSEQ=%ld\n",
                                  qMgrCLWLUSEQ);
                          WriteToRpt(DEBUGMSGD, 1, 0, dbs);
                        }

                      }
                      /* 'ANY' will allow transfers */
                      if(qMgrCLWLUSEQ == MQCLWL_USEQ_ANY)
                        break;
                      /* Otherwise, drop through to error... */
                    default: /* 'LOCAL' */
                      /* Write an error to report the problem */
                      WriteToRpt(USEQERR, 1, 0, DefnLQ.QName);
                  }
                }

                /*******************************************************/
                /* Mainline section 12:  Build change queue PCF  msg   */
                /*******************************************************/

                /* Something needs changing */
                if ( (TargetCLWLPRTY != -1) ||
                     (TargetCLWLUSEQ != -1) )
                {
                  /* Build the PCF msg to change the CLWLPRTY and/or   */
                  /* CLWLUSEQ. */
                  PCFMsgLen = buildChangeQPCF(pAdminMsg,
                                              AdminMsgLen,
                                              DefnLQ.QName,
                                              TargetCLWLPRTY,
                                              TargetCLWLUSEQ);

                  if (DebugFlag)
                  {
                    sprintf(dbs,"Change Queue command QName=%s, " \
                                "CLWLPRTY=%ld\n",
                            DefnLQ.QName,TargetCLWLPRTY);
                    WriteToRpt(DEBUGMSGD, 1, 0, dbs);
                  }

                  /*****************************************************/
                  /* Mainline section 13:      Put the change queue    */
                  /*                           msg to PCF cmd Q        */
                  /*****************************************************/
                  PutPCFMsg(hConn,              /* Queue manager       */
                            hCommandQ,          /* command queue       */
                            ReplyQ,             /* reply to queue      */
                            (MQBYTE *)pAdminMsg,/* PCF msg body        */
                            PCFMsgLen,          /* Length of msg body  */
                            &ChgQRequestMsgId,  /* MsgID returned      */
                            &CompCode,          /* Completion code     */
                            &Reason);           /* Reason code         */

                  if (CompCode != MQCC_OK)
                    WriteToRpt( MQPUTCHGQERR, 1, 2, DefnLQ.QName,
                                CompCode, Reason);
                  else
                  {
                    /***************************************************/
                    /* Mainline section 14:      Get the response msg  */
                    /*                           with Correlation ID   */
                    /***************************************************/
                    GetPCFMsg(hConn,               /* Queue manager    */
                              hQLReplyQ,           /* Reply Q          */
                              &pAdminMsg,          /* Response message */
                              &AdminMsgLen,        /* buffer length    */
                              ChgQRequestMsgId,    /* Response correlId*/
                              PCFwaitInterval,     /* MQGET Wait time  */
                              &CompCode,           /* Completion code  */
                              &Reason);            /* Reason code      */

                    pPCFHeader  = (MQCFH *)pAdminMsg;
                    /***************************************************/
                    /* Mainline section 15:      Check PCF return and  */
                    /*                           reason code.          */
                    /* This indicates the success or failure of the    */
                    /* "Change Queue" command. A failure is not good   */
                    /* but not a reason to terminate.                  */
                    /* No further response parsing needed.             */
                    /***************************************************/
                    if (CompCode != MQCC_OK)
                      WriteToRpt( MQGETCHGQERR, 1, 2, DefnLQ.QName,
                                  CompCode, Reason);
                    else if (pPCFHeader->CompCode != MQCC_OK)
                      WriteToRpt( PCFCHGQERR, 1, 2, DefnLQ.QName,
                                  pPCFHeader->CompCode, pPCFHeader->Reason);
                    else
                    {
                      if (currentState == BECOME_ACTIVE)
                      {
                        WriteToRpt( CHGQI, 2, 2, "ACTIVE", DefnLQ.QName,
                                    0, 0);
                        currentState = ACTIVE;
                      }
                      else if (currentState == BECOME_INACTIVE)
                      {
                        WriteToRpt( CHGQI, 2, 2, "INACTIVE", DefnLQ.QName,
                                    0, 0);
                        currentState = INACTIVE;
                      }

                      if (TargetCLWLUSEQ != -1)
                      {
                        WriteToRpt( CHGUSEQI, 1, 1, DefnLQ.QName,
                                    TargetCLWLUSEQ );
                      }
                    }
                  } /* end else successfully put PCF msg */
                } /* end if TargetCLWLPRTY or TargetCLWLUSEQ != -1 */

                /*******************************************************/
                /* Mainline section 16:  If the queue is inactive and  */
                /*                       we're configured to           */
                /*                       transfer messages from queues */
                /*                       with no consumers and         */
                /*                       messages are on the queue     */
                /*                       then try to move them to a    */
                /*                       queue with consumers.         */
                /*******************************************************/

                if ( TransferFlag &&
                     (currentState == INACTIVE) &&
                     (DefnLQ.CURDEPTH > 0) )
                {
                  /*---------------------------------------------------*/
                  /* We only want to attempt to transfer messages from */
                  /* this instance of the queue to another if there is */
                  /* currently at least one active instance of the     */
                  /* queue.                                            */
                  /* If we were to try to do this with no other active */
                  /* instance the messages would be pointlessly        */
                  /* redistributed amongst all inactive instance as   */
                  /* they will all have the same CLWLPRTY value.       */
                  /* If any active instances happen to go inactive     */
                  /* during the redistribution then it would be        */
                  /* unfortunate but not a major problem as a once off.*/

                  /*---------------------------------------------------*/
                  /* Cache the queue name for later and the current    */
                  /* depth of the queue.                               */
                  strncpy(TransferQName, DefnLQ.QName, sizeof(DefnLQ.QName));
                  TransferCurDepth = DefnLQ.CURDEPTH;

                  /*****************************************************/
                  /* Mainline section 17:  Check to see if there are   */
                  /*                       any active instances of the */
                  /*                       queue in the cluster        */
                  /*****************************************************/
                  foundAltInstance = checkForAlternativeQueue(pAdminMsg,
                                                              &AdminMsgLen,
                                                              hConn,
                                                              hCommandQ,
                                                              hQLReplyQ,
                                                              &DefnLQ,
                                                              QClusterName);

                  /*****************************************************/
                  /* Mainline sections 18-19: If there is an alternate */
                  /*                          instance, requeue the    */
                  /*                          messages to move them.   */
                  /*****************************************************/
                  if (foundAltInstance)
                  {
                    msgsRequeued += ReQueue(hConn, TransferQName,
                                            TransferCurDepth);
                  }
                  else if(TargetCLWLPRTY != -1)
                  {
                    /* Report that no alt instances found at the */
                    /* time we change the activeness. */
                    WriteToRpt(NOALTINSTD, 1, 1, TransferQName,
                               TransferCurDepth);
                  }

                } /* end if (currentState == INACTIVE)&&(DefnLQ.CURDEPTH > 0)*/

              } /* end else Successfully read a queue detail message */

            } /* end if Successfully read a response message (MQGET = OK) */
            else
              WriteToRpt( MQGETPCFERR, 1, 2, ReplyQ, CompCode, Reason);


            /***********************************************************/
            /* Finished processing the current queue detail message,   */
            /* do the next one.                                        */
            /***********************************************************/

            /* end do while reading inquire queue responses */
          } while ( (lastQResponse == MQCFC_NOT_LAST) && !StopMon);

          /*************************************************************/
          /* Processing of the local queues complete                   */
          /*************************************************************/

        } /* end if successfully put PCF "Inquire Queue" msg */

        /*-------------------------------------------------------------*/
        /* If using the queue list file as the source, read the next   */
        /* one from the file.                                          */
        if ((useQListFile) && !StopMon)
        {
          QNameMask[0] = '\0';
          qmaskrc = readNextQMask(QNameMask,sizeof(QNameMask));
        }
        /* Otherwise, drop out to the polling layer */
        else
          qmaskrc = -1;

      } /* end reading QMasks */

    } /* end if both PCF cmd Q & ReplyQ are open and stop monitor not rec'd */

    /* Collect stats if we are to continue monitoring */
    if (!StopMon && !errorReported && !firstTimeHere)
    {
      /* Note the elapsed time for measurement purposes,               */
      /*   and increment the count of polling intervals thus far       */
      /*   in this measurement interval.                               */
      pollIntCount++;
      /* get current timestamp  */
      endtime = getCurrentMilliseconds();
      /* Elapsed processing time for this polling interval */
      procTime = endtime - starttime;
      totalProcTime += procTime;
      /* Elapsed time since start of measurement interval */
      intElapsedTime = (endtime - intStartTime) / 1000;

      if (DebugFlag || StatsFlag)
      {
        WriteToRpt(STATSMSGI, 1, 4, "", procTime, queuesChecked,
                   activeChanges, msgsRequeued);
      }

      if (procTime < minTime)
        minTime = procTime;
      if (procTime > maxTime)
        maxTime = procTime;

      /* Update the running totals for this measurement interval */
      totalQueuesChecked += queuesChecked;
      queuesChecked = 0;
      totalActiveChanges += activeChanges;
      activeChanges = 0;
      totalMsgsRequeued += msgsRequeued;
      msgsRequeued = 0;

      /* reached end of a measurement interval */
      if ( (pollIntCount == 600) ||
           (intElapsedTime >= MEASUREMENT_INT_MAX) )
      {
        WriteToRpt( MEASUREI, 1, 8, "", pollIntCount, intElapsedTime,
                    totalProcTime, minTime, maxTime,
                    (totalQueuesChecked / pollIntCount),
                    totalActiveChanges, totalMsgsRequeued);

        /* Reset the running totals */
        pollIntCount = 0;
        intStartTime = endtime;
        minTime = LONG_MAX;
        maxTime = 0;
        totalProcTime = 0;
        totalQueuesChecked = 0;
        totalActiveChanges = 0;
        totalMsgsRequeued = 0;
      } /* end if reached end of a measurement interval */
    } /* end if stop monitor not received (end of collect stats) */

    /*******************************************************************/
    /* Mainline section 20:  Sleep for specified interval.             */
    /*                                                                 */
    /* Instead of using actual Sleep routine, issue MQGET with WAIT on */
    /* the PCF reply Q. This serves to:                                */
    /*   a)  pause for the polling interval                            */
    /*   b)  recognize a stop monitor msg                              */
    /*   c)  recognise a get inhibited queue                           */
    /*   d)  recognize immediately of the qmgr is                      */
    /*       stopping or quiescing.                                    */
    /*                                                                 */
    /* NB: If the reply queue is not available to us we will simply    */
    /*     sleep for the poll interval, although this will not         */
    /*     immediately detect a stopping queue manager.                */
    /*                                                                 */
    /*******************************************************************/

    if (!StopMon) /* Sleep only if we are to continue monitoring */
    {
      if (DebugFlag)
      {
        sprintf(dbs,"End of polling interval\n\n");
        WriteToRpt(DEBUGMSGD, 1, 0, dbs);
      }

      /*---------------------------------------------------------------*/
      /* If we have the reply queue open then we will issue an MQGET   */
      /* with a wait for the poll interval. We explicitly try to get a */
      /* message that has the stopmonmqclq correlId. So, we'll either  */
      /* time out of the MQGET with no message, in which case we have  */
      /* completed our sleep and should go back to the start and poll  */
      /* the queue state. If however we successfully get a message     */
      /* with the stopmonmwclq correlId that's an instruction to us to */
      /* terminate the monitoring. The third option is that the MQGET  */
      /* is cut short, due to the queue manager terminating or the     */
      /* queue being get inhibited. In this case we also take it as a  */
      /* sign to stop monitoring.                                      */
      if (hQLReplyQ != MQHO_UNUSABLE_HOBJ)
      {
        GetPCFMsg(hConn,                       /* Queue manager handle */
                  hQLReplyQ,                           /* Queue handle */
                  &pAdminMsg,    /* pointer to buffer for returned msg */
                  &AdminMsgLen,                    /* length of buffer */
                  (MQBYTE *)stopmonmqclq,         /* CorrelId for stop */
                  PollInterval*1000,                  /* Poll interval */
                  &CompCode,
                  &Reason);

        if(CompCode == MQCC_OK)
        {
          WriteToRpt( STOPMONI, 1, 0, "" );
          StopMon = TRUE;
        }
        else if(!StopMon)
        {
          /* We expect no message to be available from the get, any    */
          /* other errors that aren't terminal (i.e. StopMon is TRUE)  */
          /* then that needs reporting. */
          if(Reason != MQRC_NO_MSG_AVAILABLE)
            WriteToRpt( MQGETPCFERR, 1, 2, ReplyQ, CompCode, Reason);
        }
      }
      else
      {
        /* We don't have the reply queue open so we instead have to     */
        /* sleep for the specified interval (converted to milliseconds) */
#if (MQAT_DEFAULT == MQAT_WINDOWS_NT)
        Sleep(PollInterval*1000);
#else
        sleep(PollInterval);
#endif
      }
    } /* end if stop monitor not rec'd - end of Sleep section */
  } /* End loop while stop monitor not rec'd  */

  /*********************************************************************/
  /* Mainline section 21:  Reached end of the Qmgr-available loop.     */
  /*********************************************************************/

MOD_EXIT:

  /* Release the memory for the PCF message buffer */
  if(pAdminMsg)
    free( pAdminMsg );

  /*********************************************************************/
  /* Mainline section 22:  Close the Reply queue.                      */
  /*********************************************************************/
  if (hQLReplyQ != MQHO_UNUSABLE_HOBJ)
  {
    CloseOpts = MQCO_NONE;

    MQCLOSE(hConn,
            &hQLReplyQ,
            CloseOpts,
            &CompCode,
            &Reason);
    if ( CompCode != MQCC_OK )
    {
      WriteToRpt( MQCLOSEERR, 1, 2, ReplyQ, CompCode, Reason);
    }
    else if (DebugFlag)
    {
      sprintf(dbs,"Successfully closed queue %s\n",ReplyQ);
      WriteToRpt(DEBUGMSGD, 1, 0, dbs);
    }
  } /* end if ReplyQ is open (needs closing) */

  /*********************************************************************/
  /* Mainline section 23:  Close the PCF command queue.                */
  /*********************************************************************/
  if (hCommandQ != MQHO_UNUSABLE_HOBJ)
  {
    CloseOpts = MQCO_NONE;

    MQCLOSE(hConn,
            &hCommandQ,
            CloseOpts,
            &CompCode,
            &Reason);
    if ( CompCode != MQCC_OK )
    {
      WriteToRpt( MQCLOSEERR, 1, 2, "SYSTEM.ADMIN.COMMAND.QUEUE",
                  CompCode, Reason);
    }
    else if (DebugFlag)
    {
      sprintf(dbs,"Successfully closed PCF command queue.\n");
      WriteToRpt(DEBUGMSGD, 1, 0, dbs);
    }
  } /* end if PCF cmd Q is open (needs closing) */

  /* Close the hObj for inquiring the queue manager properties */
  if (hQMgrObj != MQHO_UNUSABLE_HOBJ)
  {
    CloseOpts = MQCO_NONE;

    MQCLOSE(hConn,
            &hQMgrObj,
            CloseOpts,
            &CompCode,
            &Reason);
    if ( CompCode != MQCC_OK )
    {
      WriteToRpt( MQCLOSEERR, 1, 2, "QUEUE MANAGER",
                  CompCode, Reason);
    }
    else if (DebugFlag)
    {
      sprintf(dbs,"Successfully closed queue manager hObj.\n");
      WriteToRpt(DEBUGMSGD, 1, 0, dbs);
    }
  } /* end if PCF cmd Q is open (needs closing) */


  /*********************************************************************/
  /* Mainline section 24:  Disconnect from the queue manager.          */
  /*********************************************************************/
  if(hConn != MQHC_UNUSABLE_HCONN)
  {
    MQDISC(&hConn,
           &CompCode,
           &Reason);

    if ( CompCode != MQCC_OK )
    {
      WriteToRpt( MQDISCERR, 1, 2, QMgrName, CompCode, Reason );
    }
    else if (DebugFlag)
    {
      sprintf(dbs,"Disconnected from queue manager %s\n",QMgrName);
      WriteToRpt(DEBUGMSGD, 1, 0, dbs);
    }
  }

  /* Close the queue list inputfile (if applicable) */
  if (useQListFile && pQListFile != NULL)
  {
    fclose(pQListFile);
    if (DebugFlag)
    {
      sprintf(dbs,"Closed queue list file.\n");
      WriteToRpt(DEBUGMSGD, 1, 0, dbs);
    }
  }

  /* Record that the monitor has ended.  */
  WriteToRpt( TERMINATEMSGI, 1, 0, "" );

  /* Close the report file */
  if(pReport != NULL)
    fclose(pReport);

} /* end of main */
/***********************************************************************/
/*     End of Mainline                                                 */
/***********************************************************************/

/***********************************************************************/
/*                                                                     */
/*                          Subroutines                                */
/*                                                                     */
/***********************************************************************/

/***********************************************************************/
/* Subroutine:  ProcessStringParm                                      */
/*                                                                     */
/* Parses a single string parameter from a PCF response.               */
/* Saves the string value in the DefnLQ structure if it is one of the  */
/* following:                                                          */
/*     QName            = queue name returned                          */
/*     QClusterName     = cluster name returned                        */
/*     QClusterQmgrName = owning queue manager name                    */
/*                                                                     */
/***********************************************************************/
void ProcessStringParm(MQCFST      *pPCFString,
                       LocalQParms *DefnLQ)
{
  switch ( pPCFString->Parameter )
  {
    case MQCA_Q_NAME:
      MQParmCpy( DefnLQ->QName, pPCFString->String, MQ_Q_NAME_LENGTH+1,
                 MQ_Q_NAME_LENGTH );
      if (DebugFlag)
      {
        sprintf(dbs,"PCF response - QName=%s\n",DefnLQ->QName);
        WriteToRpt(DEBUGMSGD, 1, 0, dbs);
      }
      break;
    case MQCA_CLUSTER_NAME:
      MQParmCpy( DefnLQ->QClusterName, pPCFString->String,
                 MQ_CLUSTER_NAME_LENGTH+1, MQ_CLUSTER_NAME_LENGTH );
      if (DebugFlag)
      {
        sprintf(dbs,"PCF response - QClusterName=%s\n",
                DefnLQ->QClusterName);
        WriteToRpt(DEBUGMSGD, 1, 0, dbs);
      }
      break;
    case MQCA_CLUSTER_Q_MGR_NAME:
      MQParmCpy( DefnLQ->QClusterQmgrName, pPCFString->String,
                 MQ_Q_MGR_NAME_LENGTH+1, MQ_Q_MGR_NAME_LENGTH );
      if (DebugFlag)
      {
        sprintf(dbs,"PCF response - QClusterQmgrName=%s\n",
                DefnLQ->QClusterQmgrName);
        WriteToRpt(DEBUGMSGD, 1, 0, dbs);
      }
      break;
  } /* endswitch */
}

/***********************************************************************/
/* Subroutine:  ProcessIntegerParm                                     */
/*                                                                     */
/* Parses a single integer parameter from a PCF response.              */
/* Saves the value in the DefnLQ structure if it is one of the         */
/* following:                                                          */
/*     IPPROCS         = IPPROCS value returned                        */
/*     CLWLPRTY        = CLWLPRTY value returned                       */
/*     CLWLUSEQ        = CLWLUSEQ value returned                       */
/*     CURDEPTH        = CURDEPTH value returned                       */
/*                                                                     */
/***********************************************************************/
void ProcessIntegerParm(MQCFIN      *pPCFInteger,
                        LocalQParms *DefnLQ)
{
  switch ( pPCFInteger->Parameter )
  {
    case MQIA_OPEN_INPUT_COUNT :
      DefnLQ->IPPROCS = pPCFInteger->Value;
      if (DebugFlag)
      {
        sprintf(dbs,"PCF response - IPPROCS=%d\n", DefnLQ->IPPROCS);
        WriteToRpt(DEBUGMSGD, 1, 0, dbs);
      }
      break;
    case MQIA_CLWL_Q_PRIORITY :
      DefnLQ->CLWLPRTY = pPCFInteger->Value;
      if (DebugFlag)
      {
        sprintf(dbs,"PCF response - CLWLPRTY=%d\n",DefnLQ->CLWLPRTY);
        WriteToRpt(DEBUGMSGD, 1, 0, dbs);
      }
      break;
    case MQIA_CLWL_USEQ :
      DefnLQ->CLWLUSEQ = pPCFInteger->Value;
      if (DebugFlag)
      {
        sprintf(dbs,"PCF response - CLWLUSEQ=%d\n",DefnLQ->CLWLUSEQ);
        WriteToRpt(DEBUGMSGD, 1, 0, dbs);
      }
      break;
    case MQIA_CURRENT_Q_DEPTH:
      DefnLQ->CURDEPTH = pPCFInteger->Value;
      if (DebugFlag)
      {
        sprintf(dbs,"PCF response - CURDEPTH=%d\n",DefnLQ->CURDEPTH);
        WriteToRpt(DEBUGMSGD, 1, 0, dbs);
      }
  } /* endswitch */
}

/***********************************************************************/
/*                                                                     */
/* Subroutine:  checkState                                             */
/*                                                                     */
/* This routine determines the current state of a queue.               */
/* A queue's current state is determined by its current cluster        */
/* priority and if anyone has opened the queue for input (getting).    */
/*                                                                     */
/* The queue attributes are taken from the DefnLQ structure passed     */
/* into this routine.                                                  */
/*                                                                     */
/*                                     Current                         */
/* Condition                           State set to                    */
/* -----------------------------       -------                         */
/* IPPROCS > 0 and CLWLPRTY > 0        ACTIVE                          */
/* IPPROCS = 0 and CLWLPRTY = 0        INACTIVE                        */
/* IPPROCS > 0 and CLWLPRTY = 0        BECOME_ACTIVE                   */
/* IPPROCS = 0 and CLWLPRTY > 0        BECOME_INACTIVE                 */
/*                                                                     */
/* This routine returns currentState via the pointer passed in as an   */
/* argument.                                                           */
/*                                                                     */
/***********************************************************************/
void checkState(LocalQParms     DefnLQ,
                unsigned short *pcurrentState)
{
  unsigned short currentState;

  currentState = ACTIVE;

  /*-------------------------------------------------------------------*/
  /* The queue is currently active (CLWLPRTY > 0) but there are not    */
  /* attached consumers, make the queue inactive...                    */
  if (DefnLQ.CLWLPRTY > 0 && DefnLQ.IPPROCS == 0)
  {
    currentState = BECOME_INACTIVE;
    if (DebugFlag)
    {
      sprintf(dbs,"Queue %s is ACTIVE but has no consumers. " \
                  " Change it to INACTIVE.\n  CURDEPTH=%d\n",
              DefnLQ.QName, DefnLQ.CURDEPTH);
      WriteToRpt(DEBUGMSGD, 1, 0, dbs);
    }
  }
  /*-------------------------------------------------------------------*/
  /* The queue is currently inactive (CLWLPRTY = 0) but there are      */
  /* attached consumers, make the queue active...                      */
  else if (DefnLQ.CLWLPRTY == 0 && DefnLQ.IPPROCS > 0)
  {
    currentState = BECOME_ACTIVE;
    if (DebugFlag)
    {
      sprintf(dbs,"Queue %s is INACTIVE but it has consumers. " \
                  "Activate it.\n", DefnLQ.QName);
      WriteToRpt(DEBUGMSGD, 1, 0, dbs);
    }
  }
  /*-------------------------------------------------------------------*/
  /* The queue is currently inactive (CLWLPRTY = 0) and there are no   */
  /* attached consumers, leave the queue as inactive...                */
  else if (DefnLQ.CLWLPRTY == 0 && DefnLQ.IPPROCS == 0)
  {
    currentState = INACTIVE;
    if (DebugFlag)
    {
      sprintf(dbs,"Queue %s is INACTIVE and will remain so.\n",
              DefnLQ.QName);
      WriteToRpt(DEBUGMSGD, 1, 0, dbs);
    }
  }
  /*-------------------------------------------------------------------*/
  /* The queue is currently active (CLWLPRTY > 0) and there are still  */
  /* attached consumers, leave the queue as active...                  */
  else if (DefnLQ.CLWLPRTY > 0 && DefnLQ.IPPROCS > 0)
  {
    currentState = ACTIVE;
    if (DebugFlag)
    {
      sprintf(dbs,"Queue %s is ACTIVE and will remain so.\n",
              DefnLQ.QName);
      WriteToRpt(DEBUGMSGD, 1, 0, dbs);
    }
  }

  /* Return the value for currentState via the pointer passed in */
  *pcurrentState = currentState;
}

/***********************************************************************/
/*                                                                     */
/* Subroutine:  MQParmCpy                                              */
/*                                                                     */
/* The queue manager returns strings of the maximum length for each    */
/* specific parameter, padded with blanks.                             */
/*                                                                     */
/* We are interested in only the nonblank characters so will extract   */
/* them from the message buffer, and terminate the string with a null. */
/*                                                                     */
/***********************************************************************/
void MQParmCpy(char *target,
               char *source,
               int   tlength,
               int   slength)
{
  int   counter=0;
  int   length;

  /* length = the lesser of the source's and target's length */
  length = tlength;
  if (slength < length) length = slength;

  while ( counter < length && source[counter] != ' ' )
  {
    target[counter] = source[counter];
    counter++;
  } /* endwhile */

  if ( counter < tlength)
    target[counter] = '\0';

}

/***********************************************************************/
/*                                                                     */
/* Function:  OpenQ                                                    */
/*                                                                     */
/* Opens the queue named by argument QName using the provided          */
/* connection handle and open options.                                 */
/*                                                                     */
/* Returns the object handle to the caller.                            */
/*                                                                     */
/***********************************************************************/
MQHOBJ OpenQ(MQHCONN  hConn,
             char     QName[MQ_Q_NAME_LENGTH+1],
             MQLONG   OpenOpts,
             MQLONG  *pCompCode,
             MQLONG  *pReason)
{
  MQHOBJ Hobj = MQHO_UNUSABLE_HOBJ;


  MQOD ObjDesc = {MQOD_DEFAULT};
  ObjDesc.ObjectType = MQOT_Q;
  strncpy(ObjDesc.ObjectName, QName, sizeof(ObjDesc.ObjectName));

  MQOPEN(hConn,             /* connection handle                       */
         &ObjDesc,          /* object descriptor for queue             */
         OpenOpts,          /* open options                            */
         &Hobj,             /* object handle                           */
         pCompCode,         /* MQOPEN completion code                  */
         pReason);          /* reason code                             */

  return Hobj;
}

/***********************************************************************/
/*                                                                     */
/* Subroutine:  PutPCFMsg                                              */
/*                                                                     */
/* Puts a message to the queue represented by the handle object that   */
/* is passed in as an argument.                                        */
/*                                                                     */
/* Returns the MsgID via a pointer argument so that it can be used as  */
/* the correlId of any response message.                               */
/*                                                                     */
/* This routine is used to write PCF command messages only.            */
/*                                                                     */
/***********************************************************************/
void PutPCFMsg(MQHCONN   hConn,
               MQHOBJ    hQName,
               char      ReplyQName[MQ_Q_NAME_LENGTH+1],
               MQBYTE   *UserMsg,
               MQLONG    UserMsgLen,
               MQBYTE24 *RequestMsgId,
               MQLONG   *pCompCode,
               MQLONG   *pReason)
{
  MQPMO pmo     = { MQPMO_DEFAULT};     /* Default put message options */
  MQMD  md      = { MQMD_DEFAULT};      /* Default message descriptor  */

  /*-------------------------------------------------------------------*/
  /* Set the non-default message descriptor fields                     */

  /* A reply is always requested */
  md.MsgType        = MQMT_REQUEST;
  /* Expire the request if it is not processed promptly */
  md.Expiry         = PCFexpiry;
  /* Requests need not be persistent */
  md.Persistence    = MQPER_NOT_PERSISTENT;
  /* The response message should continue the expiry from the request. */
  /* This prevents replies being left on the reply queue if this tool  */
  /* is terminated. */
  md.Report         = MQRO_PASS_DISCARD_AND_EXPIRY;
  /* This is a PCF message, which is a MQFMT_ADMIN format message */
  memcpy(md.Format,   MQFMT_ADMIN, sizeof(md.Format) );
  /* THe queue for the reply to be sent to */
  memcpy(md.ReplyToQ, ReplyQName, MQ_Q_NAME_LENGTH );

  /* If the queue manager is quiescing, fail the put */
  pmo.Options    = MQPMO_FAIL_IF_QUIESCING;

  MQPUT( hConn,                        /* connection handle            */
         hQName,                       /* queue handle                 */
         &md,                          /* message descriptor           */
         &pmo,                         /* put options                  */
         UserMsgLen,                   /* message length               */
         (MQBYTE *)UserMsg,            /* message buffer               */
         pCompCode,                    /* completion code              */
         pReason);                     /* reason code                  */

  if (*pReason != MQRC_NONE)
  {
    WriteToRpt( MQPUTPCFERR, 1, 2,  "SYSTEM.ADMIN.COMMAND.QUEUE",
                *pCompCode, *pReason);
    CheckReason(*pReason);
  }
  /* Return the msgid from the request. */
  else
  {
    memcpy(RequestMsgId,md.MsgId,sizeof(md.MsgId));
    if (DebugFlag)
    {
      sprintf(dbs,"Successfully put a PCF command msg\n");
      WriteToRpt(DEBUGMSGD, 1, 0, dbs);
    }
  }

}

/***********************************************************************/
/*                                                                     */
/* Subroutine:  GetPCFMsg                                              */
/*                                                                     */
/* Gets a message from the queue represented by the handle             */
/* passed in as an argument.  Uses the CorrelationID passed in         */
/* to filter the message received by MQGET.                            */
/*                                                                     */
/* The message data is received into the buffer pointed to             */
/* by UserMsg of length ReadBufferLen.                                 */
/*                                                                     */
/* If the buffer is not big enough to contain the message the          */
/* buffer is increased.                                                */
/*                                                                     */
/* This routine is used to read PCF response messages only.            */
/* The success or failure of the MQGET is reflected in the MQ API      */
/* CompCode & Reason global variables upon exit from this routine.     */
/*                                                                     */
/***********************************************************************/
void GetPCFMsg(MQHCONN   hConn,
               MQHOBJ    hQName,
               MQBYTE  **ppPCFMsg,
               MQLONG   *pReadBufferLen,
               MQBYTE24  CorrelId,
               unsigned  waitTime,
               MQLONG   *pCompCode,
               MQLONG   *pReason)
{
  MQLONG  msglen;
  MQGMO   gmo     = { MQGMO_DEFAULT};
  MQMD    md      = { MQMD_DEFAULT};

  gmo.Version      = MQGMO_VERSION_2;
  gmo.MatchOptions = MQMO_MATCH_CORREL_ID;
  gmo.Options      = gmo.Options | MQGMO_WAIT | MQGMO_FAIL_IF_QUIESCING;
  gmo.WaitInterval = waitTime;

  /* Set CorrelId to read only this application's response messages. */
  memcpy(md.CorrelId, CorrelId, sizeof(md.CorrelId) );

  MQGET( hConn,                    /* connection handle                */
         hQName,                   /* object handle                    */
         &md,                      /* message descriptor               */
         &gmo,                     /* get message options              */
         *pReadBufferLen,          /* Buffer length                    */
         (MQBYTE *)(*ppPCFMsg),    /* message buffer                   */
         &msglen,                  /* message length returned (ignored)*/
         pCompCode,                /* completion code                  */
         pReason);                 /* reason code                      */

  /*-------------------------------------------------------------------*/
  /* If the MQGET fails because the supplied message buffer is not big */
  /* enough to contain the message we create a larger buffer and retry */
  /* the MQGET. This new buffer is then used from now on by the tool.  */
  while(*pReason == MQRC_TRUNCATED_MSG_FAILED)
  {
    free( *ppPCFMsg );           /* Release the existing message buffer*/
    *pReadBufferLen = msglen;    /* Remember the new size              */
    *ppPCFMsg  = (MQBYTE *)malloc( *pReadBufferLen ); /* Create new buffer */

    MQGET(hConn,                   /* connection handle                */
          hQName,                  /* object handle                    */
          &md,                     /* message descriptor               */
          &gmo,                    /* get message options              */
          *pReadBufferLen,         /* Buffer length                    */
          (MQBYTE *)(*ppPCFMsg),   /* message buffer                   */
          &msglen,                 /* message length returned (ignored)*/
          pCompCode,               /* completion code                  */
          pReason);                /* reason code                      */
  }

  if (*pCompCode != MQCC_OK)
    CheckReason(*pReason);
  else if (DebugFlag)
  {
    sprintf(dbs,"Successfully got a PCF response msg\n");
    WriteToRpt(DEBUGMSGD, 1, 0, dbs);
  }

}

/***********************************************************************/
/*                                                                     */
/* Subroutine:  ReQueue                                                */
/*                                                                     */
/* Requeues a specified number of messages from the named queue        */
/* saving & passing all context from the original message.             */
/*                                                                     */
/* Get the queue name and # of messages from the QInfo argument passed */
/* in.  Use the connection handle passed in.                           */
/*                                                                     */
/* Open the queue for input and output under separate handles.         */
/* This is to facilitate saving and passing context.                   */
/* Get and put messages under syncpoint.  Commit once per batch, and   */
/* again when all messages have been requeued.                         */
/* Close the queue.                                                    */
/*                                                                     */
/* Open the queue for input shared so as to allow a consumer to resume.*/
/* Open the queue for output with BIND_NOT_FIXED so that the cluster   */
/* workload algorithm routes each message.                             */
/* The cluster workload algorithm will select a different              */
/* instance of the queue for output assuming CLWLUSEQ is set           */
/* to ANY, and CLWLPRTY is 0 for this local instance,                  */
/* and CLWLPRTY > 0 for another instance on another qmgr.              */
/*                                                                     */
/* Memory is allocated for the buffer to receive the message.          */
/* If the buffer is not large enough, free it and obtain a new buffer  */
/* that is large enough.                                               */
/*                                                                     */
/* Only the specified # of messages are requeued.  If fewer than that  */
/* are available, that is OK; not considered an error.                 */
/* When done requeuing the specified number of messages, or when there */
/* are no more messages (whichever comes first), close the queues and  */
/* free the buffer area.                                               */
/*                                                                     */
/* Do not indicate success or failure to the caller.                   */
/* Report problems found in the error log.                             */
/*                                                                     */
/***********************************************************************/
long ReQueue (MQHCONN hConn,
              char    *QName,
              MQLONG   CurDepth)
{

  MQOD    ObjDesc = { MQOD_DEFAULT};        /* queue object descriptor */
  MQLONG  OpenOptsI;                         /* Open options for input */
  MQLONG  OpenOptsO;                        /* Open options for output */
  MQHOBJ  HobjI = MQHO_UNUSABLE_HOBJ;        /* queue handle for input */
  MQHOBJ  HobjO = MQHO_UNUSABLE_HOBJ;      /* queue handles for output */
  MQLONG  CloseOpts = MQCO_NONE;                    /* MQCLOSE options */
  MQLONG  CompCode;                  /* Completion code from MQI calls */
  MQLONG  Reason;                        /* Reason code from MQI calls */
  MQLONG  msglen;                  /* Length of message being requeued */
  MQBYTE *pUserMsg;         /* Pointer to buffer that receives the msg */
  MQLONG  usermsgsize = INITIAL_MSG_SIZE; /* Initial size of buffer to */
                        /* hold user msg during ReQueue, increased as  */
                        /* needed within ReQueue routine.              */

  MQGMO   gmo = { MQGMO_DEFAULT};               /* Get message options */
  MQMD    md = { MQMD_DEFAULT};                  /* Message descriptor */
  MQPMO   pmo = { MQPMO_DEFAULT};               /* Put message options */
  char    resolvedQMgrName[MQ_Q_MGR_NAME_LENGTH+1]; /* Null-terminated */
                                        /* qmgr name resolved on MQPUT */
  int     i;                      /* Number of queued messages to move */
  int     msgcount = 0;            /* Number of message moved in batch */
  int     haltedReq = FALSE;            /* Terminate the requeue early */
  long    totalMsgsRequeued = 0;

  if (DebugFlag)
  {
    sprintf(dbs,"Start requeue process for %s on qmgr %s, " \
                "message count=%d\n",
            QName,QMgrName,CurDepth);
    WriteToRpt(DEBUGMSGD, 1, 0, dbs);
  }

  OpenOptsI = MQOO_INPUT_SHARED                /* open queue for input */
              | MQOO_SAVE_ALL_CONTEXT       /* save context from input */
              | MQOO_FAIL_IF_QUIESCING;     /* but not if MQM stopping */
  OpenOptsO = MQOO_OUTPUT                     /* open queue for output */
              | MQOO_BIND_NOT_FIXED   /* route each message separately */
              | MQOO_PASS_ALL_CONTEXT    /* preserve context on output */
              | MQOO_FAIL_IF_QUIESCING;     /* but not if MQM stopping */

  /* Set the OD for the queue provided */
  ObjDesc.ObjectType = MQOT_Q;
  strncpy(ObjDesc.ObjectName, QName, sizeof(ObjDesc.ObjectName));

  /* Open queue for input (to get the queued messages) */
  MQOPEN(hConn,                                   /* connection handle */
         &ObjDesc,                      /* object descriptor for queue */
         OpenOptsI,                            /* open options (input) */
         &HobjI,                              /* object handle (input) */
         &CompCode,
         &Reason);

  if ( CompCode != MQCC_OK )
  {
    WriteToRpt( MQOPENREQIERR, 1, 2, QName, CompCode, Reason);
    CheckReason(Reason);
    goto MOD_EXIT;
  }

  /* Open queue for output (to re-put the queued messages */
  MQOPEN(hConn,                                   /* connection handle */
         &ObjDesc,                      /* object descriptor for queue */
         OpenOptsO,                           /* open options (output) */
         &HobjO,                             /* object handle (output) */
         &CompCode,
         &Reason);

  if ( CompCode != MQCC_OK )
  {
    WriteToRpt( MQOPENREQOERR, 1, 2, QName, CompCode, Reason);
    CheckReason(Reason);
    goto MOD_EXIT;
  }

  /*-------------------------------------------------------------------*/
  /* To move each queued message on the inactive local instance of     */
  /* the queue we simple get the message from the local queue and      */
  /* put it back to the same named queue. Because the queue is         */
  /* clustered and we believe there are one or more active instances   */
  /* of the queue elsewhere in the cluster (with a CLWLPRTY greater    */
  /* than zero) MQ's cluster workload balancing will automatically     */
  /* send the re-queued message to one of those instance of the queue, */
  /* not the less preferable local instance of the queue.              */
  /* This is dependant on the local queue having its CLWLUSEQ          */
  /* attribute set to ANY, otherwise the local instance will still be  */
  /* preferable even if it has a lower CLWLPRTY value than others.     */

  /* Obtain initial storage for the buffer to receive the message */
  pUserMsg  = (MQBYTE *)malloc( usermsgsize );

  /*-------------------------------------------------------------------*/
  /* The get and re-put of each message is performed under a unit of   */
  /* work to ensure that no messages are lost in the event of a        */
  /* failure.                                                          */
  /* For performance reasons, multiple get/put operations are          */
  /* batched together.                                                 */
  /* All message properties are added to the message to ensure they    */
  /* are preserved when re-putting it.                                 */
  gmo.Options = MQGMO_FAIL_IF_QUIESCING
                | MQGMO_SYNCPOINT
                | MQGMO_PROPERTIES_FORCE_MQRFH2
                | MQGMO_NO_WAIT;

  /* Get the next message, not a specific one */
  gmo.Version = MQGMO_VERSION_2;
  gmo.MatchOptions = MQMO_NONE;

  /*-------------------------------------------------------------------*/
  /* We only try to requeue the number of messages that were on the    */
  /* queue at the time that we detected that it was inactive. This     */
  /* protects against requeued messages being put back onto the local  */
  /* queue (for example the remote active queues become inactive while */
  /* we are requeuing) and the requeue processing continuing           */
  /* indefinitely.                                                     */
  /* If more messages have been queued since obtaining the curdepth of */
  /* the queue they will not be requeued this time, instead they will  */
  /* be requeued after the next poll interval, just as any other       */
  /* messages still being delivered to this queue will (e.g. messages  */
  /* still arriving due to the inactive state not yet being            */
  /* propagated across the cluster).                                   */
  for (i=1; i <= CurDepth; i++)
  {
    /* If the queue manager is terminating or an error has occurred,   */
    /* exit the requeue loop. */
    if (StopMon)
       break;

    /* Get the next message from the local queue */
    MQGET(hConn,
          HobjI,
          &md,
          &gmo,
          usermsgsize,
          (MQBYTE *)pUserMsg,
          &msglen,
          &CompCode,
          &Reason);

    /* If the message buffer was too small, increase it and try again. */
    while(Reason == MQRC_TRUNCATED_MSG_FAILED)
    {
      free( pUserMsg );
      usermsgsize = msglen;
      pUserMsg  = (MQBYTE *)malloc( usermsgsize );

      MQGET(hConn,
            HobjI,
            &md,
            &gmo,
            usermsgsize,
            (MQBYTE *)pUserMsg,
            &msglen,
            &CompCode,
            &Reason);
    }

    /* If there was no message on the queue we have re-queued all      */
    /* messages, so exit the loop. */
    if (Reason == MQRC_NO_MSG_AVAILABLE)
      break;

    /* If any other problem occurred, roll back the current batch and  */
    /* exit the loop. */
    if (Reason != MQRC_NONE)
    {
      WriteToRpt( MQGETREQERR, 1, 2, QName, CompCode, Reason);
      CheckReason(Reason);

      /* If we have lost our connection to the queue manager the is no */
      /* point in rolling back (this will be implicit by the queue     */
      /* manager). */
      if(!StopMon || (Reason == MQRC_Q_MGR_QUIESCING))
      {
        MQBACK(hConn,
               &CompCode,
               &Reason);
        if ( CompCode != MQCC_OK )
          WriteToRpt( MQBACKREQERR, 1, 2, QName, CompCode, Reason);
      }
      break; /* Exit the loop.  Stop requeuing this queue. */
    }
    /* If we've successfully got a message, re-queue it. */
    else
    {
      if (DebugFlag)
      {
        sprintf(dbs,"Requeue got a msg\n");
        WriteToRpt(DEBUGMSGD, 1, 0, dbs);
      }

      /* Under the same unit of work as the get, re-put the message.   */
      /* We pass the context of the original message to the re-put     */
      /* message to ensure it remains as it was originally put. */
      pmo.Options = MQPMO_FAIL_IF_QUIESCING
                    | MQPMO_PASS_ALL_CONTEXT
                    | MQPMO_SYNCPOINT;
      pmo.Context = HobjI;   /* required in order to pass context */

      MQPUT(hConn,
            HobjO,
            &md,
            &pmo,
            msglen,
            (MQBYTE *)pUserMsg,
            &CompCode,
            &Reason);

      if (DebugFlag && Reason == MQRC_NONE)
      {
        sprintf(dbs,"Within ReQueue of queue %s, MQPUT " \
                "ResolvedQMgrName=%48s.\n",
                QName, pmo.ResolvedQMgrName);
        WriteToRpt(DEBUGMSGD, 1, 0, dbs);
      }

      /* If the MQPUT failed we roll back the unit of work to          */
      /* reinstate the got message back on the local queue. */
      if (Reason != MQRC_NONE)
      {
        WriteToRpt( MQPUTREQERR, 1, 2, QName, CompCode, Reason);
        CheckReason(Reason);

        /* If we have lost our connection to the queue manager there   */
        /* is no point in rolling back (this will be implicit by the   */
        /* queue manager). */
        if(!StopMon || (Reason == MQRC_Q_MGR_QUIESCING))
        {
          MQBACK(hConn,
                 &CompCode,
                 &Reason);
          if ( CompCode != MQCC_OK )
          {
            WriteToRpt( MQBACKREQERR, 1, 2, QName, CompCode, Reason);
          }
        }
        break; /* Exit the loop.  Stop requeuing this queue. */
      }
      /* The MQPUT succeeded. */
      else
      {
        /*-------------------------------------------------------------*/
        /* Check if the resolved qmgr name is actually this qmgr.      */
        /* If it is it means that MQ has decided that the local        */
        /* instance of the queue is just as preferable as any other    */
        /* instance. This may be because those instances have just     */
        /* become inactive or possibly their queue manager(s) are      */
        /* no-longer accessible. Either way, there is no point in      */
        /* continuing the requeue logic as we will simply put the      */
        /* messages back on the same local queue.                      */
        MQParmCpy( resolvedQMgrName,
                   pmo.ResolvedQMgrName,
                   MQ_Q_MGR_NAME_LENGTH+1,
                   MQ_Q_MGR_NAME_LENGTH );

        if (strncmp(resolvedQMgrName,QMgrName,MQ_Q_MGR_NAME_LENGTH) == 0)
        {
          /* Roll back the batch to save putting the message back on   */
          /* the same queue, but at the bottom. */
          MQBACK(hConn,
                 &CompCode,
                 &Reason);
          if ( CompCode != MQCC_OK )
            WriteToRpt( MQBACKREQERR, 1, 2, QName, CompCode, Reason);

          /* Indicate requeue was halted but not due to an error */
          haltedReq = TRUE;

          WriteToRpt(REQNOALTW, 1, 0, QName);  /* Report this situation */
          if (DebugFlag)
          {
            sprintf(dbs,"ReQueue halted for queue %s to prevent " \
                        "requeue back to same queue.\n",
                    QName);
            WriteToRpt(DEBUGMSGD, 1, 0, dbs);
            sprintf(dbs,"Current requeue batch backed out at msg " \
                        "number %d.\n",
                    msgcount+1);
            WriteToRpt(DEBUGMSGD, 1, 0, dbs);
          }

          /* Exit the loop.  Stop requeuing this queue. */
          break;
        } /* end if resolved name would result in putting back to same queue */

        /* increment msg moved count on successful put */
        else
        {
          totalMsgsRequeued++;
          msgcount++;
        }
      } /* end else MQPUT succeeded */
    } /* end else Got a message to requeue */

    /* We batch every REQUEUE_BATCH messages together for performance  */
    /* If we've processed that many messages, commit the current unit  */
    /* of work. */
    if ( (i % REQUEUE_BATCH == 0) && !StopMon )
    {
      if (DebugFlag)
      {
        sprintf(dbs,"Within ReQueue, commit a batch of 50 " \
                    "messages for queue %s\n",
                QName);
        WriteToRpt(DEBUGMSGD, 1, 0, dbs);
      }

      MQCMIT(hConn,
             &CompCode,
             &Reason);
      if ( CompCode != MQCC_OK )
      {
        WriteToRpt( MQCMITREQERR, 1, 2, QName, CompCode, Reason);
        CheckReason(Reason);

        break;  /* Exit the loop.  Stop requeuing this queue. */
      }
    } /* end if this is end of a batch */

  } /* end for main message loop */

  /*-------------------------------------------------------------------*/
  /* Commit the last batch, but only if we didn't break out in the     */
  /* middle of the loop because qmgr is unavailable (a quiescing       */
  /* queue manager is still available so we can commit in that case).  */
  if(!StopMon || (Reason == MQRC_Q_MGR_QUIESCING))
  {
    if (DebugFlag)
    {
      sprintf(dbs,"Within ReQueue, commit the final batch of 50 " \
                  "messages for queue %s\n",
              QName);
      WriteToRpt(DEBUGMSGD, 1, 0, dbs);
    }

    MQCMIT(hConn,
           &CompCode,
           &Reason);
    if ( CompCode != MQCC_OK )
    {
      WriteToRpt( MQCMITREQERR, 1, 2, QName, CompCode, Reason);
      CheckReason(Reason);

      if(!StopMon)
      {
        MQBACK(hConn,
               &CompCode,
               &Reason);
        if ( CompCode != MQCC_OK )
          WriteToRpt( MQBACKREQERR, 1, 2, QName, CompCode, Reason);
      }
    }
    else /* The commit was successful */
    {
      /* Report if any messages were requeued */
      if (!haltedReq && msgcount > 0)
        WriteToRpt(REQI, 1, 1, QName, msgcount);
      else if (haltedReq && msgcount > 0)
      {
        /* Requeue was halted due to unavailability of alternate       */
        /* instance. Actual # msgs requeued is the # up thru the last  */
        /* batch committed. */
        if (msgcount > 50)
          msgcount = (msgcount / REQUEUE_BATCH) * REQUEUE_BATCH;
        /* No batch was committed; No messages actually req'd. */
        else if (msgcount < 50)
          msgcount = 0;
        if (msgcount > 0)
          WriteToRpt(REQI, 1, 1, QName, msgcount);
      } /* end else if halted requeue & msgcount > 0 */
    }
  } /* end if !StopMon */

MOD_EXIT:

  /* Free storage used for message buffer */
  if(pUserMsg)
        free( pUserMsg );

  /* Close the queue for input */
  if(HobjI != MQHO_UNUSABLE_HOBJ)
  {
    MQCLOSE(hConn,
            &HobjI,
            CloseOpts,
            &CompCode,
            &Reason);
    if ( CompCode != MQCC_OK )
      WriteToRpt( MQCLOSEREQIERR, 1, 2, QName, CompCode, Reason);
  }

  /* Close the queue for output */
  if(HobjO != MQHO_UNUSABLE_HOBJ)
  {
    MQCLOSE(hConn,
            &HobjO,
            CloseOpts,
            &CompCode,
            &Reason);
    if ( CompCode != MQCC_OK )
      WriteToRpt( MQCLOSEREQOERR, 1, 2, QName, CompCode, Reason);
  }

  return totalMsgsRequeued;
}

/***********************************************************************/
/*                                                                     */
/* Function:  readNextQMask                                            */
/*                                                                     */
/* Reads the next line in the QList input file and returns it to the   */
/* caller in parameter singleQMask.  At most, qMaskSize characters are */
/* returned inclusive of a terminating null.                           */
/* Returns a return code indicating the results.                       */
/*     0  if the next line was successfully read, and                  */
/*        the line is returned in singleQMask.                         */
/*     -1 if end-of-file reached                                       */
/*     99 if an error occurs reading the file.                         */
/*                                                                     */
/***********************************************************************/
int readNextQMask(char *singleQMask, unsigned qMaskSize)
{
  char         record[256];   /* working copy of record read from file */
  char         *newrec;       /* pointer to nonblank portion of record */
  char         *rc;                            /* fgets returned value */
  int          filerc;                       /* file close return code */
  size_t       bc;              /* positions of blank or \n characters */

  /* Open the queue file if not already open, this will result in      */
  /* parsing from the beginning of the file. */
  if (pQListFile == NULL)
  {
    if ((pQListFile = fopen(QListFileName, "r" )) == NULL)
    {
      WriteToRpt( QLISTOPENERR, 1, 0, "" );
      return(99);
    }
    else if (DebugFlag)
      WriteToRpt(QLISTOPEND, 1, 0, "");
  }

  /*-------------------------------------------------------------------*/
  /* Read until a valid record is found, or until EOF                  */
  rc = fgets(record, sizeof(record), pQListFile);
  while (rc != NULL)
  {
    record[sizeof(record) - 1] = '\0'; /* Null terminate the record */
    if (DebugFlag)
      WriteToRpt(QLISTREADRD, 1, 1, record, strlen(record));

    /* Strip off leading blanks and line feeds.                        */
    bc = strspn(record," \n"); /* find first non-blank */
    if( (bc > 0) && (bc < sizeof(record)) )
      newrec = &record[bc];
    else
      newrec = &record[0];

    /* Strip off trailing white space & line feeds and ensure the      */
    /* string returned is null-terminated.                             */
    bc = strcspn(newrec," \n"); /* find first blank and null terminate */
    if (bc > 0)
      newrec[bc] = '\0';

    /* If a string of a valid length, return it */
    if( (strlen(newrec) > 0) && (strlen(newrec) < qMaskSize) )
    {
      strncpy(singleQMask, newrec, qMaskSize);
      if (DebugFlag)
        WriteToRpt(QLISTRETURND, 1, 1, singleQMask, strlen(singleQMask));

      /* Returning here will leave the file open so that the next read */
      /* is performed from the next line. */
      return(0);
    }
    /* If the line is blank, ignore it and check the next line */
    else if (strlen(newrec) == 0)
    {
      if (DebugFlag)
        WriteToRpt(QLISTEMPTYRECD, 1, 0, "");
    }
    /* If the line is too long for a queue name, ignore it and check   */
    /* the next line. */
    else
      WriteToRpt(QLISTREADE, 1, 1, newrec, strlen(newrec));

    /* Read another record are try again */
    rc = fgets(record, sizeof(record), pQListFile);
  } /* end while not EOF */

  /*-------------------------------------------------------------------*/
  /* To reach here we must have parsed all lines in the file without   */
  /* returning a valid line (this time). We must now close the file so */
  /* that the next time this function is called we will re-open the    */
  /* file and start from the beginning again.                          */
  filerc = fclose(pQListFile);
  pQListFile = NULL;
  if (filerc)
    WriteToRpt( QLISTCLOSEERR, 1, 0, "");

  if (DebugFlag)
    WriteToRpt(QLISTEOFD, 1, 0, "");

  return(-1);
}

/***********************************************************************/
/*                                                                     */
/* Subroutine:  CheckReason                                            */
/*                                                                     */
/* Checks if the Reason code indicates qmgr is stopping or the queue   */
/* cannot be accessed.                                                 */
/* If so, set global StopMon flag to TRUE and report the reason.       */
/*                                                                     */
/***********************************************************************/
void CheckReason (MQLONG Reason)
{
  if (Reason == MQRC_Q_MGR_QUIESCING)
  {
    WriteToRpt( QMGRTERMI, 1, 0, "The queue manager is quiescing." );
    StopMon = TRUE;
  }
  else if (Reason == MQRC_Q_MGR_STOPPING)
  {
    WriteToRpt( QMGRTERMI, 1, 0, "The queue manager is stopping." );
    StopMon = TRUE;
  }
  else if (Reason == MQRC_Q_MGR_NOT_AVAILABLE)
  {
    WriteToRpt( QMGRTERMI, 1, 0, "The queue manager is not available." );
    StopMon = TRUE;
  }
  else if (Reason == MQRC_CONNECTION_BROKEN)
  {
    WriteToRpt( QMGRTERMI, 1, 0,
                "Connection to the queue manager is broken." );
    StopMon = TRUE;
  }
  else if (Reason == MQRC_GET_INHIBITED)
  {
    WriteToRpt( QMGRTERMI, 1, 0, "Monitor queue has been get inhibited" );
    StopMon = TRUE;
  }
  else if (Reason == MQRC_OBJECT_CHANGED)
  {
    WriteToRpt( QMGRTERMI, 1, 0, "Monitor queue has been changed" );
    StopMon = TRUE;
  }
  else if (Reason == MQRC_OBJECT_DAMAGED)
  {
    WriteToRpt( QMGRTERMI, 1, 0, "Monitor queue has been damaged" );
    StopMon = TRUE;
  }
}

/***********************************************************************/
/*                                                                     */
/* Subroutine:  WriteToRpt                                             */
/*                                                                     */
/* Reports error messages to an error log file.  The caller to this    */
/* routine passes the message identifier and the inserts.              */
/* This routine looks up the message text using the message identifier,*/
/* builds the error message, and writes it to the error log file.      */
/*                                                                     */
/* The first insert must be a null-terminated string.                  */
/* The remaining inserts can be a list of strings followed by a list   */
/* of longs. numSInserts and numLInserts define how many of each are   */
/* supplied                                                            */
/*                                                                     */
/* Sample invocation:                                                  */
/*    WriteToRpt(517, 1, 2, "TEST.QUEUE",2,2033);                      */
/*     Reports error #517 using 3 inserts:                             */
/*             String insert   = "TEST.QUEUE"                          */
/*             Long insert     = 2                                     */
/*             Long insert     = 2033                                  */
/*                                                                     */
/***********************************************************************/
void WriteToRpt (unsigned errid,
                 int      numSInserts,
                 int      numLInserts,
                 char    *insert,
                 ...)
{

  char           curtimes[20];                /* MM/DD/YYYY hh:mm:ss\0 */
  time_t         curtime;
  struct tm      *timeinfo;
  char           prefix[32];
  struct stat    filestat;
  long           filesize;
  int            filerc;
  int            prc;
  va_list        ap;
  char          *insertString[10];
  long           insertLong[10];
  char          *nullString = "NULL";
  int            i, j;
  FILE           *pOutputFile;

  /* If this is the first time, we need to open the report file */
  if(pReport == NULL)
  {
    /* If we cannot open the report file we must terminate */
    if ((pReport = fopen(RptFilename, "a" )) == NULL)
    {
      fprintf(stderr,"%s%04u%c Error opening report file %s\n",
              RPT_PREFIX,RPTOPENERR,E,RptFilename);

      /* We can't work without a log, exit */
      StopMon = TRUE;

      /* While terminating, redirect errors to standard out (as the    */
      /* log is not available). */
      pReport = stdout;
      return;
    }
  }

  /* Get the current local time in a printable format */
  time(&curtime);
  timeinfo = localtime(&curtime);
  curtimes[0]='\0';
  /* Build printable date/time string */
  strftime(curtimes,sizeof(curtimes),"%m/%d/%Y %H:%M:%S",timeinfo);
  /* Add the process ID into the prefix data */
  sprintf(prefix, "%s %s", curtimes, pid);

  /*-------------------------------------------------------------------*/
  /* A report file is capped at a certain size. The tool always writes */
  /* into report file 01, but it also keeps the two previous report    */
  /* files, renamed to 02 and 03 as a new 01 file is created.          */

  /* When using an actual log file, check its current size */
  if ((pReport != stdout) && (stat(RptFilename,&filestat) == 0))
  {
    filesize = filestat.st_size;
    if (filesize >= MAX_RPT_SIZE)
    {
      if (DebugFlag)
        prc=fprintf(pReport,"%s  %s%04u%c Report file %s reached " \
                            "or surpassed maximum size. File size is %ld\n",
                    prefix,RPT_PREFIX,DEBUGMSGD,D,RptFilename,filesize);

      /* Remove RPT03 if it exists */
      filerc = remove(RptFilename03);
      if (DebugFlag)
        prc=fprintf(pReport,"%s  %s%04u%c Remove oldest report " \
                            "file %s return code=%d.\n",
                    prefix,RPT_PREFIX,DEBUGMSGD,D,RptFilename03,filerc);

      filerc = rename(RptFilename02,RptFilename03);
      if (DebugFlag)
        prc=fprintf(pReport,"%s  %s%04u%c Rename report file " \
                            "from %s to %s return code=%d.\n",
                    prefix,RPT_PREFIX,DEBUGMSGD,D,RptFilename02,
                    RptFilename03,filerc);

      /* Close the current report file */
      fflush(pReport);
      filerc = fclose(pReport);

      /* Rename the current report file from RPT01 to RPT02 */
      filerc = rename(RptFilename,RptFilename02);
      /* Report error but don't terminate yet; try to keep writing to  */
      /* RPT01. */
      if (filerc != 0)
        fprintf(stderr,"%s%04u%c Error renaming report file %s to %s. " \
                       "Return code = %d.\n",
                RPT_PREFIX,RPTRENAMEERR,E,RptFilename,RptFilename02,filerc);

      /* Open a new copy of the report file RPT01        */
      if ((pReport = fopen(RptFilename, "a" )) == NULL)
      {
        fprintf(stderr,"%s%04u%c Error opening report file %s\n",
                RPT_PREFIX,RPTOPENERR,E,RptFilename);

        /* Signal reporting problem */
        StopMon = TRUE;

        /* While terminating, redirect errors to standard out (as the  */
        /* log is not available). */
        pReport = stdout;
      }
      /* Record the start configuration in each report file */
      else
      {
        prc=fprintf(pReport,"%s  %s%04u%c New report log created\n",
                    prefix,RPT_PREFIX,NEWREPORTI,I);
        prc=fprintf(pReport,"      Queue name(s) to monitor are in cluster %s\n",
                    QClusterName);
        if (QListFileName[0] != '\0')
          prc=fprintf(pReport,"      Queue name list to be read from %s\n",
                      QListFileName);
        else
          prc=fprintf(pReport,"      Queue name(s) to monitor are %s\n",
                      QNameMask);
        prc=fprintf(pReport,"      Local queue to be used exclusively by " \
                            "the monitor is %s\n",ReplyQ);
        prc=fprintf(pReport,"      Polling interval is %ld seconds.\n",
                    PollInterval);
        prc=fprintf(pReport,"      Transfer messages: %s\n",
                    (TransferFlag ? "TRUE" : "FALSE") );
        prc=fprintf(pReport,"      Modify CLWLUSEQ: %s",
                    (SwitchUseQ ? "TRUE" : "FALSE"));
        if (SwitchUseQ)
          prc=fprintf(pReport,"(%s)",
                      ((UseQActiveVal == MQCLWL_USEQ_LOCAL) ? "LOCAL"
                                                            : "QMGR"));
        prc=fprintf(pReport,"\n      AMQSCLM Version: %s\n",version);
      }
    } /* end if file is too large */
  } /* end if using real report file */

  /* Transfer the variable number of string and long arguments to */
  /* usable arrays */
  va_start(ap, insert);
  insertString[0] = insert; /* Always the first argument */
  for (i = 1; i < 10; i++)
  {
    if(i < numSInserts)
      insertString[i] = va_arg(ap,char *);
    else
      insertString[i] = nullString; /* Default to "NULL" */
  }
  for (i = 0; i < 10; i++)
  {
    if(i < numLInserts)
      insertLong[i] = va_arg(ap,int);
    else
      insertLong[i] = -1; /* Default to -1 */
  }
  va_end(ap);

  /* If output to stdout has been enabled, output all messages to the  */
  /* report rile and to stdout. */
  for (j=0;
       j < (LogToStdout ? 2 : 1); /* Go round twice when LogToStdout is set */
       j++)
  {
    /* First time, write to the report file */
    if(j == 0)
      pOutputFile = pReport;
    /* Second time write to stdout */
    else
    {
      /* If we had a problem writing to the report we'll already be    */
      /* writing to stdout, so skip this one. */
      if(pReport == stdout)
        break;

      pOutputFile = stdout;
    }

    /* Locate the specific error message */
    switch (errid)
    {
      case DEBUGMSGD: /* Generic debug msg */
        prc=fprintf(pOutputFile,"%s  %s%04u%c %s",
                    prefix,RPT_PREFIX,errid,D,insertString[0]);
        break;
      case QLISTREADRD: /* Read a queue name/mask record from input file */
        prc=fprintf(pOutputFile,"%s  %s%04u%c Read queue name/mask " \
                            "record %s from queue list input file. " \
                            "Rec length=%ld\n",
                    prefix,RPT_PREFIX,errid,D,insertString[0],insertLong[0]);
        break;
      case QLISTRETURND: /* Found a valid queue name record from input file */
        prc=fprintf(pOutputFile,"%s  %s%04u%c Found valid queue " \
                            "name/mask in queue list file.\n",
                    prefix,RPT_PREFIX,errid,D);
        prc=fprintf(pOutputFile,"      Queue name=%s  Name length=%ld\n",
                    insertString[0],insertLong[0]);
        break;
      case QLISTREADE: /* invalid queue name/mask record found in input file */
        prc=fprintf(pOutputFile,"%s  %s%04u%c Error: Invalid record in queue " \
                            "list file.  Queue name is too long, " \
                            "length=%ld\n",
                    prefix,RPT_PREFIX,errid,E,insertLong[0]);
        prc=fprintf(pOutputFile,"      Queue name=%s\n",
                    insertString[0]);
        break;
      case QLISTOPEND: /* Successfully opened the queue list input file */
          prc=fprintf(pOutputFile,"%s  %s%04u%c Successfully opened the " \
                              "queue list file %s for input.\n",
                      prefix,RPT_PREFIX,errid,D,QListFileName);
        break;
      case QLISTEOFD: /* Reached end of file reading queue list input file */
          prc=fprintf(pOutputFile,"%s  %s%04u%c Reached end of file " \
                              "reading queue list input file.\n",
                      prefix,RPT_PREFIX,errid,D);
        break;
      case QLISTEMPTYRECD: /* Empty record found in the queue list input file */
          prc=fprintf(pOutputFile,"%s  %s%04u%c Empty record found in " \
                              "the queue list file.  Record is ignored.\n",
                      prefix,RPT_PREFIX,errid,D);
        break;
      case STARTMSGI:
        prc=fprintf(pOutputFile,"%s  %s%04u%c AMQSCLM started for queue " \
                            "manager %s.\n",
                    prefix,RPT_PREFIX,errid,I,QMgrName);
        prc=fprintf(pOutputFile,"      Queue name(s) to monitor are in cluster %s\n",
                    QClusterName);
        if (QListFileName[0] != '\0')
          prc=fprintf(pOutputFile,"      Queue name list to be read from %s\n",
                      QListFileName);
        else
          prc=fprintf(pOutputFile,"      Queue name(s) to monitor are %s\n",
                      QNameMask);
        prc=fprintf(pOutputFile,"      Local queue to be used exclusively by the " \
                            "monitor is %s\n",
                    ReplyQ);
        prc=fprintf(pOutputFile,"      Polling interval is %ld seconds.\n",
                    PollInterval);
        prc=fprintf(pOutputFile,"      Transfer messages: %s\n",
                    (TransferFlag ? "TRUE" : "FALSE") );
        prc=fprintf(pOutputFile,"      Modify CLWLUSEQ: %s",
                    (SwitchUseQ ? "TRUE" : "FALSE"));
        if (SwitchUseQ)
          prc=fprintf(pOutputFile,"(%s)",
                      ((UseQActiveVal == MQCLWL_USEQ_LOCAL) ? "LOCAL"
                                                            : "QMGR"));
        prc=fprintf(pOutputFile,"\n      AMQSCLM Version: %s\n",version);
        break;
      case MONACTI:
        prc=fprintf(pOutputFile,"%s  %s%04u%c AMQSCLM now monitoring\n",
                    prefix,RPT_PREFIX,errid,I);
        break;
      case MQCONNERR:
        prc=fprintf(pOutputFile,"%s  %s%04u%c MQCONN failed for queue " \
                            "manager %s, CC=%ld, RC=%ld\n",
                    prefix,RPT_PREFIX,errid,E,
                    insertString[0],insertLong[0],insertLong[1]);
        break;
      case MQDISCERR:
        prc=fprintf(pOutputFile,"%s  %s%04u%c MQDISC failed for queue " \
                            "manager %s, CC=%ld, RC=%ld\n",
                    prefix,RPT_PREFIX,errid,E,insertString[0],insertLong[0],
                    insertLong[1]);
        break;
      case MQOPENERR:
        prc=fprintf(pOutputFile,"%s  %s%04u%c Failed to open queue %s, " \
                            "CC=%ld, RC=%ld\n",
                    prefix,RPT_PREFIX,errid,E,insertString[0],insertLong[0],
                    insertLong[1]);
        prc=fprintf(pOutputFile,"      AMQSCLM will not start monitoring until " \
                            "the open succeeds\n");
        break;
      case MQCLOSEERR:
        prc=fprintf(pOutputFile,"%s  %s%04u%c MQCLOSE failed for queue %s, " \
                            "CC=%ld, RC=%ld\n",
                    prefix,RPT_PREFIX,errid,E,insertString[0],insertLong[0],
                    insertLong[1]);
        break;
      case MQOPENREQIERR:
        prc=fprintf(pOutputFile,"%s  %s%04u%c Within ReQueue, MQOPEN for " \
                            "input failed for queue %s, CC=%ld, RC=%ld\n",
                    prefix,RPT_PREFIX,errid,E,insertString[0],insertLong[0],
                    insertLong[1]);
        break;
      case MQOPENREQOERR:
        prc=fprintf(pOutputFile,"%s  %s%04u%c Within ReQueue, MQOPEN for " \
                            "output failed for queue %s, CC=%ld, RC=%ld\n",
                    prefix,RPT_PREFIX,errid,E,insertString[0],insertLong[0],
                    insertLong[1]);
        break;
      case MQCLOSEREQIERR:
        prc=fprintf(pOutputFile,"%s  %s%04u%c Within ReQueue, MQCLOSE failed " \
                            "for input queue %s, CC=%ld, RC=%ld\n",
                    prefix,RPT_PREFIX,errid,E,insertString[0],insertLong[0],
                    insertLong[1]);
        break;
      case MQCLOSEREQOERR:
        prc=fprintf(pOutputFile,"%s  %s%04u%c Within ReQueue, MQCLOSE failed " \
                            "for output queue %s, CC=%ld, RC=%ld\n",
                    prefix,RPT_PREFIX,errid,E,insertString[0],insertLong[0],
                    insertLong[1]);
        break;
      case NOQERR:
        prc=fprintf(pOutputFile,"%s  %s%04u%c No queues found of name '%s' in " \
                            "cluster '%s'",
                    prefix,RPT_PREFIX,errid,E,insertString[0],insertString[1]);
        break;
      case PCFINQQERR:
        prc=fprintf(pOutputFile,"%s  %s%04u%c PCF Inquire Queue failed for " \
                            "queue %s in cluster %s. CC=%ld, RC=%ld\n",
                    prefix,RPT_PREFIX,errid,E,insertString[0],insertString[1],
                    insertLong[0],insertLong[1]);
        break;
      case PCFINQCLQERR:
        prc=fprintf(pOutputFile,"%s  %s%04u%c PCF Inquire Queue ClusterInfo " \
                            "failed for queue %s  PCF CC=%ld, RC=%ld\n",
                    prefix,RPT_PREFIX,errid,E,insertString[0],insertLong[0],
                    insertLong[1]);
        break;
      case MQPUTCHGQERR:
        prc=fprintf(pOutputFile,"%s  %s%04u%c MQPUT for PCF Change Queue " \
                            "command failed while attempting to change " \
                            "queue %s, CC=%ld, RC=%ld\n",
                    prefix,RPT_PREFIX,errid,E,insertString[0],insertLong[0],
                    insertLong[1]);
        break;
      case MQGETCHGQERR:
        prc=fprintf(pOutputFile,"%s  %s%04u%c MQGET for PCF Change Queue " \
                            "response failed while attempting to " \
                            "change queue %s, CC=%ld, RC=%ld\n",
                    prefix,RPT_PREFIX,errid,E,insertString[0],insertLong[0],
                    insertLong[1]);
        break;
      case PCFCHGQERR:
        prc=fprintf(pOutputFile,"%s  %s%04u%c PCF Change Queue failed for " \
                            "queue %s, PCF CC=%ld, RC=%ld\n",
                    prefix,RPT_PREFIX,errid,E,insertString[0],insertLong[0],
                    insertLong[1]);
        break;
      case MQPUTPCFERR:
        prc=fprintf(pOutputFile,"%s  %s%04u%c MQPUT for PCF command failed " \
                            "while writing to queue %s, CC=%ld, RC=%ld\n",
                    prefix,RPT_PREFIX,errid,E,insertString[0],insertLong[0],
                    insertLong[1]);
        break;
      case MQGETPCFERR:
        prc=fprintf(pOutputFile,"%s  %s%04u%c MQGET for PCF response failed " \
                            "while reading from reply queue %s, " \
                            "CC=%ld, RC=%ld\n",
                    prefix,RPT_PREFIX,errid,E,insertString[0],insertLong[0],
                    insertLong[1]);
        break;
      case MQGETREQERR:
        prc=fprintf(pOutputFile,"%s  %s%04u%c Within ReQueue, MQGET failed " \
                            "for queue %s, CC=%ld, RC=%ld.\n",
                    prefix,RPT_PREFIX,errid,E,insertString[0],insertLong[0],
                    insertLong[1]);
        prc=fprintf(pOutputFile,"      The transaction for the current batch " \
                            "will be backed out.\n");
        break;
      case MQPUTREQERR:
        prc=fprintf(pOutputFile,"%s  %s%04u%c Within ReQueue, MQPUT failed " \
                            "for queue %s, CC=%ld, RC=%ld.\n",
                    prefix,RPT_PREFIX,errid,E,insertString[0],insertLong[0],
                    insertLong[1]);
        prc=fprintf(pOutputFile,"      The transaction for the current " \
                            "batch will be backed out.\n");
        break;
      case MQCMITREQERR:
        prc=fprintf(pOutputFile,"%s  %s%04u%c Within ReQueue, MQCMIT failed " \
                            "for queue %s, CC=%ld, RC=%ld\n",
                    prefix,RPT_PREFIX,errid,E,insertString[0],insertLong[0],
                    insertLong[1]);
        break;
      case MQBACKREQERR:
        prc=fprintf(pOutputFile,"%s  %s%04u%c Within ReQueue, MQBACK failed " \
                            "for queue %s, CC=%ld, RC=%ld\n",
                    prefix,RPT_PREFIX,errid,E,insertString[0],insertLong[0],
                    insertLong[1]);
        break;
      case CHGQI:
        prc=fprintf(pOutputFile,"%s  %s%04u%c Queue %s successfully changed " \
                            "to %s\n",
                    prefix,RPT_PREFIX,errid,I, insertString[1],
                    insertString[0]);
        break;
      case CHGUSEQI:
        {
          char useq[6];

          switch(insertLong[0])
          {
            case MQCLWL_USEQ_ANY:
              strcpy(useq, "ANY");
              break;
            case MQCLWL_USEQ_LOCAL:
              strcpy(useq, "LOCAL");
              break;
            case MQCLWL_USEQ_AS_Q_MGR:
              strcpy(useq, "QMGR");
              break;
          }

          prc=fprintf(pOutputFile,"%s  %s%04u%c CLWLUSEQ of queue %s changed " \
                              "to %s\n",
                      prefix,RPT_PREFIX,errid,I,insertString[0],useq);
        }
        break;
      case REQI:
        prc=fprintf(pOutputFile,"%s  %s%04u%c Successfully requeued %ld " \
                            "messages from queue %s to alternate " \
                            "instance(s).\n",
                    prefix,RPT_PREFIX,errid,I,insertLong[0],insertString[0]);
        break;
      case REQNOALTW:
        prc=fprintf(pOutputFile,"%s  %s%04u%c Alternate instance(s) for " \
                            "queue %s are currently unavailable.\n",
                    prefix,RPT_PREFIX,errid,W,insertString[0]);
        prc=fprintf(pOutputFile,"      Remaining messages are not moved off " \
                            "of this inactive queue.\n");
        break;
      case NOALTINSTD:
        prc=fprintf(pOutputFile,"%s  %s%04u%c No active instances " \
                            "found for queue %s. CURDEPTH=%ld. Messages " \
                            "will not be moved.\n",
                    prefix,RPT_PREFIX,errid,W,insertString[0],insertLong[0]);
        break;
      case MEASUREI:
        prc=fprintf(pOutputFile,"%s  %s%04u%c Time Statistics:\n      Average " \
                            "processing time per polling interval = %ld " \
                            "milliseconds\n",
                    prefix,RPT_PREFIX,errid,I,(insertLong[2]/insertLong[0]));
        prc=fprintf(pOutputFile,"      Total processing time (net of sleep " \
                            "time) = %ld milliseconds\n",
                    insertLong[2]);
        prc=fprintf(pOutputFile,"      Minimum = %ld seconds, Maximum = %ld\n",
                    insertLong[3],insertLong[4]);
        prc=fprintf(pOutputFile,"      Measurement interval = %ld seconds, " \
                            "Poll count = %ld\n",
                    insertLong[1],insertLong[0]);
        prc=fprintf(pOutputFile,"      Average number of queues checked per " \
                            "poll = %ld\n",
                    insertLong[5]);
        prc=fprintf(pOutputFile,"      Total number of activity state changes " \
                            "= %ld\n",
                    insertLong[6]);
        prc=fprintf(pOutputFile,"      Total number of messages transferred " \
                            "= %ld\n",
                    insertLong[7]);
        break;
      case STATSMSGI:
        prc=fprintf(pOutputFile,"%s  %s%04u%c STATS: Time %ld, Queues %ld, " \
                            "Changes %ld, Msgs %ld\n",
                    prefix,RPT_PREFIX,errid,I,insertLong[0],insertLong[1],
                    insertLong[2],insertLong[3]);
        break;
      case QLISTOPENERR:
        prc=fprintf(pOutputFile,"%s  %s%04u%c Error opening queue list input " \
                            "file %s.\n",
                    prefix,RPT_PREFIX,errid,E,QListFileName);
        break;
      case QLISTCLOSEERR:
        prc=fprintf(pOutputFile,"%s  %s%04u%c Error closing queue list file %s.\n",
                    prefix,RPT_PREFIX,errid,E,QListFileName);
        break;
      case QMGRTERMI:
        prc=fprintf(pOutputFile,"%s  %s%04u%c AMQSCLM has detected a " \
                            "problem with queue manager %s\n",
                    prefix,RPT_PREFIX,errid,I,QMgrName);
        prc=fprintf(pOutputFile,"      Reason:  %s\n",
                    insertString[0]);
        break;
      case STOPMONI:
        prc=fprintf(pOutputFile,"%s  %s%04u%c AMQSCLM received the command " \
                            "to stop monitoring queue manager %s and " \
                            "cluster %s.\n",
                    prefix,RPT_PREFIX,errid,I,QMgrName,QClusterName);
        break;
      case TERMINATEMSGI:
        prc=fprintf(pOutputFile,"%s  %s%04u%c AMQSCLM ended for queue " \
                            "manager %s and cluster %s.\n",
                    prefix,RPT_PREFIX,errid,I,QMgrName,QClusterName);
        break;
      case STARTDELAYI:
        prc=fprintf(pOutputFile,"%s  %s%04u%c Monitoring delayed for %ld seconds\n",
                    prefix,RPT_PREFIX,errid,I,insertLong[0] );
        break;
      case USEQERR:
        prc=fprintf(pOutputFile,"%s  %s%04u%c Invalid CLWLUSEQ value for queue %s\n",
                    prefix,RPT_PREFIX,errid,E,insertString[0]);
        prc=fprintf(pOutputFile,"      For msg transfer the value must be ANY or QMGR " \
                            "with ANY set on the qmgr\n");
        prc=fprintf(pOutputFile,"      Alternatively use the -u argument with the " \
                            "-t argument\n");
        break;
      case MQINQERR:
        prc=fprintf(pOutputFile,"%s  %s%04u%c Queue manager MQINQ failed. " \
                            "CC=%ld, RC=%ld\n",
                    prefix,RPT_PREFIX,errid,E,
                    insertLong[0],insertLong[1]);
        break;
      case MSGLENERR:
        prc=fprintf(pOutputFile,"%s  %s%04u%c Error in PCF message length from %s. "\
                            "AvailLen=%ld, ActualLen=%ld\n",
                    prefix,RPT_PREFIX,errid,E, insertString[0],
                    insertLong[0],insertLong[1]);
        break;
    } /* end switch */

    /* Only check the outcome from writing to a report file, not stdout */
    if(j == 0)
    {
      /* fprintf to report file failed */
      if (prc < 0)
      {
        fprintf(stderr,"%s%04u%c Error writing to report file %s\n",
                RPT_PREFIX,RPTWRITEERR,E,RptFilename);

        /* Close the report file */
        fclose(pReport);
        /* we can't log it, so change to stdout while we're terminating */
        pReport = stdout;
        /* Terminate the tool */
        StopMon = TRUE;
      }
      /* Flush out the message to the log file */
      else
        fflush(pReport);
    }
  } /* for(j) pReport/stdout repeat*/
}

/***********************************************************************/
/*                                                                     */
/* Subroutine:  ParseArguments                                         */
/*                                                                     */
/* Parse the arguments set on the command line. An incorrect set of    */
/* arguments will terminate the tool.                                  */
/*                                                                     */
/***********************************************************************/
MQLONG ParseArguments(int argc, char *argv[])
{
  char           arglabel;       /* Main program argument label        */

  /* Optional arguments */
  ReportPath[0]       = '\0';                               /* not set */
  LogToStdout         = FALSE;       /* Don't log to stdout by default */
  DebugFlag           = FALSE;            /* Debug disabled by default */
  PollInterval        = DEFAULT_POLLING_INT;    /* Default polling int */
  TransferFlag        = FALSE;       /* No message transfer by default */
  SwitchUseQ          = FALSE;     /* By default don't switch CLWLUSEQ */
  UseQActiveVal       = -1;                                 /* not set */
  StatsFlag           = FALSE;    /* No per-iteration stats by default */

  /* Required parameters */
  QMgrName[0]         = '\0';
  QClusterName[0]     = '\0';
  QNameMask[0]        = '\0';
  QListFileName[0]    = '\0';
  ReplyQ[0]           = '\0';

  if ( argc < 9 )
  {
    fprintf(stderr,
      "AMQSCLM - Active Cluster Queue Message Distribution\n");
    fprintf(stderr,
      "Version: %s\n\n",version);
    fprintf(stderr,
      "Usage:   AMQSCLM -m QMgrName -c ClusterName\n");
    fprintf(stderr,
      "                 (-q QNameMask | -f QListFile) -r MonitorQName\n");
    fprintf(stderr,
      "                 -l ReportDir [-i Interval] [-t]\n");
    fprintf(stderr,
      "                 [-u ActiveVal] [-d] [-s] [-v]\n\n");
    fprintf(stderr,
      "Where:   -m QMgrName     Queue manager to monitor\n");
    fprintf(stderr,
      "         -c ClusterName  Cluster containing the queues to monitor\n");
    fprintf(stderr,
      "         -q QNameMask    Queue(s) to monitor, where a trailing '*'\n");
    fprintf(stderr,
      "                         will monitor all matching queues\n");
    fprintf(stderr,
      "         -f QListFile    File name containing a list of queue(s) or\n");
    fprintf(stderr,
      "                         queue masks to monitor. Contains one queue " \
      "name\n");
    fprintf(stderr,
      "                         per line.\n");
    fprintf(stderr,
      "                         (not valid if -q argument provided)\n");
    fprintf(stderr,
      "         -r MonitorQName Local queue to be used exclusively by the\n");
    fprintf(stderr,
      "                         monitor\n");
    fprintf(stderr,
      "         -l ReportDir    (optional) Directory path to store logged \n");
    fprintf(stderr,
      "                         messages (log to standard out by default)\n");
    fprintf(stderr,
      "         -i Interval     (optional) Interval in seconds at which " \
      "the \n");
    fprintf(stderr,
      "                         monitor checks the queues\n");
    fprintf(stderr,
      "                         Defaults to 300 (5 minutes)\n");
    fprintf(stderr,
      "         -t              (optional) Transfer messages from inactive " \
      "queues\n" \
      "                         The queue must either have a CLWLUSEQ of " \
      "ANY to allow\n" \
      "                         transfer or the -u argument must be used\n" );
    fprintf(stderr,
      "                         (No transfer by default)\n");
    fprintf(stderr,
      "         -u ActiveVal    (optional) Automatically switch the CLWLUSEQ "\
      "value\n");
    fprintf(stderr,
      "                         of a queue from the 'ActiveVal' to 'ANY'\n" \
      "                         while the queue is inactive\n");
    fprintf(stderr,
      "                         (ActiveVal may be 'LOCAL' or 'QMGR')\n");
    fprintf(stderr,
      "                         (not modified by default)\n");
    fprintf(stderr,
      "         -d              (optional) Enable additional diagnostic " \
      "output\n");
    fprintf(stderr,
      "                         (No diagnostic output by default)\n");
    fprintf(stderr,
      "         -s              (optional) Enable minimal statistics\n");
    fprintf(stderr,
      "                         output per interval\n");
    fprintf(stderr,
      "                         (No per-iteration statistics output by\n");
    fprintf(stderr,
      "                         default) \n");
    fprintf(stderr,
      "         -v              (optional) Log report information to\n");
    fprintf(stderr,
      "                         standard out\n");
    fprintf(stderr,
      "                         (No standard out output by\n");
    fprintf(stderr,
      "                         default) \n");
    fprintf(stderr,
      "Example: AMQSCLM -m QM1 -c CLUS1 -f /QList.txt -r MONQ\n");
    fprintf(stderr,
      "                 -l /mon_reports -t -u LOCAL -s\n\n");
    return MQCC_FAILED;
  } /* end if argc < 11 */
  else
  {
    /* Parse each argument */
    while (--argc)
    {
      ++argv;
      if (*argv[0] == '-')
      {
        arglabel = *++argv[0];
        switch (arglabel)
        {
          case 'm':
            /* Jump on one to the parm's value */
            ++argv;
            --argc;
            if (argc == 0)
            {
              fprintf(stderr,"%s%04u%c AMQSCLM command error: missing " \
                             "value for -m argument\n",
                      RPT_PREFIX,ARGLENERR,E);
              return MQCC_FAILED;
            }
            else if (strlen(argv[0]) > MQ_Q_MGR_NAME_LENGTH)
            {
              fprintf(stderr,"%s%04u%c AMQSCLM command error: Queue " \
                             "manager name too long \'-m QMgrName\'\n",
                      RPT_PREFIX,ARGLENERR,E);
              return MQCC_FAILED;
            }
            strncpy(QMgrName,  argv[0], sizeof(QMgrName));
            break;
          case 'c':
            /* Jump on one to the parm's value */
            ++argv;
            --argc;
            if (argc == 0)
            {
              fprintf(stderr,"%s%04u%c AMQSCLM command error: missing " \
                             "value for -c argument\n",
                      RPT_PREFIX,ARGLENERR,E);
              return MQCC_FAILED;
            }
            else if (strlen(argv[0]) > MQ_CLUSTER_NAME_LENGTH)
            {
              fprintf(stderr,"%s%04u%c AMQSCLM command error: Cluster " \
                             "name too long \'-c ClusterName\'\n",
                      RPT_PREFIX,ARGLENERR,E);
              return MQCC_FAILED;
            }
            strncpy(QClusterName,argv[0],sizeof(QClusterName));
            break;
          case 'q':
            /* Jump on one to the parm's value */
            ++argv;
            --argc;
            if (argc == 0)
            {
              fprintf(stderr,"%s%04u%c AMQSCLM command error: missing " \
                             "value for -q argument\n",
                      RPT_PREFIX,ARGLENERR,E);
              return MQCC_FAILED;
            }
            else if (strlen(argv[0]) > MQ_Q_NAME_LENGTH)
            {
              fprintf(stderr,"%s%04u%c AMQSCLM command error: Queue " \
                             "name mask too long \'-q QNameMask\'\n",
                      RPT_PREFIX,ARGLENERR,E);
              return MQCC_FAILED;
            }
            strncpy(QNameMask, argv[0], sizeof(QNameMask));
            break;
          case 'f':
            /* Jump on one to the parm's value */
            ++argv;
            --argc;
            if (argc == 0)
            {
              fprintf(stderr,"%s%04u%c AMQSCLM command error: missing " \
                             "value for -f argument\n",
                      RPT_PREFIX,ARGLENERR,E);
              return MQCC_FAILED;
            }
            else if (strlen(argv[0]) > sizeof(QListFileName)-1)
            {
              fprintf(stderr,"%s%04u%c AMQSCLM command error: Queue list " \
                             "file name too long \'-f QListFile\'\n",
                      RPT_PREFIX,ARGLENERR,E);
              return MQCC_FAILED;
            }
            strncpy(QListFileName, argv[0], sizeof(QListFileName));
            break;
          case 'r':
            /* Jump on one to the parm's value */
            ++argv;
            --argc;
            if (argc == 0)
            {
              fprintf(stderr,"%s%04u%c AMQSCLM command error: missing " \
                             "value for -r argument\n",
                      RPT_PREFIX,ARGLENERR,E);
              return MQCC_FAILED;
            }
            else if (strlen(argv[0]) > MQ_Q_NAME_LENGTH)
            {
              fprintf(stderr,"%s%04u%c AMQSCLM command error: Monitor " \
                             "queue name too long \'-r MonitorQName\'\n",
                      RPT_PREFIX,ARGLENERR,E);
              return MQCC_FAILED;
            }
            strncpy(ReplyQ,    argv[0], sizeof(ReplyQ));
            break;
          case 'l':
            /* Jump on one to the parm's value */
            ++argv;
            --argc;
            if (argc == 0)
            {
              fprintf(stderr,"%s%04u%c AMQSCLM command error: missing " \
                             "value for -l argument\n",
                      RPT_PREFIX,ARGLENERR,E);
              return MQCC_FAILED;
            }
            else if (strlen(argv[0]) > sizeof(ReportPath)-1)
            {
              fprintf(stderr,"%s%04u%c AMQSCLM command error: Directory " \
                             "path name too long \'-l ReportDir\'\n",
                      RPT_PREFIX,ARGLENERR,E);
              return MQCC_FAILED;
            }
            strncpy(ReportPath,   argv[0], sizeof(ReportPath));
            break;
          case 'u':
            /* Jump on one to the parm's value */
            ++argv;
            --argc;
            if (argc == 0)
            {
              fprintf(stderr,"%s%04u%c AMQSCLM command error: missing " \
                             "value for -u argument\n",
                      RPT_PREFIX,ARGLENERR,E);
              return MQCC_FAILED;
            }
            else
            {
              if(strcmp(argv[0], "LOCAL") == 0)
              {
                SwitchUseQ = TRUE;
                UseQActiveVal = MQCLWL_USEQ_LOCAL;
              }
              else if(strcmp(argv[0], "QMGR") == 0)
              {
                SwitchUseQ = TRUE;
                UseQActiveVal = MQCLWL_USEQ_AS_Q_MGR;
              }
              else
              {
                fprintf(stderr,"%s%04u%c AMQSCLM command error: Invalid " \
                               "-u value, 'LOCAL' or 'QMGR' allowed\n",
                        RPT_PREFIX,ARGLENERR,E);
                return MQCC_FAILED;
              }
            }
            break;
          case 'i':
            /* Jump on one to the parm's value */
            ++argv;
            --argc;
            if (argc == 0)
            {
              fprintf(stderr,"%s%04u%c AMQSCLM command error: missing " \
                             "value for -i argument\n",
                      RPT_PREFIX,ARGLENERR,E);
              return MQCC_FAILED;
            }
            else
            {
                PollInterval = atoi(   argv[0] );

                if ((PollInterval < 1) || (PollInterval > 86400))
                {
                  fprintf(stderr,"%s%04u%c AMQSCLM command error: invalid " \
                                 "value for -i argument\n" \
                                 "      (valid values are between 1 and 86400 " \
                                 "inclusive)\n",
                          RPT_PREFIX,ARGLENERR,E);
                  return MQCC_FAILED;
                }
            }
            break;
          case 'v':
            LogToStdout = TRUE;
            break;
          case 'd':
            DebugFlag = TRUE;
            break;
          case 't':
            TransferFlag = TRUE;
            break;
          case 's':
            StatsFlag = TRUE;
            break;
          default:
            fprintf(stderr,"%s%04u%c AMQSCLM command error:  Illegal " \
                           "parameter \'-%c\'\n",
                    RPT_PREFIX,ARGERR,E,arglabel);
            return MQCC_FAILED;
        } /* end switch */
      } /* end if found '-' parm */
      else /* found unrecognizable parm */
      {
        fprintf(stderr,"%s%04u%c AMQSCLM command error:  Illegal " \
                       "parameter \'%s\'\n",
                RPT_PREFIX,ARGERR,E,argv[0]);
        return MQCC_FAILED;
      }
    } /* end while --argc */

  } /* end else have the minimum number of arguments */

  /* Check for all required parameters                                           */
  if (QMgrName[0] == '\0')
  {
    fprintf(stderr,"%s%04u%c AMQSCLM error:  Missing required parameter " \
                   "\'-m QMgrName\'\n",RPT_PREFIX,ARGMISSINGERR,E);
    return MQCC_FAILED;
  }
  if (QClusterName[0] == '\0')
  {
    fprintf(stderr,"%s%04u%c AMQSCLM error:  Missing required parameter " \
                   "\'-c ClusterName\'\n",RPT_PREFIX,ARGMISSINGERR,E);
    return MQCC_FAILED;
  }
  if (QListFileName[0] == '\0' && QNameMask[0] == '\0')
  {
    fprintf(stderr,"%s%04u%c AMQSCLM error:  Missing required parameters." \
                   "\n      Specify either \'-f QListFile\' or \'-q " \
                   "QNameMask\'\n",RPT_PREFIX,ARGMISSINGERR,E);
    return MQCC_FAILED;
  }
  else if (QListFileName[0] != '\0' && QNameMask[0] != '\0')
  {
    fprintf(stderr,"%s%04u%c AMQSCLM error:  Conflicting parameters " \
                   "\'-f\' and \'-q\'.\n      Specify one or the other, " \
                   "not both.\n",RPT_PREFIX,ARGCONFLICTERR,E);
    return MQCC_FAILED;
  }
  if (ReplyQ[0] == '\0')
  {
    fprintf(stderr,"%s%04u%c AMQSCLM error:  Missing required parameter " \
                   "\'-r ReplyQ\'\n",RPT_PREFIX,ARGMISSINGERR,E);
    return MQCC_FAILED;
  }

  /* The tool either monitors queues that match a single queue mask or */
  /* all queues listed within a file                                   */
  if (QNameMask[0] == '\0')
    useQListFile = TRUE;
  else
    useQListFile = FALSE;

  if( ReportPath[0] == '\0')
  {
    fprintf(stderr,"%s%04u%c AMQSCLM error:  Missing required parameter " \
                   "\'-l ReportDir\'\n",RPT_PREFIX,ARGMISSINGERR,E);
    return MQCC_FAILED;
  }
  else
  {
    /* Construct the report filename: path\QmgrName.ClusterName.RPT01.LOG*/
    strncpy(RptFilename,ReportPath,sizeof(RptFilename));
    /* Add trailing '/' to path name if needed */
    if( (RptFilename[strlen(RptFilename)-1] != '/') &&
        (RptFilename[strlen(RptFilename)-1] != '\\') )
#if (MQAT_DEFAULT == MQAT_WINDOWS_NT)
      strcat(RptFilename,"\\");
#else
      strcat(RptFilename,"/");
#endif

    /* Check if the fully-qualified file name is a valid length.         */
    /* The fully-qualified file name is path length + qmgrname length +  */
    /* cluster name length + 9 for "RPT0n.LOG" suffix plus 2 periods '.' */
    /* following the qmgr & cluster qualifiers                           */
    /* path/QmgrName.ClusterName.RPT0n.LOG                               */
    if ( (strlen(RptFilename)+strlen(QMgrName)+strlen(QClusterName)+9+2) >
            MAX_FILE_PATH)
    {
      fprintf(stderr,"%s%04u%c Error: Report path is too long. It exceeds " \
                     "the maximum length (%d)\n",
              RPT_PREFIX,RPTPATHERR,E,MAX_FILE_PATH);
      fprintf(stderr,"      when combined with the file name:  " \
                     "path/QMgrName.ClusterName.RPT0n.LOG\n");
      return MQCC_FAILED;
    }
    strcat(RptFilename,QMgrName);
    strcat(RptFilename,".");
    strcat(RptFilename,QClusterName);
    strcat(RptFilename,".RPT01.LOG");
    strncpy(RptFilename02,RptFilename,sizeof(RptFilename02));
    strncpy(RptFilename03,RptFilename,sizeof(RptFilename03));
    RptFilename02[strlen(RptFilename)-5] = '2';
    RptFilename03[strlen(RptFilename)-5] = '3';
  }

  return MQCC_OK;
}

/***********************************************************************/
/*                                                                     */
/* Subroutine:  buildInquireQPCF                                       */
/*                                                                     */
/* Build a PCF command message to inquire a list of queues that        */
/* match the supplied queue mask and defined to the supplied           */
/* cluster.                                                            */
/*                                                                     */
/* The message consists of a Request Header and a parameter block      */
/* used to specify the generic search. The header and the parameter    */
/* block follow each other in a contiguous buffer which is pointed     */
/* to by the variable pAdminMsg. This entire buffer is then put to     */
/* the queue.                                                          */
/*                                                                     */
/* Our parameter block contains 4 parameter sections:                  */
/*     QName                                                           */
/*     QType (local)                                                   */
/*     ClusterName                                                     */
/*     The list of queue attibutes required in the responses           */
/*                                                                     */
/***********************************************************************/
MQLONG buildInquireQPCF(MQBYTE *pAdminMsg,
                        MQLONG  AdminMsgLen,
                        char   *QNameMask,
                        char   *QClusterName)
{
  MQCFH          *pPCFHeader;    /* Ptr to PCF header structure        */
  MQCFST         *pPCFString;    /* Ptr to string parm in PCF cmd  */
  MQCFIN         *pPCFInteger;   /* Ptr to integer parm in PCF cmd */
  MQCFIL         *pPCFFieldList; /* Ptr to field list parm in PCF cmd */
  MQLONG          MsgLength;

  /* Setup request header */
  pPCFHeader                 = (MQCFH *)pAdminMsg;
  pPCFHeader->Type           = MQCFT_COMMAND;
  pPCFHeader->StrucLength    = MQCFH_STRUC_LENGTH;
  pPCFHeader->Version        = MQCFH_VERSION_3;
  pPCFHeader->Command        = MQCMD_INQUIRE_Q;
  pPCFHeader->MsgSeqNumber   = MQCFC_LAST;
  pPCFHeader->Control        = MQCFC_LAST;
  pPCFHeader->ParameterCount = 4;
  MsgLength                  = pPCFHeader->StrucLength;

  /* Setup QName parameter block */
  pPCFString                 = (MQCFST *)(pAdminMsg + MsgLength);
  pPCFString->Type           = MQCFT_STRING;
  pPCFString->StrucLength    = MQCFST_STRUC_LENGTH_FIXED + MQ_Q_NAME_LENGTH;
  pPCFString->Parameter      = MQCA_Q_NAME;
  pPCFString->CodedCharSetId = MQCCSI_DEFAULT;
  pPCFString->StringLength   = MQ_Q_NAME_LENGTH;
  strncpy( pPCFString->String, QNameMask, MQ_Q_NAME_LENGTH );
  MsgLength                 += pPCFString->StrucLength;

  /* Setup QType = Local parameter block */
  pPCFInteger              = (MQCFIN *)(pAdminMsg + MsgLength);
  pPCFInteger->Type        = MQCFT_INTEGER;
  pPCFInteger->StrucLength = MQCFIN_STRUC_LENGTH;
  pPCFInteger->Parameter   = MQIA_Q_TYPE ;
  pPCFInteger->Value       = MQQT_LOCAL;
  MsgLength               += pPCFInteger->StrucLength;

  /* Setup ClusterName parameter block */
  pPCFString                 = (MQCFST *)(pAdminMsg + MsgLength);
  pPCFString->Type           = MQCFT_STRING;
  pPCFString->StrucLength    = MQCFST_STRUC_LENGTH_FIXED +
                                 MQ_CLUSTER_NAME_LENGTH;
  pPCFString->Parameter      = MQCA_CLUSTER_NAME;
  pPCFString->CodedCharSetId = MQCCSI_DEFAULT;
  pPCFString->StringLength   = MQ_CLUSTER_NAME_LENGTH;
  memcpy( pPCFString->String, QClusterName, MQ_CLUSTER_NAME_LENGTH );
  MsgLength                 += pPCFString->StrucLength;

  /* Define the list of queue attributes we're interested in */
  pPCFFieldList              = (MQCFIL *)(pAdminMsg + MsgLength);
  pPCFFieldList->Type        = MQCFT_INTEGER_LIST;
  pPCFFieldList->StrucLength = MQCFIL_STRUC_LENGTH_FIXED +
                                 (sizeof(MQLONG) * 6);
  pPCFFieldList->Parameter   = MQIACF_Q_ATTRS;
  pPCFFieldList->Count       = 6;
  pPCFFieldList->Values[0]   = MQIA_OPEN_INPUT_COUNT;
  pPCFFieldList->Values[1]   = MQIA_CLWL_Q_PRIORITY;
  pPCFFieldList->Values[2]   = MQIA_CLWL_USEQ;
  pPCFFieldList->Values[3]   = MQIA_CURRENT_Q_DEPTH;
  pPCFFieldList->Values[4]   = MQCA_Q_NAME;
  pPCFFieldList->Values[5]   = MQCA_CLUSTER_NAME;
  MsgLength                 += pPCFFieldList->StrucLength;

  if (AdminMsgLen < MsgLength)
  {
    WriteToRpt(MSGLENERR, 1, 2, "buildInquireQPCF", AdminMsgLen, MsgLength);
    StopMon = TRUE;
  }

  return MsgLength;
}

/***********************************************************************/
/*                                                                     */
/* Subroutine:  buildChangeQPCF                                        */
/*                                                                     */
/* Build a PCF command message to change a queue's CLWLPRTY and/or     */
/* CLWLUSEQ attribute.                                                 */
/*                                                                     */
/* This "Change Queue" command has up to four parameters:              */
/*   QName                           = String type                     */
/*   QType (local)                   = Integer type                    */
/*   Changed attribute (CLWLPRTY)    = Integer type                    */
/*   Changed attribute (CLWLUSEQ)    = Integer type                    */
/*                                                                     */
/***********************************************************************/
MQLONG buildChangeQPCF(MQBYTE *pAdminMsg,
                       MQLONG  AdminMsgLen,
                       char   *QName,
                       MQLONG  TargetCLWLPRTY,
                       MQLONG  TargetCLWLUSEQ)
{
  MQCFH          *pPCFHeader;    /* Ptr to PCF header structure        */
  MQCFST         *pPCFString;    /* Ptr to 1st string parm in PCF cmd  */
  MQCFIN         *pPCFInteger;  /* Ptr to next integer parm in PCF cmd */
  MQLONG          MsgLength;

  /* Setup request header */
  pPCFHeader                 = (MQCFH *)pAdminMsg;
  pPCFHeader->Type           = MQCFT_COMMAND;
  pPCFHeader->StrucLength    = MQCFH_STRUC_LENGTH;
  pPCFHeader->Version        = MQCFH_VERSION_1;
  pPCFHeader->Command        = MQCMD_CHANGE_Q; /* "Change Queue" */
  pPCFHeader->MsgSeqNumber   = MQCFC_LAST;
  pPCFHeader->Control        = MQCFC_LAST;
  pPCFHeader->ParameterCount = 0; /* increment as we go */
  MsgLength                  = pPCFHeader->StrucLength;

  /* Setup QName parameter block */
  pPCFString  = (MQCFST *)(pAdminMsg + MsgLength);
  pPCFString->Type           = MQCFT_STRING;
  pPCFString->StrucLength    = MQCFST_STRUC_LENGTH_FIXED + MQ_Q_NAME_LENGTH;
  pPCFString->Parameter      = MQCA_Q_NAME;
  pPCFString->CodedCharSetId = MQCCSI_DEFAULT;
  pPCFString->StringLength   = MQ_Q_NAME_LENGTH;
  strncpy( pPCFString->String, QName, MQ_Q_NAME_LENGTH); /* Specify queue */
  MsgLength                 += pPCFString->StrucLength;
  pPCFHeader->ParameterCount++;

  /* Setup QType = Local parameter block */
  pPCFInteger                = (MQCFIN *)(pAdminMsg + MsgLength);
  pPCFInteger->Type          = MQCFT_INTEGER;
  pPCFInteger->StrucLength   = MQCFIN_STRUC_LENGTH;
  pPCFInteger->Parameter     = MQIA_Q_TYPE ;
  pPCFInteger->Value         = MQQT_LOCAL;
  MsgLength                 += pPCFInteger->StrucLength;
  pPCFHeader->ParameterCount++;

  /* Setup CLWLQueuePriority if provided (not -1) */
  if ( TargetCLWLPRTY != -1 )
  {
    /* Move to the next available space in the buffer */
    pPCFInteger              = (MQCFIN *)(pAdminMsg + MsgLength);
    pPCFInteger->Type        = MQCFT_INTEGER;
    pPCFInteger->StrucLength = MQCFIN_STRUC_LENGTH;
    pPCFInteger->Parameter   = MQIA_CLWL_Q_PRIORITY;
    pPCFInteger->Value       = TargetCLWLPRTY;
    MsgLength               += pPCFInteger->StrucLength;
    pPCFHeader->ParameterCount++;
  }

  /* Setup CLWLQueueUseQ if provided (not -1) */
  if ( TargetCLWLUSEQ != -1 )
  {
    /* Move to the next available space in the buffer */
    pPCFInteger              = (MQCFIN *)(pAdminMsg + MsgLength);
    pPCFInteger->Type        = MQCFT_INTEGER;
    pPCFInteger->StrucLength = MQCFIN_STRUC_LENGTH;
    pPCFInteger->Parameter   = MQIA_CLWL_USEQ;
    pPCFInteger->Value       = TargetCLWLUSEQ;
    MsgLength               += pPCFInteger->StrucLength;
    pPCFHeader->ParameterCount++;
  }

  if (AdminMsgLen < MsgLength)
  {
    WriteToRpt(MSGLENERR, 1, 2, "buildChangeQPCF", AdminMsgLen, MsgLength);
    StopMon = TRUE;
  }

  return MsgLength;
}

/***********************************************************************/
/*                                                                     */
/* Subroutine:  buildInquireClusterQPCF                                */
/*                                                                     */
/* Build a PCF command message to inquire on the attributes of cluster */
/* queues within a cluster.                                            */
/*                                                                     */
/* This is comprised of the PCF header followed by 5 parameters:       */
/*     Queue name                                                      */
/*     Queue type = Cluster queue                                      */
/*     Cluster name                                                    */
/*     ClusterInfo = a flag indicating this info is requested          */
/*     The list of queue attibutes required in the responses           */
/*                                                                     */
/***********************************************************************/
MQLONG buildInquireClusterQPCF(MQBYTE *pAdminMsg,
                               MQLONG  AdminMsgLen,
                               char   *QName,
                               char   *QClusterName)
{
  MQCFH          *pPCFHeader;    /* Ptr to PCF header structure        */
  MQCFST         *pPCFString;    /* Ptr to string parm in PCF cmd  */
  MQCFIN         *pPCFInteger;   /* Ptr to integer parm in PCF cmd */
  MQCFIL         *pPCFFieldList; /* Ptr to field list parm in PCF cmd */
  MQLONG          MsgLength;

  /* Setup request header */
  pPCFHeader                 = (MQCFH *)pAdminMsg;
  pPCFHeader->Type           = MQCFT_COMMAND;
  pPCFHeader->StrucLength    = MQCFH_STRUC_LENGTH;
  pPCFHeader->Version        = MQCFH_VERSION_1;
  pPCFHeader->Command        = MQCMD_INQUIRE_Q;
  pPCFHeader->MsgSeqNumber   = MQCFC_LAST;
  pPCFHeader->Control        = MQCFC_LAST;
  pPCFHeader->ParameterCount = 5;
  MsgLength                  = pPCFHeader->StrucLength;

  /* Setup QName parameter block */
  pPCFString                 = (MQCFST *)(pAdminMsg + MsgLength);
  pPCFString->Type           = MQCFT_STRING;
  pPCFString->StrucLength    = MQCFST_STRUC_LENGTH_FIXED + MQ_Q_NAME_LENGTH;
  pPCFString->Parameter      = MQCA_Q_NAME;
  pPCFString->CodedCharSetId = MQCCSI_DEFAULT;
  pPCFString->StringLength   = MQ_Q_NAME_LENGTH;
  strncpy( pPCFString->String, QName, MQ_Q_NAME_LENGTH); /* queue name */
  MsgLength                 += pPCFString->StrucLength;

  /* Setup QType = Cluster parameter block */
  pPCFInteger              = (MQCFIN *)(pAdminMsg + MsgLength);
  pPCFInteger->Type        = MQCFT_INTEGER;
  pPCFInteger->StrucLength = MQCFIN_STRUC_LENGTH;
  pPCFInteger->Parameter   = MQIA_Q_TYPE  ;
  pPCFInteger->Value       = MQQT_CLUSTER; /* queue type = cluster */
  MsgLength               += pPCFInteger->StrucLength;

  /* Setup ClusterName parameter block */
  pPCFString                 = (MQCFST *)(pAdminMsg + MsgLength);
  pPCFString->Type           = MQCFT_STRING;
  pPCFString->StrucLength    = MQCFST_STRUC_LENGTH_FIXED +
                                 MQ_CLUSTER_NAME_LENGTH;
  pPCFString->Parameter      = MQCA_CLUSTER_NAME;
  pPCFString->CodedCharSetId = MQCCSI_DEFAULT;
  pPCFString->StringLength   = MQ_CLUSTER_NAME_LENGTH;
  memcpy( pPCFString->String, QClusterName, MQ_CLUSTER_NAME_LENGTH );
  MsgLength                 += pPCFString->StrucLength;

  /* Setup QType = Cluster parameter block */
  pPCFInteger              = (MQCFIN *)(pAdminMsg + MsgLength);
  pPCFInteger->Type        = MQCFT_INTEGER;
  pPCFInteger->StrucLength = MQCFIN_STRUC_LENGTH;
  pPCFInteger->Parameter   = MQIACF_CLUSTER_INFO; /* ClusterInfo request */
  /* any value will suffice to retrieve cluster Q instances */
  pPCFInteger->Value       = 0;
  MsgLength               += pPCFInteger->StrucLength;

  /* Define the list of queue attributes we're interested in */
  pPCFFieldList              = (MQCFIL *)(pAdminMsg + MsgLength);
  pPCFFieldList->Type        = MQCFT_INTEGER_LIST;
  pPCFFieldList->StrucLength = MQCFIL_STRUC_LENGTH_FIXED +
                                 (sizeof(MQLONG) * 2);
  pPCFFieldList->Parameter   = MQIACF_Q_ATTRS;
  pPCFFieldList->Count       = 2;
  pPCFFieldList->Values[0]   = MQIA_CLWL_Q_PRIORITY;
  pPCFFieldList->Values[1]   = MQCA_CLUSTER_Q_MGR_NAME;
  MsgLength                 += pPCFFieldList->StrucLength;

  if (AdminMsgLen < MsgLength)
  {
    WriteToRpt(MSGLENERR, 1, 2, "buildInquireClusterQPCF", AdminMsgLen, MsgLength);
    StopMon = TRUE;
  }

  return MsgLength;
}

/***********************************************************************/
/*                                                                     */
/* Subroutine:  parseInqQRespPCF                                       */
/*                                                                     */
/* Reads the PCF response message and extracts the attributes returned.*/
/* Calls either subroutine ProcessIntegerParm or ProcessStringParm     */
/* for each attribute, depending on its type.                          */
/* Those subroutines fill in fields of the DefnLQ structure which is   */
/* passed to this function.                                            */
/*                                                                     */
/***********************************************************************/
int parseInqQRespPCF(MQBYTE      *pAdminMsg,
                     LocalQParms *pDefnLQ,
                     MQLONG      *pLastQResponse)
{
  int             success = TRUE;
  MQCFH          *pPCFHeader;    /* Ptr to PCF header structure        */
  MQCFST         *pPCFString;    /* Ptr to 1st string parm in PCF cmd  */
  MQCFIN         *pPCFInteger;   /* Ptr to 1st integer parm in PCF cmd */
  MQLONG         *pPCFType;      /* Ptr to PCF response details        */
  short           Index; /* Loop counter for parsing PCF response data */

  /* Examine PCF Header */
  pPCFHeader = (MQCFH *)pAdminMsg;
  *pLastQResponse = pPCFHeader->Control;

  /* Check for bad PCF RC */
  if(pPCFHeader->CompCode != MQCC_OK)
  {
    /* Log the error unless it is the generic 'PCF command failed'     */
    /* response, in which case a more specific error will already have */
    /* been reported. */
    if( (pPCFHeader->Reason != MQRCCF_COMMAND_FAILED) ||
        (DebugFlag) )
    {
      if (pPCFHeader->Reason == MQRC_UNKNOWN_OBJECT_NAME)
        WriteToRpt( NOQERR, 2, 0, QNameMask, QClusterName);
      else
        WriteToRpt( PCFINQQERR, 2, 2, QNameMask, QClusterName,
                    pPCFHeader->CompCode, pPCFHeader->Reason );
    }

    /* Nothing further to do with this response message. */
    success = FALSE;
  }
  /* Successfully read a queue detail message, now parse it */
  else
  {
    /* Initialize attributes in the DefnLQ structure to null and/or    */
    /* undefined values */
    pDefnLQ->QName[0] = '\0';
    pDefnLQ->QClusterName[0] = '\0';
    pDefnLQ->QClusterQmgrName[0] = '\0';
    pDefnLQ->IPPROCS = -1;
    pDefnLQ->CLWLPRTY = -1;
    pDefnLQ->CLWLUSEQ = -1;
    pDefnLQ->CURDEPTH = -1;

    /* Pointer to each PCF attribute, initially set to the first one. */
    pPCFType = (MQLONG *)(pAdminMsg + MQCFH_STRUC_LENGTH);
    Index = 1;
    /* Loop through all parameters */
    while ( Index <= pPCFHeader->ParameterCount )
    {
      /* Establish the type of each parameter and allocate a pointer   */
      /* of the correct type to reference it. */
      switch ( *pPCFType )
      {
        case MQCFT_INTEGER:
          pPCFInteger = (MQCFIN *)pPCFType;
          ProcessIntegerParm( pPCFInteger, pDefnLQ );
          /* Increment the pointer to the next parameter by the length */
          /* of the current parm. */
          pPCFType = (MQLONG *)( (MQBYTE *)pPCFType +
                                  pPCFInteger->StrucLength );
          break;
        case MQCFT_STRING:
          pPCFString = (MQCFST *)pPCFType;
          ProcessStringParm( pPCFString, pDefnLQ );
          /* Increment the pointer to the next parameter by the length */
          /* of the current parm. */
          pPCFType = (MQLONG *)( (MQBYTE *)pPCFType +
                                            pPCFString->StrucLength );
          break;
        default:
          /* We're not interested in any other type of structure so    */
          /* just jump over it. */

          /* Pretend it's an MQCFIN just to get the StrucLength value */
          pPCFInteger = (MQCFIN *)pPCFType;
          pPCFType = (MQLONG *)( (MQBYTE *)pPCFType +
                                            pPCFInteger->StrucLength );
          break;
      } /* endswitch */

      Index++;
    } /* endwhile */
  }

  return success;
}

/***********************************************************************/
/*                                                                     */
/* Subroutine:  checkForAlternativeQueue                               */
/*                                                                     */
/* Before attempting to move queued messages on a local inactive       */
/* instance of a cluster queue we must first check that there is at    */
/* least one active instance of the queue in the cluster.              */
/*                                                                     */
/* If this check was not made the re-queued messages would simply be   */
/* spread across all inactive instances of the queue as they would all */
/* be as eligible as each other, which would be of no benefit.        */
/*                                                                     */
/* The function checks if there is an alternate active instance of the */
/* queue named in DefnLQ.QName.                                        */
/*                                                                     */
/* This is done by sending a PCF "Inquire Queue" command with          */
/* ClusterInfo specified.  The MQ command server returns a response    */
/* for each instance of the queue in the cluster, along with its       */
/* queue attributes.                                                   */
/*                                                                     */
/* Loop through the responses until an alternate active instance       */
/* is found, meaning:                                                  */
/*     queue has CLWLPRTY > 0                                          */
/*     and queue is on a different queue manager                       */
/***********************************************************************/
unsigned short checkForAlternativeQueue(MQBYTE      *pAdminMsg,
                                        MQLONG      *pAdminMsgLen,
                                        MQHCONN      hConn,
                                        MQHOBJ       hCommandQ,
                                        MQHOBJ       hQLReplyQ,
                                        LocalQParms *pDefnLQ,
                                        MQCHAR      *QClusterName)
{
  MQBYTE24       InqCLQMsgId; /* MQMD msgId from PCF Inq Q w/clus info */
  MQLONG         lastCLQResponse;  /* Flag indicating this is the last */
          /* PCF response for the "Inquire Queue" with clusterinfo cmd */
  unsigned short foundAltInstance = FALSE; /* Alternate instance found */
  MQCFH         *pPCFHeader;            /* Ptr to PCF header structure */
  MQCFST        *pPCFString;      /* Ptr to 1st string parm in PCF cmd */
  MQCFIN        *pPCFInteger;    /* Ptr to 1st integer parm in PCF cmd */
  MQLONG        *pPCFType;              /* Ptr to PCF response details */
  short          Index;  /* Loop counter for parsing PCF response data */
  MQLONG         CompCode;
  MQLONG         Reason;
  MQLONG         PCFMsgLen;

  /*-------------------------------------------------------------------*/
  /* Build "Inquire Queue" command with ClusterInfo requested.         */
  PCFMsgLen = buildInquireClusterQPCF(pAdminMsg,
                                      *pAdminMsgLen,
                                      pDefnLQ->QName,
                                      QClusterName);

  /*-------------------------------------------------------------------*/
  /* Put "Inquire Queue" with ClusterInfo command message to the       */
  /* queue manager's command queue for the command server to process   */
  /* and respond to.                                                   */
  PutPCFMsg(hConn,
            hCommandQ,
            ReplyQ,
            (MQBYTE *)pAdminMsg,
            PCFMsgLen,
            &InqCLQMsgId,
            &CompCode,
            &Reason);

  /* Successfully put PCF msg */
  if (CompCode == MQCC_OK)
  {
    /*-----------------------------------------------------------------*/
    /* Start of DO-WHILE loop to read all response messages from one   */
    /* "Inquire Queue" w/ClusterInfo request.                          */
    /*                                                                 */
    /* There will be one response message per queue matched in the     */
    /* cluster.                                                        */
    /*                                                                 */
    /* The last message will have the Control field of the PCF header  */
    /* set to MQCFC_LAST. All others will be MQCFC_NOT_LAST.           */
    /*                                                                 */
    /* An individual Reply message consists of a header followed by a  */
    /* number a parameters, the exact number, type and order will      */
    /* depend upon the type of request.                                */
    /*                                                                 */
    /* The message is retrieved into a buffer pointed to by pAdminMsg. */
    /* This buffer will grow to accomodate a larger reply if           */
    /* necessary.                                                      */
    /*                                                                 */
    do
    {
      /*---------------------------------------------------------------*/
      /* Set the loop control variable to MQCFC_LAST as the            */
      /* pessimistic default. If the PCF command is successful, it     */
      /* will be reset to the Control value. If MQGET fails for any    */
      /* reason, the loop will terminate & we are done reading queue   */
      /* detail responses from this "inquire queue" cmd.               */
      lastCLQResponse = MQCFC_LAST;

      /* Get the next response from the command server */
      GetPCFMsg(hConn,
                hQLReplyQ,
                &pAdminMsg,
                pAdminMsgLen,
                InqCLQMsgId,
                PCFwaitInterval,
                &CompCode,
                &Reason);

      /* Successfully read a response message */
      if (CompCode == MQCC_OK)
      {
        /* Examine PCF Header */
        pPCFHeader = (MQCFH *)pAdminMsg;
        lastCLQResponse = pPCFHeader->Control;

        /* Check for bad PCF RC indicating this is not a qdetail msg */
        if (pPCFHeader->CompCode != MQCC_OK)
        {
          WriteToRpt( PCFINQCLQERR, 1, 2, pDefnLQ->QName,
                      pPCFHeader->CompCode, pPCFHeader->Reason);
          /* Nothing further to do with this response message. But     */
          /* allow loop to continue checking for more response msgs so */
          /* that they are not left on the reply queue.                */
        }
        /* Successfully read a queue clusterinfo message, now parse it */
        else
        {
          /*-----------------------------------------------------------*/
          /* Parse the response and update DefnLQ.                     */
          /* If we've already found an alternative active queue        */
          /* instance this in unnecessary as we're only looking for   */
          /* at least one alternative, so that's enough information.   */
          /* However, we still need to get every response so that they */
          /* are not left on the reply queue.                          */
          if (!foundAltInstance)
          {
            /* Initialize attributes in the DefnLQ structure to null   */
            /* and/or undefined values */
            pDefnLQ->QName[0] = '\0';
            pDefnLQ->QClusterName[0] = '\0';
            pDefnLQ->QClusterQmgrName[0] = '\0';
            pDefnLQ->IPPROCS = -1;
            pDefnLQ->CLWLPRTY = -1;
            pDefnLQ->CLWLUSEQ = -1;

            /* Pointer to each PCF attribute, initially set to the     */
            /* first one. */
            pPCFType = (MQLONG *)(pAdminMsg + MQCFH_STRUC_LENGTH);
            Index = 1;
            /* Loop through all parameters */
            while ( Index <= pPCFHeader->ParameterCount )
            {
              /* Establish the type of each parameter and allocate a   */
              /* pointer of the correct type to reference it. */
              switch ( *pPCFType )
              {
                case MQCFT_INTEGER:
                  pPCFInteger = (MQCFIN *)pPCFType;
                  ProcessIntegerParm( pPCFInteger, pDefnLQ );
                  /* Increment the pointer to the next parameter by    */
                  /* the length of the current parm. */
                  pPCFType = (MQLONG *)( (MQBYTE *)pPCFType +
                                        pPCFInteger->StrucLength );
                  break;
                case MQCFT_STRING:
                  pPCFString = (MQCFST *)pPCFType;
                  ProcessStringParm( pPCFString, pDefnLQ );
                  /* Increment the pointer to the next parameter by    */
                  /* the length of the current parm. */
                  pPCFType = (MQLONG *)( (MQBYTE *)pPCFType +
                                        pPCFString->StrucLength );
                  break;
                default:
                  /* We're not interested in any other type of         */
                  /*structure so just jump over it. */

                  /* Pretend it's an MQCFIN just to get the            */
                  /* StrucLength value (it's always the second field). */
                  pPCFInteger = (MQCFIN *)pPCFType;
                  pPCFType = (MQLONG *)( (MQBYTE *)pPCFType +
                                        pPCFInteger->StrucLength );
                  break;
              } /* endswitch */

              Index++;
            } /* endwhile */

            /*---------------------------------------------------------*/
            /* Determine whether this newly found Q instance is        */
            /*      (a) active (CLWLPRTY > 0)                          */
            /*  and (b) on a different queue manager                   */
            if ( (pDefnLQ->CLWLPRTY > 0)
                 && (strlen(pDefnLQ->QClusterQmgrName) > 0)
                 && (strncmp(pDefnLQ->QClusterQmgrName,
                             QMgrName,MQ_Q_MGR_NAME_LENGTH) != 0) )
            {
              /* Indicate we have found an alternate instance */
              foundAltInstance = TRUE;
              if (DebugFlag)
              {
                sprintf(dbs,"Found an alternate instance on QMgr=%s\n",
                        pDefnLQ->QClusterQmgrName);
                WriteToRpt(DEBUGMSGD, 1, 0, dbs);
              }
            } /* end if this alt instance is active and on a different qmgr */
          } /* end if !foundAltInstance */
        } /* end else Successfully read a queue clusterinfo message */
      } /* end if successfully read a response message */
      /* end of do while reading "Inquire Queue w/clusterinfo" responses */
    } while ( (lastCLQResponse == MQCFC_NOT_LAST) && !StopMon);

  } /* end if successfully put PCF msg */

  return foundAltInstance;
}

/***********************************************************************/
/*                                                                     */
/* Subroutine:  getCurrentMilliseconds                                 */
/*                                                                     */
/* Return the current time at a millisecond accuracy (where the system */
/* supports it).                                                       */
/*                                                                     */
/***********************************************************************/
long getCurrentMilliseconds()
{
#if (MQAT_DEFAULT == MQAT_WINDOWS_NT)
  return GetTickCount();
#else
 struct timeval curTime;

 gettimeofday(&curTime, NULL );
 return (curTime.tv_sec * 1000 + (curTime.tv_usec / 1000));
#endif
}

