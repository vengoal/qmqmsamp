/* %Z% %W% %I% %E% %U% */
/******************************************************************************/
/*                                                                            */
/* Program name: AMQSLOG0.C                                                   */
/*                                                                            */
/* Description:  Sample C program to monitor the logger event queue and       */
/*               display formatted messsage content to stdout when a logger   */
/*               event occurs                                                 */
/*   <copyright                                                               */
/*   notice="lm-source-program"                                               */
/*   pids="5724-H72,"                                                         */
/*   years="2005,2012"                                                        */
/*   crc="186943832" >                                                        */
/*   Licensed Materials - Property of IBM                                     */
/*                                                                            */
/*   5724-H72,                                                                */
/*                                                                            */
/*   (C) Copyright IBM Corp. 2005, 2012 All Rights Reserved.                  */
/*                                                                            */
/*   US Government Users Restricted Rights - Use, duplication or              */
/*   disclosure restricted by GSA ADP Schedule Contract with                  */
/*   IBM Corp.                                                                */
/*   </copyright>                                                             */
/******************************************************************************/
/*                                                                            */
/* Function: AMQSLOG is a sample program which monitors the logger event      */
/* queue for new event messages, reads those messages, and displays the       */
/* formatted contents of the message to stdout.                               */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* AMQSLOG has 1 parameter - the queue manager name (optional, if not         */
/* specified then the default queue manager is implied)                       */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/* Includes                                                                   */
/******************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <cmqc.h>        /* MQI constants*/
#include <cmqcfc.h>      /* PCF constants*/

/******************************************************************************/
/* Constants                                                                  */
/******************************************************************************/

#define   MAX_MESSAGE_LENGTH   8000

typedef struct _ParmTableEntry
{
  MQLONG  ConstVal;
  PMQCHAR Desc;
} ParmTableEntry;

ParmTableEntry ParmTable[] =
{
  0                             ,"",
  MQCA_Q_MGR_NAME               ,"Queue Manager Name",
  MQCMD_LOGGER_EVENT            ,"Logger Event Command",
  MQRC_LOGGER_STATUS            ,"Logger Status",
  MQCACF_CURRENT_LOG_EXTENT_NAME,"Current Log Extent",
  MQCACF_RESTART_LOG_EXTENT_NAME,"Restart Log Extent",
  MQCACF_MEDIA_LOG_EXTENT_NAME  ,"Media Log Extent",
  MQCACF_LOG_PATH               ,"Log Path"
};

/******************************************************************************/
/* Function prototypes                                                        */
/******************************************************************************/

static void ProcessPCF(MQHCONN    hConn,
                       MQHOBJ     hEventQueue,
                       PMQCHAR    pBuffer);

static PMQCHAR ParmToString(MQLONG Parameter);

/******************************************************************************/
/* Function: main                                                             */
/******************************************************************************/

int main(int argc, char * argv[])
{
  MQLONG    CompCode;
  MQLONG    Reason;
  MQHCONN   hConn = MQHC_UNUSABLE_HCONN;
  MQOD      ObjDesc = { MQOD_DEFAULT };
  MQCHAR    QMName[MQ_Q_MGR_NAME_LENGTH+1]  = "";
  MQCHAR    LogEvQ[MQ_Q_NAME_LENGTH]        = "SYSTEM.ADMIN.LOGGER.EVENT";
  MQHOBJ    hEventQueue = MQHO_UNUSABLE_HOBJ;
  PMQCHAR   pBuffer = NULL;

  printf("\n/*************************************/\n");
  printf("/* Sample Logger Event Monitor start */\n");
  printf("/*************************************/\n");

  /********************************************************************/
  /* Parse any command line options                                   */
  /********************************************************************/
  if (argc > 1)
  {
    strncpy(QMName, argv[1], (size_t)MQ_Q_MGR_NAME_LENGTH);
  }

  pBuffer = (PMQCHAR)malloc(MAX_MESSAGE_LENGTH);
  if (pBuffer == NULL)
  {
    printf("Can't allocate %d bytes\n", MAX_MESSAGE_LENGTH);
    goto MOD_EXIT;
  }

  /********************************************************************/
  /* Connect to the specified (or default) queue manager              */
  /********************************************************************/
  MQCONN( QMName,
         &hConn,
         &CompCode,
         &Reason);

  if (Reason != MQRC_NONE)
  {
    printf("MQCONN ended with reason code %d\n", Reason);
    goto MOD_EXIT;
  }

  /********************************************************************/
  /* Open the logger event queue for input                            */
  /********************************************************************/
  strncpy(ObjDesc.ObjectQMgrName, QMName, MQ_Q_MGR_NAME_LENGTH);
  strncpy(ObjDesc.ObjectName, LogEvQ, MQ_Q_NAME_LENGTH);

  MQOPEN( hConn,
         &ObjDesc,
          MQOO_INPUT_EXCLUSIVE,
         &hEventQueue,
         &CompCode,
         &Reason );

  if (Reason != MQRC_NONE)
  {
    printf("MQOPEN failed for queue manager %.48s Queue %.48s Reason: %d\n",
                                   ObjDesc.ObjectQMgrName,
           ObjDesc.ObjectName,
           Reason);
    goto MOD_EXIT;
  }
  else
  {
    /******************************************************************/
    /* Start processing event messages                                */
    /******************************************************************/
    ProcessPCF(hConn, hEventQueue, pBuffer);
  }

MOD_EXIT:
  if (pBuffer != NULL)
  {
    free(pBuffer);
  }

  /********************************************************************/
  /* Close the logger event queue                                     */
  /********************************************************************/
  if (hEventQueue != MQHO_UNUSABLE_HOBJ)
  {
    MQCLOSE(hConn, &hEventQueue, MQCO_NONE, &CompCode, &Reason);
  }


  /********************************************************************/
  /* Disconnect                                                       */
  /********************************************************************/
  if (hConn != MQHC_UNUSABLE_HCONN)
  {
    MQDISC(&hConn, &CompCode, &Reason);
  }

  return 0;
}

/******************************************************************************/
/* Function: ProcessPCF                                                       */
/******************************************************************************/
/*                                                                            */
/* Input Parameters:  Handle to queue manager connection                      */
/*                    Handle to the opened logger event queue object          */
/*                    Pointer to a memory buffer to store the incoming PCF    */
/*                      message                                               */
/*                                                                            */
/* Output Parameters: None                                                    */
/*                                                                            */
/* Logic: Wait for messages to appear on the logger event queue and display   */
/*        their formatted contents.                                           */
/*                                                                            */
/******************************************************************************/

static void ProcessPCF(MQHCONN    hConn,
                       MQHOBJ     hEventQueue,
                       PMQCHAR    pBuffer)
{
  MQCFH   * pCfh;
  MQCFST  * pCfst;
  MQGMO     Gmo      = { MQGMO_DEFAULT };
  MQMD      Mqmd     = { MQMD_DEFAULT };
  PMQCHAR   pPCFCmd;
  MQLONG    CompCode = MQCC_OK;
  MQLONG    Reason   = MQRC_NONE;
  MQLONG    MsgLen;
  PMQCHAR   Parm = NULL;

  Gmo.Options = MQGMO_WAIT +
                MQGMO_CONVERT +
                MQGMO_FAIL_IF_QUIESCING;
  Gmo.WaitInterval = MQWI_UNLIMITED;   /* Set timeout value           */

  /********************************************************************/
  /* Process response Queue                                           */
  /********************************************************************/
  while (Reason == MQRC_NONE)
  {
    memcpy(&Mqmd.MsgId, MQMI_NONE, sizeof(Mqmd.MsgId));
    memset(&Mqmd.CorrelId, 0, sizeof(Mqmd.CorrelId));

    MQGET( hConn,
           hEventQueue,
          &Mqmd,
          &Gmo,
           MAX_MESSAGE_LENGTH,
           pBuffer,
          &MsgLen,
          &CompCode,
          &Reason );

    if (Reason != MQRC_NONE)
    {
      switch(Reason)
      {
        case MQRC_NO_MSG_AVAILABLE:
             printf("Timed out");
             break;

        default:
             printf("MQGET ended with reason code %d\n", Reason);
             break;
      }
      goto MOD_EXIT;
    }

    /******************************************************************/
    /* Only expect PCF event messages on this queue                   */
    /******************************************************************/
    if (memcmp(Mqmd.Format, MQFMT_EVENT, MQ_FORMAT_LENGTH))
    {
      printf("Unexpected message format '%8.8s' received\n", Mqmd.Format);
      continue;
    }

    /*******************************************************************/
    /* Build the output by parsing the received PCF message, first the */
    /* header, then each of the parameters                             */
    /*******************************************************************/
    pCfh = (MQCFH *)pBuffer;

    if (pCfh->Reason != MQRC_NONE)
    {
      printf("-----------------------------------------------------------------\n");
      printf("Event Message Received\n");

      Parm = ParmToString(pCfh->Command);
      if (Parm != NULL)
      {
        printf("Command  :%s \n",Parm);
      }
      else
      {
        printf("Command  :%d \n",pCfh->Command);
      }

      printf("CompCode :%d\n"   ,pCfh->CompCode);

      Parm = ParmToString(pCfh->Reason);
      if (Parm != NULL)
      {
        printf("Reason   :%s \n",Parm);
      }
      else
      {
        printf("Reason   :%d \n",pCfh->Reason);
      }
    }

    pPCFCmd  = (PMQCHAR)  (pCfh+1);
    printf("-----------------------------------------------------------------\n");
    while(pCfh->ParameterCount--)
    {
      pCfst = (MQCFST *) pPCFCmd;
      switch(pCfst->Type)
      {
        case MQCFT_STRING:
          Parm = ParmToString(pCfst->Parameter);
          if (Parm != NULL)
          {
            printf("%-32s",Parm);
          }
          else
          {
            printf("%-32d",pCfst->Parameter);
          }

          fwrite(pCfst->String, pCfst->StringLength, 1, stdout);
          pPCFCmd += pCfst->StrucLength;
          break;

        default:
          printf("Unrecoginised datatype %d returned\n", pCfst->Type);
          goto MOD_EXIT;
      }
      putchar('\n');
    }
    printf("-----------------------------------------------------------------\n");
  }

MOD_EXIT:
  return;
}

/******************************************************************************/
/* Function: ParmToString                                                     */
/******************************************************************************/
/*                                                                            */
/* Input Parameters:  Parameter for which to get string description           */
/*                                                                            */
/* Output Parameters: None                                                    */
/*                                                                            */
/* Logic: Takes a parameter as input and returns a pointer to a string        */
/*        description for that parameter, or NULL if the parameter does not   */
/*        have an associated string description                               */
/*                                                                            */
/******************************************************************************/

static PMQCHAR ParmToString(MQLONG Parameter)
{
  MQINT64 i;
  for (i = 0; i < sizeof(ParmTable) / sizeof(ParmTableEntry); i++)
  {
    if ((ParmTable[i].ConstVal == Parameter) && ParmTable[i].Desc)
    {
      return ParmTable[i].Desc;
    }
  }
  return NULL;
}

