     H
      ****************************************************************
      *                                                              *
      * Program name: AMQ3ECH4                                       *
      *                                                              *
      * Description: Sample ILE RPG echo program                     *
      *                                                              *
      *   <copyright                                                 *
      *   notice="lm-source-program"                                 *
      *   pids="5724-H72,"                                           *
      *   years="1994,2012"                                          *
      *   crc="1810773596" >                                         *
      *   Licensed Materials - Property of IBM                       *
      *                                                              *
      *   5724-H72,                                                  *
      *                                                              *
      *   (C) Copyright IBM Corp. 1994, 2012 All Rights Reserved.    *
      *                                                              *
      *   US Government Users Restricted Rights - Use, duplication or  *
      *   disclosure restricted by GSA ADP Schedule Contract with    *
      *   IBM Corp.                                                  *
      *   </copyright>                                               *
      *                                                              *
      ****************************************************************
      *                                                              *
      * Function:                                                    *
      *                                                              *
      *                                                              *
      *   AMQ3ECH4 is a sample RPG program that echoes request       *
      *   messages to the Reply queue.  It includes the use of       *
      *   the Conversion on MQGET.                                   *
      *   It is intended to be run as a triggered program, and so    *
      *   receives input in a trigger parameter.                     *
      *                                                              *
      *   Note: one way to run AMQ3ECH4 is to be running AMQSERV4    *
      *         trigger server, and to use AMQ3REQ4 to send          *
      *         request messages (containing any text) to a queue    *
      *         (eg SYSTEM.SAMPLE.ECHO) which must have been         *
      *         defined to trigger AMQ3ECH4.                         *
      *                                                              *
      *      -- gets request messages from a queue whose name is     *
      *         in the trigger parameter; the content of each        *
      *         message is assumed to be text                        *
      *                                                              *
      *      -- sends a reply message to the named reply queue       *
      *         which contains the same text as the request          *
      *                                                              *
      *      -- stops when the input queue becomes empty             *
      *                                                              *
      *      -- writes a message for each MQI reason other than      *
      *         RCNONE; stops if there is a MQI completion code      *
      *         of CCFAIL                                            *
      *                                                              *
      *    Program logic:                                            *
      *         Connect to the queue manager                         *
      *         MQOPEN message queue for shared input                *
      *         while no MQI failures,                               *
      *         .  MQGET next message from input queue               *
      *         .  if message is a request,                          *
      *         .  .  MQPUT1, send reply to named reply queue        *
      *         MQCLOSe input queue                                  *
      *         MQDISConnect from queue manager                      *
      *                                                              *
      *                                                              *
      ****************************************************************
      *                                                              *
      *                                                              *
      *                                                              *
      *   Exceptions signaled:  none                                 *
      *   Exceptions monitored: none                                 *
      *                                                              *
      *   AMQ1ECH4 has 1 parameter - a character trigger message;    *
      *                                                              *
      ****************************************************************

      // note, sample uses defaults where it can

     FQSYSPRT   O    F  132        PRINTER

      // Connection handle
     D HConn           S             10I 0

      // Options
     D Options         S             10I 0

      // Object handles
     D HObj            S             10I 0

      // Completion codes
     D OpenCode        S             10I 0
     D CompletionCode  S             10I 0

      // MQCONN successfull flag
     D Connected       S               N   INZ(*OFF)

      // Reasons
     D Reason          S             10I 0
     D ConnReason      S             10I 0

      // Data area for request and reply
     D Buffer          S             64A

      // Buffer length
     D BufferLength    S             10I 0

      // Message length
     D MessageLength   S             10I 0

      // Queue manager name
     D Qmgr            S             48A

      //  Declare MQI structures needed - this sample uses single
      //    occurence data structures for use in PARM fields

      // MQI named constants
     D/COPY CMQG

      // Object Descriptor - source and reply
     D MQOD            DS                  Qualified
     D/COPY CMQODG

      // Message Descriptor
     D MQMD            DS                  Qualified
     D/COPY CMQMDG

      // Get message options
     D MQGMO           DS                  Qualified
     D/COPY CMQGMOG

      // Put message options
     D MQPMO           DS                  Qualified
     D/COPY CMQPMOG

      // Trigger message (parameter) includes queue manager name
      // This just serves as a template for the parameter and is allocated no storage,
      // since it is using a dummy basing pointer
     D MQTMC2_t        DS                  Qualified Based(template)
     D/COPY CMQTMC2G

      // Output fields
     D Status          S              5A
     D MQCall          S              7A
     D ReasonCode      S              4A
     D Message         S             50A

      //  program parameter is a character trigger message
     D Main            PR                  ExtPgm('AMQ3ECH4')
     D MQTMC2                              LikeDS(MQTMC2_t)

     D Main            PI
     D MQTMC2                              LikeDS(MQTMC2_t)

      /free
       // indicate that sample program has started
       Status = 'start';
       Except Sample;

       // connect to queue manager and open input queue
       ExSr Start;

       // start loop to process the messages
       DoW CompletionCode <> CCFAIL;
         // get a message from the input queue
         ExSr GetRequest;
         If CompletionCode = CCOK;
           // only process requests
           If MQMD.MDMT <> MTRQST;
             Message = ' - not a request and discarded';
             Except DspMessage;
           Else;
             // send answer to the Reply queue
             ExSr Respond;
           EndIf;
         EndIf;
       EndDo;

       // close input queue and disconnect (if connected)
       If Connected;
         ExSr Finish;
       EndIf;

       // indicate that sample program has finished
       Status = 'end';
       Except Sample;

       *INLR = *ON;

       //**************************************************************
       //                                                             *
       //   Start - connect to the queue manager and open the input   *
       //                                                             *
       //     connection handle returned as HConn; reason code is     *
       //     retained as ConnReason, and there is no MQDISC if MQM   *
       //     was already connected before this call                  *
       //                                                             *
       //     input queue handle is HObj                              *
       //                                                             *
       //**************************************************************
       BegSr Start;
         // input Qmgr from trigger message
         Qmgr = MQTMC2.TC2QMN;

         // use explicit connection in this sample
         MQCONN(Qmgr : HConn : CompletionCode : ConnReason);

         // report reason and stop if it failed
         If CompletionCode = CCFAIL;
           //  "MQCONN ended with reason code ...."
           MQCall = 'MQCONN';
           ReasonCode = %CHAR(ConnReason);
           Except MQFailure;

           Message = 'Unable to connect to queue manager';
           Except DspMessage;
         Else;
           Connected = *ON;
           // input queue name is taken from the trigger parameter
           MQOD.ODON = MQTMC2.TC2QN;

           // options are Shared-input and Fail-if-quiescing
           Options = OOINPS + OOFIQ;

           // call for MQOPEN ...
           MQOPEN(HConn : MQOD : Options : HObj : OpenCode : Reason);

           // report reason, if any; stop if it failed
           If Reason <> RCNONE;
             //  "MQOPEN ended with reason code ...."
             MQCall = 'MQOPEN';
             ReasonCode = %CHAR(Reason);
             Except MQFailure;
           EndIf;

           If OpenCode = CCFAIL;
             Message = 'Unable to open queue for input';
             Except DspMessage;
           EndIf;

          CompletionCode = OpenCode;
         EndIf;
       EndSr;

       //**************************************************************
       //                                                             *
       //   GetRequest - get a request message from the input queue   *
       //                                                             *
       //**************************************************************
       BegSr GetRequest;

         // options are Accept-truncated-message and Wait up to 5 seconds
         MQGMO.GMOPT = GMATM +  GMWT;
         MQGMO.GMWI = 5000;

         // request (possibly remote) message received in OS/400 format
         MQGMO.GMOPT += 16384;
         MQMD.MDENC = ENNAT;
         MQMD.MDCSI = CSDEF;

         //  MsgId and CorrelId are selectors that must be cleared
         //  to get messages in sequence, and they are set each MQGET
         MQMD.MDMID = MINONE;
         MQMD.MDCID = CINONE;

         //  clear buffer because MQGET only fills to length of message
         Buffer = *BLANKS;

         // length of buffer available
         BufferLength = %LEN(Buffer);

         MQGET(HConn : HObj : MQMD : MQGMO :  BufferLength :
               %ADDR(Buffer) : MessageLength : CompletionCode : Reason);

         // set BufferLength to what was received
         If MessageLength < BufferLength;
           BufferLength = MessageLength;
         EndIf;

         // report reason, if any
         If Reason <> RCNONE;
           //  specific text for "no more messages"
           If Reason = RC2033;
             Message = 'no more requests';
             Except DspMessage;
           Else;
             //  otherwise say "MQGET ended with reason code ...."
             MQCall = 'MQGET ';
             ReasonCode = %CHAR(Reason);
             Except MQFailure;
           EndIf;
         EndIf;
       EndSr;

       //**************************************************************
       //                                                             *
       //   Respond - send the reply or report to the reply queue     *
       //                                                             *
       //**************************************************************
       BegSr Respond;
         // If ROPDAE was set in MQDEP of the original message, then
         // the reply or report should inherit RODISC if it was also set,
         // else any further reports should be suppressed
         If %BITAND(MQMD.MDREP : ROPDAE) > 0 AND
            %BITAND(MQMD.MDREP : RODISC) > 0;
           MQMD.MDREP = RODISC;
         Else;
           MQMD.MDREP = RONONE;
         EndIf;

         // copy reply queue and queue manager names
         MQOD.ODON = MQMD.MDRQ;
         MQOD.ODMN = MQMD.MDRM;

         // respond with CorrelId set to the request's MsgId
         MQMD.MDCID = MQMD.MDMID;
         MQMD.MDMID = MINONE;

         // send the response using MQPUT1
         MQPUT1(HConn : MQOD : MQMD : MQPMO : BufferLength :  %ADDR(Buffer) :
                CompletionCode : Reason);

         // report reason, if any
         If Reason <> RCNONE;
           //  "MQPUT1 ended with reason code ...."
           MQCall = 'MQPUT1';
           ReasonCode = %CHAR(Reason);
           Except MQFailure;
         EndIf;
       EndSr;

       //**************************************************************
       //                                                             *
       //   Finish - close the input queue (if open) and disconnect   *
       //                                                             *
       //**************************************************************
       BegSr Finish;
         // if input queue was opened, close it (without options)
         If OpenCode <> CCFAIL;
           Options = CONONE;
           MQCLOSE(HConn : HObj : Options : CompletionCode : Reason);

           // report reason, if any
           If Reason <> RCNONE;
             //  "MQCLOSE ended with reason code ...."
             MQCall = 'MQCLOSE';
             ReasonCode = %CHAR(Reason);
             Except MQFailure;
           EndIf;
         EndIf;

         // disconnect from MQM, only if it was not previously connected
         If ConnReason <> RC2002;
           MQDISC(HConn : CompletionCode : Reason);
           // report reason, if any
           If Reason <> RCNONE;
             //  "MQDISC ended with reason code ...."
             MQCall = 'MQDISC';
             ReasonCode = %CHAR(Reason);
             Except MQFailure;
           EndIf;
         EndIf;
       EndSr;

      /end-free
     OQSYSPRT   E            Sample         1
     O                                              'Sample AMQ3ECH4 '
     O                       Status
     O          E            MQFailure      1
     O                       MQCall
     O                                              ' ended with reason code '
     O                       ReasonCode
     O          E            DspMessage     1
     O                       Message
      //***************************************************************
      //                                                              *
      // END OF AMQ3ECH4                                              *
      //                                                              *
      //***************************************************************
