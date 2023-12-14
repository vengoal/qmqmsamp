     H
      ****************************************************************
      *                                                              *
      * Program name: AMQ3SRV4                                       *
      *                                                              *
      * Description: Sample ILE RPG program that acts as a trigger   *
      *              server.                                         *
      *                                                              *
      *   <copyright                                                 *
      *   notice="lm-source-program"                                 *
      *   pids="5724-H72,"                                           *
      *   years="1994,2012"                                          *
      *   crc="2751565723" >                                         *
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
      *   AMQ3SRV4 is a sample RPG program that acts as a trigger    *
      *   server. It reads from an initiation queue then itself      *
      *   runs the OS/400 program associated with each trigger       *
      *   message.                                                   *
      *                                                              *
      *      -- reads message from trigger initiation queue named    *
      *         in the input parameter.                              *
      *                                                              *
      *      -- performs command for each valid trigger message      *
      *                                                              *
      *        -- command runs process named in application id       *
      *        -- parm = character version of trigger message        *
      *        -- envdata is not used by the trigger server          *
      *                                                              *
      *      -- writes a message for each MQI reason other than      *
      *         RCNONE; stops if there is a MQI completion code      *
      *         of CCFAIL                                            *
      *                                                              *
      *                                                              *
      *   Program Logic                                              *
      *         MQCONN to the target queue manager                   *
      *         MQOPEN initiation queue for INPUT                    *
      *         while no MQI failures:                               *
      *         .  MQGET next message & remove from queue            *
      *         .  invoke command based on trigger message           *
      *         .  .  Applid is name of program to call  s            *
      *         .  .  MQTMC2 is parameter                            *
      *         MQCLOSE the initiation queue                         *
      *         MQDISC from the queue manager                        *
      *                                                              *
      ****************************************************************
      *                                                              *
      *   Exceptions signaled:  none                                 *
      *   Exceptions monitored: none                                 *
      *                                                              *
      *   AMQ3SRV4 has 2 parameters - (1) name of the init. queue    *
      *                               (2) name of the queue manager  *
      *                                                              *
      *   Note: The second parameter is optional                     *
      *                                                              *
      ****************************************************************

      // note, sample uses defaults where it can

     FQSYSPRT   O    F 1024        PRINTER

      // Connection Handle
     D HConn           S             10I 0

      // Options
     D Options         S             10I 0

      // Object handle
     D HObj            S             10I 0

      // Completion codes
     D OpenCode        S             10I 0
     D CompletionCode  S             10I 0

      // Reasons
     D Reason          S             10I 0
     D ConnReason      S             10I 0

     D TriggerLength   S             10I 0
     D MessageLength   S             10I 0

      // Print first 80 chars of message
     D Buffer          S             80A

      // Holds command to run. Max length is 1012
      // 732 (MQTMC2) + 256 (MQTMC2.TC2AI) + 24 ( QSYS/CALL CMD(...) PARM('...') )
     D CommandString   S           1012A
     D CommandLength   S             15P 5 INZ(%LEN(CommandString))

      // QCMDEXC - Prototyped Call
     D QCMDEXC         PR                  EXTPGM('QCMDEXC')
     D  Command                    1012A   OPTIONS(*VARSIZE) CONST
     D  CommandLength                15P 5 CONST

      // Declare MQI structures needed
      // MQI named constants
     D/COPY CMQG

      // Object Descriptor
     D MQOD            DS                  qualified
     D/COPY CMQODG

      // Message Descriptor
     D MQMD            DS                  qualified
     D/COPY CMQMDG

      // Get message options
     D MQGMO           DS                  qualified
     I/COPY CMQGMOG

      // Trigger Message Options
     D MQTMC2          DS                  qualified
     I/COPY CMQTMC2G

      // Optional parameters
     D Qmgr            S             48A

      // Output fields
     D Status          S              5A
     D MQCall          S              7A
     D ReasonCode      S              4A
     D Message         S           1024A

      // program parameters are: 48-character target queue name
      //                         48-character queue manager name (optional)
     D Main            PR                  ExtPgm('AMQ3SRV4')
     D Queue                         48A
     D QmgrName                      48A   Options(*NOPASS)

     D Main            PI
     D Queue                         48A
     D QmgrName                      48A   Options(*NOPASS)

      /free
       // indicate that sample program has started
       Status = 'start';
       Except Sample;

       If %parms >= 1;
         Qmgr = QmgrName;
         Message = 'Using Queue Manager ' + Qmgr;
       Else;
         Message = 'Using Default Queue Manager';
       EndIf;
       Except DspMessage;

       //***************************************************************
       //                                                              *
       //   Create object descriptor for source queue                  *
       //                                                              *
       //***************************************************************
       MQOD.ODON = Queue;

       //***************************************************************
       //                                                              *
       //   Connect to the queue manager                               *
       //                                                              *
       //***************************************************************
       MQCONN(Qmgr : HConn : OpenCode : Reason);

       // report reason and stop if it failed
       If OpenCode = CCFAIL;
         // "MQCONN ended with reason code ...."
         MQCall = 'MQCONN';
         ReasonCode = %CHAR(Reason);
         Except MQFailure;

         Message = 'Unable to connect to queue manager';
         Except DspMessage;
         *INLR = *ON;
         Return;
       EndIf;

       //***************************************************************
       //                                                              *
       //   Open the named message queue for input (and fail if MQM    *
       //   is quiescing); exclusive or shared use of the queue is     *
       //   controlled by the queue definition here                    *
       //                                                              *
       //   Resulting queue handle is HObj                             *
       //                                                              *
       //***************************************************************
       // options are input-as-queue-def and fail-if-quiescing
       Options = OOINPQ + OOFIQ;
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

       //***************************************************************
       //                                                              *
       //   Get messages from message queue (initiation)               *
       //                                                              *
       //***************************************************************
       // initial loop condition based on result of MQOPEN
       CompletionCode = OpenCode;

       //   buffer length available for trigger message..
       TriggerLength = %SIZE(MQTMC2);

       // start of loop to read all the messages
       DoW CompletionCode <> CCFAIL;

         // option is to wait indefinitely for next message
         // and allow truncation.
         MQGMO.GMOPT = GMWT;
         MQGMO.GMOPT += GMCONV;
         MQGMO.GMOPT += GMATM;
         MQGMO.GMWI = WIULIM;

         // MsgId and CorrelId are selectors that must be cleared
         // to get messages in sequence, and they are set each MQGET
         MQMD.MDMID = MINONE;
         MQMD.MDCID = CINONE;

         // clear buffer because MQGET only fills to length of message
         MQTMC2 = *BLANKS;

         MQGET(HConn : HObj : MQMD : MQGMO : TriggerLength : %ADDR(MQTMC2) :
               MessageLength : CompletionCode : Reason);

         // GET completes normally
         If Reason = RCNONE;

           // Construct command string to execute OS/400 CALL and run via QCMDEXC.
           MQTMC2.TC2QMN = Qmgr;
           CommandString='QSYS/CALL PGM(' + %TRIMR(MQTMC2.TC2AI) + ') ' +
                         'PARM(''' + MQTMC2 + ''')';

           Message = 'Application Type OS/400 requested.';
           Except DspMessage;

           Message = 'Command String: ' + CommandString;
           Except DspMessage;

           QCMDEXC(CommandString : CommandLength);

         Else; // the GET has failed
           Select;
           When Reason = RC2033;
             Message = 'no more messages';
             Except DspMessage;
           Other;
             // "MQGET ended with reason code ...."
             MQCall = 'MQGET';
             ReasonCode = %CHAR(Reason);
             Except MQFailure;
           EndSl;
         EndIf;

         // display each message received
         If CompletionCode <> CCFAIL;
           Buffer = MQTMC2;
           Except Text;
         EndIf;
       EndDo; // end DO-while loop; MQI failure causes loop to end

       //***************************************************************
       //                                                              *
       //   Close the source queue (if it was opened)                  *
       //                                                              *
       //***************************************************************
       If OpenCode <> CCFAIL;
         Options = CONONE;
         MQCLOSE(HConn : HObj : Options : CompletionCode : Reason);

         // report reason, if any, resulting from this call
         If Reason <> RCNONE;
           // "MQCLOSE ended with reason code ...."
           MQCall = 'MQCLOSE';
           ReasonCode = %CHAR(Reason);
           Except MQFailure;
         EndIf;
       EndIf;

       //***************************************************************
       //                                                              *
       //  Disconnect from queue manager (if not previously connected) *
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

       // "Sample AMQ3SRV4 end"
       Status = 'end';
       Except Sample;

       *INLR = *ON;
      /end-free
      //***************************************************************
     OQSYSPRT   E            Sample         1
     O                                              'Sample AMQ3SRV4 '
     O                       Status
     O          E            DspMessage     1
     O                       Message
     O          E            Text           1
     O                                              'message <'
     O                       Buffer
     O                                              '>'
     O          E            MQFailure      1
     O                       MQCall
     O                                              ' ended with reason code '
     O                       ReasonCode
      //***************************************************************
      //                                                              *
      // END OF AMQ3SRV4                                              *
      //                                                              *
      //***************************************************************
