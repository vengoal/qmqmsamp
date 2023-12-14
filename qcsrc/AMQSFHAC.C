const static char sccsid[] = "%Z% %W% %I% %E% %U%";
 /********************************************************************/
 /*                                                                  */
 /* Program name: AMQSFHAC                                           */
 /*                                                                  */
 /* Description: Sample C program that puts and gets messages        */
 /*              under syncpoint using a reconnectable connection.   */
 /*              It checks that no messages are lost or duplicated   */
 /*              even when the queue becomes temporarily unavailable */
 /*              such as during a failover.                          */
 /*                                                                  */
 /*   <copyright                                                     */
 /*   notice="lm-source-program"                                     */
 /*   pids="5724-H72,"                                               */
 /*   years="2010,2016"                                              */
 /*   crc="2484789497" >                                             */
 /*   Licensed Materials - Property of IBM                           */
 /*                                                                  */
 /*   5724-H72,                                                      */
 /*                                                                  */
 /*   (C) Copyright IBM Corp. 2010, 2016 All Rights Reserved.        */
 /*                                                                  */
 /*   US Government Users Restricted Rights - Use, duplication or    */
 /*   disclosure restricted by GSA ADP Schedule Contract with        */
 /*   IBM Corp.                                                      */
 /*   </copyright>                                                   */
 /********************************************************************/
 /*                                                                  */
 /* Function:                                                        */
 /*                                                                  */
 /*   AMQSFHAC is an sample C program that puts and gets persistent  */
 /*   messages under syncpoint. It checks the number of messages it  */
 /*   got back is the same as the number of messages it put.         */
 /*                                                                  */
 /*   It is designed to validate that an MQ multi-instance           */
 /*   queue manager using networked storage, such as a NAS or        */
 /*   a cluster filesystem, maintains data integrity.                */
 /*                                                                  */
 /*   This program automatically reconnects if its connection to the */
 /*   queue manager is broken, as occurs for a multi-instance        */
 /*   queue manager when it fails over. The program checks for       */
 /*   message corruption and missing messages, since these could     */
 /*   indicate a problem with the networked storage. For example,    */
 /*   the program is able to notice situations in which a failover   */
 /*   causes information to be lost from the queue manager's log.    */
 /*                                                                  */
 /*    Program logic:                                                */
 /*                                                                  */
 /*  - uses the MQCNO_RECONNECT_Q_MGR option when connecting to the  */
 /*    queue manager so that it will automatically reconnect when    */
 /*    the queue manager fails over.                                 */
 /*                                                                  */
 /*  - puts and gets a batch of messages under syncpoint.            */
 /*                                                                  */
 /*  - repeats putting and getting the messages over a               */
 /*    variable number of iterations.                                */
 /*                                                                  */
 /*  - tolerates a transaction being rolled back, which might        */
 /*    happen when the queue manager fails over.                     */
 /*                                                                  */
 /*  - maps a reason of MQRC_CALL_INTERRUPTED to either              */
 /*    committed or rolled back when returned from a commit,         */
 /*    which might happen during failover. This is done by           */
 /*    updating a separate message to a side queue inside            */
 /*    the original transaction. If MQRC_CALL_INTERRUPTED            */
 /*    is returned, the message put to the side queue can be         */
 /*    browsed to see if was updated by the previous                 */
 /*    transaction or not.                                           */
 /*                                                                  */
 /*    This logic is slightly unusual because this application has   */
 /*    a special requirement to understand the interaction between   */
 /*    reconnecting to the queue manager and any active transaction. */
 /*                                                                  */
 /********************************************************************/
 /*                                                                  */
 /*   AMQSFHAC has the following parameters                          */
 /*           the queue manager name                                 */
 /*           the name of the queue to put and get messages          */
 /*           the name of the side queue to put and get the side     */
 /*              message                                             */
 /*           the number of messages to put or get inside a single   */
 /*              transaction                                         */
 /*           the number of times the messages will be put and got   */
 /*           either 0 (meaning not verbose), 1 (meaning verbose) or */
 /*              2 (meaning very verbose)                            */
 /*                                                                  */
 /********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmqc.h>

#define TRUE 1
#define FALSE 0

/* MESSAGELEN is the length of each message.                         */
#define MESSAGELEN 5000

/* Constants used with the verbose global variable that specify      */
/* how much information to display.                                  */
#define NOTVERBOSE  0
#define VERBOSE     1
#define VERYVERBOSE 2
static int verbose = NOTVERBOSE;

/*********************************************************************/
/*                                                                   */
/* struct SideInfo is the data about the side message.               */
/*                                                                   */
/* The side message is written to the side queue to allow the        */
/* program to keep track of transaction commit and rollback.         */
/*                                                                   */
/* The side message is updated inside every transaction so that if   */
/* the MQCMIT returns MQRC_CALL_INTERRUPTED, the side message can be */
/* browsed to determine whether the transaction committed or rolled  */
/* back before the MQI connection broke. There is exactly one side   */
/* message written to the side queue, and this message is got by     */
/* message id when it is to be updated. The transaction id in the    */
/* side message is incremented when the side message is updated.     */
/* The latest transaction id is kept in this side information        */
/* structure in a local variable.                                    */
/*                                                                   */
/* After MQCMIT has returned MQRC_CALL_INTERRUPTED, if the           */
/* transaction id in the local variable matches the transaction id   */
/* in the side message on the side queue, the transaction must have  */
/* committed. If they do not match, the transaction must have rolled */
/* back.                                                             */
/*                                                                   */
/*********************************************************************/
struct SideInfo
{
  MQHOBJ hobj;                 /* Side queue object handle   */
  MQLONG tranid;               /* Transaction id             */
  MQBYTE24 msgid;              /* Message id of side message */
};


/*********************************************************************/
/*                                                                   */
/* Function checkReason                                              */
/*    Checks whether an MQI reason code indicates an error           */
/*                                                                   */
/* Parameters                                                        */
/*    MQIcall  (input)  the MQI call that was executed               */
/*    CompCode (input)  the MQI completion code to be checked        */
/*    Reason   (input)  the MQI reason code to be checked            */
/*                                                                   */
/* Returns                                                           */
/*    TRUE if the completion code and reason code indicate an error  */
/*    FALSE if there was no error or it can be ignored               */
/*                                                                   */
/* Outputs                                                           */
/*    An error message is printed for warnings and errors            */
/*                                                                   */
/*********************************************************************/
static int checkReason(char *MQIcall, MQLONG CompCode, MQLONG Reason)
{
  int wasError = FALSE;
  int ignore = FALSE;

  /*******************************************************************/
  /* MQRC_BACKED_OUT is not an error because it can be returned from */
  /* any put, get or commit when the queue manager fails over. It    */
  /* indicates the current transaction was interrupted by a broken   */
  /* connection or could not be committed due to a resource          */
  /* constraint.                                                     */
  /*******************************************************************/
  if (MQRC_BACKED_OUT == Reason)
    ignore = TRUE;

  /*******************************************************************/
  /* MQRC_CALL_INTERRUPTED is not an error when returned from MQCMIT */
  /* because it can be returned when the queue manager fails over.   */
  /*******************************************************************/
  if (!strcmp(MQIcall, "MQCMIT") && (MQRC_CALL_INTERRUPTED == Reason))
    ignore = TRUE;

  /*******************************************************************/
  /* Print an error message for warnings or failures which are not   */
  /* being ignored. In verbose mode, print an informational message. */
  /*******************************************************************/
  if ((MQCC_OK != CompCode) && !ignore)
  {
    printf("%s failed with reason code %ld\n", MQIcall, Reason);
    wasError = TRUE;
  }
  else if (verbose)
    printf("%s rc = %ld\n", MQIcall, Reason);

  return wasError;
}

/*********************************************************************/
/*                                                                   */
/* Function handleBackedOut                                          */
/*    Issues an MQBACK on the connection handle. This is the correct */
/*    recovery action when an MQPUT, MQGET or MQCMIT returns         */
/*    MQRC_BACKED_OUT. The connection handle is then able to begin   */
/*    a new transaction.                                             */
/*                                                                   */
/* Parameters                                                        */
/*    MQIcall   (input)  the MQI call that was executed              */
/*    hconn     (input)  connection handle                               */
/*    pCompCode (output) MQI completion code                         */
/*    pReason   (output) MQI reason code                             */
/*                                                                   */
/* Returns                                                           */
/*    void                                                           */
/*                                                                   */
/* Outputs                                                           */
/*    Prints an informational message if a rollback occurred         */
/*                                                                   */
/*********************************************************************/
static void handleBackedOut(char *MQIcall,
                            MQHCONN hconn,
                            PMQLONG pCompCode,
                            PMQLONG pReason)
{
  MQLONG CompCode;             /* MQI completion code                */
  MQLONG Reason;               /* MQI reason code                    */

  printf("%s indicated transaction backed out\n", MQIcall);
  MQBACK(hconn, &CompCode, &Reason);
  if (MQCC_OK != CompCode)
  {
    checkReason("MQBACK", CompCode, Reason);
  }

  *pCompCode = CompCode;
  *pReason = Reason;
}

/*********************************************************************/
/*                                                                   */
/* Function initSide                                                 */
/*    Initialises the side message. Open the side queue and put the  */
/*    first side message with a transaction id of zero.              */
/*                                                                   */
/* Parameters                                                        */
/*    hconn     (input)  connection handle                           */
/*    sidename  (input)  name of the side queue                      */
/*    pSideinfo (output) the side queue information                  */
/*    pCompCode (output) MQI completion code                         */
/*    pReason   (output) MQI reason code                             */
/*                                                                   */
/* Returns                                                           */
/*    void                                                           */
/*                                                                   */
/*********************************************************************/
static void initSide(MQHCONN          hconn,
                     char            *sidename,
                     struct SideInfo *pSideinfo,
                     PMQLONG          pCompCode,
                     PMQLONG          pReason)
{
  MQOD   od = {MQOD_DEFAULT};  /* side q object descriptor           */
  MQMD   md = {MQMD_DEFAULT};  /* side msg descriptor                */
  MQPMO  pmo = {MQPMO_DEFAULT};/* put message options                */
  MQLONG CompCode;             /* MQI completion code                */
  MQLONG Reason;               /* MQI reason code                    */

  /*******************************************************************/
  /* Open the side queue for putting and getting messages            */
  /*******************************************************************/
  strncpy(od.ObjectName, sidename, MQ_Q_NAME_LENGTH);

  MQOPEN(hconn,
         &od,
         MQOO_FAIL_IF_QUIESCING | MQOO_INPUT_SHARED |
         MQOO_OUTPUT | MQOO_BROWSE,
         &pSideinfo->hobj,
         &CompCode,
         &Reason);

  if (MQCC_OK != CompCode)
  {
    if (checkReason("MQOPEN side", CompCode, Reason))
      goto MOD_EXIT;
  }

  /*******************************************************************/
  /* No need to put the side message under syncpoint here because    */
  /* we're still preparing for the real work.                        */
  /*******************************************************************/
  pmo.Options = MQPMO_NEW_MSG_ID
              | MQPMO_NEW_CORREL_ID
              | MQPMO_FAIL_IF_QUIESCING
              | MQPMO_NO_SYNCPOINT;

  /*******************************************************************/
  /* Initialise the transaction id in the side message               */
  /*******************************************************************/
  pSideinfo->tranid = 0;

  /*******************************************************************/
  /* The side message must be persistent to survive a failover       */
  /*******************************************************************/
  md.Persistence = MQPER_PERSISTENT;

  if (verbose)
    printf("MQPUT tranid=%d\n", pSideinfo->tranid);

  MQPUT(hconn,
        pSideinfo->hobj,
        &md,
        &pmo,
        sizeof(MQLONG),
        &pSideinfo->tranid,
        &CompCode,
        &Reason);

  if (MQCC_OK != CompCode)
  {
    checkReason("MQPUT side", CompCode, Reason);
  }

  /*******************************************************************/
  /* Remember the id of the side message so that it can be got by    */
  /* message id later.                                               */
  /*******************************************************************/
  memcpy(pSideinfo->msgid, md.MsgId, sizeof(MQBYTE24));

MOD_EXIT:
  *pCompCode = CompCode;
  *pReason = Reason;
}

/*********************************************************************/
/*                                                                   */
/* Function termSide                                                 */
/*    Terminates the side message. Removes the side message and      */
/*    closes the side queue.                                         */
/*                                                                   */
/* Parameters                                                        */
/*    hconn     (input)  connection handle                           */
/*    pSideinfo (input)  the side queue information                  */
/*    pCompCode (output) MQI completion code                         */
/*    pReason   (output) MQI reason code                             */
/*                                                                   */
/* Returns                                                           */
/*    void                                                           */
/*                                                                   */
/*********************************************************************/
static void termSide(MQHCONN hconn,
                     struct SideInfo *pSideinfo,
                     PMQLONG pCompCode,
                     PMQLONG pReason)
{
  MQMD   md = {MQMD_DEFAULT};  /* side msg descriptor                */
  MQGMO  gmo = {MQGMO_DEFAULT};/* get message options                */
  MQLONG msglen = 0;           /* side message length                */
  MQLONG CompCode = MQCC_OK;   /* MQI completion code                */
  MQLONG Reason = MQRC_NONE;   /* MQI reason code                    */

  /*******************************************************************/
  /* If the side queue was never opened, there's nothing to do       */
  /*******************************************************************/
  if (MQHO_UNUSABLE_HOBJ == pSideinfo->hobj)
    goto MOD_EXIT;

  /*******************************************************************/
  /* Get the side message destructively by message id. No need to    */
  /* use syncpoint since we don't expect the queue manager to fail   */
  /* over during application termination.                            */
  /*******************************************************************/
  memcpy(md.MsgId, pSideinfo->msgid, sizeof(MQBYTE24));
  gmo.Version = MQGMO_VERSION_2;
  gmo.MatchOptions = MQMO_MATCH_MSG_ID;
  gmo.Options = MQGMO_FAIL_IF_QUIESCING
              | MQGMO_NO_WAIT
              | MQGMO_NO_SYNCPOINT;

  MQGET(hconn,
        pSideinfo->hobj,
        &md,
        &gmo,
        sizeof(MQLONG),
        &pSideinfo->tranid,
        &msglen,
        &CompCode,
        &Reason);

  if (MQCC_OK == CompCode)
  {
    if (verbose)
      printf("MQGET side tranid=%d\n", pSideinfo->tranid);
  }
  else
  {
    checkReason("MQGET side", CompCode, Reason);
  }

  /*******************************************************************/
  /* Close the side queue                                            */
  /*******************************************************************/
  MQCLOSE(hconn, &pSideinfo->hobj, MQCO_NONE, &CompCode, &Reason);
  if (MQCC_OK != CompCode)
  {
    checkReason("MQCLOSE side", CompCode, Reason);
  }

MOD_EXIT:
  *pCompCode = CompCode;
  *pReason = Reason;
}

/*********************************************************************/
/*                                                                   */
/* Function didItBackout                                             */
/*    Discovers whether the previous transaction committed or rolled */
/*    back by inquiring the side message.                            */
/*                                                                   */
/* Parameters                                                        */
/*    hconn     (input)  connection handle                           */
/*    pSideinfo (input)  the side queue information                  */
/*    pCompCode (output) MQI completion code                         */
/*    pReason   (output) MQI reason code                             */
/*                                                                   */
/* Returns                                                           */
/*    TRUE  if the previous transaction rolled back                  */
/*    FALSE if the previous transaction committed                    */
/*                                                                   */
/* Outputs                                                           */
/*    An informational message that indicates whether                */
/*    MQRC_CALL_INTERRUPTED resolves to a commit or a roll back.     */
/*                                                                   */
/*********************************************************************/
static int didItBackout(MQHCONN hconn,
                        struct SideInfo *pSideinfo,
                        PMQLONG pCompCode,
                        PMQLONG pReason)
{
  MQMD   md = {MQMD_DEFAULT};  /* side message descriptor            */
  MQGMO  gmo = {MQGMO_DEFAULT};/* get message options                */
  MQLONG msglen = 0;           /* side message length                */
  MQLONG tranid = 0;           /* transaction id                     */
  MQLONG CompCode;             /* MQI completion code                */
  MQLONG Reason;               /* MQI reason code                    */
  int    backedout = FALSE;    /* return value                       */

  printf("Resolving MQRC_CALL_INTERRUPTED\n");

  /*******************************************************************/
  /* Browse the side message by message id                           */
  /*******************************************************************/
  memcpy(md.MsgId, pSideinfo->msgid, sizeof(MQBYTE24));
  gmo.Version = MQGMO_VERSION_2;
  gmo.MatchOptions = MQMO_MATCH_MSG_ID;
  gmo.Options = MQGMO_FAIL_IF_QUIESCING
              | MQGMO_BROWSE_FIRST
              | MQGMO_NO_WAIT;

  MQGET(hconn,
        pSideinfo->hobj,
        &md,
        &gmo,
        sizeof(MQLONG),
        &tranid,
        &msglen,
        &CompCode,
        &Reason);

  if (MQCC_OK != CompCode)
  {
    checkReason("MQGET browse side", CompCode, Reason);
    goto MOD_EXIT;
  }

  /*******************************************************************/
  /* Compare the transaction id in the side message with the         */
  /* transaction id in the side information. If they are the same,   */
  /* the transaction committed. If they are different, the           */
  /* transaction rolled back.                                        */
  /*******************************************************************/
  printf("MQGET browse side tranid=%d pSideinfo->tranid=%d\n",
         tranid,
         pSideinfo->tranid);

  if (tranid != pSideinfo->tranid)
  {
    printf("Resolving to backed out\n");
    backedout = TRUE;
  }
  else
  {
    printf("Resolving to committed\n");
    backedout = FALSE;
  }

MOD_EXIT:
  *pCompCode = CompCode;
  *pReason = Reason;
  return backedout;
}

/*********************************************************************/
/*                                                                   */
/* Function commit                                                   */
/*    Commits a transaction. This function also updates the side     */
/*    message in the transaction, and handles failure of MQCMIT.     */
/*                                                                   */
/* Parameters                                                        */
/*    hconn     (input) connection handle                            */
/*    pSideinfo (input) the side queue information                   */
/*                                                                   */
/* Returns                                                           */
/*    TRUE  if the previous transaction rolled back                  */
/*    FALSE if the previous transaction committed                    */
/*                                                                   */
/* Outputs                                                           */
/*    If the MQCMIT failed with reason code MQRC_CALL_INTERRUPTED,   */
/*    prints an informational message.                               */
/*                                                                   */
/*********************************************************************/
static int commit(MQHCONN hconn,
                  struct SideInfo *pSideinfo,
                  PMQLONG pCompCode,
                  PMQLONG pReason)
{
  MQMD   md = {MQMD_DEFAULT};  /* side message descriptor            */
  MQGMO  gmo = {MQGMO_DEFAULT};/* get message options                */
  MQPMO  pmo = {MQPMO_DEFAULT};/* put message options                */
  MQLONG msglen = 0;           /* side message length                */
  MQLONG CompCode;             /* MQI completion code                */
  MQLONG Reason;               /* MQI reason code                    */
  int    backedout = FALSE;    /* return value                       */

  /*******************************************************************/
  /* Get the side message in the caller's transaction by message id. */
  /*******************************************************************/
  memcpy(md.MsgId, pSideinfo->msgid, sizeof(MQBYTE24));
  gmo.Version = MQGMO_VERSION_2;
  gmo.MatchOptions = MQMO_MATCH_MSG_ID;

  gmo.Options = MQGMO_FAIL_IF_QUIESCING
              | MQGMO_NO_WAIT
              | MQGMO_SYNCPOINT;

  MQGET(hconn,
        pSideinfo->hobj,
        &md,
        &gmo,
        sizeof(MQLONG),
        &pSideinfo->tranid,
        &msglen,
        &CompCode,
        &Reason);

  if (MQCC_OK != CompCode)
  {
    if (checkReason("MQGET side", CompCode, Reason))
      goto MOD_EXIT;
  }

  if (MQRC_BACKED_OUT == Reason)
  {
    handleBackedOut("MQGET side", hconn, &CompCode, &Reason);
    backedout = TRUE;
    goto MOD_EXIT;
  }

  if (verbose)
    printf("MQGET side tranid=%d\n", pSideinfo->tranid);

  /*******************************************************************/
  /* Increment the transaction id and put the side message also in   */
  /* the caller's transaction. This is done so if the transaction    */
  /* backs out, the replacement of the side message will also back   */
  /* out.                                                            */
  /*******************************************************************/
  pSideinfo->tranid = pSideinfo->tranid + 1;
  if (verbose)
    printf("MQPUT side tranid=%d\n", pSideinfo->tranid);

  pmo.Options = MQPMO_FAIL_IF_QUIESCING
              | MQPMO_SYNCPOINT;

  MQPUT(hconn,
        pSideinfo->hobj,
        &md,
        &pmo,
        sizeof(MQLONG),
        &pSideinfo->tranid,
        &CompCode,
        &Reason);

  if (MQCC_OK != CompCode)
  {
    if (checkReason("MQPUT side", CompCode, Reason))
      goto MOD_EXIT;
  }

  if (MQRC_BACKED_OUT == Reason)
  {
    handleBackedOut("MQPUT side", hconn, &CompCode, &Reason);
    backedout = TRUE;
    goto MOD_EXIT;
  }

  /*******************************************************************/
  /* Commit the transaction                                          */
  /*******************************************************************/
  MQCMIT(hconn, &CompCode, &Reason);
  if (MQCC_OK != CompCode)
  {
    if (checkReason("MQCMIT", CompCode, Reason))
      goto MOD_EXIT;
  }

  /*******************************************************************/
  /* MQRC_CALL_INTERRUPTED means that the transaction either         */
  /* committed or backed out but we don't yet know which. Use the    */
  /* side message to find out what happened.                         */
  /*******************************************************************/
  if (MQRC_CALL_INTERRUPTED == Reason)
  {
    backedout = didItBackout(hconn, pSideinfo, &CompCode, &Reason);
    if (MQCC_OK != CompCode)
      goto MOD_EXIT;

    if (backedout)
      Reason = MQRC_BACKED_OUT;
  }

  /*******************************************************************/
  /* Call handleBackedOut here to call MQBACK in accordance with the */
  /* MQI rules.                                                      */
  /*******************************************************************/
  if (MQRC_BACKED_OUT == Reason)
  {
    handleBackedOut("MQCMIT", hconn, &CompCode, &Reason);
  }

MOD_EXIT:
  *pCompCode = CompCode;
  *pReason = Reason;
  return backedout;
}

/*********************************************************************/
/*                                                                   */
/* Main program                                                      */
/*                                                                   */
/*********************************************************************/
int main( int argc, char** argv )
{
  MQHCONN  hconn = MQHC_UNUSABLE_HCONN;  /* connection handle        */
  MQHOBJ   hobj = MQHO_UNUSABLE_HOBJ;    /* object handle            */
  MQCNO    cno = {MQCNO_DEFAULT};   /* connection opts               */
  MQOD     od = {MQOD_DEFAULT};     /* object Descriptor             */
  MQMD     md = {MQMD_DEFAULT};     /* message descriptor            */
  MQPMO    pmo = {MQPMO_DEFAULT};   /* put message options           */
  MQGMO    gmo = {MQGMO_DEFAULT};   /* get message options           */
  MQLONG   msglen;                  /* message length                */
  char     buffer[MESSAGELEN+1];    /* message buffer                */
  char     qmname[MQ_Q_MGR_NAME_LENGTH]; /* q manager name           */
  char     qname[MQ_Q_NAME_LENGTH];      /* queue name               */
  char     sidename[MQ_Q_NAME_LENGTH];   /* side queue name          */
  int      transize;                /* transaction size              */
  int      iterations;              /* number of iterations          */
  MQLONG   buflen = 0;              /* message buffer length         */
  char     alphabet[]="abcdefghijklmnopqrstuvwxyz";
  char     msgdata[MESSAGELEN+1];   /* message payload               */
  int      nmsg;                    /* message counter               */
  int      it;                      /* iteration counter             */
  int      backedout = FALSE;       /* rollback flag                 */
  struct SideInfo sideinfo;         /* side queue information        */
  MQLONG   CompCode;                /* MQI completion code           */
  MQLONG   Reason;                  /* MQI reason code               */

  printf("Sample AMQSFHAC start\n");
  if (argc < 7)
  {
    printf("Usage: %s qmname qname sidename transize iterations verbose\n",
           argv[0]);
    goto MOD_EXIT;
  }

  /*******************************************************************/
  /* Copy the command line arguments into local variables            */
  /*******************************************************************/
  strncpy(qmname, argv[1], MQ_Q_MGR_NAME_LENGTH);
  strncpy(qname, argv[2], MQ_Q_NAME_LENGTH);
  strncpy(sidename, argv[3], MQ_Q_NAME_LENGTH);
  transize = atoi(argv[4]);
  iterations = atoi(argv[5]);
  verbose = atoi(argv[6]);
  printf("qmname = %s\n", qmname);
  printf("qname = %s\n", qname);
  printf("sidename = %s\n", sidename);
  printf("transize = %d\n", transize);
  printf("iterations = %d\n", iterations);
  printf("verbose = %d\n", verbose);

  /*******************************************************************/
  /* Every message put to the queue is the same length and contains  */
  /* the same information. Initialise a copy of the message payload  */
  /* here so we can copy it into each message and compare it with    */
  /* each message we get back.                                       */
  /*******************************************************************/
  memset(buffer, 0, MESSAGELEN+1);
  memset(msgdata, 0, MESSAGELEN+1);
  for (nmsg = 0; nmsg < MESSAGELEN; nmsg++)
    msgdata[nmsg] = alphabet[nmsg%strlen(alphabet)];

  /*******************************************************************/
  /* Initialise the side information                                 */
  /*******************************************************************/
  memset(&sideinfo, 0, sizeof(struct SideInfo));
  sideinfo.hobj = MQHO_UNUSABLE_HOBJ;

  /*******************************************************************/
  /* Connect to the queue manager specifying reconnection to the     */
  /* same queue manager.                                             */
  /*******************************************************************/
  cno.Version = MQCNO_VERSION_5;
  cno.Options = MQCNO_RECONNECT_Q_MGR;
  MQCONNX(qmname, &cno, &hconn, &CompCode, &Reason);

  if (MQCC_OK != CompCode)
  {
    if (checkReason("MQCONNX", CompCode, Reason))
      goto MOD_EXIT;
  }

  /*******************************************************************/
  /* Initialise the side message here                                */
  /*******************************************************************/
  initSide(hconn, sidename, &sideinfo, &CompCode, &Reason);
  if (MQCC_OK != CompCode)
    goto MOD_EXIT;

  /*******************************************************************/
  /* Open the queue that we will put and get messages to             */
  /*******************************************************************/
  strncpy(od.ObjectName, qname, MQ_Q_NAME_LENGTH);
  MQOPEN(hconn,
         &od,
         MQOO_FAIL_IF_QUIESCING |
         MQOO_INPUT_EXCLUSIVE |
         MQOO_OUTPUT,
         &hobj,
         &CompCode,
         &Reason);

  if (MQCC_OK != CompCode)
  {
    if (checkReason("MQOPEN", CompCode, Reason))
      goto MOD_EXIT;
  }

  /*******************************************************************/
  /* Messages are put and got under syncpoint.                       */
  /*******************************************************************/
  pmo.Options = MQPMO_FAIL_IF_QUIESCING | MQPMO_SYNCPOINT;
  gmo.Options = MQGMO_FAIL_IF_QUIESCING | MQGMO_NO_WAIT | MQGMO_SYNCPOINT;

  /*******************************************************************/
  /* Loop round iterations times putting and getting a batch         */
  /* of messages to the queue                                        */
  /*******************************************************************/
  for (it = 0; it < iterations; it++)
  {
    printf("Iteration %d\n", it);
    backedout = FALSE;

    /*****************************************************************/
    /* Put a batch of persistent messages to the queue under         */
    /* syncpoint. If a rollback happens in the middle of the batch,  */
    /* stop putting messages since the transaction can accept no     */
    /* more work.                                                    */
    /*****************************************************************/
    for (nmsg = 0; (nmsg < transize) && !backedout; nmsg++)
    {
      if (verbose)
        printf("Put Message %d\n", nmsg);

      msglen = MESSAGELEN;
      memcpy(buffer, msgdata, MESSAGELEN);

      if (VERYVERBOSE == verbose)
        printf("MQPUT msglen=%d buffer = %s\n", msglen, buffer);

      md.Persistence = MQPER_PERSISTENT;

      MQPUT(hconn,
            hobj,
            &md,
            &pmo,
            msglen,
            buffer,
            &CompCode,
            &Reason);

      if (MQCC_OK != CompCode)
      {
        if (checkReason("MQPUT", CompCode, Reason))
          goto MOD_EXIT;
      }

      /***************************************************************/
      /* handleBackedOut will do nothing if the transaction did not  */
      /* roll back                                                   */
      /***************************************************************/
      if (MQRC_BACKED_OUT == Reason)
      {
        backedout = TRUE;
        handleBackedOut("MQPUT", hconn, &CompCode, &Reason);
        if (MQCC_OK != CompCode)
          goto MOD_EXIT;
      }
    }

    /*****************************************************************/
    /* If the transaction backed out already, presumably because the */
    /* queue manager failed over, ignore the batch because it should */
    /* have all been rolled back. Check that there are no messages   */
    /* left on any of the queues at the end of the test because that */
    /* would indicate a partial transaction being rolled back here.  */
    /* Immediately continue with the next iteration because there's  */
    /* nothing to commit or get here.                                */
    /*****************************************************************/
    if (backedout)
      continue;

    /*****************************************************************/
    /* Commit the transaction, but if it rolls back continue with    */
    /* the next iteration because there'll be nothing to get.        */
    /*****************************************************************/
    backedout = commit(hconn, &sideinfo, &CompCode, &Reason);
    if (MQCC_OK != CompCode)
      goto MOD_EXIT;

    if (backedout)
      continue;

    /*****************************************************************/
    /* Get the batch of messages from the queue under syncpoint.     */
    /* If a rollback happens in the middle of the batch, get all the */
    /* messages in a new transaction, otherwise they'll be messages  */
    /* left on the queue at the end of the test.                     */
    /*****************************************************************/
    do
    {
      backedout = FALSE;
      for (nmsg = 0; (nmsg < transize) && !backedout; nmsg++)
      {
        if (verbose)
          printf("Get Message %d\n", nmsg);

        memset(buffer, 0, MESSAGELEN);
        MQGET(hconn,
              hobj,
              &md,
              &gmo,
              MESSAGELEN,
              buffer,
              &msglen,
              &CompCode,
              &Reason);

        if (MQCC_OK != CompCode)
        {
          if (checkReason("MQGET", CompCode, Reason))
            goto MOD_EXIT;
        }

        if (MQRC_BACKED_OUT == Reason)
        {
          backedout = TRUE;
          handleBackedOut("MQGET", hconn, &CompCode, &Reason);
          if (MQCC_OK != CompCode)
            goto MOD_EXIT;
        }

        /*************************************************************/
        /* If this message did not back out check that the length and*/
        /* payload of the message is correct and that no corruption  */
        /* has occurred that might imply a filesystem issue.         */
        /*************************************************************/
        if (!backedout)
        {
          if (verbose)
            printf("MQGET msglen = %d strlen(buffer) = %d\n",
                   msglen, strlen(buffer));

          if (VERYVERBOSE == verbose)
            printf("MQGET buffer = %s\n", buffer);

          if (msglen != MESSAGELEN)
            printf("ERROR bad msglen %d, expected %d\n",
                   msglen,
                   MESSAGELEN);

          if (memcmp(buffer, msgdata, MESSAGELEN))
          {
            printf("ERROR bad buffer msglen = %d\n", msglen);
            printf("Expected %s\n", msgdata);
            printf("Received %s\n", buffer);
          }
        }
      } /* end for each message in batch */

      /***************************************************************/
      /* Only commit if there's something to commit                  */
      /***************************************************************/
      if (!backedout)
        backedout = commit(hconn, &sideinfo, &CompCode, &Reason);

    } while (backedout);

  } /* end for each iteration */

  /*******************************************************************/
  /* Close the queue                                                 */
  /*******************************************************************/
  MQCLOSE(hconn, &hobj, MQCO_NONE, &CompCode, &Reason);
  if (MQCC_OK != CompCode)
  {
    checkReason("MQCLOSE", CompCode, Reason);
  }

  /*******************************************************************/
  /* Remove the side message and close the side queue                */
  /*******************************************************************/
  termSide(hconn, &sideinfo, &CompCode, &Reason);

MOD_EXIT:
  /*******************************************************************/
  /* Disconnect from the queue manager                               */
  /*******************************************************************/
  if (hconn != MQHC_UNUSABLE_HCONN)
  {
    MQDISC(&hconn, &CompCode, &Reason);
    if (MQCC_OK != CompCode)
    {
      checkReason("MQDISC", CompCode, Reason);
    }
  }

  /*******************************************************************/
  /*                                                                 */
  /* END OF AMQSFHAC                                                 */
  /*                                                                 */
  /*******************************************************************/
  printf("Sample AMQSFHAC end\n");
  return 0;
}
