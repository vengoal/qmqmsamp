     H
      ****************************************************************
      *                                                              *
      * Program name: AMQ3TRG4                                       *
      *                                                              *
      * Description: Sample ILE RPG program that acts as a trigger   *
      *              monitor which submits program call jobs.        *
      *                                                              *
      *   <copyright                                                 *
      *   notice="lm-source-program"                                 *
      *   pids="5724-H72,"                                           *
      *   years="1994,2012"                                          *
      *   crc="1142432520" >                                         *
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
      *   AMQ3TRG4 is a sample RPG program that acts as a trigger    *
      *   monitor. It reads from an initiation queue then submits    *
      *   a job which in turn calls the supplied program with each   *
      *   trigger message.                                           *
      *                                                              *
      *      -- reads message from trigger initiation queue named    *
      *         in the input parameter.                              *
      *                                                              *
      *      -- submits job for each valid trigger message           *
      *                                                              *
      *        -- command calls process named in application id.     *
      *          -- parm = character version of trigger message      *
      *                                                              *
      *      -- writes a message for each MQI reason other than      *
      *         RCNONE; stops if there is a MQI completion code      *
      *         of CCFAIL                                            *
      *                                                              *
      *                                                              *
      *   Program Logic                                              *
      *         Use program parameter as initiation queue name       *
      *         MQOPEN initiation queue for INPUT                    *
      *         while no MQI failures:                               *
      *         .  MQGET next message & remove from queue            *
      *         .  submit job based on trigger message               *
      *         .  .  Applid is name of program to call              *
      *         .  .  MQTMC2 is parameter                            *
      *         MQCLOSE the initiation queue                         *
      *         MQDISC from the queue manager                        *
      *                                                              *
      ****************************************************************
      *                                                              *
      *   Exceptions signaled:  none                                 *
      *   Exceptions monitored: none                                 *
      *                                                              *
      *   AMQ3TRG4 has 2 parameters - (1) name of the init queue     *
      *                               (2) name of the queue manager  *
      *                                                              *
      *   Note: The second parameter is optional                     *
      *                                                              *
      ****************************************************************

      // note, sample uses defaults where it can

     FQSYSPRT   O    F  132        PRINTER

      // Connection Handle
     D Hconn           S             10I 0

      // Options
     D Options         S             10I 0

      // Object handle
     D Hobj            S             10I 0

      // Completion codes
     D OpenCode        S             10I 0
     D CompletionCode  S             10I 0

      // Reasons
     D Reason          S             10I 0
     D ConnReason      S             10I 0

     D TriggerLength   S             10I 0 INZ(%SIZE(MQTMC2))
     D MessageLength   S             10I 0

      // Command to run - max length is 1043
      // 732 (MQTMC2) + 256 (MQTMC2.TC2AI) +
      // 55 ( QSYS/SBMJOB CMD(QSYS/CALL PGM(...) PARM('...')) JOB(AMQ3TRG4) )
     D CommandString   S           1043A
     D CommandLength   S             15P 5 INZ(%LEN(CommandString))

      // QCMDEXC - Prototyped Call
     D QCMDEXC         PR                  EXTPGM('QCMDEXC')
     D  CommandString              1200A   OPTIONS(*VARSIZE) CONST
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
     D Message         S            100A
     D Buffer          S             80A

      // program parameters are: 48-character target queue name
      //                         48-character queue manager name (optional)
     D Main            PR                  ExtPgm('AMQ3TRG4')
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
       MQCONN(Qmgr : HConn : OpenCode : ConnReason);

       // report reason and stop if it failed
       If OpenCode = CCFAIL;
         // "MQCONN ended with reason code ...."
         MQCall = 'MQCONN';
         ReasonCode = %CHAR(ConnReason);
         Except MQFailure;

         CompletionCode = OpenCode;
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
       //   Resulting queue handle is Hobj                             *
       //                                                              *
       //***************************************************************

       // options are input-as-queue-def and fail-if-quiescing
       Options = OOINPQ + OOFIQ;

       MQOPEN(Hconn : MQOD : Options : Hobj : OpenCode : Reason);

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

         //   GET Trigger..
         MQGET(Hconn : Hobj : MQMD : MQGMO : TriggerLength : %ADDR(MQTMC2) :
               MessageLength : CompletionCode : Reason);

         // GET completes normally
         If Reason = RCNONE;

           // Construct command string to submit OS/400 CALL command and
           // run via QCMDEXC.

           CommandString='QSYS/SBMJOB CMD(QSYS/CALL ' +
                         'PGM(' + %TRIMR(MQTMC2.TC2AI) + ')' +
                         'PARM(''' + MQTMC2 + ''')) +
                         JOB(AMQ3TRG4)';
                         // Add additional SBMJOB parms here

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
         MQCLOSE(Hconn : Hobj : Options : CompletionCode : Reason);

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

       // "Sample AMQ3TRG4 end"
       Status = 'end';
       Except Sample;

       *INLR = *ON;
      /end-free
      //***************************************************************
     OQSYSPRT   E            Sample         1
     O                                              'Sample AMQ3TRG4 '
     O                       Status
     O          E            DspMessage     1
     O                       Message
     O          E            Text           1
     O                                              'message <'
     O                       Buffer
     O                                              '>'
     O          E            MQFailure         1
     O                       MQCall
     O                                              ' ended with reason code '
     O                       ReasonCode
      //***************************************************************
      //                                                              *
      // END OF AMQ3TRG4                                              *
      //                                                              *
      //***************************************************************
