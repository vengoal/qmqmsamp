/* %Z% %W% %I% %E% %U% */
 /********************************************************************/
 /*                                                                  */
 /* Program name: AMQSEVT                                            */
 /*                                                                  */
 /* Description: Sample C program that gets messages from            */
 /*              event queues using MQCB and then formats them       */
 /*   <copyright                                                     */
 /*   notice="lm-source-program"                                     */
 /*   pids="5724-H72"                                                */
 /*   years="2007,2019"                                              */
 /*   crc="706078058" >                                              */
 /*   Licensed Materials - Property of IBM                           */
 /*                                                                  */
 /*   5724-H72                                                       */
 /*                                                                  */
 /*   (C) Copyright IBM Corp. 2007, 2019 All Rights Reserved.        */
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
 /*   AMQSEVT is a sample C program to format event messages on a    */
 /*   queue, and is also an example of MQCB.                         */
 /*                                                                  */
 /*   It is intended for the events on the SYSTEM.ADMIN.*.EVENT      */
 /*   queues. Other sample programs are provided to deal with        */
 /*   messages on the ACCOUNTING, STATISTICS and ACTIVITY queues.    */
 /*                                                                  */
 /*      -- sample reads from message queues named in the parameters */
 /*         and subscribes to topics using managed destinations      */
 /*         If no queue or topic names are given, a default set of   */
 /*         event queues is used.                                    */
 /*                                                                  */
 /*      -- displays the contents of each event                      */
 /*                                                                  */
 /*         messages are browsed or removed from the queue           */
 /*                                                                  */
 /*      -- writes a message for each MQI reason other than          */
 /*         MQRC_NONE; stops if there is a MQI completion code       */
 /*         of MQCC_FAILED                                           */
 /*                                                                  */
 /*                                                                  */
 /*   Program logic:                                                 */
 /*      Take name of input queues/topics from the parameter list    */
 /*      MQOPEN queues for INPUT or MQSUB for topics                 */
 /*      MQCB   register a callback function to receive messages     */
 /*      MQCTL  start consumption of messages                        */
 /*      wait for user to press enter                                */
 /*      MQCTL  stop consumptions of messages                        */
 /*      MQCLOSE the subject objects                                 */
 /*      MQDISC  disconnect from queue manager                       */
 /*                                                                  */
 /*  When compiling from source, this program must be built to       */
 /*  support multi-thread behaviour. On platforms that provide       */
 /*  separate MQI libraries, link with libmqm_r and NOT libmqm.      */
 /*                                                                  */
 /********************************************************************/
 /*                                                                  */
 /*   AMQSEVT  has the following options                             */
 /*     -m <Queue Manager Name>                                      */
 /*     -b Browse messages instead of reading destructively          */
 /*     -c Connect as client                                         */
 /*     -d Print definitions without formatting (as in cmq*.h)       */
 /*     -w Wait interval (seconds)                                   */
 /*     -r <Reconnect Type>                                          */
 /*        d Reconnect Disabled                                      */
 /*        r Reconnect                                               */
 /*        m Reconnect Queue Manager                                 */
 /*     -u userid                                                    */
 /*     -q queue name (can be multiple named)                        */
 /*     -t topic string (can be multiple named)                      */
 /*  If no queues or topics are named, a default set of              */
 /*  event queues are used.                                          */
 /*                                                                  */
 /*  Note: Although this program is primarily aimed at formatting    */
 /*  the standard events, it will also do a basic level of format    */
 /*  for Accounting, Statistics and Activity Trace reports if it     */
 /*  is directed to use those queues.                                */
 /********************************************************************/
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <ctype.h>

 #include <cmqc.h>
 #include <cmqcfc.h>

#if MQAT_DEFAULT == MQAT_WINDOWS_NT
 #include <windows.h>
 #include <conio.h>
 #define INT64FMTSPEC "%I64d"
#else
 #include <unistd.h>
 #include <errno.h>
 #include <sys/types.h>
 #include <sys/time.h>
 #define INT64FMTSPEC "%lld"
#endif

/********************************************************************/
/* Include file containing all the functions that map to strings    */
/********************************************************************/
 #include <cmqstrc.h>

/********************************************************************/
/* Common definitions                                               */
/********************************************************************/
#if !defined(FALSE)
#define FALSE (0)
#endif

#if !defined(TRUE)
#define TRUE (1)
#endif

/********************************************************************/
/* prototype the internal functions                                 */
/********************************************************************/
static char *lookup(MQLONG val,char *map(MQLONG),char *buf,int buflen);

static void printLine(int,char *,char *);
static void printLineNN(int offset, char *attr, char *val,size_t vallen);

static char *formatConstant(char *);
static char *formatConstantBase(char *,MQBOOL);
static char *formatHex(PMQBYTE data,char *buf,int datalen);
static char *formatOperator(MQLONG op);
static char *formatOpenOptions(MQLONG v);
static char *formatCloseOptions(MQLONG v);
static char *formatSubOptions(MQLONG v);
static char *formatMQRC(MQLONG);
static MQBOOL  formatEvent(PMQMD pMsgDesc,MQLONG Length,PMQBYTE Buffer);
static void Usage(void);

/********************************************************************/
/* Not all platforms have getopt, so use our own version. Prefix    */
/* the standard names with "mq" to keep distinct.                   */
/********************************************************************/
static int   mqgetopt(int, char **, char *);
static int   mqoptind = 1;                    /* getopt index         */
static int   mqoptopt;                        /* getopt option        */
static char* mqoptarg;                        /* getopt argument      */

/********************************************************************/
/* global variables                                                 */
/********************************************************************/
char *blank64 =
  "                                                                ";
char workBuf[1024];   /* used for temporary storage                     */
char valbuf[1024*11]; /* More than big enough for any attribute's value */
                      /* Biggest MQ definitions are topic strings - 10K */
char printbuf[1024*11]; /* Formatted version of the value               */

MQHCONN  Hcon = MQHC_UNUSABLE_HCONN;  /* connection handle   */

MQBOOL Reconnectable = FALSE;         /* Command line options */
MQLONG WaitInterval = MQWI_UNLIMITED;
MQBOOL Unformatted = FALSE;
MQBOOL ClientConnection = FALSE;

char    *UserId = NULL;            /* UserId for authentication     */
char     Password[MQ_CSP_PASSWORD_LENGTH + 1] = {0};   /* For auth  */
MQLONG MessageNumber = 1;
volatile MQBOOL EndProgram = FALSE; /* End the program */

#define MAX_FORMAT_DATA_LEN (40)  /* Max bytes to print of message data */


#define MAX_OBJECTS  10
struct  {
  MQHOBJ   Hobj;
  MQLONG   ObjectType;
  char *   ObjectName;
  MQHOBJ   Hsub;
} OpenObjects[MAX_OBJECTS] = {{MQHO_UNUSABLE_HOBJ,MQOT_Q,NULL,MQHO_UNUSABLE_HOBJ}};


static char *DefaultQueues[] =
   {"SYSTEM.ADMIN.PERFM.EVENT",
    "SYSTEM.ADMIN.CHANNEL.EVENT",
    "SYSTEM.ADMIN.QMGR.EVENT",
    "SYSTEM.ADMIN.LOGGER.EVENT",
    "SYSTEM.ADMIN.PUBSUB.EVENT",
    "SYSTEM.ADMIN.CONFIG.EVENT",
    "SYSTEM.ADMIN.COMMAND.EVENT"};

#if MQAT_DEFAULT == MQAT_WINDOWS_NT
#define snprintf _snprintf
void WaitForEnd(void)
{
  do
  {
    int c;
    /* Wait for keyboard input */
    while (!_kbhit())
    {
      if(EndProgram == TRUE)
      {
        return;
      }
      /* Wait for 0.5 seconds */
      Sleep(500);
    }
    c = _getch();
    if ((c == '\n') || (c == '\r'))
    {
      EndProgram = TRUE;
      return;
    }
  } while(EndProgram == FALSE);
  return;
}
#else
void WaitForEnd(void)
{
  struct timeval tv;
  fd_set fd;

  do
  {
    /****************************************************************/
    /* If stdin is not a terminal then do not attempt to read input */
    /****************************************************************/
#if MQAT_DEFAULT == MQAT_UNIX
    if(!isatty(STDIN_FILENO))
    {
      sleep(5);
    }
    else
#endif
    {
      /* Wait for 0.5 seconds */
      tv.tv_sec = 0;
      tv.tv_usec = 500;

      FD_ZERO(&fd);
      FD_SET(STDIN_FILENO, &fd);
      if ( (select(STDIN_FILENO+1, &fd, NULL, NULL, &tv) < 0) && (errno != ETIMEDOUT) )
      {
        EndProgram = TRUE;
        return;
      }

      /* If data is available to read from stdin, read it */
      if (FD_ISSET(STDIN_FILENO, &fd))
      {
        int c = getc(stdin);
        if ( (c == '\n') || (c == '\r') )
        {
          EndProgram = TRUE;
          return;
        }
      }
    }
  } while(EndProgram == FALSE);
  return;
}
#endif


/********************************************************************/
/* FUNCTION: MessageConsumer                                        */
/* PURPOSE : Callback function called when messages arrive          */
/********************************************************************/
void MessageConsumer(MQHCONN   hConn,
                     MQMD    * pMsgDesc,
                     MQGMO   * pGetMsgOpts,
                     MQBYTE  * Buffer,
                     MQCBC   * pContext)
{
  MQLONG i;
  MQLONG Length;
  char *oName = NULL;
  char *oType;

  switch(pContext->CallType)
  {
    case MQCBCT_MSG_REMOVED:
    case MQCBCT_MSG_NOT_REMOVED:
      Length = pGetMsgOpts -> ReturnedLength;

      printf("\n");
      for (i=0;i<MAX_OBJECTS;i++) /* Which queue did the message come from */
      {
        if (OpenObjects[i].Hobj == pContext->Hobj)
        {
           oName = OpenObjects[i].ObjectName;
           oType = (OpenObjects[i].ObjectType==MQOT_TOPIC)?"Topic":"Queue";
           break;
        }
      }

      if (pContext->Reason != 0)
        printf("**** Message #%d (%d Bytes) on %s %s Reason = %d ****\n",
               MessageNumber++,Length,oType,oName?oName:"Unknown",pContext->Reason);
      else
        printf("**** Message #%d (%d Bytes) on %s %s ****\n",
               MessageNumber++,Length,oType,oName?oName:"Unknown");

      /***********************************************************/
      /* Print out the event. If it is not an EVENT, then show   */
      /* some of the message data. But do not go overboard with  */
      /* the formatting.                                         */
      /***********************************************************/
      if (formatEvent(pMsgDesc,Length,Buffer))
      {
        printf("  Message format %.8s:\n",pMsgDesc->Format);
        for (i=0; i<Length && i<MAX_FORMAT_DATA_LEN ; i++)
        {
           if (isprint(Buffer[i])) fputc(Buffer[i],stdout);
           else fputc('.',stdout);
        }
        fputc('\n',stdout);
        if (i < Length)
          printf("......plus %d bytes.\n",Length-i);

      }
      break;

    case MQCBCT_EVENT_CALL:
      printf("\n");
      printf("**** Event Call Reason = %s [%d] ****\n",
          formatMQRC(pContext->Reason),
          pContext->Reason);
      if ( (pContext->Reason == MQRC_OBJECT_CHANGED) ||
           (pContext->Reason == MQRC_CONNECTION_BROKEN) ||
           (pContext->Reason == MQRC_Q_MGR_STOPPING) ||
           (pContext->Reason == MQRC_Q_MGR_QUIESCING) ||
           (pContext->Reason == MQRC_CONNECTION_QUIESCING) ||
           (pContext->Reason == MQRC_CONNECTION_STOPPING) ||
           (pContext->Reason == MQRC_NO_MSG_AVAILABLE))
      {
        printf("Ending consumer.\n");
        EndProgram = TRUE;
      }
      break;

    default:
      printf("\n");
      printf("**** Unexpected CallType = %d\n ****",pContext->CallType);
      break;
  }

}

/********************************************************************/
/* FUNCTION: main                                                   */
/* PURPOSE : Main program entry point                               */
/********************************************************************/
int main(int argc, char **argv)
{

  /*   Declare MQI structures needed                                */
  MQCNO   cno = {MQCNO_DEFAULT};                         /* Connect Options*/
  MQOD     od = {MQOD_DEFAULT};                        /* Object Descriptor*/
  MQOD   qmod = {MQOD_DEFAULT};                        /* Object Descriptor*/
  MQMD     md = {MQMD_DEFAULT};                       /* Message Descriptor*/
  MQSD     sd = {MQSD_DEFAULT};                  /* Subscription Descriptor*/
  MQGMO   gmo = {MQGMO_DEFAULT};                     /* get message options*/
  MQCBD   cbd = {MQCBD_DEFAULT};                     /* Callback Descriptor*/
  MQCTLO  ctlo= {MQCTLO_DEFAULT};                        /* Control Options*/
  MQCSP   csp = {MQCSP_DEFAULT};                     /* Security Parameters*/
  MQHOBJ  QMHObj;                      /* Queue manager object handle      */

  MQLONG   O_options;                  /* MQOPEN options                   */
  MQLONG   QMO_options;                /* MQOPEN options                   */
  MQLONG   CompCode;                   /* completion code                  */
  MQLONG   OpenCode;                   /* MQOPEN/MQSUB completion code     */
  MQLONG   Reason = 999;               /* reason code                      */
  MQLONG   CReason;                /* reason code for MQCONN        */
  char     QMName[50] = "";        /* queue manager name            */
  MQLONG   Platform = MQPL_NATIVE;
  MQLONG   Selectors[1] = {MQIA_PLATFORM};
  MQLONG   Attrs[1] = {0};

  MQBOOL   UsingDefaultQueues = FALSE;
  MQBOOL   Browse = FALSE;
  MQBOOL   error = FALSE;
  int      ObjCount = 0;
  int      ObjIndex;
  int      c;
  int i;

  printf("Sample AMQSEVT start\n\n");

  O_options = MQOO_INPUT_AS_Q_DEF      /* open queue for input      */
            | MQOO_FAIL_IF_QUIESCING;  /* but not if Qmgr stopping  */

  /******************************************************************/
  /* Parse the parameters                                           */
  /******************************************************************/
  while((c = mqgetopt(argc, argv, "bcdm:q:r:t:u:w:")) != EOF)
  {
    switch(c)
    {
      case 'b':
        Browse = TRUE;
        break;

      case 'c':
        cno.Options |= MQCNO_CLIENT_BINDING;
        ClientConnection = TRUE;
        break;

      case 'd':
        Unformatted = TRUE;
        break;

      case 'm':
        strncpy(QMName, mqoptarg, MQ_Q_MGR_NAME_LENGTH);
        break;

      case 'r':
        if (mqoptarg && (strlen(mqoptarg)==1))
        {
          switch (mqoptarg[0])
          {
            case 'd':
              cno.Options |= MQCNO_RECONNECT_DISABLED;
              break;
            case 'm':
              cno.Options |= MQCNO_RECONNECT_Q_MGR;
              Reconnectable = TRUE;
              break;
            case 'r':
              cno.Options |= MQCNO_RECONNECT;
              Reconnectable = TRUE;
              break;
            default:
              error = TRUE;
              break;
            }
        }
        else
        {
          error = TRUE;
        }
        break;

      case 't':
      case 'q':
        OpenObjects[ObjCount].ObjectName = mqoptarg;
        OpenObjects[ObjCount].ObjectType = (c == 't')?MQOT_TOPIC:MQOT_Q;
        ObjCount++;
        if (ObjCount == MAX_OBJECTS)
        {
          printf("Maximum of %d objects is supported\n", MAX_OBJECTS);
          error = TRUE;
        }
        break;

      case 'u':
        UserId = mqoptarg;
        cno.SecurityParmsPtr = &csp;
        cno.Version = MQCNO_VERSION_5;
        csp.AuthenticationType = MQCSP_AUTH_USER_ID_AND_PWD;
        csp.CSPUserIdPtr = UserId;
        csp.CSPUserIdLength = (MQLONG)strlen(UserId);

        /****************************************************************/
        /* Get the password.                                            */
        /* On Unix, there's a convenient function to call when stdin    */
        /* is a real tty. For other platforms, or                       */
        /* if that doesn't work, use a very simple mechanism which does */
        /* not turn off echoing or replace characters with '*'.         */
        /* We don't want to clutter a sample with too much extraneous   */
        /* platform-specific code.                                      */
        /****************************************************************/
        sprintf(workBuf,"Enter password for %s: ",UserId);
#if MQAT_DEFAULT == MQAT_UNIX
        if (isatty(0))
        {
          char *c;
          c = getpass(workBuf);
          if (c)
            strncpy(Password,c,sizeof(Password)-1);
        }
#endif
        if (strlen(Password)==0)
        {
          printf(workBuf);
          fgets(Password,sizeof(Password)-1,stdin);
        }
        if (strlen(Password) > 0 && Password[strlen(Password) - 1] == '\n')
          Password[strlen(Password) -1] = 0;
        csp.CSPPasswordPtr = Password;
        csp.CSPPasswordLength =(MQLONG) strlen(csp.CSPPasswordPtr);
        break;

      case 'w':
        WaitInterval = atoi(mqoptarg) * 1000; /* Convert seconds to millisec */
        break;

      default:
        error = TRUE;
        break;
    }
  }

  /******************************************************************/
  /* Were there any problems parsing the parameters.                */
  /* Any remaining parameters are taken as an error                 */
  /******************************************************************/
  if (error || mqoptind<argc)
  {
    Usage();
    goto MOD_EXIT;
  }


  /******************************************************************/
  /*                                                                */
  /*   Connect to queue manager                                     */
  /*                                                                */
  /******************************************************************/
  MQCONNX(QMName,                   /* queue manager                */
            &cno,                   /* connect options              */
            &Hcon,                  /* connection handle            */
            &CompCode,              /* completion code              */
            &CReason);              /* reason code                  */
                         /* report reason and stop if it failed     */

  memset(Password,' ',sizeof(Password)); /* Clear the password      */

  if (CompCode == MQCC_FAILED)
  {
    printf("MQCONNX ended with reason code %s [%d]\n",
        formatMQRC(CReason),CReason);
    exit( (int)CReason );
  }

  /******************************************************************/
  /* Work out which platform we have connected to if it is a client */
  /* connection.                                                    */
  /******************************************************************/
  if (ClientConnection)
  {
    QMO_options = MQOO_INQUIRE | MQOO_FAIL_IF_QUIESCING;
    qmod.ObjectType = MQOT_Q_MGR;

    /****************************************************************/
    /*   Open the queue manager for an inquire operation            */
    /****************************************************************/
    MQOPEN(Hcon,&qmod,QMO_options,&QMHObj,&CompCode,&Reason);
    if (CompCode == MQCC_FAILED)
    {
      printf("MQOPEN of queue manager ended with reason code %s [%d]\n",
          formatMQRC(Reason),Reason);
      printf("Exiting ...\n");
      goto MOD_EXIT;
    }

    /****************************************************************/
    /*   Inquire which platform is connected. Only asking for       */
    /*   a single integer selector.                                 */
    /****************************************************************/
    MQINQ(Hcon,QMHObj,1,Selectors,1,Attrs,0,NULL,&CompCode,&Reason);
    if (CompCode == MQCC_FAILED)
    {
      printf("MQINQ ended with reason code %s [%d]\n",
          formatMQRC(Reason),Reason);
      printf("Exiting ...\n");
      goto MOD_EXIT;
    }

    /****************************************************************/
    /* Extract the response from MQINQ                              */
    /****************************************************************/
    Platform = Attrs[0];

    /****************************************************************/
    /* And close the open handle as it is no longer needed.         */
    /****************************************************************/
    MQCLOSE(Hcon,&QMHObj,0,&CompCode,&Reason);
  }

  /******************************************************************/
  /* Use the default set of queues if none specified on command line*/
  /******************************************************************/
  if (ObjCount == 0)
  {
    int eventQueueCount = sizeof(DefaultQueues) / sizeof(DefaultQueues[0]);

    UsingDefaultQueues = TRUE;

    printf("Using default set of event queues.\n");
    for (i=0;i<eventQueueCount;i++)
    {
      if ((strcmp(DefaultQueues[i],"SYSTEM.ADMIN.LOGGER.EVENT")==0) &&
          (Platform == MQPL_ZOS))
      {
        /* Skip event queue that is not used on z/OS */
      }
      else
      {
        OpenObjects[ObjCount].ObjectName = DefaultQueues[i];
        OpenObjects[ObjCount].ObjectType = MQOT_Q;
        ObjCount++;
      }
    }
  }

  /******************************************************************/
  /*                                                                */
  /*   Loop round and open and register the consumers               */
  /*                                                                */
  /******************************************************************/
  ObjIndex = ObjCount;
  while (ObjIndex--)
  {
    strncpy(od.ObjectName, OpenObjects[ObjIndex].ObjectName,sizeof(od.ObjectName));
    if (Browse)
      O_options |= MQOO_BROWSE;

    if (OpenObjects[ObjIndex].ObjectType == MQOT_TOPIC)
    {
      /**************************************************************/
      /*   Subscribe to a topic. Use a managed destination so that  */
      /*   the queue on which publications arrive is automatically  */
      /*   created. The returned object handle can be used for      */
      /*   browse operations without further specification of open  */
      /*   options.                                                 */
      /**************************************************************/
      sd.Options =  MQSO_CREATE
                | MQSO_NON_DURABLE
                | MQSO_FAIL_IF_QUIESCING
                | MQSO_MANAGED;

      sd.ObjectString.VSPtr    = OpenObjects[ObjIndex].ObjectName;
      sd.ObjectString.VSLength = (MQLONG)strlen(OpenObjects[ObjIndex].ObjectName);

      MQSUB(Hcon,                                      /* connection handle*/
            &sd,                               /* object descriptor for sub*/
            &OpenObjects[ObjIndex].Hobj, /* object handle for managed queue*/
            &OpenObjects[ObjIndex].Hsub,  /* object handle for subscription*/
            &OpenCode,                                   /* completion code*/
            &Reason);                                        /* reason code*/

      if (OpenCode == MQCC_FAILED)
      {
        printf("MQSUB of '%s' ended with reason code %s [%d]\n",
           OpenObjects[ObjIndex].ObjectName,formatMQRC(Reason),Reason);
        printf("Exiting ...\n");
        goto MOD_EXIT;
      }
    }
    else
    {
      /**************************************************************/
      /*   Open the queue                                           */
      /**************************************************************/
      MQOPEN(Hcon,                                     /* connection handle*/
             &od,                            /* object descriptor for queue*/
             O_options,                                     /* open options*/
             &OpenObjects[ObjIndex].Hobj,                  /* object handle*/
             &OpenCode,                                  /* completion code*/
             &Reason);                                       /* reason code*/

      if (OpenCode == MQCC_FAILED)
      {
        /**************************************************************/
        /* Not all platforms have all of the event queues. So ignore  */
        /* an error caused by a missing name.                         */
        /**************************************************************/
        if (UsingDefaultQueues && Reason == MQRC_UNKNOWN_OBJECT_NAME)
        {
          printf("MQOPEN of '%.48s' failure ignored.\n",od.ObjectName);
        }
        else
        {
          printf("MQOPEN of '%.48s' ended with reason code %s [%d]\n",
               od.ObjectName,formatMQRC(Reason),Reason);
          printf("Exiting ...\n");
          goto MOD_EXIT;
        }
      }
    }

    /****************************************************************/
    /*                                                              */
    /*   Register a consumer                                        */
    /*                                                              */
    /****************************************************************/
    if (OpenCode == MQCC_OK)
    {
      cbd.CallbackFunction = (MQPTR)MessageConsumer;

      gmo.Options = MQGMO_NO_SYNCPOINT | MQGMO_CONVERT;
      gmo.WaitInterval = WaitInterval;
      if (WaitInterval != MQWI_UNLIMITED)
        gmo.Options |= MQGMO_WAIT;
      if (Browse)
        gmo.Options |= MQGMO_BROWSE_NEXT;

      MQCB(Hcon,
           MQOP_REGISTER,
           &cbd,
           OpenObjects[ObjIndex].Hobj,
           &md,
           &gmo,
           &CompCode,
           &Reason);
      if (CompCode == MQCC_FAILED)
      {
        printf("MQCB ended with reason code %s [%d]\n",
          formatMQRC(Reason),Reason);

        /* Likely reason for this error is SHARECNV setting */
        if (Reason == MQRC_ENVIRONMENT_ERROR && ClientConnection)
          printf("Using MQCB requires non-zero SHARECNV on the SVRCONN configuration.\n");

        printf("Exiting ...\n");
        goto MOD_EXIT;
      }
    }
  }

  /******************************************************************/
  /*                                                                */
  /*  Issue a message to the user to press enter before we start    */
  /*  consuming messages. This should prevent interleaved printfs   */
  /*  from between the threads.                                     */
  /*                                                                */
  /******************************************************************/
  printf("\nPress ENTER to end\n");
  fflush(stdout);

  /******************************************************************/
  /*                                                                */
  /*  Start consumption of messages                                 */
  /*                                                                */
  /******************************************************************/
  MQCTL(Hcon,
        MQOP_START,
        &ctlo,
        &CompCode,
        &Reason);
  if (CompCode == MQCC_FAILED)
  {
    printf("MQCTL ended with reason code %s [%d]\n",
      formatMQRC(Reason),Reason);
    if (Reason == MQRC_OPERATION_ERROR)
      printf("Program may need rebuilding with threaded options and libraries.\n");
    printf("Exiting ...\n");
    goto MOD_EXIT;
  }

  /******************************************************************/
  /*                                                                */
  /*  Wait for the user to press enter, or the consume thread to    */
  /*  indicate that we should terminate.                            */
  /*                                                                */
  /******************************************************************/
  WaitForEnd();

  /******************************************************************/
  /*                                                                */
  /*  Stop consumption of messages                                  */
  /*                                                                */
  /******************************************************************/
  MQCTL(Hcon,
        MQOP_STOP,
        &ctlo,
        &CompCode,
        &Reason);
  if (CompCode == MQCC_FAILED)
  {
    printf("MQCTL ended with reason code %s [%d]\n",
      formatMQRC(Reason),Reason);
    goto MOD_EXIT;
  }

MOD_EXIT:
  /******************************************************************/
  /*                                                                */
  /*   Close the source objects (if any were opened)                */
  /*                                                                */
  /******************************************************************/
  ObjIndex = ObjCount;
  while (ObjIndex--)
  {
    if (OpenObjects[ObjIndex].ObjectType == MQOT_TOPIC)
    {
      if (OpenObjects[ObjIndex].Hsub != MQHO_UNUSABLE_HOBJ)
      {
        MQCLOSE(Hcon,                        /* connection handle     */
                &OpenObjects[ObjIndex].Hsub, /* object handle         */
                MQCO_NONE,                   /* close options         */
                &CompCode,                   /* completion code       */
                &Reason);                    /* reason code           */

        /* report reason, if any     */
        if (Reason != MQRC_NONE)
        {
          printf("MQCLOSE (subscription handle) ended with reason code %s [%d]\n",
            formatMQRC(Reason),Reason);
        }
      }
    }
    if (OpenObjects[ObjIndex].Hobj != MQHO_UNUSABLE_HOBJ)
    {
      MQCLOSE(Hcon,                        /* connection handle     */
              &OpenObjects[ObjIndex].Hobj, /* object handle         */
              MQCO_NONE,                   /* close options         */
              &CompCode,                   /* completion code       */
              &Reason);                    /* reason code           */

      /* report reason, if any     */
      if (Reason != MQRC_NONE)
      {
        printf("MQCLOSE (object handle) ended with reason code %s [%d]\n",
          formatMQRC(Reason),Reason);
      }
    }
  }
  /******************************************************************/
  /*   Disconnect from MQM if not already connected                 */
  /*   The disconnection will automatically close any queues and    */
  /*   remove any active subscriptions.                             */
  /******************************************************************/
  if (Hcon != MQHC_UNUSABLE_HCONN)
  {
    if (CReason != MQRC_ALREADY_CONNECTED )
    {
      MQDISC(&Hcon,                   /* connection handle          */
             &CompCode,               /* completion code            */
             &Reason);                /* reason code                */

      /* report reason, if any     */
      if (Reason != MQRC_NONE)
      {
        printf("MQDISC ended with reason code %s [%d]\n",
          formatMQRC(Reason),Reason);
      }
    }
  }
  /******************************************************************/
  /*                                                                */
  /* END OF AMQSEVT                                                 */
  /*                                                                */
  /******************************************************************/
  printf("\nSample AMQSEVT end\n");
  return((int)Reason);
}

/********************************************************************/
/* FUNCTION: Usage                                                  */
/* PURPOSE : Print out the usage for the program                    */
/********************************************************************/
static void Usage(void)
{
  printf("Usage: amqsevt [-m Qmgr] [-r d|r|m] [-b] [-c] [-d] \n");
  printf("         [-u User ID] [-w wait] {-t Topic} {-q Queue}\n");
  printf("  -m <Queue Manager Name>\n");
  printf("  -t <Topic> Can have multiple entries\n");
  printf("  -q <Queue> Can have multiple entries\n");
  printf("  -b Browse messages\n");
  printf("  -c Connect as client\n");
  printf("  -d Print definitions without formatting\n");
  printf("  -r <Reconnect Type>\n");
  printf("     d Reconnect Disabled\n");
  printf("     r Reconnect\n");
  printf("     m Reconnect Queue Manager\n");
  printf("  -u User ID\n");
  printf("  -w <Wait time in seconds>\n");
  printf("\n");
  printf("Example:\n");
  printf("  amqsevt -m QM1 -q SYSTEM.ADMIN.QMGR.EVENT -q SYSTEM.ADMIN.PERM.EVENT -w 1\n");

}

/*****************************************************************/
/* FUNCTION: mqgetopt                                            */
/* PURPOSE:                                                      */
/*   Not all platforms have a getopt function, so this is a      */
/*   simple reimplementation.                                    */
/*****************************************************************/
int mqgetopt(int argc, char *argv[], char *opts)
{
  static int sp = 1;
  int c;
  char *cp;

  if(sp == 1)  {
    if(mqoptind >= argc ||  argv[mqoptind][1] == '\0' ||
      (argv[mqoptind][0] != '-' && argv[mqoptind][0] != '/'))  {
      return(EOF);
    }
    else if(strcmp(argv[mqoptind], "--") == 0 ||
      strcmp(argv[mqoptind], "//") == 0)  {
      mqoptind++;
      return(EOF);
    }
  }

  mqoptopt = c = argv[mqoptind][sp];

  if(c == ':' || (cp=strchr(opts, c)) == NULL)  {
    if(argv[mqoptind][++sp] == '\0')  {
      mqoptind++;
      sp = 1;
    }
    return('?');
  }

  if(*++cp == ':')  {
    if(argv[mqoptind][sp+1] != '\0')  {
      mqoptarg = &argv[mqoptind++][sp+1];
    }
    else if(++mqoptind >= argc)  {
      sp = 1;
      return('?');
    }
    else  {
      mqoptarg = argv[mqoptind++];
    }

    sp = 1;
  }
  else  {
    if(argv[mqoptind][++sp] == '\0')  {
      sp = 1;
      mqoptind++;
    }
    mqoptarg = NULL;
  }
  return(c);
}

/********************************************************************/
/* FUNCTION: formatEvent                                            */
/* Parameters:                                                      */
/*   pMsgDesc: MQMD from the message                                */
/*   Length  : Size of message                                      */
/*   Buffer  : Message data                                         */
/* Returns TRUE if unexpected format of message. FALSE otherwise.   */
/********************************************************************/
static MQBOOL formatEvent(PMQMD pMsgDesc,MQLONG Length,PMQBYTE Buffer)
{

  MQCFH   *evtmsg;             /* message buffer                    */
  char    *paras;              /* the parameters                    */

  MQCFGR  *cfgr;
                               /* Each of the various PCF datatypes */
  MQCFBS  *cfbs;
  MQCFIN  *cfin;
  MQCFIL  *cfil;
  MQCFIN64  *cfin64;
  MQCFIL64  *cfil64;

  MQCFST  *cfst;
  MQCFSL  *cfsl;

  MQCFIF  *cfif;
  MQCFSF  *cfsf;
  MQCFBF  *cfbf;

  MQBOOL   error = FALSE;
  MQBOOL   inGroup = FALSE;

  char *(*fn)(MQLONG);  /* Points to function that maps value to a string */

  int counter;
  int groupCount = 0;
  int totalParameters;

  int offset = 0 ;

  char attrbuf[48];  /* Attribute name */
  char opbuf[33];    /* Filter operation */

  char *tmpbuf;
  MQINT64 int64;
  int i;

  evtmsg = (MQCFH *)Buffer;

  /************************************************************/
  /* Check the data                                           */
  /************************************************************/
  if (!error
     && strncmp(pMsgDesc->Format,MQFMT_EVENT,8)
     && strncmp(pMsgDesc->Format,MQFMT_ADMIN,8))
  {
    printf("Message is not a recognised event format.\n");
    error = TRUE;
  }
  /************************************************************/
  /* Check the data                                           */
  /************************************************************/
  if (!error && evtmsg->Type > MQCFT_APP_ACTIVITY)
  {
    printf("Message is not in event message range. It is of type %ld\n",evtmsg->Type);
    error = TRUE;
  }

  /**********************************************************/
  /* Verify that it's the right length                      */
  /**********************************************************/
  if (!error && evtmsg->StrucLength != MQCFH_STRUC_LENGTH)
  {
    printf("Header is the wrong length, %ld\n",evtmsg->StrucLength);
    error = TRUE;
  }

  /**********************************************************/
  /* Verify that it's the right version                     */
  /**********************************************************/
  if (!error && (evtmsg->Version < MQCFH_VERSION_1
       || evtmsg->Version > MQCFH_CURRENT_VERSION))
  {
    printf("Header is the wrong version, %ld\n",evtmsg->Version);
    error = TRUE;
  }

  /**********************************************************/
  /* If message failed basic sanity checks, do not try to   */
  /* format the rest of it                                  */
  /**********************************************************/
  if (error)
    return error;

  /**********************************************************/
  /* Start formatting of the event. For the most important  */
  /* fields in the event (command/reason) we print both     */
  /* a text version and the number to make it easier to     */
  /* look for it in the documentation.                      */
  /**********************************************************/
  offset = 0;
  lookup(evtmsg->Command,MQCMD_STR,valbuf,sizeof(valbuf));
  sprintf(printbuf,"%s [%d]",formatConstant(valbuf),evtmsg->Command);
  printLine(offset,"Event Type",printbuf);

  lookup(evtmsg->Reason,MQRC_STR,valbuf,sizeof(valbuf));
  sprintf(printbuf,"%s [%d]",formatConstant(valbuf),evtmsg->Reason);
  printLine(offset,"Reason",printbuf);

  /**********************************************************/
  /* Config events have before/after status indicated       */
  /* by the Control field in the event.                     */
  /**********************************************************/
  if (evtmsg->Reason == MQRC_CONFIG_CHANGE_OBJECT)
  {
    if (evtmsg->Control == MQCFC_LAST)
      strncpy(valbuf,"After Change",sizeof(valbuf));
    else
      strncpy(valbuf,"Before Change",sizeof(valbuf));
    printLine(offset,"Object state", valbuf);
  }

  /**********************************************************/
  /* Timestamp is read from the MQMD - it is always in GMT  */
  /* regardless of local timezone. Do not want to try to    */
  /* convert it, because this machine may be a client in a  */
  /* different timezone than the server generating the      */
  /* event. So stick to GMT (or UCT if you prefer).         */
  /**********************************************************/
  sprintf(valbuf,"%4.4s/%2.2s/%2.2s %2.2s:%2.2s:%2.2s.%2.2s GMT",
     &pMsgDesc->PutDate[0],
     &pMsgDesc->PutDate[4],
     &pMsgDesc->PutDate[6],
     &pMsgDesc->PutTime[0],
     &pMsgDesc->PutTime[2],
     &pMsgDesc->PutTime[4],
     &pMsgDesc->PutTime[6]);
  printLine(offset,"Event created",valbuf);


  /**********************************************************/
  /* The CorrelId is used to tie config events to each other*/
  /* and to command events                                  */
  /**********************************************************/
  if (evtmsg->Command == MQCMD_CONFIG_EVENT
      || evtmsg->Command == MQCMD_COMMAND_EVENT  ) {
    tmpbuf = malloc(MQ_CORREL_ID_LENGTH * 2 + 1);
    printLine(offset,"Correlation ID",formatHex(pMsgDesc->CorrelId,tmpbuf,MQ_CORREL_ID_LENGTH));
    free(tmpbuf);
  }

  /**********************************************************/
  /* Get a pointer to the start of the parameters.          */
  /**********************************************************/
  paras = (char *)(evtmsg + 1);

  totalParameters = evtmsg->ParameterCount;
  counter = 1;

  offset += 2;
  inGroup = FALSE;

  while (counter <= totalParameters)
  {
    /********************************************************/
    /* While inside a PCF group (usually from a COMMAND     */
    /* event), offset each attribute to make it easier to   */
    /* read. Revert to previous offset once out of the      */
    /* group. Only need to cope with one level of nesting   */
    /* of group, although the PCF definitions could allow   */
    /* more if someone wanted to construct such a beast.    */
    /********************************************************/
    if (groupCount == 0 && inGroup)
    {
      if (offset >= 2)
        offset -=2;
      inGroup = FALSE;
    }

    if (inGroup)
      groupCount --;

    /********************************************************/
    /* Go through the parameters and print out the data     */
    /* associated with each one. Many integer values get    */
    /* decoded into a definition format to make them easier */
    /* to understand.                                       */
    /********************************************************/
    switch (((MQCFST *)paras)->Type) /* Cast to any of the types */
    {
      case MQCFT_GROUP:
        cfgr = (MQCFGR *)paras;
        groupCount = cfgr->ParameterCount;
        totalParameters += groupCount;
        inGroup = TRUE;
        lookup(cfgr->Parameter,MQGACF_STR,attrbuf,sizeof(attrbuf));
        printLine(offset,formatConstantBase(attrbuf,FALSE),NULL);
        offset += 2;
        paras += cfgr->StrucLength;
        break;

      case MQCFT_INTEGER64_LIST:
        cfil64 = (MQCFIL64 *)paras;

        lookup(cfil64->Parameter,MQIA_STR,attrbuf,sizeof(attrbuf));
        for (i=0;i<cfil64->Count;i++)
        {
          switch (cfil64->Parameter)
          {
          default:
            /* IT12861     */
            memcpy(&int64,&cfil64->Values[i],sizeof(MQINT64));
            sprintf(printbuf,INT64FMTSPEC,int64);
            break;
          }

          if (i==0)
            printLine(offset,formatConstant(attrbuf),printbuf);
          else
            printLine(offset,"",printbuf);
        }
        paras += cfil64->StrucLength;
        break;

      case MQCFT_INTEGER_LIST:
        cfil = (MQCFIL *)paras;

        lookup(cfil->Parameter,MQIA_STR,attrbuf,sizeof(attrbuf));
        for (i=0;i<cfil->Count;i++)
        {
          char *c;
          switch (cfil->Parameter)
          {
          case MQIACH_HDR_COMPRESSION:
          case MQIACH_MSG_COMPRESSION:
            lookup(cfil->Values[i],MQCOMPRESS_STR,valbuf,sizeof(valbuf));
            sprintf(printbuf,"%s",formatConstant(valbuf));
            break;
          case MQIACF_AUTH_ADD_AUTHS:
          case MQIACF_AUTH_REMOVE_AUTHS:
          case MQIACF_AUTHORIZATION_LIST:
            lookup(cfil->Values[i],MQAUTH_STR,valbuf,sizeof(valbuf));
            sprintf(printbuf,"%s",formatConstant(valbuf));
            break;
          /*If new object types are defined, this block will need updating */
          case MQIACF_AMQP_ATTRS:
          case MQIACF_AUTH_INFO_ATTRS:
          case MQIACF_AUTH_PROFILE_ATTRS:
          case MQIACF_AUTH_SERVICE_ATTRS:
          case MQIACF_CF_STRUC_ATTRS:
          case MQIACF_CHANNEL_ATTRS:
          case MQIACF_CHLAUTH_ATTRS:
          case MQIACF_CLUSTER_Q_MGR_ATTRS:
          case MQIACF_COMM_INFO_ATTRS:
          case MQIACF_CONNECTION_ATTRS:
          case MQIACF_INT_ATTRS:
          case MQIACF_LISTENER_ATTRS:
          case MQIACF_LISTENER_STATUS_ATTRS:
          case MQIACF_NAMELIST_ATTRS:
          case MQIACF_PROCESS_ATTRS:
          case MQIACF_PUBSUB_STATUS_ATTRS:
          case MQIACF_Q_ATTRS:
          case MQIACF_Q_MGR_ATTRS:
          case MQIACF_Q_MGR_STATUS_ATTRS:
          case MQIACF_Q_STATUS_ATTRS:
          case MQIACF_SECURITY_ATTRS:
          case MQIACF_SERVICE_ATTRS:
          case MQIACF_SERVICE_STATUS_ATTRS:
          case MQIACF_SMDS_ATTRS:
          case MQIACF_STORAGE_CLASS_ATTRS:
          case MQIACF_SUB_ATTRS:
          case MQIACF_SUB_STATUS_ATTRS:
          case MQIACF_TOPIC_ATTRS:
          case MQIACF_TOPIC_STATUS_ATTRS:
          case MQIACF_XR_ATTRS:
          case MQIACH_CHANNEL_INSTANCE_ATTRS:
          case MQIACH_CHANNEL_SUMMARY_ATTRS:
            c = lookup(cfil->Values[i],MQIA_STR,valbuf,sizeof(valbuf));
            if (!c)
              c = lookup(cfil->Values[i],MQCA_STR,valbuf,sizeof(valbuf));
            if (!c)
              c = lookup(cfil->Values[i],MQBACF_STR,valbuf,sizeof(valbuf));
            strcpy(printbuf,formatConstant(valbuf));
            break;
          case MQIA_SUITE_B_STRENGTH:
            lookup(cfil->Values[i],MQ_SUITE_STR,valbuf,sizeof(valbuf));
            strcpy(printbuf,formatConstant(valbuf));
            break;
          default:
            sprintf(printbuf,"%d",cfil->Values[i]);
            break;
          }

          if (inGroup && strstr(attrbuf,"_ATTRS"))
          {
            if (cfil->Count == 1 &&
                cfil->Values[0] == MQIACF_ALL )
              sprintf(printbuf,"%s","All attributes");
          }

          if (i==0)
            printLine(offset,formatConstant(attrbuf),printbuf);
          else
            printLine(offset,"",printbuf);
        }
        paras += cfil->StrucLength;
        break;

      case MQCFT_STRING_LIST:
        cfsl = (MQCFSL *)paras;
        lookup(cfsl->Parameter,MQCA_STR,attrbuf,sizeof(attrbuf));
        /* All strings in an MQCFSL block have the same length */
        printLineNN(offset,formatConstant(attrbuf),
            cfsl->Strings,cfsl->StringLength);

        for (i=1;i<cfsl->Count;i++)
        {
          printLineNN(offset,"",
            (char *)(cfsl->Strings) + (cfsl->StringLength*i),
          cfsl->StringLength);
        }
        paras += cfsl->StrucLength;
        break;

      case MQCFT_STRING:
        cfst = (MQCFST *)paras;
        lookup(cfst->Parameter,MQCA_STR,attrbuf,sizeof(attrbuf));
        printLineNN(offset,formatConstant(attrbuf),cfst->String,cfst->StringLength);
        paras += cfst->StrucLength;
        break;

      case MQCFT_INTEGER64 :
        cfin64 = (MQCFIN64 *)paras;
        lookup(cfin64->Parameter,MQIA_STR,attrbuf,sizeof(attrbuf));
        /* IT12861     */
        memcpy(&int64,&cfin64->Value,sizeof(MQINT64)); /* Ensure data is aligned. */
        sprintf(printbuf,INT64FMTSPEC,int64);
        printLine(offset,formatConstant(attrbuf),printbuf);
        paras += cfin64->StrucLength;
        break;

      case MQCFT_INTEGER:
        cfin = (MQCFIN *)paras;
        fn = NULL;
        lookup(cfin->Parameter,MQIA_STR,attrbuf,sizeof(attrbuf));

        /**********************************************************/
        /* Formatting for many attributes.  Some attributes have  */
        /* special values where, for example, -1 means something  */
        /* like "unlimited" while positive integers are taken as  */
        /* that actual value.                                     */
        /**********************************************************/
        switch (cfin->Parameter)
        {
        case MQIA_ACCOUNTING_CONN_OVERRIDE:
        case MQIA_ACCOUNTING_MQI:
        case MQIA_ACCOUNTING_Q:
        case MQIA_ACTIVITY_CONN_OVERRIDE:
        case MQIA_ACTIVITY_TRACE:
        case MQIA_MONITORING_AUTO_CLUSSDR:
        case MQIA_MONITORING_CHANNEL:
        case MQIA_MONITORING_Q:
        case MQIA_STATISTICS_AUTO_CLUSSDR:
        case MQIA_STATISTICS_CHANNEL:
        case MQIA_STATISTICS_MQI:
        case MQIA_STATISTICS_Q:
          if (cfin->Value == 253) /* Bug in some z/OS versions */
            cfin->Value = -3;
          fn = MQMON_STR;
          break;
        case MQIA_ACTIVITY_RECORDING:
        case MQIA_TRACE_ROUTE_RECORDING:
          fn = MQRECORDING_STR;
          break;
        case MQIA_ADOPT_CONTEXT:
          fn = MQADPCTX_STR;
          break;
        case MQIA_ADOPTNEWMCA_CHECK:
          fn = MQADOPT_CHECK_STR;
          break;
        case MQIA_ADOPTNEWMCA_TYPE:
          fn = MQADOPT_TYPE_STR;
          break;
        case MQIA_APPL_TYPE :
        case MQIACF_EVENT_APPL_TYPE :
          fn = MQAT_STR;
          break;
        case MQIA_AUTH_INFO_TYPE :
          fn = MQAIT_STR;
          break;
        case MQIA_AUTHENTICATION_METHOD:
          fn = MQAUTHENTICATE_STR;
          break;
        case MQIA_AUTHORITY_EVENT:
        case MQIA_BRIDGE_EVENT:
        case MQIA_CHANNEL_AUTO_DEF_EVENT:
        case MQIA_CHANNEL_EVENT:
        case MQIA_COMMAND_EVENT:
        case MQIA_COMM_EVENT:
        case MQIA_CONFIGURATION_EVENT:
        case MQIA_INHIBIT_EVENT:
        case MQIA_LOCAL_EVENT:
        case MQIA_LOGGER_EVENT:
        case MQIA_PERFORMANCE_EVENT:
        case MQIA_Q_DEPTH_HIGH_EVENT:
        case MQIA_Q_DEPTH_LOW_EVENT:
        case MQIA_Q_DEPTH_MAX_EVENT:
        case MQIA_Q_SERVICE_INTERVAL_EVENT:
        case MQIA_REMOTE_EVENT:
        case MQIA_START_STOP_EVENT:
        case MQIA_SSL_EVENT:
          fn = MQEVR_STR;
          break;
        case MQIA_AUTO_REORGANIZATION:
          fn = MQREORG_STR;
          break;
        case MQIA_BASE_TYPE :
          fn = MQOT_STR;
          break;
        case MQIA_CERT_VAL_POLICY:
          fn = MQ_CERT_STR;
          break;
        case MQIA_CF_CFCONLOS:
        case MQIA_QMGR_CFCONLOS:
          fn = MQCFCONLOS_STR;
          break;
        case MQIA_CF_RECAUTO:
          fn = MQRECAUTO_STR;
          break;
        case MQIA_CF_RECOVER:
          fn = MQCFR_STR;
          break;
        case MQIA_CHANNEL_AUTO_DEF:
          fn = MQCHAD_STR;
          break;
        case MQIA_CHECK_CLIENT_BINDING:
        case MQIA_CHECK_LOCAL_BINDING:
          fn = MQCHK_STR;
          break;
        case MQIA_CHINIT_CONTROL:
        case MQIA_CMD_SERVER_CONTROL:
        case MQIA_SERVICE_CONTROL:
        case MQIACH_LISTENER_CONTROL:
          fn = MQSVC_CONTROL_STR;
          break;
        case MQIA_CHINIT_TRACE_AUTO_START:
          fn = MQTRAXSTR_STR;
          break;
        case MQIA_CHLAUTH_RECORDS:
          fn = MQCHLA_STR;
          break;
        case MQIA_CLUSTER_PUB_ROUTE:
          fn = MQCLROUTE_STR;
          break;
        case MQIA_CLWL_USEQ :
          fn = MQCLWL_STR;
          break;
        case MQIA_COMM_INFO_TYPE:
          fn = MQCIT_STR;
          break;
        case MQIA_DEF_BIND:
          fn = MQBND_STR;
          break;
        case MQIA_DEF_CLUSTER_XMIT_Q_TYPE:
          fn = MQCLXQ_STR;
          break;
        case MQIA_DEF_INPUT_OPEN_OPTION:
          fn = MQOO_STR;
          break;
        case MQIA_DEF_PERSISTENCE:
        case MQIA_TOPIC_DEF_PERSISTENCE:
          fn = MQPER_STR;
          break;
        case MQIA_DEF_PUT_RESPONSE_TYPE:
          fn = MQPRT_STR;
          break;
        case MQIA_DEF_READ_AHEAD:
          fn = MQREADA_STR;
          break;
        case MQIA_DEFINITION_TYPE :
          fn = MQQDT_STR;
          break;
        case MQIA_DIST_LISTS:
          fn = MQDL_STR;
          break;
        case MQIA_DNS_WLM:
          fn = MQDNSWLM_STR;
          break;
        case MQIA_DURABLE_SUB:
          fn = MQSUB_STR;
          break;
        case MQIA_ENCRYPTION_ALGORITHM:
          fn = MQMLP_ENCRYPTION_STR;
          break;
        case MQIA_GROUP_UR:
          fn = MQGUR_STR;
          break;
        case MQIA_HARDEN_GET_BACKOUT:
          fn = MQQA_BACKOUT_STR;
          break;
        case MQIA_IGQ_PUT_AUTHORITY:
          fn = MQIGQPA_STR;
          break;
        case MQIA_INDEX_TYPE :
          fn = MQIT_STR;
          break;
        case MQIA_INHIBIT_GET :
          fn = MQQA_GET_STR;
          break;
        case MQIA_INHIBIT_PUB :
          fn = MQTA_PUB_STR;
          break;
        case MQIA_INHIBIT_PUT :
          fn = MQQA_PUT_STR;
          break;
        case MQIA_INHIBIT_SUB :
          fn = MQTA_SUB_STR;
          break;
        case MQIA_INTRA_GROUP_QUEUING:
          fn = MQIGQ_STR;
          break;
        case MQIA_IP_ADDRESS_VERSION:
          fn = MQIPADDR_STR;
          break;
        case MQIA_LDAP_AUTHORMD:
          fn = MQLDAP_AUTHORMD_STR;
          break;
        case MQIA_LDAP_NESTGRP:
          fn = MQLDAP_NESTGRP_STR;
          break;
        case MQIA_LDAP_SECURE_COMM:
          fn = MQSECCOMM_STR;
          break;
        case MQIA_MCAST_BRIDGE:
          fn = MQMCB_STR;
          break;
        case MQIA_MSG_DELIVERY_SEQUENCE :
          fn = MQMDS_STR;
          break;
        case MQIA_MULTICAST:
          fn = MQMC_STR;
          break;
        case MQIA_NAMELIST_TYPE:
          fn = MQNT_STR;
          break;
        case MQIA_NPM_CLASS:
          fn = MQNPM_STR;
          break;
        case MQIA_PLATFORM:
          fn = MQPL_STR;
          break;
        case MQIA_PM_DELIVERY:
        case MQIA_NPM_DELIVERY:
          fn = MQDLV_STR;
          break;
        case MQIA_PROPERTY_CONTROL:
          fn = MQPROP_STR;
          break;
        case MQIA_PROXY_SUB:
          fn = MQTA_PROXY_STR;
          break;
        case MQIA_PUB_SCOPE :
        case MQIA_SUB_SCOPE :
          fn = MQSCOPE_STR;
          break;
        case MQIA_PUBSUB_CLUSTER:
          fn = MQPSCLUS_STR;
          break;
        case MQIA_PUBSUB_MODE:
          fn = MQPSM_STR;
          break;
        case MQIA_PUBSUB_NP_MSG:
        case MQIA_PUBSUB_NP_RESP:
          fn = MQUNDELIVERED_STR;
          break;
        case MQIA_PUBSUB_SYNC_PT:
          fn = MQSYNCPOINT_STR;
          break;
        case MQIA_Q_TYPE :
          fn = MQQT_STR;
          break;
        case MQIA_QSG_DISP:
          fn = MQQSGD_STR;
          break;
        case MQIA_RECEIVE_TIMEOUT_TYPE:
          fn = MQRCVTIME_STR;
          break;
        case MQIACF_REFRESH_TYPE:
          fn = MQRT_STR;
          break;
        case MQIA_REVERSE_DNS_LOOKUP:
          fn = MQRDNS_STR;
          break;
        case MQIA_SCOPE:
          fn = MQSCO_STR;
          break;
        case MQIA_SECURITY_CASE:
          fn = MQSCYC_STR;
          break;
        case MQIA_SERVICE_TYPE:
          fn = MQSVC_TYPE_STR;
          break;
        case MQIA_SHARED_Q_Q_MGR_NAME:
          fn = MQSQQM_STR;
          break;
        case MQIA_SIGNATURE_ALGORITHM:
          fn = MQMLP_SIGN_STR;
          break;
        case MQIA_SSL_FIPS_REQUIRED:
          fn = MQSSL_STR;
          break;
        case MQIA_SYNCPOINT:
          fn = MQSP_STR;
          break;
        case MQIA_TCP_KEEP_ALIVE:
          fn = MQTCPKEEP_STR;
          break;
        case MQIA_TCP_STACK_TYPE:
          fn = MQTCPSTACK_STR;
          break;
        case MQIA_TOLERATE_UNPROTECTED:
          fn = MQMLP_TOLERATE_STR;
          break;
        case MQIA_TOPIC_TYPE:
          fn = MQTOPT_STR;
          break;
        case MQIA_TRIGGER_CONTROL:
          fn = MQTC_STR;
          break;
        case MQIA_TRIGGER_TYPE :
          fn = MQTT_STR;
          break;
        case MQIA_USAGE:
          fn = MQUS_STR;
          break;
        case MQIA_USE_DEAD_LETTER_Q:
          fn = MQUSEDLQ_STR;
          break;
        case MQIA_WILDCARD_OPERATION:
          fn = MQTA_STR;
          break;

        case MQIA_CODED_CHAR_SET_ID:
          if (cfin->Value <=0)
          {
            lookup(cfin->Value,MQCCSI_STR,valbuf,sizeof(valbuf));
            sprintf(printbuf,"%s",formatConstant(valbuf));
          }
          else
            sprintf(printbuf,"%d",cfin->Value);
          break;
        case MQIA_MAX_PROPERTIES_LENGTH:
          if (cfin->Value <0)
          {
            lookup(cfin->Value,MQPROP_STR,valbuf,sizeof(valbuf));
            sprintf(printbuf,"%s",formatConstant(valbuf));
          }
          else
            sprintf(printbuf,"%d",cfin->Value);
          break;
        case MQIA_DEF_PRIORITY:
          if (cfin->Value <0)
          {
            lookup(cfin->Value,MQPRI_STR,valbuf,sizeof(valbuf));
            sprintf(printbuf,"%s",formatConstant(valbuf));
          }
          else
            sprintf(printbuf,"%d",cfin->Value);
          break;
        case MQIA_SHAREABILITY: /* There's no function to decode this */
          if (cfin->Value)
            strcpy(valbuf,"MQQA_SHAREABLE");
          else
            strcpy(valbuf,"MQQA_NOT_SHAREABLE");
          sprintf(printbuf,"%s",formatConstant(valbuf));
          break;

        /* MQIACF attributes */
        case MQIACF_AUTH_REC_TYPE :
        case MQIACH_CHANNEL_INSTANCE_TYPE :
        case MQIACF_OBJECT_TYPE :
          fn = MQOT_STR;
          break;
        case MQIACF_CF_SMDS_BLOCK_SIZE:
          fn = MQDSB_STR;
          break;
        case MQIACF_CF_SMDS_EXPAND:
          fn = MQUSAGE_EXPAND_STR;
          break;
        case MQIACF_CHLAUTH_TYPE:
          fn = MQCAUT_STR;
          break;
        case MQIACF_COMMAND :
          fn = MQCMD_STR;
          break;
        case MQIACF_ENTITY_TYPE:
          fn = MQZAET_STR;
          break;
        case MQIACF_EVENT_ORIGIN :
          fn = MQEVO_STR;
          break;
        case MQIACF_OPERATION_ID:
          fn = MQXF_STR;
          break;
        case MQIACF_Q_STATUS_TYPE :
          fn = MQIA_STR;
          break;
        case MQIACF_REASON_QUALIFIER:
          fn = MQRQ_STR;
          break;
        case MQIACF_SECURITY_TYPE:
          fn = MQSECTYPE_STR;
          break;
        case MQIACF_SECURITY_ITEM:
          fn = MQSECITEM_STR;
          break;
        case MQIACF_COMP_CODE:
          lookup(cfin->Value,MQCC_STR,valbuf,sizeof(valbuf));
          sprintf(printbuf,"%s [%d]",formatConstant(valbuf),cfin->Value);
          break;
        case MQIACF_ENCODING:
          sprintf(printbuf,"0x%08X ",cfin->Value);
          break;
        case MQIACF_ERROR_ID:
          sprintf(printbuf,"0x%08X ",cfin->Value);
          break;
        case MQIACF_REASON_CODE:
          lookup(cfin->Value,MQRC_STR,valbuf,sizeof(valbuf));
          sprintf(printbuf,"%s [%d]",formatConstant(valbuf),cfin->Value);
          break;
        case MQIACF_CONNECT_OPTIONS:
        case MQIACF_GET_OPTIONS:
        case MQIACF_MQCB_OPTIONS:
        case MQIACF_PUT_OPTIONS:
        case MQIACF_SUBRQ_OPTIONS:
          sprintf(valbuf,"0x%08X ",cfin->Value);
          strcpy(printbuf,valbuf);
          break;

        /* MQIACH attributes */
        case MQIACH_AMQP_KEEP_ALIVE:
        case MQIACH_KEEP_ALIVE_INTERVAL:
          if (cfin->Value <0)
          {
            strcpy(valbuf,"MQKAI_AUTO");
            sprintf(printbuf,"%s",formatConstant(valbuf));
          }
          else
            sprintf(printbuf,"%d",cfin->Value);
          break;
        case MQIACH_CHANNEL_DISP:
        case MQIACH_DEF_CHANNEL_DISP:
          fn = MQCHLD_STR;
          break;
        case MQIACH_CHANNEL_TABLE :
          fn = MQCHTAB_STR;
          break;
        case MQIACH_CHANNEL_TYPE :
          fn = MQCHT_STR;
          break;
        case MQIACH_CONNECTION_AFFINITY:
          fn = MQCAFTY_STR;
          break;
        case MQIACH_DATA_CONVERSION:
          fn = MQCDC_STR;
          break;
        case MQIACH_DEF_RECONNECT:
          fn = MQRCN_STR;
          break;
        case MQIACH_MCA_TYPE:
          fn = MQMCAT_STR;
          break;
        case MQIACH_MULTICAST_PROPERTIES:
          fn = MQMCP_STR;
          break;
        case MQIACH_NEW_SUBSCRIBER_HISTORY:
          fn = MQNSH_STR;
          break;
        case MQIACH_NPM_SPEED:
          fn = MQNPMS_STR;
          break;
        case MQIACH_PUT_AUTHORITY:
          fn = MQPA_STR;
          break;
        case MQIACH_SSL_CLIENT_AUTH:
          fn = MQSCA_STR;
          break;
        case MQIACH_USE_CLIENT_ID:
          fn = MQUCI_STR;
          break;
        case MQIACH_USER_SOURCE:
          fn = MQUSRC_STR;
          break;
        case MQIACH_WARNING:
          fn = MQWARN_STR;
          break;
        case MQIACH_XMIT_PROTOCOL_TYPE:
          fn = MQXPT_STR;
          break;

        case MQIACF_OPEN_OPTIONS:
          sprintf(valbuf,"0x%08X ",cfin->Value);
          strcat(valbuf,formatOpenOptions(cfin->Value));
          strcpy(printbuf,valbuf);
          break;
        case MQIACF_CLOSE_OPTIONS:
          sprintf(valbuf,"0x%08X ",cfin->Value);
          strcat(valbuf,formatCloseOptions(cfin->Value));
          strcpy(printbuf,valbuf);
          break;
        case MQIACF_SUB_OPTIONS:
          sprintf(valbuf,"0x%08X ",cfin->Value);
          strcat(valbuf,formatSubOptions(cfin->Value));
          strcpy(printbuf,valbuf);
          break;

        default:
          sprintf(printbuf,"%d",cfin->Value);
          break;
        }

        if (fn)
        {
          lookup(cfin->Value,fn,valbuf,sizeof(valbuf));
          strcpy(printbuf,formatConstant(valbuf));
        }

        printLine(offset,formatConstant(attrbuf),printbuf);

        paras += cfin->StrucLength;
        break;

      case MQCFT_BYTE_STRING:
        cfbs = (MQCFBS *)paras;
        lookup(cfbs->Parameter,MQBACF_STR,attrbuf,sizeof(attrbuf));
        {
          int l;
          l = cfbs->StringLength;
          /* Don't go overboard with potentially large buffers. */
          /* Maybe eventually do something prettier. But that's */
          /* why we have amqsact.                               */
          if ((cfbs->Parameter == MQBACF_MESSAGE_DATA)
              && (l > MAX_FORMAT_DATA_LEN))
            l = MAX_FORMAT_DATA_LEN;
          tmpbuf = malloc(l * 2 + 1);
          if (tmpbuf)
          {
            printLine(offset,formatConstant(attrbuf),
              formatHex(cfbs->String,tmpbuf,l));
            free(tmpbuf);
          }
        }
        paras += cfbs->StrucLength;
        break;

     case MQCFT_INTEGER_FILTER:
       cfif = (MQCFIF *)paras;
       lookup(cfif->Parameter,MQIA_STR,attrbuf,sizeof(attrbuf));
       lookup(cfif->Operator,MQCFOP_STR,opbuf,sizeof(opbuf));
       sprintf(printbuf,"WHERE '%s' %s '%d'",
         formatConstant(attrbuf),
         formatConstant(opbuf),
         cfif->FilterValue);
       sprintf(attrbuf,"Filter");
       printLine(offset,formatConstant(attrbuf),printbuf);
       paras += cfif->StrucLength;
       break;

     case MQCFT_STRING_FILTER:
       cfsf = (MQCFSF *)paras;
       lookup(cfsf->Parameter,MQCA_STR,attrbuf,sizeof(attrbuf));
       lookup(cfsf->Operator,MQCFOP_STR,opbuf,sizeof(opbuf));
       sprintf(printbuf,"WHERE '%s' %s '%-*.*s'",
         formatConstant(attrbuf),
         formatConstant(opbuf),
         cfsf->FilterValueLength,
         cfsf->FilterValueLength,
         cfsf->FilterValue);
       sprintf(attrbuf,"Filter");

       printLine(offset,formatConstant(attrbuf),printbuf);
       paras += cfsf->StrucLength;
       break;

     case MQCFT_BYTE_STRING_FILTER :
       cfbf = (MQCFBF *)paras;
       lookup(cfbf->Parameter,MQBACF_STR,attrbuf,sizeof(attrbuf));
       lookup(cfbf->Operator,MQCFOP_STR,opbuf,sizeof(opbuf));
       tmpbuf = malloc(cfbf->FilterValueLength * 2 + 1);
       if (tmpbuf)
       {
         formatHex(cfbf->FilterValue,tmpbuf,cfbf->FilterValueLength);
         sprintf(printbuf,"WHERE '%s' %s '%-*.*s'",
           formatConstant(attrbuf),
           formatConstant(opbuf),
           cfbf->FilterValueLength *2,
           cfbf->FilterValueLength *2,
           tmpbuf);
         sprintf(attrbuf,"Filter");

         printLine(offset,formatConstant(attrbuf),printbuf);
         free(tmpbuf);
       }
       paras += cfbf->StrucLength;
       break;

      default:
        /******************************************************/
        /* there are other MQCFT types  such as MQCFT_COMMAND */
        /* and MQCFT_RESPONSE but there are no instances of   */
        /* these expected in events.        If they appear,   */
        /* then quit processing the message.                  */
        /******************************************************/
        printf("  Unexpected parameter type, %d\n", ((MQCFST *)paras)->Type);
        counter = totalParameters;
        break;
    }
    counter++;
  }
  return FALSE;
}


/*************************************************************************/
/* FUNCTION: lookup                                                      */
/*                                                                       */
/* Convert a number into the corresponding definition string.            */
/* For example, convert 2035 into "MQRC_NOT_AUTHORIZED"                  */
/*                                                                       */
/* Parameters:                                                           */
/*   val : the value (eg 2035)                                           */
/*   map : a function mapping constants relevant to definition (eg MQRC) */
/*   buf : buffer into which the definition is copied                    */
/*   buflen: length of buffer (must be large enough for string and NULL) */
/* Returns:                                                              */
/*   If the value is found in the relevant map, the buffer has the       */
/*   string and the buffer's address is returned.                        */
/*   If the value is not found, the buffer contains "Unknown" and NULL   */
/*   is returned.                                                        */
/*************************************************************************/
static char *lookup(MQLONG val,char *map(MQLONG),char *buf,int buflen)
{
  char *c;
  char *rc;
  buf[buflen-1]=0;

  /***************************************************************/
  /* Some of the mapping functions are split into separate       */
  /* ranges. For these split groups, look at each subrange until */
  /* a match is found.                                           */
  /***************************************************************/
  if (map == MQIA_STR)
  {
    c = map(val);
    if (!c[0]) c = MQIACF_STR(val);
    if (!c[0]) c = MQIACH_STR(val);
    if (!c[0]) c = MQIAMO_STR(val);
    if (!c[0]) c = MQIAMO64_STR(val);
  }
  else if (map == MQCA_STR)
  {
    c = map(val);
    if (!c[0]) c = MQCACF_STR(val);
    if (!c[0]) c = MQCACH_STR(val);
    if (!c[0]) c = MQCAMO_STR(val);
  }
  else if (map == MQRC_STR)
  {
    c = map(val);
    if (!c[0]) c = MQRCCF_STR(val);
  }
  else
    c = map(val);

  if (c[0])
  {
    /*************************************************************/
    /* It looks nicer to modify a single "Q" into "Queue" where  */
    /* possible. Look for _Q_ in the middle or _Q at the end.    */
    /* Don't do that if we've been asked to print unformatted    */
    /* constants.                                                */
    /*************************************************************/
    char *p = strstr(c,"_Q_");
    if (p && !Unformatted)
      snprintf(buf,buflen-1,"%.*s_QUEUE_%s",p-c,c,p+3);
    else if (!Unformatted && !strncmp(&c[strlen(c)-2],"_Q",2))
      snprintf(buf,buflen-1,"%.*s_QUEUE",strlen(c)-2,c);
    else
      strncpy(buf,c,buflen-1);
    rc = buf;
  }
  else
  {
    snprintf(buf,buflen-1,"Unknown [%d]",val);
    rc = NULL;
  }
  return rc;

}

/*************************************************************************/
/* FUNCTION: printLine/printLineNN                                       */
/*                                                                       */
/* Use a consistent format for printing attr/value pairs. The ':'        */
/* separating them should always end up in the same column regardless of */
/* the starting offset.                                                  */
/* The top  function assumes the value is a null-terminated string. The  */
/* detailed function does not make that assumption; the length must be   */
/* supplied or set to -1 to indicate it is null-terminated.              */
/*************************************************************************/
static void printLine(int offset, char *attr, char *val)
{
  printLineNN(offset,attr,val,-1);
  return;
}

static void printLineNN(int offset, char *attr, char *val,size_t vallen)
{
  int col1 = 32;
  size_t pad;
  char *c;
  MQBOOL colon = TRUE;

  if (Unformatted)
    col1 += 6;
  pad = col1 - offset - strlen(attr);

  /**********************************************************/
  /* Need to handle values that are not null-terminated.    */
  /* vallen shows the length of such records. If there is   */
  /* a space at the end of the buffer, we can trim it down. */
  /**********************************************************/
  if (val)
  {
    if (vallen != -1)
    {
      if (val[vallen-1] == ' ')
      {
        val[vallen-1] = 0;
        vallen = -1;
      }
    }


    if (vallen == -1)
    {
      /* Remove trailing spaces */
      for (c = val+strlen(val)-1;c>= val && *c==' ';c--);
      *(++c) = 0;
      vallen = strlen(val);
    }
    colon = TRUE;
  }
  else
    colon = FALSE;

  if (!colon)
    printf("%*.*s%s\n",
      offset,offset,blank64,
      attr);
  else
    printf("%*.*s%s%*.*s : %-.*s\n",
      offset,offset,blank64,
      attr,
      pad,pad,blank64,
      vallen,val);

  return;
}

/*************************************************************************/
/* FUNCTION: formatConstant                                              */
/*                                                                       */
/* Make an MQI constant more readable.                                   */
/*                                                                       */
/* Given a string like MQRC_UNEXPECTED_ERROR, remove the '_' characters, */
/* convert the upper case into mixed case for each word, and return a    */
/* pointer to the second word. So returned string is "Unexpected Error". */
/*                                                                       */
/* The input string is modified by this process, so must not be in a     */
/* readonly area.                                                        */
/*                                                                       */
/* Some strings still look better in all-upper-case. So we look for the  */
/* converted mixed-case version and force them back into upper. We do not*/
/* extend the length of the buffer.                                      */
/*************************************************************************/
const char *forceUpper[] = {
  "Amqp",
  "Clwl",
  "Cpi",
  "Crl",
  "Csp",
  "Dns", /* Do this before "Dn" */
  "Dn",
  "Idpw",
  "Igq",
  "Ip ",  /* note trailing space */
  "Ipv",
  "Ldap",
  "Lu62",
  "Mca ", /* note trailing space */
  "Mqi",
  "Mqsc",
  "Mr ",
  "Mru",
  "Pcf",
  "Sctq",
  "Ssl",
  "Tcp",
};


static char* formatConstant(char *s)
{
  return formatConstantBase(s,TRUE);
}

static char* formatConstantBase(char *s,MQBOOL mixedCase )
{
  char *c;
  unsigned int i,j;
  MQBOOL swapNext = FALSE;
  MQBOOL firstUnderscore = TRUE;

  if (!s || Unformatted)
    return s;

  /* If no '_' characters, make no modifications */
  if (!strchr(s,'_'))
    return s;

  /* One special case for reformatting */
  if (!strcmp(s,"MQOT_Q"))
    return "Queue";


  /* And now work on the strings to make them mixed case */
  for (c=s;*c;c++)
  {
    if (!isspace(*c))
    {
      if (isupper(*c) && swapNext && mixedCase)
        *c = tolower(*c);
      if (*c == '_')
      {
        swapNext = FALSE;
        if (firstUnderscore)
          firstUnderscore = FALSE;
        else
        {
          *c = ' ';
        }
      }
      else
      {
        swapNext = TRUE;
      }
    }
  }

  /***********************************************************************/
  /* Patch up a few items that look better without mixed case            */
  /* An item may appear more than once in the string so loop until all   */
  /* have been converted.                                                */
  /***********************************************************************/
  for (i=0;i<(sizeof(forceUpper)/sizeof(forceUpper[0]));i++)
  {
    const char *m = forceUpper[i];
    MQBOOL done = FALSE;
    while (!done)
    {
      c = strstr(s,m);
      if (c)
      {
        for (j=0;j<strlen(m);j++)
           c[j] = toupper(c[j]);
      }
      else
        done = TRUE;
    }
  }

  /***********************************************************************/
  /* After converting to mixed-case and resetting a few strings, there   */
  /* are still a small number of cases that look better with special     */
  /* handling.                                                           */
  /***********************************************************************/
  c = strstr(s,"Zos");
  if (c)
   memcpy(c,"zOS",3);    /* Need to keep same length so can't say "z/OS" */

  c = strstr(s," Os");   /* "Operating system", also OS2 and OS400 */
  if (c)
    memcpy(c," OS",3);
  c = strstr(s,"_Os");   /* May be first token after an underscore */
  if (c)
    memcpy(c,"_OS",3);

  /***********************************************************************/
  /* And finally return a pointer to the second word (remove the prefix) */
  /* provided we can.                                                    */
  /***********************************************************************/
  c = strchr(s,'_');
  if (!c || c == &s[strlen(s)-1])
    c= s;
  else
    c++;

  return c;
}

/*************************************************************************/
/* FUNCTION: formatHex                                                   */
/*                                                                       */
/* Format a set of bytes into a string as hex.                           */
/* Buffer size must be at least 2 * data length + 1 byte for NULL        */
/*************************************************************************/
static char *formatHex(PMQBYTE data,char *buf,int datalen)
{
  int i;
  for (i=0;i<datalen;i++)
  {
    sprintf(&buf[2*i],"%02X",data[i]);
  }
  buf[2*i]=0;

  return buf;
}

/*************************************************************************/
/* FUNCTION: formatOpenOptions                                           */
/*                                                                       */
/* Open Options appear in some Not Authorised events. This decodes them  */
/* to show what might need to be issued on a setmqaut command.           */
/*************************************************************************/
static char *formatOpenOptions(MQLONG v)
{
  if (v == 0)
  {
    strcpy(workBuf,"[ None ]");
  }
  else
  {
    strcpy(workBuf,"[ ");
    if (v & MQOO_ALTERNATE_USER_AUTHORITY)
      strcat(workBuf,"altusr ");
    if (v & MQOO_BIND_ON_OPEN)
      strcat(workBuf,"bind_open ");
    if (v & MQOO_BIND_NOT_FIXED)
      strcat(workBuf,"bind_not_fix ");
    if (v & MQOO_BIND_AS_Q_DEF)
      strcat(workBuf,"bind_as_q ");
    if (v & MQOO_BROWSE)
      strcat(workBuf,"brw ");
    if (v & MQOO_CO_OP)
      strcat(workBuf,"coop ");
    if (v & MQOO_FAIL_IF_QUIESCING)
      strcat(workBuf,"fiq ");
    if (v & MQOO_INPUT_AS_Q_DEF)
      strcat(workBuf,"in_as_q ");
    if (v & MQOO_INPUT_SHARED)
      strcat(workBuf,"in_shared ");
    if (v & MQOO_INPUT_EXCLUSIVE)
      strcat(workBuf,"in_excl ");
    if (v & MQOO_INQUIRE)
      strcat(workBuf,"inq ");
    if (v & MQOO_NO_READ_AHEAD)
      strcat(workBuf,"nora ");
    if (v & MQOO_OUTPUT)
      strcat(workBuf,"out ");
    if (v & MQOO_PASS_ALL_CONTEXT)
      strcat(workBuf,"passall ");
    if (v & MQOO_PASS_IDENTITY_CONTEXT)
      strcat(workBuf,"passid ");
    if (v & MQOO_READ_AHEAD)
      strcat(workBuf,"ra ");
    if (v & MQOO_READ_AHEAD_AS_Q_DEF)
      strcat(workBuf,"ra_as_q ");
    if (v & MQOO_RESOLVE_LOCAL_Q)
      strcat(workBuf,"rslv_q ");
    if (v & MQOO_RESOLVE_NAMES)
      strcat(workBuf,"rslv_names ");
    if (v & MQOO_SAVE_ALL_CONTEXT)
      strcat(workBuf,"save_ctx ");
    if (v & MQOO_SET)
      strcat(workBuf,"set ");
    if (v & MQOO_SET_ALL_CONTEXT)
      strcat(workBuf,"setall ");
    if (v & MQOO_SET_IDENTITY_CONTEXT)
      strcat(workBuf,"setid ");

    strcat(workBuf,"]");
  }
  return workBuf;
}

/*************************************************************************/
/* FUNCTION: formatCloseOptions                                          */
/*                                                                       */
/* Close Options appear in some Not Authorised events. This decodes them */
/* to show what might need to be issued on a setmqaut command.           */
/*************************************************************************/
static char *formatCloseOptions(MQLONG v)
{
  if (v == 0)
  {
    strcpy(workBuf,"[ None ] ");
  }
  else
  {
    strcpy(workBuf,"[ ");
    if (v & MQCO_DELETE)
      strcat(workBuf,"del ");
    if (v & MQCO_DELETE_PURGE)
      strcat(workBuf,"del_purge ");
    if (v & MQCO_KEEP_SUB)
      strcat(workBuf,"keep_sub ");
    if (v & MQCO_REMOVE_SUB)
      strcat(workBuf,"remove_sub ");
    if (v & MQCO_QUIESCE)
      strcat(workBuf,"quiesce ");

    strcat(workBuf,"]");
  }
  return workBuf;
}

/*************************************************************************/
/* FUNCTION: formatSubOptions                                            */
/*                                                                       */
/* Sub Options appear in some Not Authorised events. This decodes them   */
/* to show what might need to be issued on a setmqaut command.           */
/*************************************************************************/
static char *formatSubOptions(MQLONG v)
{
  if (v == 0)
  {
    strcpy(workBuf,"[ None ] ");
  }
  else
  {
    strcpy(workBuf,"[ ");
    if (v & MQSO_ALTERNATE_USER_AUTHORITY)
      strcat(workBuf,"altusr ");
    if (v & MQSO_ALTER)
      strcat(workBuf,"alter ");
    if (v & MQSO_CREATE)
      strcat(workBuf,"create ");
    if (v & MQSO_RESUME)
      strcat(workBuf,"resume ");
    if (v & MQSO_DURABLE)
      strcat(workBuf,"dur ");
    if (v & MQSO_GROUP_SUB)
      strcat(workBuf,"group_sub ");
    if (v & MQSO_MANAGED)
      strcat(workBuf,"managed ");
    if (v & MQSO_SET_IDENTITY_CONTEXT)
      strcat(workBuf,"setid ");
    if (v & MQSO_NO_MULTICAST)
      strcat(workBuf,"mcast ");
    if (v & MQSO_FIXED_USERID)
      strcat(workBuf,"fixed_id ");
    if (v & MQSO_ANY_USERID)
      strcat(workBuf,"any_id ");
    if (v & MQSO_PUBLICATIONS_ON_REQUEST)
      strcat(workBuf,"on_req ");
    if (v & MQSO_NEW_PUBLICATIONS_ONLY)
      strcat(workBuf,"new_only ");
    if (v & MQSO_FAIL_IF_QUIESCING)
      strcat(workBuf,"fiq ");
    if (v & MQSO_WILDCARD_CHAR)
      strcat(workBuf,"wc_char ");
    if (v & MQSO_WILDCARD_TOPIC)
      strcat(workBuf,"wc_topic ");
    if (v & MQSO_SET_CORREL_ID)
      strcat(workBuf,"set_cid ");
    if (v & MQSO_SCOPE_QMGR)
      strcat(workBuf,"sc_qmgr ");
    if (v & MQSO_NO_READ_AHEAD)
      strcat(workBuf,"nora ");
    if (v & MQSO_READ_AHEAD)
      strcat(workBuf,"ra ");

    strcat(workBuf,"]");
  }
  return workBuf;
}

/*************************************************************************/
/* FUNCTION: formatMQRC                                                  */
/*                                                                       */
/* Decode just the MQRC value into its string with no further changes.   */
/* The string is put into a static buffer so must be used immediately or */
/* copied elsewhere before it is overwritten by another call to this     */
/* function.                                                             */
/*************************************************************************/
static char *formatMQRC(MQLONG mqrc)
{
  lookup(mqrc,MQRC_STR,workBuf,sizeof(workBuf));
  return workBuf;
}
