/* %Z% %W% %I% %E% %U% */
/********************************************************************/
/*                                                                  */
/* Program name: AMQSAPT0                                           */
/*                                                                  */
/* Description: Sample C program that asynchronously puts messages  */
/*              to a message queue (example using MQPUT & MQSTAT).  */
/*                                                                  */
/*   <copyright                                                     */
/*   notice="lm-source-program"                                     */
/*   pids="5724-H72,"                                               */
/*   years="2008,2012"                                              */
/*   crc="1375888533" >                                             */
/*   Licensed Materials - Property of IBM                           */
/*                                                                  */
/*   5724-H72,                                                      */
/*                                                                  */
/*   (C) Copyright IBM Corp. 2008, 2012 All Rights Reserved.        */
/*                                                                  */
/*   US Government Users Restricted Rights - Use, duplication or    */
/*   disclosure restricted by GSA ADP Schedule Contract with        */
/*   IBM Corp.                                                      */
/*   </copyright>                                                   */
/********************************************************************/
/*                                                                  */
/* Function:                                                        */
/*                                                                  */
/*   AMQSAPT0 is a sample C program to put messages on a message    */
/*   queue with asynchronous response option, querying the success  */
/*   of the put operations with MQSTAT.                             */
/*                                                                  */
/*      -- messages are sent to the queue named by the parameter    */
/*                                                                  */
/*      -- gets lines from StdIn, and adds each to target           */
/*         queue, taking each line of text as the content           */
/*         of a datagram message; the sample stops when a null      */
/*         line (or EOF) is read.                                   */
/*         New-line characters are removed.                         */
/*         If a line is longer than 99 characters it is broken up   */
/*         into 99-character pieces. Each piece becomes the         */
/*         content of a datagram message.                           */
/*         If the length of a line is a multiple of 99 plus 1       */
/*         e.g. 199, the last piece will only contain a new-line    */
/*         character so will terminate the input.                   */
/*                                                                  */
/*      -- writes a message for each MQI reason other than          */
/*         MQRC_NONE; stops if there is a MQI completion code       */
/*         of MQCC_FAILED                                           */
/*                                                                  */
/*      -- summarizes the overall success of the put operations     */
/*         through a call to MQSTAT to query MQSTAT_TYPE_ASYNC_ERROR*/
/*                                                                  */
/*    Program logic:                                                */
/*         MQOPEN target queue for OUTPUT                           */
/*         while end of input file not reached,                     */
/*         .  read next line of text                                */
/*         .  MQPUT datagram message with text line as data         */
/*         MQCLOSE target queue                                     */
/*         MQSTAT connection                                        */
/*                                                                  */
/*                                                                  */
/********************************************************************/
/*                                                                  */
/*   AMQSAPT0 has the following parameters                          */
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
  MQMD     md = {MQMD_DEFAULT};    /* Message Descriptor            */
  MQPMO   pmo = {MQPMO_DEFAULT};   /* put message options           */
  MQSTS   sts = {MQSTS_DEFAULT};   /* status information            */
  /** note, sample uses defaults where it can **/
  MQHCONN  Hcon;                   /* connection handle             */
  MQHOBJ   Hobj;                   /* object handle                 */
  MQLONG   O_options;              /* MQOPEN options                */
  MQLONG   C_options;              /* MQCLOSE options               */
  MQLONG   CompCode;               /* completion code               */
  MQLONG   OpenCode;               /* MQOPEN completion code        */
  MQLONG   Reason;                 /* reason code                   */
  MQLONG   CReason;                /* reason code for MQCONN        */
  MQLONG   messlen;                /* message length                */
  char     buffer[100];            /* message buffer                */
  char     QMName[50];             /* queue manager name            */

  printf("Sample AMQSAPT0 start\n");
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
  /*   Read lines from the file and put them to the message queue   */
  /*   Loop until null line or end of file, or there is a failure   */
  /*                                                                */
  /******************************************************************/
  CompCode = OpenCode;        /* use MQOPEN result for initial test */
  fp = stdin;

  memcpy(md.Format,           /* character string format            */
         MQFMT_STRING, (size_t)MQ_FORMAT_LENGTH);

  /******************************************************************/
  /* These options specify that put operation should occur          */
  /* asynchronously and the application will check the success      */
  /* using MQSTAT at a later time.                                  */
  /******************************************************************/
  md.Persistence = MQPER_NOT_PERSISTENT;
  pmo.Options |= MQPMO_ASYNC_RESPONSE;
  pmo.Options |= MQPMO_NO_SYNCPOINT;

  /******************************************************************/
  /* These options cause the MsgId and CorrelId to be replaced, so  */
  /* that there is no need to reset them before each MQPUT          */
  /******************************************************************/
  pmo.Options |= MQPMO_NEW_MSG_ID;
  pmo.Options |= MQPMO_NEW_CORREL_ID;

  while (CompCode != MQCC_FAILED)
  {
    if (fgets(buffer, sizeof(buffer), fp) != NULL)
    {
      messlen = (MQLONG)strlen(buffer); /* length without null      */
      if (buffer[messlen-1] == '\n')  /* last char is a new-line    */
      {
        buffer[messlen-1]  = '\0';    /* replace new-line with null */
        --messlen;                    /* reduce buffer length       */
      }
    }
    else messlen = 0;        /* treat EOF same as null line         */

    /****************************************************************/
    /*                                                              */
    /*   Put each buffer to the message queue                       */
    /*                                                              */
    /****************************************************************/
    if (messlen > 0)
    {
      MQPUT(Hcon,                /* connection handle               */
            Hobj,                /* object handle                   */
            &md,                 /* message descriptor              */
            &pmo,                /* default options (datagram)      */
            messlen,             /* message length                  */
            buffer,              /* message buffer                  */
            &CompCode,           /* completion code                 */
            &Reason);            /* reason code                     */

      /* report reason, if any */
      if (Reason != MQRC_NONE)
      {
        printf("MQPUT ended with reason code %d\n", Reason);
      }
    }
    else   /* satisfy end condition when empty line is read */
      CompCode = MQCC_FAILED;
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
  /*   Query how many asynchronous puts succeeded                   */
  /*                                                                */
  /******************************************************************/
  MQSTAT(Hcon,                    /* connection handle            */
         MQSTAT_TYPE_ASYNC_ERROR, /* status type                  */
         &sts,                    /* MQSTS structure              */
         &CompCode,               /* completion code              */
         &Reason);                /* reason code                  */

  /* report reason, if any     */
  if (Reason != MQRC_NONE)
  {
    printf("MQSTAT ended with reason code %d\n", Reason);
  }
  else
  {
    /* Display results */
    printf("Succeeded putting %d messages\n",
           sts.PutSuccessCount);
    printf("%d messages were put with a warning\n",
           sts.PutWarningCount);
    printf("Failed to put %d messages\n",
           sts.PutFailureCount);

    if (sts.CompCode == MQCC_WARNING)
    {
      printf("The first warning that occurred had reason code %d\n",
             sts.Reason);
    }
    else if (sts.CompCode == MQCC_FAILED)
    {
      printf("The first error that occurred had reason code %d\n",
             sts.Reason);
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
  /* END OF AMQSAPT0                                                */
  /*                                                                */
  /******************************************************************/
  printf("Sample AMQSAPT0 end\n");
  return(0);
}
