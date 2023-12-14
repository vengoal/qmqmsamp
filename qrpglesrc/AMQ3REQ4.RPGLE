     H
      ****************************************************************
      *                                                              *
      * Program name: AMQ3REQ4                                       *
      *                                                              *
      * Description: Sample ILE RPG program to put request messages  *
      *              to a message queue and to show the replies      *
      *              (example using REPLY queue)                     *
      *                                                              *
      *   <copyright                                                 *
      *   notice="lm-source-program"                                 *
      *   pids="5724-H72,"                                           *
      *   years="1994,2012"                                          *
      *   crc="178352635" >                                          *
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
      *   AMQ3REQ4 is a sample RPG program to put messages to a      *
      *   message queue, and then show the reply messages            *
      *                                                              *
      *   Note: AMQ3REQ4 will only receive replies to its requests   *
      *         if there is a program running, or triggered, to      *
      *         respond to them.   Samples AMQ3INQ4 or AMQ3SET4      *
      *         could be used for this purpose.                      *
      *                                                              *
      *      -- sample input is taken from an array defined in       *
      *         the source; the program parameter identifies         *
      *         the target queue                                     *
      *                                                              *
      *      -- adds each text line in the array to the message      *
      *         queue, taking each line of text as the content       *
      *         of a request message                                 *
      *                                                              *
      *      -- displays the reply messages, assuming each           *
      *         message to represent a line of text                  *
      *         This means that if successful no messages are left   *
      *         on the reply queue.  If triggering is not            *
      *         successful this program will just keep waiting!      *
      *                                                              *
      *      -- writes a message for each MQI reason other than      *
      *         RCNONE; stops if there is a MQI completion code      *
      *         of CCFAIL                                            *
      *                                                              *
      *    Program logic:                                            *
      *         MQCONN to the target queue manager                   *
      *         MQOPEN server queue for OUTPUT                       *
      *         MQOPEN reply queue for EXCLUSIVE INPUT               *
      *         for each line in the input array,                    *
      *         .  MQPUT request message with text to server queue   *
      *         while no MQI failures,                               *
      *         .  MQGET message from reply queue                    *
      *         .  display its content                               *
      *         MQCLOSE both queues                                  *
      *         MQDISC from the queue manager                        *
      *                                                              *
      ****************************************************************
      *                                                              *
      *                                                              *
      *                                                              *
      *   Exceptions signaled:  none                                 *
      *   Exceptions monitored: none                                 *
      *                                                              *
      *   AMQ3REQ4 has 2 parameters - (1) name of the target queue   *
      *                               (2) name of the queue manager  *
      *                               (3) name of reply queue        *
      *                                                              *
      *   Note: The parameters are successively optional, ie. if a   *
      *   parameter is not passed, no parameters after it can be     *
      *   passed as well.                                            *
      *                                                              *
      ****************************************************************

      // note, sample uses defaults where it can

     FQSYSPRT   O    F  132        PRINTER

      // Test data used as input
     D Data            S             48A   DIM(2) CTData

      // Message buffer
     D Buffer          S             80A

      // Buffer length
     D BufferLength    S             10I 0

      // Connection Handle
     D HConn           S             10I 0

      // Options
     D Options         S             10I 0

      // Object handles
     D HObj            S             10I 0
     D ReplyHObj       S             10I 0

      // Completion codes
     D OpenCode        S             10I 0
     D CompletionCode  S             10I 0

      // Reasons
     D Reason          S             10I 0
     D ConnReason      S             10I 0

      // Message length
     D MessageLength   S             10I 0

      // Loop counter
     D I               S             10I 0 INZ(1)

      // Declare MQI structures needed - this sample uses single
      // occurrence data structures for use in PARM result fields
      // MQI named constants
     D/COPY CMQG

      // Object Descriptors - target, reply
     D MQOD            DS                  Qualified
     D/COPY CMQODG

      // Message Descriptor
     D MQMD            DS                  Qualified
     D/COPY CMQMDG

      // Put message options
     D MQPMO           DS                  Qualified
     D/COPY CMQPMOG

      // Get message options
     D MQGMO           DS                  Qualified
     D/COPY CMQGMOG

      // Optional parameters
     D Qmgr            S             48A
     D ReplyQueue      S             48A   INZ('SYSTEM.SAMPLE.REPLY')

      // Output fields
     D Status          S              5A
     D MQCall          S              7A
     D ReasonCode      S              4A
     D Message         S             50A

      // program parameters are: 48-character target queue name
      //                         48-character queue manager name (optional)
      //                         48-character reply queue name (optional)
     D Main            PR                  ExtPgm('AMQ3REQ4')
     D Queue                         48A
     D QmgrName                      48A   Options(*NOPASS)
     D ReplyQName                    48A   Options(*NOPASS)

     D Main            PI
     D Queue                         48A
     D QmgrName                      48A   Options(*NOPASS)
     D ReplyQName                    48A   Options(*NOPASS)

      /free
       // indicate that sample program has started
       Status = 'start';
       Except Sample;

       If %parms >= 2;
         Qmgr = QmgrName;
         Message = 'Using Queue Manager ' + Qmgr;
       Else;
         Message = 'Using Default Queue Manager';
       EndIf;
       Except DspMessage;

       If %parms >= 3;
         ReplyQueue = ReplyQName;
       EndIf;

       Message = 'Using Reply Queue ' + ReplyQueue;
       Except DspMessage;

       Message = 'Target queue is ' + Queue;
       Except DspMessage;

       //***************************************************************
       //                                                              *
       //    use parameter as the name of the target queue             *
       //                                                              *
       //***************************************************************
       MQOD.ODON = Queue;

       //***************************************************************
       //                                                              *
       //   Connect to the queue manager                               *
       //                                                              *
       //***************************************************************
       MQCONN(Qmgr : HConn : OpenCode : ConnReason);

       // report reason and stop if it failed
       If OpenCode = CCFAIL;
         // "MQCONN ended with reason code ...."
         MQCall = 'MQCONN';
         ReasonCode = %CHAR(ConnReason);
         Except MQFailure;

         Message = 'Unable to connect to queue manager';
         Except DspMessage;

         *INLR = *ON;
         Return;
       EndIf;

       //***************************************************************
       //                                                              *
       //   Open the server message queue for output (and fail if      *
       //   MQM is quiescing)                                          *
       //                                                              *
       //   Resulting queue handle is HObj                             *
       //                                                              *
       //***************************************************************

       // options are Output and Fail-if-quiescing
       Options = OOOUT + OOFIQ;

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
       Else;
         //***************************************************************
         //                                                              *
         //   Open the queue to receive reply messages                   *
         //                                                              *
         //   Resulting queue handle is ReplyHObj                        *
         //                                                              *
         //***************************************************************
         // options are Exclusive-Input and Fail-if-quiescing
         Options = OOINPX + OOFIQ;

         // reply queue name, padded with blanks
         MQOD.ODON = *BLANKS;
         MQOD.ODON = ReplyQueue;

         MQOPEN(HConn : MQOD : Options : ReplyHObj : OpenCode : Reason);

         // report reason, if any; stop if failed
         If Reason <> RCNONE;
           // "MQOPEN ended with reason code ...."
           MQCall = 'MQOPEN';
           ReasonCode = %CHAR(Reason);
           Except MQFailure;
         EndIf;

         If OpenCode = CCFAIL;
           Message = 'Unable to open reply queue';
           Except DspMessage;
         EndIf;

         //***************************************************************
         //                                                              *
         //   Put request messages to the server queue                   *
         //                                                              *
         //***************************************************************
         // initial loop condition is based on result of MQOPEN
         CompletionCode = OpenCode;

         // length of each request message,
         BufferLength = %LEN(Data(1));

         // Start loop to put request messages
         DoW CompletionCode <> CCFAIL AND I <= %ELEM(Data);
           // each message type is Request,
           MQMD.MDMT = MTRQST;

           // ask for exceptions with data,
           MQMD.MDREP = ROEXCD;

           // name the queue for all responses
           MQMD.MDRQ = ReplyQueue;

           MQPUT(HConn : HObj : MQMD : MQPMO : BufferLength :  %ADDR(Data(I)) :
                CompletionCode : Reason);

           // report reason, if any; stop if failed
           If Reason <> RCNONE;
             // "MQPUT ended with reason code ...."
             MQCall = 'MQPUT';
             ReasonCode = %CHAR(Reason);
             Except MQFailure;
           EndIf;

           I += 1;
         EndDo;

         //***************************************************************
         //                                                              *
         //   Get and display the reply messages                         *
         //                                                              *
         //***************************************************************
         // initial loop condition based on result of MQOPEN
         CompletionCode = OpenCode;

         // first x bytes of replies only
         BufferLength = %LEN(Buffer);

         // 5 minute limit for first reply, reduced for later messages
         MQGMO.GMWI = 300000;

         // Start loop to get messages
         DoW CompletionCode <> CCFAIL;
           // GET options are Wait and Accept-truncated-message
           MQGMO.GMOPT = GMWT + GMATM;

           // MsgId and CorrelId are selectors that must be cleared
           // to get messages in sequence, and they are set each MQGET
           MQMD.MDMID = MINONE;
           MQMD.MDCID = CINONE;

           // clear answer because MQGET only fills to length of message
           Buffer = *BLANKS;

           MQGET(HConn : ReplyHObj : MQMD : MQGMO : BufferLength :
                 %ADDR(Buffer) : MessageLength : CompletionCode : Reason);

           // subsequent waits are limited to 15 seconds
           MQGMO.GMWI = 15000;

           // report reason, if any; stop if failed
           Select;
             When Reason = RCNONE;

             When Reason = RC2033;
               Message = 'No more responses';
               Except DspMessage;

             When MessageLength > BufferLength;
               Message = 'Response truncated';
               Except DspMessage;

             Other;
               // "MQGET ended with reason code ...."
               MQCall = 'MQGET';
               ReasonCode = %CHAR(Reason);
               Except MQFailure;
           EndSl;

           // display reply messages
           If CompletionCode <> CCFAIL;
             Except Text;

             If MQMD.MDMT = MTRPRT;
               Message = 'Report with feedback ' + %CHAR(MQMD.MDFB);
               Except DspMessage;
             EndIf;
           EndIf;
         EndDo;

         //***************************************************************
         //                                                              *
         //   Close both queues (with no options)                        *
         //                                                              *
         //***************************************************************
         Options = CONONE;

         // server queue (program stopped if first Open failed)
         MQCLOSE(HConn : HObj : Options : CompletionCode : Reason);

         // report reason from first MQCLOSE, if any
         If Reason <> RCNONE;
           // "MQCLOSE ended with reason code ...."
           MQCall = 'MQCLOSE';
           ReasonCode = %CHAR(Reason);
           Except MQFailure;
         EndIf;

         // if Reply queue was opened successfully, close it
         If OpenCode <> CCFAIL;
           MQCLOSE(HConn : ReplyHObj : Options : CompletionCode : Reason);

           // report reason from MQCLOSE, if any
           If Reason <> RCNONE;
             // "MQCLOSE ended with reason code ...."
             MQCall = 'MQCLOSE';
             ReasonCode = %CHAR(Reason);
             Except MQFailure;
           EndIf;
         EndIf;

         //***************************************************************
         //                                                              *
         //   Disconnect from the queue manager                          *
         //                                                              *
         //***************************************************************
         If ConnReason <> RC2002;
           MQDISC(HConn : OpenCode : Reason);

           // report reason and stop if it failed
           If OpenCode = CCFAIL;
             // "MQDISC ended with reason code ...."
             MQCall = 'MQDISC';
             ReasonCode = %CHAR(Reason);
             Except MQFailure;
           EndIf;
         EndIf;
       EndIf;

       // "Sample AMQ3REQ4 end"
       Status = 'end';
       Except Sample;

       *INLR = *ON;
      /end-free
      //***************************************************************
     OQSYSPRT   E            Sample         1
     O                                              'Sample AMQ3REQ4 '
     O                       Status
     O          E            DspMessage     1
     O                       Message
     O          E            MQFailure      1
     O                       MQCall
     O                                              ' ended with reason code '
     O                       ReasonCode
     O          E            Text           1
     O                                              'Response <'
     O                       Buffer
     O                                              '>'
      //***************************************************************
      //                                                              *
      // END OF AMQ3REQ4                                              *
      //                                                              *
      //***************************************************************
      //***
      //  Sample input follows
      //***
**
SYSTEM.SAMPLE.LOCAL
OTHER.NONEXISTENT.QUEUE
