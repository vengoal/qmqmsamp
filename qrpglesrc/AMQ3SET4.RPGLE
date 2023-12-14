     H
      ****************************************************************
      *                                                              *
      * Program name: AMQ3SET4                                       *
      *                                                              *
      * Description: Sample ILE RPG program using MQSET              *
      *                                                              *
      *   <copyright                                                 *
      *   notice="lm-source-program"                                 *
      *   pids="5724-H72,"                                           *
      *   years="1994,2012"                                          *
      *   crc="1413007514" >                                         *
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
      *   AMQ3SET4 is a sample RPG program that inhibits PUTs        *
      *   to a message queue, and is an example using MQSET.         *
      *   It is intended to be run as a triggered program, and so    *
      *   receives input in a trigger parameter.                     *
      *                                                              *
      *   Note: one way to run AMQ3SET4 is to be running AMQSERV4    *
      *         trigger server, and to use AMQ3REQ4 to send          *
      *         request messages (containing queue names) to a       *
      *         queue (eg SYSTEM.Sample.SET) which must have been    *
      *         defined to trigger AMQ3SET4.                         *
      *                                                              *
      *      -- gets request messages from a queue whose name is     *
      *         in the trigger parameter; the content of each        *
      *         message is the name of a message queue               *
      *                                                              *
      *      -- inhibits PUTs to the queue named in the request,     *
      *         and sends a message to the named reply queue to      *
      *         confirm it was done                                  *
      *                                                              *
      *         alternatively, sends a report message if the queue   *
      *         can't be inhibited - eg if the queue to be           *
      *         inhibited does not exist                             *
      *                                                              *
      *      -- stops when the input queue becomes empty             *
      *                                                              *
      *      -- writes a message for each MQI reason other than      *
      *         RCNONE; stops if there is a MQI completion code      *
      *         of CCFAIL                                            *
      *                                                              *
      *    Program logic:                                            *
      *         Connect to the queue manager                         *
      *         MQOPEN message queue (A) for shared input            *
      *         while no MQI failures,                               *
      *         .  MQGET next message from queue A                   *
      *         .  if message is a request,                          *
      *         .  .  MQOPEN queue (B) named in message for SET      *
      *         .  .  Use MQSET to inhibit PUTs to queue B           *
      *         .  .  MQCLOSE queue B                                *
      *         .  .  prepare a report if no reply available         *
      *         .  .  MQPUT1, send reply/report to named reply queue *
      *         Close queue A                                        *
      *         Disconnect from queue manager                        *
      *                                                              *
      *                                                              *
      ****************************************************************
      *                                                              *
      *                                                              *
      *                                                              *
      *   Exceptions signaled:  none                                 *
      *   Exceptions monitored: none                                 *
      *                                                              *
      *   AMQ3SET4 has 1 parameter - a character trigger message;    *
      *                                                              *
      ****************************************************************

      // note, sample uses defaults where it can

     FQSYSPRT   O    F  132        PRINTER

      // Array of Selectors (only one element needed in this sample)
     D Selectors       S             10I 0 DIM(1)

      // Corresponding array of Integer Attributes
     D IntegerAttrs    S             10I 0 DIM(1)

      // Array of Character Attributes - not used in this example
     D CharAttrs       S             20A   DIM(1)

      // Selector count
     D SelectorCount   S             10I 0

      // Integer attributes count
     D IntAttrsCount   S             10I 0

      // Character attribute length
     D CharAttrsLen    S             10I 0

      // Connection handle
     D HConn           S             10I 0

      // Options
     D Options         S             10I 0

      // Object handles
     D HObj            S             10I 0
     D SetHObj         S             10I 0

      // Completion codes
     D OpenCode        S             10I 0
     D CompletionCode  S             10I 0

      // MQCONN successfull flag
     D Connected       S               N   INZ(*OFF)

      // Reasons
     D Reason          S             10I 0
     D ConnReason      S             10I 0

      // Message buffer
     D Buffer          S             48A

      // Buffer length
     D BufferLength    S             10I 0

     D ReplyMessage    S            100A   Varying

      // Message length
     D MessageLength   S             10I 0

      // Queue manager name
     D QueueManager    S             48A

      // Declare MQI structures needed - this sample uses single
      // occurrence data structures for use in PARM fields

      // MQI named constants
     D/COPY CMQG

      // Object Descriptor - source, reply, set
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

      // Trigger message (parameter)
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
     D Main            PR                  ExtPgm('AMQ3SET4')
     D MQTMC2                              LikeDS(MQTMC2_t)

     D Main            PI
     D MQTMC2                              LikeDS(MQTMC2_t)

      /free
       // indicate that sample program has started
       Status = 'start';
       Except Sample;

       // connect to queue manager and open input queue
       ExSr Start;

       //  start loop to process the messages
       DoW CompletionCode <> CCFAIL;
         //  get a message from the input queue
         ExSr GetRequest;
         If CompletionCode = CCOK;
           // only process requests
           If MQMD.MDMT <> MTRQST;
             Message = ' - not a request and discarded';
             Except DspMessage;
           Else;
             ReplyMessage = %TRIMR(Buffer);

             // set Put-Inhibited attribute for queue named in request
             ExSr SetAttributes;

             // send a response to Reply queue
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

       //***************************************************************
       //                                                              *
       //   Start - connect to the queue manager and open the input    *
       //                                                              *
       //     connection handle returned as HConn; reason code is      *
       //     retained as ConnReason, and there is no MQDISC if MQM    *
       //     was already connected before this call                   *
       //                                                              *
       //     input queue handle is HObj                               *
       //                                                              *
       //***************************************************************
       BegSr Start;
         // input QueueManager from the trigger message
         QueueManager = MQTMC2.TC2QMN;

         // use explicit connection in this example
         MQCONN(QueueManager : HConn : CompletionCode : ConnReason);

         // report reason and stop if failed
         If CompletionCode = CCFAIL;
           // "MQCONN ended with reason code ...."
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
           Options = OOFIQ + OOINPS;

           MQOPEN(HConn : MQOD : Options : HObj : OpenCode : Reason);

           // report reason, if any; stop if failed
           If Reason <> RCNONE;
             // "MQOPEN ended with reason code ...."
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

       //***************************************************************
       //                                                              *
       //   GetRequest - get a request message from the input queue    *
       //                                                              *
       //***************************************************************
       BegSr GetRequest;
         // options are Accept-truncated-message and Wait up to 5 seconds
         MQGMO.GMOPT = GMATM + GMWT ;
         MQGMO.GMWI = 5000;

         // MsgId and CorrelId are selectors that must be cleared
         // to get messages in sequence, and they are set each MQGET
         MQMD.MDMID = MINONE;
         MQMD.MDCID = CINONE;

         // clear buffer because MQGET only fills to length of message
         Buffer = *BLANKS;
         BufferLength = %LEN(Buffer);

         MQGET(HConn : HObj : MQMD : MQGMO :  BufferLength : %ADDR(Buffer) :
               MessageLength : CompletionCode : Reason);

         // report reason, if any
         If Reason <> RCNONE;
           If Reason = RC2033;
             Message = 'no more messages';
             Except DspMessage;
           Else;
             // "MQGET ended with reason code ...."
             MQCall = 'MQGET';
             ReasonCode = %CHAR(Reason);
             Except MQFailure;
           EndIf;
         EndIf;
       EndSr;

       //***************************************************************
       //                                                              *
       //   SetAttributes - inhibits PUTs to the queue named in        *
       //          request; result is a reply or report message        *
       //          prepared, with Feedback also set for a report       *
       //                                                              *
       //    SetHObj is queue handle when opening for SET              *
       //                                                              *
       //***************************************************************
       BegSr SetAttributes;
         // messages are listed by this sample
         Message = Buffer;
         Except DspMessage;

         // open (local) queue named in request message
         MQOD.ODON = Buffer;
         MQOD.ODMN = *BLANKS;

         // options are Set and Fail-if-quiescing
         Options = OOSET + OOFIQ;

         MQOPEN(HConn : MQOD : Options : SetHObj : CompletionCode : Reason);

         // note feedback code if subject queue can't be opened
         If CompletionCode = CCFAIL;
           MQMD.MDMT = MTRPRT;
           MQMD.MDFB = Reason;
         Else;
           // if queue opened, prepare counts for SET
           SelectorCount = 1;
           IntAttrsCount = 1;
           CharAttrsLen = 0;

           // attribute selector(s) and value(s) (Inhibit Put)
           Selectors(1) = IAIPUT;
           IntegerAttrs(1) = QAPUTI;

           // Note: When passing an array on a bound call we only have to pass
           // the first element, no matter the number of elements in the array.
           MQSET(HConn : SetHObj : SelectorCount : Selectors(1) :
                 IntAttrsCount : IntegerAttrs(1) : CharAttrsLen :
                 %ADDR(CharAttrs) : CompletionCode : Reason);


           ReplyMessage = ReplyMessage + ': PUT inhibited';

           // note feedback code if MQSET failed
           If CompletionCode = CCFAIL;
             MQMD.MDMT = MTRPRT;
             MQMD.MDFB = Reason;
             ReplyMessage = ReplyMessage + ' failed';
           Else;
             // build reply text if MQSET worked
             MQMD.MDMT = MTRPLY;
           EndIf;

           // close the MQSET queue (no options)
           Options = CONONE;
           MQCLOSE(HConn : SetHObj : Options : CompletionCode : Reason);

           // result of close not checked in this example
         EndIf;
       EndSr;

       //***************************************************************
       //                                                              *
       //   Respond - send the reply or report to the reply queue      *
       //                                                              *
       //***************************************************************
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

         BufferLength = %LEN(ReplyMessage);

         // send the response using MQPUT1
         MQPUT1(HConn : MQOD : MQMD : MQPMO :  BufferLength :
                %ADDR(ReplyMessage) + 2 : CompletionCode : Reason);

         // report reason, if any
         If Reason <> RCNONE;
           // "MQPUT1 ended with reason code ...."
           MQCall = 'MQPUT1';
           ReasonCode = %CHAR(Reason);
           Except MQFailure;
         EndIf;
       EndSr;

       //***************************************************************
       //                                                              *
       //   Finish - close the input queue (if open) and disconnect    *
       //                                                              *
       //***************************************************************
       BegSr Finish;

         // if input queue was opened, close it (without options)
         If OpenCode <> CCFAIL;
           Options = CONONE;
           MQCLOSE(HConn : HObj : Options : CompletionCode : Reason);

           // report reason, if any
           If Reason <> RCNONE;
             // "MQCLOSE ended with reason code ...."
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
             // "MQDISC ended with reason code ...."
             MQCall = 'MQDISC';
             ReasonCode = %CHAR(Reason);
             Except MQFailure;
           EndIf;
         EndIf;
       EndSr;
      /end-free
      //***************************************************************
     OQSYSPRT   E            Sample         1
     O                                              'Sample AMQ3SET4 '
     O                       Status
     O          E            DspMessage     1
     O                       Message
     O          E            MQFailure      1
     O                       MQCall
     O                                              ' ended with reason code '
     O                       ReasonCode
       //***************************************************************
       //                                                              *
       // END OF AMQ3SET4                                              *
       //                                                              *
       //***************************************************************
