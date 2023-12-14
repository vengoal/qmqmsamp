     H
      ****************************************************************
      *                                                              *
      * Program name: AMQ3INQ4                                       *
      *                                                              *
      * Description: Sample ILE RPG program using MQINQ              *
      *                                                              *
      *   <copyright                                                 *
      *   notice="lm-source-program"                                 *
      *   pids="5724-H72,"                                           *
      *   years="1994,2012"                                          *
      *   crc="1410916459" >                                         *
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
      *   AMQ3INQ4 is a sample RPG program that inquires selected    *
      *   attributes of a message queue, an example using MQINQ.     *
      *   It is intended to be run as a triggered program, and so    *
      *   receives input in a trigger parameter.                     *
      *                                                              *
      *   Note: one way to run AMQ3INQ4 is to be running AMQSERV4    *
      *         trigger server, and to use AMQ3REQ4 to send          *
      *         request messages (containing queue names) to a       *
      *         queue (eg SYSTEM.SAMPLE.INQ) which must have been    *
      *         defined to trigger AMQ3INQ4.                         *
      *                                                              *
      *      -- gets request messages from a queue whose name is     *
      *         in the trigger parameter; the content of each        *
      *         message is the name of a message queue               *
      *                                                              *
      *      -- sends a reply message to the names reply queue       *
      *         which contains the values of the following           *
      *         attributes                                           *
      *                                                              *
      *         -- current depth of the queue                        *
      *         -- whether the queue has GET inhibited               *
      *         -- number of jobs with the queue open for input      *
      *                                                              *
      *         alternatively, sends a report message if the queue   *
      *         can't be inquired - eg if the queue to be            *
      *         inquired does not exist                              *
      *                                                              *
      *      -- stops when the input queue becomes empty             *
      *                                                              *
      *      -- writes a message for each MQI reason other than      *
      *         RCNONE; stops if there is a MQI completion code      *
      *         of CCFAIL                                            *
      *                                                              *
      *    Program logic:                                            *
      *         MQCONNect to the queue manager                       *
      *         MQOPEN message queue (A) for shared input            *
      *         while no MQI failures,                               *
      *         .  MQGET next message from queue A                   *
      *         .  if message is a request,                          *
      *         .  .  MQOPEN queue (B) named in message for INQUIRE  *
      *         .  .  Use MQINQ to find required attributes of B     *
      *         .  .  MQCLOSE queue B                                *
      *         .  .  prepare a report if no reply available         *
      *         .  .  MQPUT1, send reply/report to named reply queue *
      *         MQCLOSe queue A                                      *
      *         MQDISConnect from queue manager                      *
      *                                                              *
      //                                                              *
      //***************************************************************
      //                                                              *
      //                                                              *
      //                                                              *
      //   Exceptions signaled:  none                                 *
      //   Exceptions monitored: none                                 *
      //                                                              *
      //   AMQ3INQ4 has 1 parameter - a character trigger message;    *
      //                                                              *
      //***************************************************************

      // note, sample uses defaults where it can

     FQSYSPRT   O    F  132        PRINTER

      // Array of Selectors
     D Selectors       S             10I 0 DIM(3)

      // Array of Integer Attributes
     D IntegerAttrs    S             10I 0 DIM(3)

      // Array of Character Attributes - not used in this example
     D CharAttrs       S             20A   DIM(1)

      // Selector counts/length
     D SelectorCount   S             10I 0
     D IntAttrsCount   S             10I 0
     D CharAttrsLen    S             10I 0

      // Connection handle
     D HConn           S             10I 0

      // MQOPEN/MQCLOSE Options
     D Options         S             10I 0

      // Object handles
     D HObj            S             10I 0
     D InqHObj         S             10I 0

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
      // occurence data structures for use in PARM fields

      // MQI named constants
     D/COPY CMQG
      // Object Descriptor - source, reply, inquire
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
     DMQTMC2_t         DS                  Qualified Based(template)
     D/COPY CMQTMC2G

      // Output fields
     D Status          S              5A
     D MQCall          S              7A
     D ReasonCode      S              4A
     D Message         S             50A

      //  program parameter is a character trigger message
     D Main            PR                  ExtPgm('AMQ3INQ4')
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
           ELSE;
             // inquire attributes of queue named in the request message
             ExSr Inquire;

             // send answer to the Reply queue
             ExSr Respond;
           EndIf;
         EndIf;
       EndDo;

       // close input queue and disconnect (if connected)
       If Connected;
         ExSr FINISH;
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
         // input QueueManager from trigger message
         QueueManager = MQTMC2.TC2QMN;
         // use explicit connection in this sample
         MQCONN(QueueManager : HConn : OpenCode : ConnReason);

         // report reason and stop if it failed
         If OpenCode = CCFAIL;
           CompletionCode = OpenCode;

           // "MQCONN ended with reason code ...."
           MQCall = 'MQCONN';
           ReasonCode = %CHAR(ConnReason);
           Except MQFailure;

           Message = 'Unable to connect to Queue Manager';
           Except DspMessage;
         ELSE;
           Connected = *ON;

           // input queue name is taken from the trigger parameter
           MQOD.ODON = MQTMC2.TC2QN;

           // options are Shared-input and Fail-if-quiescing
           Options = OOINPS + OOFIQ;

           // call for MQOPEN ...
           MQOPEN(HConn : MQOD : Options : HObj : OpenCode : Reason);

           // report reason, if any; stop if it failed
           If Reason <> RCNONE;
             // "MQOPEN ended with reason code ...."
             MQCall = 'MQOPEN';
             ReasonCode = %CHAR(Reason);
             Except MQFailure;
           EndIf;

           If OpenCode = CCFAIL;
             Message = 'unable to open queue for input';
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
         MQGMO.GMOPT = GMWT + GMATM;
         MQGMO.GMWI = 5000;

         // MsgId and CorrelId are selectors that must be cleared
         // to get messages in sequence, and they are set each MQGET
         MQMD.MDMID = MINONE;
         MQMD.MDCID = CINONE;

         // clear buffer because MQGET only fills to length of message
         Buffer = *BLANKS;

         // length of buffer available
         BufferLength = %LEN(Buffer);

         MQGET(HConn : HObj : MQMD : MQGMO : BufferLength : %ADDR(Buffer) :
               MessageLength : CompletionCode : Reason);


         // report reason, if any
         If Reason <> RCNONE;
           If Reason = RC2033;
             Message = 'no more requests';
             Except DspMessage;
           ELSE;
             // "MQGET ended with reason code ...."
             MQCall = 'MQGET';
             ReasonCode = %CHAR(Reason);
             Except MQFailure;
           EndIf;
         EndIf;

       EndSr;

       //***************************************************************
       //                                                              *
       //   Inquire - inquire attributes of the queue named in request;*
       //          result is a reply or report message prepared,       *
       //          with Feedback also set for a report                 *
       //                                                              *
       //    InqHObj is queue handle when opening for INQUIRE          *
       //                                                              *
       //***************************************************************
       BegSr Inquire;
         // messages are listed by this sample
         Message = Buffer;
         Except DspMessage;

         // open (local) queue named in the request message
         MQOD.ODON = Buffer;
         MQOD.ODMN = *BLANKS;

         // options are Inquire and Fail-if-quiescing
         Options = OOFIQ + OOINQ;

         // call to MQOPEN ...
         MQOPEN(HConn : MQOD : Options : InqHObj : CompletionCode : Reason);

         // note feedback code if subject queue can't be opened
         If CompletionCode = CCFAIL;
           MQMD.MDMT = MTRPRT;
           MQMD.MDFB = Reason;
         ELSE;
           // if queue opened, prepare counts for INQUIRE
           SelectorCount = 3;
           IntAttrsCount = 3;
           CharAttrsLen = 0;

           // define attribute selectors for Get Inhibit status, current
           //   queue depth and open-input count
           Selectors(1) = IAIGET;
           Selectors(2) = IACDEP;
           Selectors(3) = IAOIC;

           // call for MQINQ ...
           // Note: When passing an array on a bound call we only have to pass
           // the first element, not matter the number of elements in the array.
           MQINQ(HConn : InqHObj : SelectorCount : Selectors(1) : IntAttrsCount:
                 IntegerAttrs(1) : CharAttrsLen : %ADDR(CharAttrs) :
                 CompletionCode : Reason);

           // note feedback code if INQUIRE failed
           If CompletionCode = CCFAIL;
             MQMD.MDMT = MTRPRT;
             MQMD.MDFB = Reason;
           Else;
             // build reply text if INQUIRE worked ...
             MQMD.MDMT = MTRPLY;

             ReplyMessage = %TRIMR(Buffer);
             ReplyMessage = ReplyMessage + ': ';
             ReplyMessage = ReplyMessage + %CHAR(IntegerAttrs(2));
             ReplyMessage = ReplyMessage + ' messages; ';
             ReplyMessage = ReplyMessage + %CHAR(IntegerAttrs(3));
             ReplyMessage = ReplyMessage + ' jobs;';

             If IntegerAttrs(1) = QAGETI;
               ReplyMessage = ReplyMessage + ' Get Inhibited';
             EndIf;
           EndIf;

           // close the INQUIRE queue (no options)
           Options = CONONE;
           MQCLOSE(HConn : InqHObj : Options : CompletionCode : Reason);

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
         If  %BITAND(MQMD.MDREP : ROPDAE) > 0 AND
             %BITAND(MQMD.MDREP : RODISC) > 0;
           MQMD.MDREP = RODISC;
         ELSE;
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
         MQPUT1(HConn : MQOD : MQMD : MQPMO : BufferLength :
                %ADDR(ReplyMessage) + 2 : CompletionCode : Reason);

         // report reason, if any
         If Reason <> RCNONE;
           // "MQPUT1 ended with reason code ...."
           MQCall ='MQPUT1';
           ReasonCode = %CHAR(Reason);
           Except MQFailure;
         EndIf;
       EndSr;

       //***************************************************************
       //                                                              *
       //   FINISH - close the iput queue (if open) and disconnect     *
       //                                                              *
       //***************************************************************
       BegSr FINISH;

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
     OQSYSPRT   E            DspMessage     1
     O                       Message
     O          E            Sample         1
     O                                              'Sample AMQ3INQ4 '
     O                       Status
     O          E            MQFailure      1
     O                       MQCall
     O                                              ' ended with reason code '
     O                       ReasonCode
       //***************************************************************
       //                                                              *
       // END OF AMQ3INQ4                                              *
       //                                                              *
       //***************************************************************
