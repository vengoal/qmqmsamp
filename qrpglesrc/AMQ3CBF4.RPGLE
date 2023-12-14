     H BNDDIR('QC2LE')
      ****************************************************************
      *                                                              *
      * Program name: AMQ3CBF4                                       *
      *                                                              *
      * Description: Sample ILE RPG program that gets messages from  *
      *              a message queue (example using MQCB)            *
      *                                                              *
      *   <copyright                                                 *
      *   notice="lm-source-program"                                 *
      *   pids="5724-H72,"                                           *
      *   years="1994,2012"                                          *
      *   crc="2339794541" >                                         *
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
      *   AMQ3CBF4 is a sample RPG program to get messages on a      *
      *   message queue, and is an example of the use of MQCB.       *
      *                                                              *
      *      -- sample reads from message queue named in the         *
      *         parameter                                            *
      *
      *      -- displays the contents of the message queue,          *
      *         assuming each message data to represent a line of    *
      *         text to be written                                   *
      *                                                              *
      *         messages are removed from the queue                  *
      *                                                              *
      *      -- writes a message for each MQI reason other than      *
      *         MQRC_NONE; stops if there is a MQI completion code   *
      *         of MQCC_FAILED                                       *
      *                                                              *
      *   Program logic:                                             *
      *      Take name of input queue from the parameter             *
      *      MQOPEN queue for INPUT                                  *
      *      MQCB   register a callback function to receive messages *
      *      MQCTL  start consumption of messages                    *
      *      wait for use to press enter                             *
      *      MQCTL  stop consumptions of messages                    *
      *      MQCLOSE the subject queue                               *
      *      MQDISC  disconnect from queue manager                   *
      *                                                              *
      ****************************************************************
      *                                                              *
      *                                                              *
      *                                                              *
      *   Exceptions signaled:  none                                 *
      *   Exceptions monitored: none                                 *
      *                                                              *
      *   AMQ3CBF4 has 3 parameters - (1) name of the target queue   *
      *                               (2) name of the queue manager  *
      *                               (3) seconds to consume messages*
      *                                                              *
      *   Note: The second and parameters are optional               *
      *                                                              *
      ****************************************************************

      // note, sample uses defaults where it can
     FQSYSPRT   O    F  132        PRINTER

      // Output fields
     D Status          S              5A
     D MQCall          S              7A
     D ReasonCode      S              4A
     D Message         S             50A

     D MQMD_t          DS                  Qualified Based(template)
     D/COPY CMQMDH

     D MQGMO_t         DS                  Qualified Based(template)
     D/COPY CMQGMOH

     D MQCBC_t         DS                  Qualified Based(template)
     D/COPY CMQCBCH

     DMessageConsumer  PR
     D HConn                         10I 0 Value
     D MD                                  LikeDS(MQMD_t)
     D GMO                                 LikeDS(MQGMO_t)
     D Buffer                        50A
     D Context                             LikeDS(MQCBC_t)

     d sleep           PR                  extproc('sleep')
     d secs                          10u 0 value

      // program parameters are: 48-character target queue name
      //                         48-character queue manager name (optional)
      //                         3-digit number of seconds to consume messages
     D Main            PR                  ExtPgm('AMQ3CBF4')
     D Queue                         48A
     D QmgrName                      48A   Options(*NOPASS)
     D Seconds                        3A   Options(*NOPASS)

     D Main            PI
     D Queue                         48A
     D QmgrName                      48A   Options(*NOPASS)
     D Seconds                        3A   Options(*NOPASS)

      // Optional parameters
     D Qmgr            S             48A

      // Connection Handle
     D HConn           S             10I 0

      // Object handle
     D HObj            S             10I 0

      // MQOPEN options
     D Options         S             10I 0

      // Completion codes
     D CompletionCode  S             10I 0

      // Reasons
     D Reason          S             10I 0
     D ConnReason      S             10I 0

     D ConsumeTime     S             10I 0 INZ(10)

      // Declare MQI structures needed
      // MQI named constants
     D/COPY CMQG

      // Object Descriptor
     D MQOD            DS                  Qualified
     D/COPY CMQODG

      // Callback descriptor
     D MQCBD           DS                  Qualified
     I/COPY CMQCBDG

      // Control options
     D MQCTLO          DS                  Qualified
     I/COPY CMQCTLOG

      // Message Descriptor
     D MQMD            DS                  Qualified
     D/COPY CMQMDG

      // Put message options
     D MQGMO           DS                  Qualified
     D/COPY CMQGMOG

      /Free
       // indicate that sample program has started
       Status = 'start';
       Except Sample;

       If %PARMS >= 2;
         Qmgr = QmgrName;
         Message = 'Using Queue Manager ' + Qmgr;
       Else;
         Message = 'Using Default Queue Manager';
       EndIf;
       Except DspMessage;

       If %PARMS >= 3;
         ConsumeTime = %INT(Seconds);
       EndIf;

       Message = 'Target queue is ' + Queue;
       Except DspMessage;

       //***************************************************************
       //                                                              *
       //   Connect to the queue manager                               *
       //                                                              *
       //***************************************************************
       MQCONN(Qmgr : HConn : CompletionCode : ConnReason);

       // report reason and stop if it failed
       If CompletionCode = CCFAIL;
         // "MQCONN ended with reason code ...."
         MQCall = 'MQCONN';
         ReasonCode = %CHAR(ConnReason);
         Except MQFailure;

         Message = 'Unable to connect to Queue Manager';
         Except DspMessage;

         *INLR = *ON;
         RETURN;
       EndIf;

       //***************************************************************
       //                                                              *
       //   Open the target message queue for output (and fail if      *
       //   MQM is quiescing)                                          *
       //                                                              *
       //   Resulting queue handle is HObj                             *
       //                                                              *
       //***************************************************************

       // options are Input as Queue Default and Fail-if-quiescing
       Options = OOINPQ + OOFIQ;

       //***************************************************************
       //                                                              *
       //    use parameter as the name of the target queue             *
       //                                                              *
       //***************************************************************
       MQOD.ODON = Queue;

       //***************************************************************
       //                                                              *
       //   Open the queue                                               *
       //                                                              *
       //***************************************************************
       MQOPEN(HConn : MQOD : Options : HObj : CompletionCode : Reason);

       // report reason, if any; stop if failed
       If Reason <> RCNONE;
         // "MQOPEN ended with reason code ...."
         MQCall = 'MQOPEN';
         ReasonCode = %CHAR(Reason);
         Except MQFailure;
       EndIf;

       If CompletionCode = CCFAIL;
         Message = 'Unable to open queue for input';
         Except DspMessage;
       EndIf;

       If CompletionCode = 0;
         //***************************************************************
         //                                                              *
         //   Register a consumer                                        *
         //                                                              *
         //***************************************************************

         // Use CBDCALLBN if you would like to call a consumer in an external
         // service program.The example show assumes that you have compiled
         // this moule in to a service program with the same name.
         //
         // Note: RPG is case-insensitive and externalizes all procedures
         // in upper-case, but calling procedures is not case-insensitive,
         // so the upper-case procedure name must be used or MQ will fail
         // to find the procedure. This is not an issue if calling ILE
         // procedures written in C.
         //MQCBD.CBDCALLBN = '*LIBL/AMQ3CBF4(MESSAGECONSUMER)';

         // Use CBDCALLBF if you would like to call a consumer defined in
         // this program or one in a bound service program.
         MQCBD.CBDCALLBF = %Paddr(MessageConsumer);

         MQGMO.GMOPT = GMNSYP;
         MQCB(HConn : CBREG : MQCBD : HObj : MQMD : MQGMO : CompletionCode :
              Reason);

         // report reason, if any; stop if failed
         If Reason <> RCNONE;
           // "MQOPEN ended with reason code ...."
           MQCall = 'MQCB';
           ReasonCode = %CHAR(Reason);
           Except MQFailure;
         EndIf;

         If CompletionCode = 0;
           //***************************************************************
           //                                                              *
           //   Start consumption of messages                              *
           //                                                              *
           //***************************************************************
           MQCTL(HConn : CTLSR : MQCTLO : CompletionCode : Reason);

           //***************************************************************
           //                                                              *
           //   Sleep for specified time (default 10 seconds)              *
           //                                                              *
           //***************************************************************
           Message = 'Consuming for ' + %CHAR(ConsumeTime) + ' seconds';
           Except DspMessage;
           sleep(ConsumeTime);

           //***************************************************************
           //                                                              *
           //   Stop consumption of messages                               *
           //                                                              *
           //***************************************************************
           MQCTL(HConn : CTLSP : MQCTLO : CompletionCode : Reason);
         EndIf;
       EndIf;

       //***************************************************************
       //                                                              *
       //  Disconnect from queue manager (if not previously connected) *
       //                                                              *
       //***************************************************************
       If ConnReason <> RC2002;
         MQDISC(HConn : CompletionCode : Reason);

         // report reason and stop if it failed
         If CompletionCode = CCFAIL;
           // "MQDISC ended with reason code ...."
           MQCall = 'MQDISC';
           ReasonCode = %CHAR(Reason);
           Except MQFailure;
         EndIf;
       EndIf;

       // "Sample AMQ3CBF4 end"
       Status = 'end';
       Except Sample;

       *INLR = *ON;
      /End-Free

      //***************************************************************
     OQSYSPRT   E            Sample         1
     O                                              'Sample AMQ3CBF4 '
     O                       Status
     O          E            DspMessage     1
     O                       Message
     O          E            MQFailure      1
     O                       MQCall
     O                                              ' ended with reason code '
     O                       ReasonCode

     PMessageConsumer  B                   Export
     DMessageConsumer  PI
     D HConn                         10I 0 Value
     D MD                                  LikeDS(MQMD_t)
     D GMO                                 LikeDS(MQGMO_t)
     D Buffer                        50A
     D Context                             LikeDS(MQCBC_t)

     D i               S             10I 0 INZ(0)
     D Max             S             10I 0
     D Length          S             10I 0
     D CallType        S             10I 0

      /free
       CallType = Context.CBCCALLT;
       If CallType = CBCTMR OR CallType = CBCTMN;
         Length = GMO.GMRL;
         Message = 'Message Call (' + %CHAR(Length) + ') Bytes';
         If Context.CBCREA <> 0;
           Message = Message + ': Reason = ' + %Char(Context.CBCREA);
         EndIf;
         Except DspMessage;

         Max = Length;
         If Max > 50;
           Max = 50;
         EndIf;

         Message = %SUBST(Buffer : 1 : Max);
         Except DspMessage;

         If Max < Length;
           Message = '... plus ' + %CHAR(Length - Max) + ' bytes.';
           Except DspMessage;
         EndIf;

       ElseIf CallType = CBCTEC;
         Message = 'Event Call : Reason = ' + %CHAR(Context.CBCREA);
         Except DspMessage;
       Else;
         Message = 'Calltype = ' + %CHAR(Context.CBCCALLT);
         Except DspMessage;
       EndIf;
      /end-free
     P                 E
      //***************************************************************
      //                                                              *
      // END OF AMQ3CBF4                                              *
      //                                                              *
      //***************************************************************
