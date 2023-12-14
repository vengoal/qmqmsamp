     H
      ****************************************************************
      *                                                              *
      * Program name: AMQ3GET4                                       *
      *                                                              *
      * Description: Sample RPG program that gets messages from      *
      *              a message queue (example using MQGET)           *
      *                                                              *
      *   <copyright                                                 *
      *   notice="lm-source-program"                                 *
      *   pids="5724-H72,"                                           *
      *   years="1994,2012"                                          *
      *   crc="1063725545" >                                         *
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
      *   AMQ3GET4 is a sample RPG program to get messages from a    *
      *   message queue, and is an example of MQGET                  *
      *                                                              *
      *      -- sample gets messages from the queue named in         *
      *         the parameter                                        *
      *                                                              *
      *      -- displays the contents of the message queue,          *
      *         assuming each message data to represent a line       *
      *         of text to be written                                *
      *                                                              *
      *         messages are removed from the queue                  *
      *                                                              *
      *      -- writes a message for each MQI reason other than      *
      *         RCNONE; stops if there is a MQI completion code      *
      *         of CCFAIL                                            *
      *                                                              *
      *    Program logic:                                            *
      *         MQCONN to the target queue manager                   *
      *         MQOPEN queue for INPUT                               *
      *         while no MQI failures,                               *
      *         .  MQGET next message, remove from queue             *
      *         .  display the result                                *
      *         .  (no message available is failure, and ends loop)  *
      *         MQCLOSE the subject queue                            *
      *         MQDISC from the queue manager                        *
      *                                                              *
      ****************************************************************
      *                                                              *
      *                                                              *
      *                                                              *
      *   Exceptions signaled:  none                                 *
      *   Exceptions monitored: none                                 *
      *                                                              *
      *   AMQ3GET4 has 2 parameters - (1) name of the target queue   *
      *                               (2) name of the queue manager  *
      *                                                              *
      *   Note: The second parameter is optional                     *
      *                                                              *
      ****************************************************************

      // note, sample uses defaults where it can

     FQSYSPRT   O    F  132        PRINTER

      //  Connection handle
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

      // Buffer
     D Buffer          S             60A

      // Buffer length
     D BufferLength    S             10I 0 INZ(%LEN(Buffer))

      // Actual Message length received
     D MessageLength   S             10I 0

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

      // Optional parameters
     D Qmgr            S             48A

      // Output fields
     D Status          S              5A
     D MQCall          S              7A
     D ReasonCode      S              4A
     D Message         S            100A

      // program parameters are: 48-character target queue name
      //                         48-character queue manager name (optional)
     D Main            PR                  ExtPgm('AMQ3GET4')
     D Queue                         48A
     D QmgrName                      48A   Options(*NOPASS)

     D Main            PI
     D Queue                         48A
     D QmgrName                      48A   Options(*NOPASS)

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

         Message = 'Unable to connect to to Queue Manager';
         Except DspMessage;

         *INLR = *ON;
         RETURN;
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

       //***************************************************************
       //                                                              *
       //   Create object descriptor for source queue                  *
       //                                                              *
       //***************************************************************
       MQOD.ODON = Queue;

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
       //   Get messages from message queue                            *
       //                                                              *
       //***************************************************************

       // option is to wait up to 15 seconds for next message
       MQGMO.GMOPT = GMWT;
       MQGMO.GMWI = 15000;

       MQGMO.GMOPT += GMCONV;

       // initial loop condition is based on result of MQOPEN
       CompletionCode = OpenCode;

       // start of loop to read all the messages
       DoW CompletionCode <> CCFAIL;


         // MsgId and CorrelId are selectors that must be cleared
         // to get messages in sequence, and they are set each MQGET
         MQMD.MDMID = MINONE;
         MQMD.MDCID = CINONE;

         // clear buffer because MQGET only fills to length of message
         Buffer = *BLANKS;

         MQGET(HConn : HObj : MQMD : MQGMO :  BufferLength : %ADDR(Buffer) :
               MessageLength : CompletionCode : Reason);

         // report reason, if any
         Select;
           When Reason = RCNONE;

           When Reason = RC2033;
             Message = 'no more messages';
             Except DspMessage;

           Other;
             // "MQGET ended with reason code ...."
             MQCall = 'MQGET';
             ReasonCode = %CHAR(Reason);
             Except MQFailure;
         EndSl;

         // display each message received
         If CompletionCode <> CCFAIL;
           Except Text;
         EndIf;

         // end DO-while loop; MQI failure causes loop to end
       EndDo;

       //***************************************************************
       //                                                              *
       //   Close the source queue (if it was opened)                  *
       //                                                              *
       //***************************************************************
       If OpenCode <> CCFAIL;
         Options = CONONE;
         MQCLOSE(HConn : HObj : Options :  CompletionCode : Reason);

         // report reason, if any, resulting from this call
         If Reason <> RCNONE;
           // "MQCLOSE ended with reason code ...."
           MQCall = 'MQCLOSE';
           ReasonCode = %CHAR(Reason);
           Except MQFailure;
         EndIf;

         // end if (OpenCode not CCFAIL)
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

       // "Sample AMQ3GET4 end"
       Status = 'end';
       Except Sample;

       *INLR = *ON;
      /End-Free
      //***************************************************************
     OQSYSPRT   E            Sample         1
     O                                              'Sample AMQ3GET4 '
     O                       Status
     O          E            Text           1
     O                                              'message <'
     O                       Buffer
     O                                              '>'
     O          E            DspMessage     1
     O                       Message
     O          E            MQFailure      1
     O                       MQCall
     O                                              ' ended with reason code '
     O                       ReasonCode
      //***************************************************************
      //                                                              *
      // END OF AMQ3GET4                                              *
      //                                                              *
      //***************************************************************
