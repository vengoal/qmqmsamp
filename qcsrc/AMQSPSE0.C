static const char sccsid[] = "%Z% %W% %I% %E% %U%";
/******************************************************************************/
/*                                                                            */
/* Program name: AMQSPSE                                                      */
/*                                                                            */
/* Description: Sample publish exit                                           */
/*                                                                            */
/*   <copyright                                                               */
/*   notice="lm-source-program"                                               */
/*   pids="5724-H72"                                                          */
/*   years="2009,2016"                                                        */
/*   crc="2684795735" >                                                       */
/*   Licensed Materials - Property of IBM                                     */
/*                                                                            */
/*   5724-H72                                                                 */
/*                                                                            */
/*   (C) Copyright IBM Corp. 2009, 2016 All Rights Reserved.                  */
/*                                                                            */
/*   US Government Users Restricted Rights - Use, duplication or              */
/*   disclosure restricted by GSA ADP Schedule Contract with                  */
/*   IBM Corp.                                                                */
/*   </copyright>                                                             */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Function:                                                                  */
/*                                                                            */
/*   AMQSPSE is a sample C publish exit.                                      */
/*                                                                            */
/*   AMQSPSE is a model of an exit that wants to intercept a publication      */
/*   before it is delivered to a subscriber. The exit can then, for example,  */
/*   alter the message headers, payload or destination or it can even prevent */
/*   the message being published to that subscriber at all. Steps for running */
/*   the sample are given below:                                              */
/*                                                                            */
/*      -- Configure the queue manager                                        */
/*                                                                            */
/*         Add a stanza like the following to the "qm.ini" file               */
/*                                                                            */
/*         PublishSubscribe:                                                  */
/*           PublishExitPath=<Module>                                         */
/*           PublishExitFunction=EntryPoint                                   */
/*                                                                            */
/*           Where <Module> for the default installation path is:             */
/*             AIX:         /usr/mqm/samp/bin/amqspse                         */
/*             Other UNIXs: /opt/mqm/samp/bin/amqspse                         */
/*             Win:         C:\Program Files\IBM\MQ\tools-                    */
/*                          \c\Samples\Bin\amqspse                            */
/*                                                                            */
/*      -- Make sure the Module is accessible to MQ                           */
/*                                                                            */
/*      -- Restart the Queue Manager to pick up these attributes              */
/*                                                                            */
/*      -- In the application process to be traced, describe where the trace  */
/*         files should be written to. For example:                           */
/*                                                                            */
/*         On UNIX, make sure the directory "/var/mqm/trace" exists and       */
/*         export the following environment variable                          */
/*                                                                            */
/*         "export MQPSE_TRACE_LOGFILE=/var/mqm/trace/PubTrace"               */
/*                                                                            */
/*         On Windows, make sure the directory "C:\temp" exists and set the   */
/*         following environment variable                                     */
/*                                                                            */
/*         "set MQPSE_TRACE_LOGFILE=C:\temp\PubTrace"                         */
/*                                                                            */
/*         The queue manager needs to see the above environment variable,     */
/*         therefore on UNIX systems it needs to be set in the environment    */
/*         where the queue manager is started, on Windows systems it needs to */
/*         be set in the 'System' environment variables.                      */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Notes:                                                                     */
/*                                                                            */
/* AMQSPSE must be built into a dynamically loadable library                  */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/* Includes                                                                   */
/******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <time.h>

#include <cmqec.h>

#if (MQAT_DEFAULT == MQAT_WINDOWS_NT)
  #include <windows.h>
#else
  #include <sys/types.h>
  #include <sys/time.h>
  #include <pthread.h>
#endif

/******************************************************************************/
/* Defines                                                                    */
/******************************************************************************/
#ifndef OK
  #define OK                0                   /* define OK as zero         */
#endif

#if (MQAT_DEFAULT == MQAT_WINDOWS_NT)
  #define HMUTEXOBJ HANDLE
#else
  #define HMUTEXOBJ pthread_mutex_t *
#endif

#define MTX_NAME "amqspseSampleMutex"

/*----------------------------------------------------------------------------*/
/* Default US English CCSID for compiler on various platforms, if this is not */
/* the value used by your compiler you will need to change this.              */
/*----------------------------------------------------------------------------*/
#if (MQAT_DEFAULT == MQAT_WINDOWS_NT)
  #define COMPILER_CCSID    850
#elif (MQAT_DEFAULT == MQAT_OS400)
  #define COMPILER_CCSID    37
#elif (MQAT_DEFAULT == MQAT_MVS)
  #define COMPILER_CCSID    1047
#else
  #define COMPILER_CCSID    819
#endif

#ifdef MQIEP_CURRENT_VERSION
  #define MQXCNVC           pExitParms->pEntryPoints->MQXCNVC_Call
#endif

/******************************************************************************/
/* Structures                                                                 */
/******************************************************************************/

/******************************************************************************/
/* Definition of exit user area structure                                     */
/******************************************************************************/
typedef struct myExitUserArea
{
  FILE *pLogFile;                               /* log file                  */
  HMUTEXOBJ hMutexObj;                          /* handle to mutex object    */
}  MYEXITUSERAREA;

/******************************************************************************/
/* Function Prototypes                                                        */
/******************************************************************************/
MQ_PUBLISH_EXIT EntryPoint;
static void Initialize(PMQPSXP pExitParms);
static void Publish(PMQPSXP pExitParms, PMQPBC  pPubContext, PMQSBC  pSubContext);
static void Terminate(PMQPSXP pExitParms);
static MQINT32 CreateOSMutexObj(PMQPSXP pExitParms, char *Name, HMUTEXOBJ* phMutex);
static MQINT32 RequestOSMutexObj(PMQPSXP pExitParms, HMUTEXOBJ hMutex);
static MQINT32 ReleaseOSMutexObj(PMQPSXP pExitParms, HMUTEXOBJ hMutex);
static MQINT32 DeleteOSMutexObj(PMQPSXP pExitParms, HMUTEXOBJ hMutex);
static MQINT32 Convert(PMQPSXP   pExitParms,
                       MQINT32   SrcCCSID,
                       MQINT32   SrcLen,
                       PMQCHAR   pSrcBuf,
                       MQINT32   TgtCCSID,
                       PMQINT32  pTgtLen,
                       PMQCHAR   pTgtBuf);

/******************************************************************************/
/* Macros                                                                     */
/******************************************************************************/

/*----------------------------------------------------------------------------*/
/* Get the minimum of two values                                              */
/*----------------------------------------------------------------------------*/
#ifndef min
  #define min(a,b)        (((a) < (b)) ? (a) : (b))
#endif

/*----------------------------------------------------------------------------*/
/* Get the date and time down to the millisecond                              */
/*                                                                            */
/* A string of the form "YYYY-MM-DD HH:MM:SS.TTT" will be returned, that is:  */
/* 4 digits of year; 2 digits of month; 2 digits of day; 2 digits of hours;   */
/* 2 digits of seconds and 3 digits of thousandths.                           */
/*----------------------------------------------------------------------------*/
#if (MQAT_DEFAULT == MQAT_WINDOWS_NT)

  #define GetDateAndTimeToMS(pStr)                                             \
  {                                                                            \
    SYSTEMTIME sysTime;                        /* system time structure     */ \
                                                                               \
    GetLocalTime(&sysTime);                                                    \
                                                                               \
    sprintf(pStr, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d.%.3d",                        \
                  sysTime.wYear,                                               \
                  sysTime.wMonth,                                              \
                  sysTime.wDay,                                                \
                  sysTime.wHour,                                               \
                  sysTime.wMinute,                                             \
                  sysTime.wSecond,                                             \
                  sysTime.wMilliseconds);                                      \
  }

#elif (MQAT_DEFAULT == MQAT_OS400) || !defined(CLOCK_REALTIME)

  #define GetDateAndTimeToMS(pStr)                                             \
  {                                                                            \
    struct timeval timeVal;                    /* sec & microsec structure  */ \
    struct timezone timeZone;                  /* mins west GMT & dst flag  */ \
    struct tm timeStrt;                        /* time structure            */ \
    struct tm *pTimeStrt;                      /* ptr to time structure     */ \
                                                                               \
    gettimeofday(&timeVal, &timeZone);                                         \
    pTimeStrt = localtime_r(&timeVal.tv_sec, &timeStrt);                       \
                                                                               \
    sprintf(pStr, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d.%.3d",                        \
                  pTimeStrt->tm_year + 1900,                                   \
                  pTimeStrt->tm_mon + 1,                                       \
                  pTimeStrt->tm_mday,                                          \
                  pTimeStrt->tm_hour,                                          \
                  pTimeStrt->tm_min,                                           \
                  pTimeStrt->tm_sec,                                           \
                  (MQINT32) timeVal.tv_usec / 1000);                           \
  }

#else

  #define GetDateAndTimeToMS(pStr)                                             \
  {                                                                            \
    struct timespec timeSpec;                  /* sec & nanosec structure   */ \
    struct tm timeStrt;                        /* time structure            */ \
    struct tm *pTimeStrt;                      /* ptr to time structure     */ \
                                                                               \
    clock_gettime(CLOCK_REALTIME, &timeSpec);                                  \
                                                                               \
    pTimeStrt = localtime_r(&timeSpec.tv_sec, &timeStrt);                      \
                                                                               \
    sprintf(pStr, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d.%.3d",                        \
                  pTimeStrt->tm_year + 1900,                                   \
                  pTimeStrt->tm_mon + 1,                                       \
                  pTimeStrt->tm_mday,                                          \
                  pTimeStrt->tm_hour,                                          \
                  pTimeStrt->tm_min,                                           \
                  pTimeStrt->tm_sec,                                           \
                  (MQINT32)(timeSpec.tv_nsec / 1000000));                      \
  }

#endif

/******************************************************************************/
/*                                                                            */
/* Function: MQStart                                                          */
/*                                                                            */
/* Description: Standard MQ entry point for dynamically loadable library      */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Function:                                                                  */
/* ---------                                                                  */
/* Needed by MQ at load time, no functional code required.                    */
/*                                                                            */
/* Input Parameters:                                                          */
/* -----------------                                                          */
/* None                                                                       */
/*                                                                            */
/* In/Out Parameters:                                                         */
/* ------------------                                                         */
/* None                                                                       */
/*                                                                            */
/* Output Parameters:                                                         */
/* ------------------                                                         */
/* None                                                                       */
/*                                                                            */
/* Returns:                                                                   */
/* --------                                                                   */
/* None                                                                       */
/*                                                                            */
/******************************************************************************/
void MQENTRY MQStart(void)
{

}

/******************************************************************************/
/*                                                                            */
/* Function: EntryPoint                                                       */
/*                                                                            */
/* Description: Exit entry point                                              */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Function:                                                                  */
/* ---------                                                                  */
/* Entry point is called for three things, initially for initialization,      */
/* then whenever a publish is about to be delivered to a subscriber, and      */
/* finally for termination.                                                   */
/*                                                                            */
/* Input Parameters:                                                          */
/* -----------------                                                          */
/* PMQPBC  pPubContext            - publish exit publication context          */
/*                                                                            */
/* In/Out Parameters:                                                         */
/* ------------------                                                         */
/* PMQPSXP pExitParms             - publish exit parameters                   */
/* PMQSBC  pSubContext            - publish exit subscription context         */
/*                                                                            */
/* Output Parameters:                                                         */
/* ------------------                                                         */
/* None                                                                       */
/*                                                                            */
/* Returns:                                                                   */
/* --------                                                                   */
/* None                                                                       */
/*                                                                            */
/******************************************************************************/
void MQENTRY EntryPoint(PMQPSXP pExitParms, PMQPBC  pPubContext, PMQSBC  pSubContext)
{
  /***************************************************************************/
  /* ExitReason contains the reason why the exit has been called. Call a     */
  /* function to handle each reason separately.                              */
  /***************************************************************************/
  switch (pExitParms->ExitReason)
  {
  /************************************************************************/
  /* The publish exit is being called for initialization.                 */
  /************************************************************************/
  case MQXR_INIT:
    Initialize(pExitParms);
    break;

    /************************************************************************/
    /* The publish exit is being called because a publish operation is      */
    /* taking place.                                                        */
    /************************************************************************/
  case MQXR_PUBLICATION:
    Publish(pExitParms, pPubContext, pSubContext);
    break;

    /************************************************************************/
    /* The publish exit is being called for termination.                    */
    /************************************************************************/
  case MQXR_TERM:
    Terminate(pExitParms);
    break;

    /************************************************************************/
    /* The publish exit is being called for an unknown reason.              */
    /************************************************************************/
  default:
    pExitParms->ExitResponse = MQXCC_FAILED;
    break;
  } /* endswitch */

  return;
}

/******************************************************************************/
/*                                                                            */
/* Function: Initialize                                                       */
/*                                                                            */
/* Description: Initialization                                                */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Function:                                                                  */
/* ---------                                                                  */
/* Allocate storage for my exit user area and open the log file if required.  */
/*                                                                            */
/* Input Parameters:                                                          */
/* -----------------                                                          */
/* None                                                                       */
/*                                                                            */
/* In/Out Parameters:                                                         */
/* ------------------                                                         */
/* PMQPSXP pExitParms             - publish exit parameters                   */
/*                                                                            */
/* Output Parameters:                                                         */
/* ------------------                                                         */
/* None                                                                       */
/*                                                                            */
/* Returns:                                                                   */
/* --------                                                                   */
/* None                                                                       */
/*                                                                            */
/******************************************************************************/
static void Initialize(PMQPSXP pExitParms)
{
  MQINT32 rc=OK;                                /* return code               */
  MYEXITUSERAREA **ppExitUserArea;              /* p to p to ExitUserArea    */
  MYEXITUSERAREA *pMyExitUserArea;              /* ptr to my exit user area  */
  char *pEnvStr;                                /* ptr to environment string */
  char logFileName[1024] = "";                  /* log file name             */
  FILE *pLogFile = NULL;                        /* ptr to log file           */
  char dateAndTimeStr[32];                      /* date and time string      */

  /***************************************************************************/
  /* Set up the pointer to the pointer to the ExitUserArea.                  */
  /***************************************************************************/
  ppExitUserArea = (MYEXITUSERAREA **) &pExitParms->ExitUserArea;

  /***************************************************************************/
  /* Allocate some storage and store the pointer to it in the ExitUserArea.  */
  /* Note the ExitUserArea is 16 bytes long and any changes we make to it    */
  /* are preserved across invocations of the exit.                           */
  /***************************************************************************/
  pMyExitUserArea = calloc(1, sizeof(MYEXITUSERAREA));

  if (pMyExitUserArea)
  {
    /************************************************************************/
    /* Save the pointer to the allocated storage in the ExitUserArea.       */
    /************************************************************************/
    *ppExitUserArea = pMyExitUserArea;

    /************************************************************************/
    /* See if a log file name has been specified.                           */
    /************************************************************************/
    pEnvStr = getenv("MQPSE_TRACE_LOGFILE");

    if (pEnvStr)
    {
      /*********************************************************************/
      /* A log file name was specified, open it for this thread.           */
      /* Note on Windows use the commit flag (c) to make the fflush calls  */
      /* to write the data to disk.                                        */
      /*********************************************************************/
      sprintf(logFileName, "%s.log", pEnvStr);

      pLogFile = fopen(logFileName, "a");

      if (pLogFile == NULL)
      {
        /******************************************************************/
        /* The log file could not be opened. Return a failure response.   */
        /******************************************************************/
        pExitParms->ExitResponse = MQXCC_FAILED;
      }
      else
      {
        /******************************************************************/
        /* The log file is now open. Write that we were called for INIT.  */
        /******************************************************************/
        pMyExitUserArea->pLogFile = pLogFile;

        GetDateAndTimeToMS(dateAndTimeStr);

        /******************************************************************/
        /* So that the log file is not a garbled mess, we will use a      */
        /* mutex to synchronize the writes to it.                         */
        /******************************************************************/
        rc = CreateOSMutexObj(pExitParms, MTX_NAME, &pMyExitUserArea->hMutexObj);

        if (rc == OK)
        {
          rc = RequestOSMutexObj(pExitParms, pMyExitUserArea->hMutexObj);

          if (rc == OK)
          {
            fprintf(pMyExitUserArea->pLogFile,
                    "%23.23s  <<<----- INIT ----->>>\n",
                    dateAndTimeStr);
            fprintf(pMyExitUserArea->pLogFile,
                    "YYYY-MM-DD HH:MM:SS.TTT  "
                    "Topic                                             "
                    "                                                    "
                    "Pub UserId    Sub UserId\n");
            /* Force writes to log file                                 */
            fflush(pMyExitUserArea->pLogFile);

            rc = ReleaseOSMutexObj(pExitParms, pMyExitUserArea->hMutexObj);

          } /* endif */
        } /* endif */

        /******************************************************************/
        /* Return a response.                                             */
        /******************************************************************/
        if (rc == OK)
          pExitParms->ExitResponse = MQXCC_OK;
        else
          pExitParms->ExitResponse = MQXCC_FAILED;
      } /* endif */
    }
    else
    {
      /*********************************************************************/
      /* No log file name was specified so there will be no output.        */
      /*********************************************************************/
      pExitParms->ExitResponse = MQXCC_OK;
    } /* endif */
  }
  else
  {
    /************************************************************************/
    /* Return a failure response if storage couldn't be allocated.          */
    /************************************************************************/
    pExitParms->ExitResponse = MQXCC_FAILED;
    *ppExitUserArea = NULL;
  } /* endif */

  return;
}

/******************************************************************************/
/*                                                                            */
/* Function: Publish                                                          */
/*                                                                            */
/* Description: A publish message is about to be delivered to a subscriber    */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Function:                                                                  */
/* ---------                                                                  */
/* Called whenever publish message is about to be delivered to a subscriber.  */
/* Write to the log file if required.                                         */
/*                                                                            */
/* Input Parameters:                                                          */
/* -----------------                                                          */
/* PMQPBC  pPubContext            - publish exit publication context          */
/*                                                                            */
/* In/Out Parameters:                                                         */
/* ------------------                                                         */
/* PMQPSXP pExitParms             - publish exit parameters                   */
/* PMQSBC  pSubContext            - publish exit subscription context         */
/*                                                                            */
/* Output Parameters:                                                         */
/* ------------------                                                         */
/* None                                                                       */
/*                                                                            */
/* Returns:                                                                   */
/* --------                                                                   */
/* None                                                                       */
/*                                                                            */
/******************************************************************************/
static void Publish(PMQPSXP pExitParms, PMQPBC  pPubContext, PMQSBC  pSubContext)
{
  MQINT32 rc=OK;                                /* return code               */
  MYEXITUSERAREA *pMyExitUserArea;              /* ptr to my exit user area  */
  char dateAndTimeStr[32];                      /* date and time string      */
  MQCHAR topicBuf[MQ_TOPIC_STR_LENGTH];         /* converted topic buffer    */
  MQINT32 topicLen=MQ_TOPIC_STR_LENGTH;         /* converted topic length    */

  /***************************************************************************/
  /* Set up a pointer to my exit user area                                   */
  /***************************************************************************/
  pMyExitUserArea = *((MYEXITUSERAREA**) &pExitParms->ExitUserArea);

  if (pMyExitUserArea)
  {
    /************************************************************************/
    /* Log that we have been called for publish.                            */
    /************************************************************************/
    if (pMyExitUserArea->pLogFile)
    {
      /*********************************************************************/
      /* Convert the topic string to the default US English CCSID          */
      /*********************************************************************/
      rc = Convert(pExitParms,                /* publish exit parameters   */
                   pPubContext->PubTopicString.VSCCSID,  /* source CCSID   */
                   pPubContext->PubTopicString.VSLength, /* source length  */
                   pPubContext->PubTopicString.VSPtr,    /* source buffer  */
                   COMPILER_CCSID,            /* target CCSID              */
                   &topicLen,                 /* target length             */
                   topicBuf);                 /* target buffer             */

      GetDateAndTimeToMS(dateAndTimeStr);

      if (rc == OK)
      {
        rc = RequestOSMutexObj(pExitParms, pMyExitUserArea->hMutexObj);

        /******************************************************************/
        /* Write to the log file regardless of getting the mutex. Write   */
        /* no more that 100 characters of the topic string.               */
        /******************************************************************/
        fprintf(pMyExitUserArea->pLogFile,
                "%23.23s  %-100.*s  %12.12s  %12.12s\n",
                dateAndTimeStr, min(100, topicLen), topicBuf,
                pPubContext->MsgDescPtr->UserIdentifier,
                pExitParms->MsgDescPtr->UserIdentifier);
        fflush(pMyExitUserArea->pLogFile);   /* force writes to log file  */

        if (rc == OK)
          rc = ReleaseOSMutexObj(pExitParms, pMyExitUserArea->hMutexObj);

        /******************************************************************/
        /* Regardless of mutex failures we are going to return OK         */
        /******************************************************************/
        rc = OK;
      } /* endif */
    } /* endif */
  }
  else
  {
    rc = MQXCC_FAILED;
  } /* endif */

  /***************************************************************************/
  /* Return the appropriate response.                                        */
  /***************************************************************************/
  if (rc == OK)
    pExitParms->ExitResponse = MQXCC_OK;
  else
    pExitParms->ExitResponse = MQXCC_FAILED;

  return;
}

/******************************************************************************/
/*                                                                            */
/* Function: Terminate                                                        */
/*                                                                            */
/* Description: Termination                                                   */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Function:                                                                  */
/* ---------                                                                  */
/* Free storage for my exit user area and close the log file if required.     */
/*                                                                            */
/* Input Parameters:                                                          */
/* -----------------                                                          */
/* None                                                                       */
/*                                                                            */
/* In/Out Parameters:                                                         */
/* ------------------                                                         */
/* PMQPSXP pExitParms             - publish exit parameters                   */
/*                                                                            */
/* Output Parameters:                                                         */
/* ------------------                                                         */
/* None                                                                       */
/*                                                                            */
/* Returns:                                                                   */
/* --------                                                                   */
/* None                                                                       */
/*                                                                            */
/******************************************************************************/
static void Terminate(PMQPSXP pExitParms)
{
  MQINT32 rc=OK;                                /* return code               */
  MYEXITUSERAREA *pMyExitUserArea;              /* ptr to exit user area     */
  char dateAndTimeStr[32];                      /* date and time string      */

  /***************************************************************************/
  /* Set up a pointer to my exit user area                                   */
  /***************************************************************************/
  pMyExitUserArea = *((MYEXITUSERAREA**) &pExitParms->ExitUserArea);

  if (pMyExitUserArea)
  {
    /************************************************************************/
    /* If the log file was opened, write we are terminating, then close it. */
    /************************************************************************/
    if (pMyExitUserArea->pLogFile)
    {
      GetDateAndTimeToMS(dateAndTimeStr);

      rc = RequestOSMutexObj(pExitParms, pMyExitUserArea->hMutexObj);

      /*********************************************************************/
      /* Write to the log file regardless of getting the mutex             */
      /*********************************************************************/
      fprintf(pMyExitUserArea->pLogFile,
              "%23.23s  <<<----- TERM ----->>>\n",
              dateAndTimeStr);
      fflush(pMyExitUserArea->pLogFile);   /* force writes to log file  */

      if (rc == OK)
        rc = ReleaseOSMutexObj(pExitParms, pMyExitUserArea->hMutexObj);

      /*********************************************************************/
      /* No need to delete the mutex as other threads will need it.        */
      /*********************************************************************/

      /*********************************************************************/
      /* Now close the log file for this thread.                           */
      /*********************************************************************/
      fclose(pMyExitUserArea->pLogFile);
    } /* endif */

    /************************************************************************/
    /* Free the storage used for the exit user area.                        */
    /************************************************************************/
    free(pMyExitUserArea);
  } /* endif */

  pExitParms->ExitResponse = MQXCC_OK;

  return;
}

/******************************************************************************/
/*                                                                            */
/* Function: CreateOSMutexObj                                                 */
/*                                                                            */
/* Description: Create mutex object                                           */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Function:                                                                  */
/* ---------                                                                  */
/* Create the appropriate operating system mutex object                       */
/*                                                                            */
/* Input Parameters:                                                          */
/* -----------------                                                          */
/* PMQPSXP   pExitParms           - publish exit parameters                   */
/* HMUTEXOBJ hMutex               - handle to mutex                           */
/*                                                                            */
/* In/Out Parameters:                                                         */
/* ------------------                                                         */
/* None                                                                       */
/*                                                                            */
/* Output Parameters:                                                         */
/* ------------------                                                         */
/* None                                                                       */
/*                                                                            */
/* Returns:                                                                   */
/* --------                                                                   */
/* OK                             - all is well                               */
/*                                                                            */
/******************************************************************************/
static MQINT32 CreateOSMutexObj(PMQPSXP pExitParms, char *Name, HMUTEXOBJ* phMutex)
{
#if (MQAT_DEFAULT == MQAT_WINDOWS_NT)
  DWORD rc;
#else
  MQINT32 rc=OK;
  static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#endif
  char dateAndTimeStr[32];                      /* date and time string      */
  MYEXITUSERAREA *pMyExitUserArea;              /* ptr to my exit user area  */

#if (MQAT_DEFAULT == MQAT_WINDOWS_NT)

  /************************************************************************/
  /* First try to open the mutex.                                         */
  /************************************************************************/
  *phMutex = OpenMutex(MUTEX_ALL_ACCESS,
                       FALSE,                /* inherit flag              */
                       Name);

  /************************************************************************/
  /* If we failed to open the mutex, it means no one has created it yet.  */
  /************************************************************************/
  if (*phMutex == NULL)
    *phMutex = CreateMutex(NULL,            /* no security               */
                           FALSE,           /* unowned                   */
                           Name);

  /************************************************************************/
  /* If we failed to create the mutex and got an error of access denied,  */
  /* it means someone else created it in the meantime, we should retry    */
  /* the open one more time.                                              */
  /************************************************************************/
  if (*phMutex == NULL && GetLastError() == ERROR_ACCESS_DENIED)
    *phMutex = OpenMutex(MUTEX_ALL_ACCESS,
                         FALSE,             /* inherit flag              */
                         Name);

  if (*phMutex == NULL)
    rc = GetLastError();
  else
    rc = OK;

#else

  /************************************************************************/
  /* Take advantage of static initialization, eliminating the need for    */
  /* the pthread_once() and pthread_mutex_init() calls.                   */
  /************************************************************************/
  *phMutex = &mutex;                         /* return the address        */

#endif

  if (rc != OK)
  {
    /* Set up a pointer to my exit user area                                */
    pMyExitUserArea = *((MYEXITUSERAREA**) &pExitParms->ExitUserArea);

    GetDateAndTimeToMS(dateAndTimeStr);
    fprintf(pMyExitUserArea->pLogFile,
            "%23.23s  <<<----- Failed to create mutex rc: %d ----->>>\n",
            dateAndTimeStr, rc);
    fflush(pMyExitUserArea->pLogFile);
  } /* endif */

  return(MQINT32) rc;
}

/******************************************************************************/
/*                                                                            */
/* Function: RequestOSMutexObj                                                */
/*                                                                            */
/* Description: Request mutex                                                 */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Function:                                                                  */
/* ---------                                                                  */
/* Request ownership of the appropriate operating system mutex object         */
/*                                                                            */
/* Input Parameters:                                                          */
/* -----------------                                                          */
/* PMQPSXP   pExitParms           - publish exit parameters                   */
/* HMUTEXOBJ hMutex               - handle to mutex object                    */
/*                                                                            */
/* In/Out Parameters:                                                         */
/* ------------------                                                         */
/* None                                                                       */
/*                                                                            */
/* Output Parameters:                                                         */
/* ------------------                                                         */
/* None                                                                       */
/*                                                                            */
/* Returns:                                                                   */
/* --------                                                                   */
/* OK                             - all is well                               */
/*                                                                            */
/******************************************************************************/
static MQINT32 RequestOSMutexObj(PMQPSXP pExitParms, HMUTEXOBJ hMutex)
{
#if (MQAT_DEFAULT == MQAT_WINDOWS_NT)
  DWORD rc;
#else
  MQINT32 rc;
#endif
  char dateAndTimeStr[32];                      /* date and time string      */
  MYEXITUSERAREA *pMyExitUserArea;              /* ptr to my exit user area  */

#if (MQAT_DEFAULT == MQAT_WINDOWS_NT)

  rc = WaitForSingleObject(hMutex, INFINITE);

  if (rc == WAIT_FAILED)
    rc = GetLastError();
  else
    rc = OK;

#else

  rc = pthread_mutex_lock(hMutex);

#endif

  if (rc != OK)
  {
    /* Set up a pointer to my exit user area                                */
    pMyExitUserArea = *((MYEXITUSERAREA**) &pExitParms->ExitUserArea);

    GetDateAndTimeToMS(dateAndTimeStr);
    fprintf(pMyExitUserArea->pLogFile,
            "%23.23s  <<<----- Failed to get mutex rc: %d ----->>>\n",
            dateAndTimeStr, rc);
    fflush(pMyExitUserArea->pLogFile);
  } /* endif */

  return(MQINT32) rc;
}

/******************************************************************************/
/*                                                                            */
/* Function: ReleaseOSMutexObj                                                */
/*                                                                            */
/* Description: Release mutex                                                 */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Function:                                                                  */
/* ---------                                                                  */
/* Release ownership of the appropriate operating system mutex object         */
/*                                                                            */
/* Input Parameters:                                                          */
/* -----------------                                                          */
/* PMQPSXP   pExitParms           - publish exit parameters                   */
/* HMUTEXOBJ hMutex               - handle to mutex object                    */
/*                                                                            */
/* In/Out Parameters:                                                         */
/* ------------------                                                         */
/* None                                                                       */
/*                                                                            */
/* Output Parameters:                                                         */
/* ------------------                                                         */
/* None                                                                       */
/*                                                                            */
/* Returns:                                                                   */
/* --------                                                                   */
/* OK                             - all is well                               */
/*                                                                            */
/******************************************************************************/
static MQINT32 ReleaseOSMutexObj(PMQPSXP pExitParms, HMUTEXOBJ hMutex)
{
#if (MQAT_DEFAULT == MQAT_WINDOWS_NT)
  DWORD rc;
#else
  MQINT32 rc;
#endif
  char dateAndTimeStr[32];                      /* date and time string      */
  MYEXITUSERAREA *pMyExitUserArea;              /* ptr to my exit user area  */

#if (MQAT_DEFAULT == MQAT_WINDOWS_NT)

  if (ReleaseMutex(hMutex) == 0)
    rc = GetLastError();
  else
    rc = OK;

#else

  rc = pthread_mutex_unlock(hMutex);

#endif

  if (rc != OK)
  {
    /* Set up a pointer to my exit user area                                */
    pMyExitUserArea = *((MYEXITUSERAREA**) &pExitParms->ExitUserArea);

    GetDateAndTimeToMS(dateAndTimeStr);
    fprintf(pMyExitUserArea->pLogFile,
            "%23.23s  <<<----- Failed to release mutex rc: %d ----->>>\n",
            dateAndTimeStr, rc);
    fflush(pMyExitUserArea->pLogFile);
  } /* endif */

  return(MQINT32) rc;
}

/******************************************************************************/
/*                                                                            */
/* Function: DeleteOSMutexObj                                                 */
/*                                                                            */
/* Description: Delete mutex                                                  */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Function:                                                                  */
/* ---------                                                                  */
/* Delete the appropriate operating system mutex object                       */
/*                                                                            */
/* Input Parameters:                                                          */
/* -----------------                                                          */
/* PMQPSXP   pExitParms           - publish exit parameters                   */
/* HMUTEXOBJ hMutex               - handle to mutex object                    */
/*                                                                            */
/* In/Out Parameters:                                                         */
/* ------------------                                                         */
/* None                                                                       */
/*                                                                            */
/* Output Parameters:                                                         */
/* ------------------                                                         */
/* None                                                                       */
/*                                                                            */
/* Returns:                                                                   */
/* --------                                                                   */
/* OK                             - all is well                               */
/*                                                                            */
/******************************************************************************/
static MQINT32 DeleteOSMutexObj(PMQPSXP pExitParms, HMUTEXOBJ hMutex)
{
#if (MQAT_DEFAULT == MQAT_WINDOWS_NT)
  DWORD rc;
#else
  MQINT32 rc;
#endif
  char dateAndTimeStr[32];                      /* date and time string      */
  MYEXITUSERAREA *pMyExitUserArea;              /* ptr to my exit user area  */

#if (MQAT_DEFAULT == MQAT_WINDOWS_NT)

  if (CloseHandle(hMutex) == 0)
    rc = GetLastError();
  else
    rc = OK;

#else

  rc = pthread_mutex_destroy(hMutex);

#endif

  if (rc != OK)
  {
    /* Set up a pointer to my exit user area                                */
    pMyExitUserArea = *((MYEXITUSERAREA**) &pExitParms->ExitUserArea);

    GetDateAndTimeToMS(dateAndTimeStr);
    fprintf(pMyExitUserArea->pLogFile,
            "%23.23s  <<<----- Failed to delete mutex rc: %d ----->>>\n",
            dateAndTimeStr, rc);
    fflush(pMyExitUserArea->pLogFile);
  } /* endif */

  return(MQINT32) rc;
}

/******************************************************************************/
/*                                                                            */
/* Function: Convert                                                          */
/*                                                                            */
/* Description: Convert the topic string into the appropriate CCSID           */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Function:                                                                  */
/* ---------                                                                  */
/* Convert the topic string into the appropriate CCSID                        */
/*                                                                            */
/* Input Parameters:                                                          */
/* -----------------                                                          */
/* PMQPSXP   pExitParms           - publish exit parameters                   */
/* MQINT32   SrcCCSID             - source CCSID                              */
/* MQINT32   SrcLen               - source length                             */
/* PMQCHAR   pSrcBuf              - ptr to source buffer                      */
/* MQINT32   TgtCCSID             - target CCSID                              */
/*                                                                            */
/* In/Out Parameters:                                                         */
/* ------------------                                                         */
/* PMQINT32  pTgtLen              - ptr to target length                      */
/*                                                                            */
/* Output Parameters:                                                         */
/* ------------------                                                         */
/* PMQCHAR   pTgtBuf              - ptr to target buffer                      */
/*                                                                            */
/* Returns:                                                                   */
/* --------                                                                   */
/* OK                             - all is well                               */
/*                                - returns from MQI                          */
/*                                                                            */
/******************************************************************************/
static MQINT32 Convert(PMQPSXP   pExitParms,
                       MQINT32   SrcCCSID,
                       MQINT32   SrcLen,
                       PMQCHAR   pSrcBuf,
                       MQINT32   TgtCCSID,
                       PMQINT32  pTgtLen,
                       PMQCHAR   pTgtBuf)
{
  MQINT32 rc=OK;                                /* return code               */
  MQINT32 compCode;                             /* completion code           */
  MQINT32 reason;                               /* reason                    */
  MQINT32 options;                              /* char conversion options   */
  MQINT32 tgtBufLen=*pTgtLen;                   /* target buffer length      */
  char dateAndTimeStr[32];                      /* date and time string      */
  MYEXITUSERAREA *pMyExitUserArea;              /* ptr to my exit user area  */

  /***************************************************************************/
  /* Set up the options for the MQ data conversion function.                 */
  /***************************************************************************/
  options = MQDCC_DEFAULT_CONVERSION +
            MQDCC_SOURCE_ENC_NATIVE +
            MQDCC_TARGET_ENC_NATIVE;

  /***************************************************************************/
  /* Call the MQ data conversion function.                                   */
  /***************************************************************************/
  MQXCNVC(pExitParms->Hconn,                    /* handle to MQ connection   */
          options,                              /* options                   */
          SrcCCSID,                             /* source CCSID              */
          SrcLen,                               /* source length             */
          pSrcBuf,                              /* source buffer             */
          TgtCCSID,                             /* target CCSID              */
          tgtBufLen,                            /* target buffer length      */
          pTgtBuf,                              /* target buffer             */
          pTgtLen,                              /* target length             */
          &compCode,                            /* completion code           */
          &reason);                             /* reason                    */

  if (compCode != MQCC_OK)
  {
    rc = compCode;

    /* Set up a pointer to my exit user area                                */
    pMyExitUserArea = *((MYEXITUSERAREA**) &pExitParms->ExitUserArea);

    GetDateAndTimeToMS(dateAndTimeStr);
    fprintf(pMyExitUserArea->pLogFile,
            "%23.23s  <<<----- Conversion failed CompCode: %d Reason: %d ----->>>\n",
            dateAndTimeStr, compCode, reason);
    fflush(pMyExitUserArea->pLogFile);
  } /* endif */

  return rc;
}

