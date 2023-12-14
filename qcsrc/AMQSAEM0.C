/* %Z% %W% %I% %E% %U% */
/*********************************************************************/
/*                                                                   */
/* Program name: AMQSAEM0                                            */
/*                                                                   */
/* Description: Sample API exit that manipulates message properties  */
/*   <copyright                                                      */
/*   notice="lm-source-program"                                      */
/*   pids="5724-H72,"                                                */
/*   years="1994,2016"                                               */
/*   crc="1335110557" >                                              */
/*   Licensed Materials - Property of IBM                            */
/*                                                                   */
/*   5724-H72,                                                       */
/*                                                                   */
/*   (C) Copyright IBM Corp. 1994, 2016 All Rights Reserved.         */
/*                                                                   */
/*   US Government Users Restricted Rights - Use, duplication or     */
/*   disclosure restricted by GSA ADP Schedule Contract with         */
/*   IBM Corp.                                                       */
/*   </copyright>                                                    */
/*********************************************************************/
/*                                                                   */
/* Function:                                                         */
/*                                                                   */
/*                                                                   */
/*   AMQSAEM is a sample C API exit that manipulates message         */
/*   properties.                                                     */
/*                                                                   */
/*   AMQSAEM is a model of an exit that wants to add a specific      */
/*   property to each message that is put, and then wants to delete  */
/*   it from the message before it is got by the end application. By */
/*   using the ExitProperties field of the MQXEPO the end            */
/*   will not observe the API eixt property at all.                  */
/*                                                                   */
/*      -- Configure the queue manager                               */
/*                                                                   */
/*         On UNIX add a stanza like the following to the            */
/*         "qm.ini" file                                             */
/*                                                                   */
/*         ApiExitLocal:                                             */
/*           Sequence=101                                            */
/*           Function=EntryPoint                                     */
/*           Module=<Module>                                         */
/*           Name=SampleApiExit                                      */
/*                                                                   */
/*           ... where the <Module> is                               */
/*                 AIX:         /usr/mqm/samp/bin/amqsaem            */
/*                 Other UNIXs: /opt/mqm/samp/bin/amqsaem            */
/*                                                                   */
/*         On Windows set the equivalent attributes in the           */
/*         registry.                                                 */
/*                                                                   */
/*      -- Make sure the Module is accessible to MQ                  */
/*                                                                   */
/*      -- Restart the Queue Manager to pick up these attributes     */
/*                                                                   */
/*   NOTE: On platforms that support separate threaded libraries     */
/*   (ie AIX, HPUX and Linux) both a non-threaded and a threaded     */
/*   version of an ApiExit module must be provided.  The threaded    */
/*   version of the ApiExit module must have an "_r" suffix.  The    */
/*   threaded version of the MQ Application Stub will implicitly     */
/*   append "_r" to the given Module name before it is loaded.       */
/*                                                                   */
/*********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <cmqec.h>

/*********************************************************************/
/*                                                                   */
/* Standard MQ Entrypoint                                            */
/*                                                                   */
/*********************************************************************/

void MQStart(){;}

/*********************************************************************/
/*                                                                   */
/* Initialization Entrypoint                                         */
/*                                                                   */
/*********************************************************************/

MQ_INIT_EXIT Initialize;

void Initialize ( PMQAXP   pExitParms
                , PMQAXC   pExitContext
                , PMQLONG  pCompCode
                , PMQLONG  pReason )
{
  /*******************************************************************/
  /* No action required.                                             */
  /*******************************************************************/

  return;
}

/*********************************************************************/
/*                                                                   */
/* Generic MQPUT/MQPUT1 Before function                              */
/*                                                                   */
/*********************************************************************/

void PutBeforeFunction ( PMQAXP    pExitParms
                       , PMQHCONN  pHconn
                       , PMQLONG   pCompCode
                       , PMQLONG   pReason )
{
  MQSMPO  SetPropOpts = {MQSMPO_DEFAULT};
  MQCHARV Name        = {MQCHARV_DEFAULT};
  MQPD    PropDesc    = {MQPD_DEFAULT};
  MQCHAR  Value[]     = "Blue";

  /*******************************************************************/
  /* If the MQAXP structure is version 2 or greater then the exit    */
  /* has been passed a message handle containing the exit            */
  /* properties.                                                     */
  /*******************************************************************/
  if(pExitParms->Version >= MQAXP_VERSION_2)
  {
    if( (pExitParms->ExitMsgHandle != MQHM_NONE)          &&
        (pExitParms->ExitMsgHandle != MQHM_UNUSABLE_HMSG) )
    {
      /***************************************************************/
      /* Set the desired property name using the MQSETMP call.       */
      /***************************************************************/
      Name.VSPtr            = "ApiExit.Color";
      Name.VSLength         = (MQLONG)strlen(Name.VSPtr);

      pExitParms->Hconfig->MQSETMP_Call( *pHconn
                                       , pExitParms->ExitMsgHandle
                                       , &SetPropOpts
                                       , &Name
                                       , &PropDesc
                                       , MQTYPE_STRING
                                       , (MQLONG)strlen(Value)
                                       , Value
                                       , pCompCode
                                       , pReason );

      if(*pReason != MQRC_NONE)
      {
        pExitParms->ExitResponse = MQXCC_FAILED;
        goto exit;
      }
    }
    else
    {
      /***************************************************************/
      /* An ExitMsgHandle was expected and not received.             */
      /***************************************************************/
      pExitParms->ExitResponse = MQXCC_FAILED;
      goto exit;
    }
  }
  else
  {
    /*****************************************************************/
    /* An ExitMsgHandle was expected and not received.               */
    /*****************************************************************/
    pExitParms->ExitResponse = MQXCC_FAILED;
    goto exit;
  }

exit:
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
                         , PMQLONG   pReason )
{
  PutBeforeFunction( pExitParms
                   , pHconn
                   , pCompCode
                   , pReason );

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
                         , PMQLONG   pReason )
{
  PutBeforeFunction( pExitParms
                   , pHconn
                   , pCompCode
                   , pReason );

  return;
}

/*********************************************************************/
/*                                                                   */
/* Generic MQGET/MQCB Before function                                */
/*                                                                   */
/*********************************************************************/

void GetBeforeFunction ( PMQAXP    pExitParms
                       , PMQAXC    pExitContext )
{
  /*******************************************************************/
  /* If the MQAXP structure is version 2 or greater then the exit    */
  /* has been passed a message handle containing the exit            */
  /* properties.                                                     */
  /*******************************************************************/
  if(pExitParms->Version >= MQAXP_VERSION_2)
  {
    /*****************************************************************/
    /* If the message is being got by an MCA, the command server of  */
    /* by runmqsc then we don't want to populate the ExitMsgHandle.  */
    /*****************************************************************/
    if( (pExitContext->Environment == MQXE_MCA)            ||
        (pExitContext->Environment == MQXE_COMMAND_SERVER) ||
        (pExitContext->Environment == MQXE_MQSC)           )
    {
      pExitParms->ExitMsgHandle = MQHM_NONE;
    }
  }
  else
  {
    /*****************************************************************/
    /* An ExitMsgHandle was expected and not received.               */
    /*****************************************************************/
    pExitParms->ExitResponse = MQXCC_FAILED;
    goto exit;
  }

exit:
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
                         , PMQLONG   pReason )
{
  GetBeforeFunction( pExitParms
                   , pExitContext );

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
                         , PMQLONG   pReason )
{
  if(*pOperation & MQOP_REGISTER)
  {
    GetBeforeFunction( pExitParms
                     , pExitContext );
  }

  return;
}

/*********************************************************************/
/*                                                                   */
/* Generic MQGET/MQCTL After function                                */
/*                                                                   */
/*********************************************************************/

void GetAfterFunction ( PMQAXP    pExitParms
                      , PMQHCONN  pHconn
                      , PMQLONG   pCompCode
                      , PMQLONG   pReason )
{
  MQIMPO   InqPropOpts = {MQIMPO_DEFAULT};
  MQCHARV  Name        = {MQCHARV_DEFAULT};
  MQPD     PropDesc    = {MQPD_DEFAULT};
  MQLONG   Type        = MQTYPE_STRING;
  MQCHAR64 Value;
  MQLONG   DataLength;
  MQLONG   CompCode    = MQCC_OK;
  MQLONG   Reason      = MQRC_NONE;

  if(*pCompCode != MQCC_FAILED)
  {
    /*****************************************************************/
    /* If the MQAXP structure is version 2 or greater then the exit  */
    /* has been passed a message handle containing the exit          */
    /* properties.                                                   */
    /*****************************************************************/
    if(pExitParms->Version >= MQAXP_VERSION_2)
    {
      if( (pExitParms->ExitMsgHandle != MQHM_NONE)          &&
          (pExitParms->ExitMsgHandle != MQHM_UNUSABLE_HMSG) )
      {
        /*************************************************************/
        /* Inquire the desired property name using the MQINQMP call. */
        /*************************************************************/
        Name.VSPtr            = "ApiExit.Color";
        Name.VSLength         = (MQLONG)strlen(Name.VSPtr);

        pExitParms->Hconfig->MQINQMP_Call( *pHconn
                                         , pExitParms->ExitMsgHandle
                                         , &InqPropOpts
                                         , &Name
                                         , &PropDesc
                                         , &Type
                                         , sizeof(Value)
                                         , Value
                                         , &DataLength
                                         , &CompCode
                                         , &Reason );
      }
      else
      {
        /*************************************************************/
        /* An ExitMsgHandle was expected and not received.           */
        /*************************************************************/
        pExitParms->ExitResponse = MQXCC_FAILED;
        goto exit;
      }
    }
    else
    {
      /***************************************************************/
      /* An ExitMsgHandle was expected and not received.             */
      /***************************************************************/
      pExitParms->ExitResponse = MQXCC_FAILED;
      goto exit;
    }
  }

exit:
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
                         , PMQLONG   pReason )
{
  GetAfterFunction( pExitParms
                  , pHconn
                  , pCompCode
                  , pReason );

  return;
}

/*********************************************************************/
/*                                                                   */
/* Before Callback Entrypoint                                        */
/*                                                                   */
/*********************************************************************/

MQ_CALLBACK_EXIT CallbackBefore;

void MQENTRY CallbackBefore ( PMQAXP    pExitParms
                            , PMQAXC    pExitContext
                            , PMQHCONN  pHconn
                            , PPMQMD    ppMsgDesc
                            , PPMQGMO   ppGetMsgOpts
                            , PPMQVOID  ppBuffer
                            , PPMQCBC   ppMQCBContext )
{
  GetAfterFunction( pExitParms
                  , pHconn
                  , &(*ppMQCBContext)->CompCode
                  , &(*ppMQCBContext)->Reason );

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
                        , PMQLONG  pReason )
{
  MQXEPO ExitOpts = {MQXEPO_DEFAULT};

  /*******************************************************************/
  /* Make sure that the Hconfig supplied contains the interface      */
  /* entry points.                                                   */
  /*******************************************************************/
  if(memcmp(pExitParms->Hconfig->StrucId, MQIEP_STRUC_ID, 4))
  {
    pExitParms->ExitResponse = MQXCC_FAILED;
    goto exit;
  }

  /*******************************************************************/
  /* Set the ExitProperties field to "ApiExit". This will cause all  */
  /* properties prefixed by "ApiExit." to be made available via the  */
  /* ExitMsgHandle field of the MQAXP structure after an MQGET.      */
  /*******************************************************************/
  ExitOpts.ExitProperties.VSPtr    = "ApiExit";
  ExitOpts.ExitProperties.VSLength =
    (MQLONG)strlen(ExitOpts.ExitProperties.VSPtr);

  /*******************************************************************/
  /* Register the initialization entrypoint                          */
  /*******************************************************************/

  pExitParms->Hconfig->MQXEP_Call( pExitParms->Hconfig
                                 , MQXR_CONNECTION
                                 , MQXF_INIT
                                 , (PMQFUNC) Initialize
                                 , &ExitOpts
                                 , pCompCode
                                 , pReason );

  if(*pReason != MQRC_NONE)
  {
    pExitParms->ExitResponse = MQXCC_FAILED;
    goto exit;
  }

  /*******************************************************************/
  /* Register the MQPUT entrypoint                                   */
  /*******************************************************************/

  pExitParms->Hconfig->MQXEP_Call( pExitParms->Hconfig
                                 , MQXR_BEFORE
                                 , MQXF_PUT
                                 , (PMQFUNC) PutBefore
                                 , NULL
                                 , pCompCode
                                 , pReason );

  if(*pReason != MQRC_NONE)
  {
    pExitParms->ExitResponse = MQXCC_FAILED;
    goto exit;
  }

  /*******************************************************************/
  /* Register the MQPUT1 entrypoint                                  */
  /*******************************************************************/

  pExitParms->Hconfig->MQXEP_Call( pExitParms->Hconfig
                                 , MQXR_BEFORE
                                 , MQXF_PUT1
                                 , (PMQFUNC) Put1Before
                                 , NULL
                                 , pCompCode
                                 , pReason );

  if(*pReason != MQRC_NONE)
  {
    pExitParms->ExitResponse = MQXCC_FAILED;
    goto exit;
  }

  /*******************************************************************/
  /* Register the before MQGET entrypoint                            */
  /*******************************************************************/

  pExitParms->Hconfig->MQXEP_Call( pExitParms->Hconfig
                                 , MQXR_BEFORE
                                 , MQXF_GET
                                 , (PMQFUNC) GetBefore
                                 , NULL
                                 , pCompCode
                                 , pReason );

  if(*pReason != MQRC_NONE)
  {
    pExitParms->ExitResponse = MQXCC_FAILED;
    goto exit;
  }

  /*******************************************************************/
  /* Register the after MQGET entrypoint                             */
  /*******************************************************************/

  pExitParms->Hconfig->MQXEP_Call( pExitParms->Hconfig
                                 , MQXR_AFTER
                                 , MQXF_GET
                                 , (PMQFUNC) GetAfter
                                 , NULL
                                 , pCompCode
                                 , pReason );

  if(*pReason != MQRC_NONE)
  {
    pExitParms->ExitResponse = MQXCC_FAILED;
    goto exit;
  }

  /*******************************************************************/
  /* Register the before MQCB entrypoint                             */
  /*******************************************************************/

  pExitParms->Hconfig->MQXEP_Call( pExitParms->Hconfig
                                 , MQXR_BEFORE
                                 , MQXF_CB
                                 , (PMQFUNC) CbBefore
                                 , NULL
                                 , pCompCode
                                 , pReason );

  if(*pReason != MQRC_NONE)
  {
    pExitParms->ExitResponse = MQXCC_FAILED;
    goto exit;
  }

  /*******************************************************************/
  /* Register the before callback entrypoint                         */
  /*******************************************************************/

  pExitParms->Hconfig->MQXEP_Call( pExitParms->Hconfig
                                 , MQXR_BEFORE
                                 , MQXF_CALLBACK
                                 , (PMQFUNC) CallbackBefore
                                 , NULL
                                 , pCompCode
                                 , pReason );

  if(*pReason != MQRC_NONE)
  {
    pExitParms->ExitResponse = MQXCC_FAILED;
    goto exit;
  }

exit:
  return;
}


