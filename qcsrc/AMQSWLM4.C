/* %Z% %W% %I% %E% %U% */
 /********************************************************************/
 /*                                                                  */
 /* Program name: AMQSWLM4                                           */
 /*                                                                  */
 /* Description: Sample C CLWL exit that chooses a destination       */
 /*              queue manager                                       */
 /*   <copyright                                                     */
 /*   notice="lm-source-program"                                     */
 /*   pids="5724-H72"                                                */
 /*   years="1994,2014"                                              */
 /*   crc="4237813149" >                                             */
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
 /********************************************************************/
 /*                                                                  */
 /* Function:                                                        */
 /*                                                                  */
 /*                                                                  */
 /*   AMQSWLM is a sample C exit which chooses a destination queue   */
 /*   manager.  To use this exit:                                    */
 /*                                                                  */
 /*      -- Use RUNMQSC to set the queue manager attribute CLWLDATA  */
 /*         to the name of the channel to the remote queue manager.  */
 /*         (This information is passed to the CLWL exit in the      */
 /*         MQWXP structure in the ExitData field) eg:               */
 /*                                                                  */
 /*             ALTER QMGR CLWLDATA('TO.myqmgr')                     */
 /*                                                                  */
 /*      -- Use RUNMQSC to set the queue manager attribute CLWLEXIT  */
 /*         to the module name followed by the function name in      */
 /*         parenthesis.  eg:                                        */
 /*                                                                  */
 /*             ALTER QMGR CLWLEXIT('amqswlm(clwlFunction)')         */
 /*                                                                  */
 /*      -- Make sure the CLWL module is accessible to MQ            */
 /*                                                                  */
 /*      -- End the queue manager then restart it, to pick up these  */
 /*         changes                                                  */
 /*                                                                  */
 /********************************************************************/

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <limits.h>
 #include <cmqc.h>
 #include <cmqxc.h>
 #include <cmqcfc.h>
 #include <cmqec.h>

 void MQENTRY clwlFunction ( MQWXP *parms )
 {
   MQLONG   index;
   MQLONG   GoodDestination;
   MQWDR  * qmgr;
   MQWQR  * queue;
   MQCD   * cd;
   MQLONG   CompCode = MQCC_OK;
   MQLONG   Reason   = MQRC_NONE;
   MQPTR    NextRecord;

   /******************************************************************/
   /* Find the best destination in the destination array             */
   /******************************************************************/

   for ( index = 0; index < parms->DestinationCount; index++ )
   {
     GoodDestination = 1;
     qmgr = parms->DestinationArrayPtr[ index ];

     parms->pEntryPoints->MQXCLWLN_Call ( parms, qmgr, qmgr->ChannelDefOffset, &NextRecord, &CompCode, &Reason );

     if ((CompCode == MQCC_OK) && NextRecord)
     {
       cd = NextRecord;

       /**************************************************************/
       /* Does the channel to the destination qmgr match the one we  */
       /* are interested in ?                                        */
       /**************************************************************/

       if ( strncmp( cd->ChannelName
                   , parms->ExitData
                   , MQ_CHANNEL_NAME_LENGTH
                   )
          )
       {
         GoodDestination = 0;
       }

       /**************************************************************/
       /* Is the queue PUT enabled ?  Note that the QArrayPtr is     */
       /* NULL in the case of choosing a destination queue manager   */
       /**************************************************************/

       if (parms->QArrayPtr)
       {
         queue = parms->QArrayPtr[ index ];

         if (queue && (queue->InhibitPut == MQQA_PUT_INHIBITED))
         {
           GoodDestination = 0;
         }
       }

       /**************************************************************/
       /* Don't choose destination qmgrs which have not joined the   */
       /* cluster                                                    */
       /**************************************************************/

       if ((qmgr->QMgrFlags & MQQMF_AVAILABLE) == 0)
       {
         GoodDestination = 0;
       }

       /**************************************************************/
       /* Only choose destinations with channels in a good state     */
       /**************************************************************/

       switch ( qmgr->ChannelState )
       {
         case MQCHS_INACTIVE:
         case MQCHS_BINDING:
         case MQCHS_STARTING:
         case MQCHS_RUNNING:
         case MQCHS_STOPPING:
         case MQCHS_INITIALIZING:
           break;

         case MQCHS_RETRYING:
         case MQCHS_REQUESTING:
         case MQCHS_PAUSED:
         case MQCHS_STOPPED:
         case MQCHS_SWITCHING:
         default:
           GoodDestination = 0;
           break;
       }

       /**************************************************************/
       /* If this destination is good, then choose it and stop       */
       /* looking for more                                           */
       /**************************************************************/

       if (GoodDestination)
       {
         parms->DestinationChosen = index + 1;
         break;
       }
     }
   }

   /******************************************************************/
   /* Set the ExitResponse2 to indicate this exit is compatable with */
   /* a dynamic cache (ie it uses MQXCLWLN to navigate along lists   */
   /* of cluster records and channel definitions)                    */
   /******************************************************************/

   parms->ExitResponse2 = MQXR2_DYNAMIC_CACHE;

   return;
 }


#if (MQAT_DEFAULT == MQAT_OS400)
main( int argc, char *argv[] )
{
  PMQWXP ClwlParms = (void*) argv[1];
  clwlFunction( ClwlParms );
}
#endif

void MQStart() {;}

