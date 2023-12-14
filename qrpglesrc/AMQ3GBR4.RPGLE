     H
      ****************************************************************
      *                                                              *
      * Program name: AMQ3GBR4                                       *
      *                                                              *
      * Description: Sample RPG program that displays messages on    *
      *              a message queue (example using Browse option    *
      *              of MQGET)                                       *
      *                                                              *
      *   <copyright                                                 *
      *   notice="lm-source-program"                                 *
      *   pids="5724-H72,"                                           *
      *   years="1994,2012"                                          *
      *   crc="650337914" >                                          *
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
      *   AMQ3GBR4 is a sample RPG program to display messages on    *
      *   a message queue, and is an example of Browse option of     *
      *   MQGET                                                      *
      *                                                              *
      *      -- sample gets messages from the queue named in         *
      *         the program parameter                                *
      *                                                              *
      *      -- displays the contents of the message queue,          *
      *         assuming each message data to represent a line       *
      *         of text to be written                                *
      *                                                              *
      *         only the first 70 characters of each message is      *
      *         shown; incomplete messages are noted                 *
      *                                                              *
      *         leaves the messages on the queue                     *
      *                                                              *
      *         the displayed name of the queue is the resolved      *
      *         local queue if the object name is an alias           *
      *                                                              *
      *      -- writes a message for each MQI reason other than      *
      *         RCNONE; stops if there is a MQI completion code      *
      *         of CCFAIL                                            *
      *                                                              *
      *    Program logic:                                            *
      *         MQCONN to the target queue manager                   *
      *         MQOPEN queue for BROWSE                              *
      *         while no MQI failures,                               *
      *         .  MQGET next message (browse)                       *
      *         .  display Resolved Queue Name first time            *
      *         .  display up to 70 bytes of message data            *
      *         MQCLOSE the source queue                             *
      *         MQDISC from the queue manager                        *
      *                                                              *
      ****************************************************************
      *                                                              *
      *                                                              *
      *                                                              *
      *   Exceptions signaled:  none                                 *
      *   Exceptions monitored: none                                 *
      *                                                              *
      *   AMQ3GBR4 has 2 parameters - (1) name of the target queue   *
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
     D Buffer          S             70A

      // Buffer length
     D BufferLength    S             10I 0 INZ(%LEN(Buffer))

      // Message length
     D MessageLength   S             10I 0

      // Loop counter
     D I               S              5I 0 INZ(0)
     D MessageNumber   S              5A

      //  Declare MQI structures needed
      // MQI named constants
     D/COPY CMQG

      // Object Descriptor
     D MQOD            DS                  Qualified
     D/COPY CMQODG

      // Message Descriptor
     D MQMD            DS                  Qualified
     D/COPY CMQMDG

      // Get message options
     D MQGMO           DS                  Qualified
     I/COPY CMQGMOG

      // Output fields
     D Status          S              5A
     D MQCall          S              7A
     D ReasonCode      S              4A
     D Message         S             50A

      // Input parameters
     D Qmgr            S             48A

      // program parameters are: 48-character target queue name
      //                         48-character queue manager name (optional)
     D Main            PR                  ExtPgm('AMQ3GBR4')
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

       //**************************************************************
       //                                                             *
       //   Connect to the queue manager                              *
       //                                                             *
       //**************************************************************
       MQCONN(Qmgr : HConn : OpenCode : ConnReason);

       // report reason and stop if it failed
       If OpenCode = CCFAIL;
         //  "MQCONN ended with reason code ...."
         MQCall = 'MQCONN';
         ReasonCode = %CHAR(ConnReason);
         Except MQFailure;

         CompletionCode = OpenCode;
         Message = 'Unable to connect to Queue Manager';
         Except DspMessage;

         *INLR = *ON;
         RETURN;
       EndIf;

       //**************************************************************
       //                                                             *
       //   Open the source message queue for browse (and fail if     *
       //   MQM is quiescing)                                         *
       //                                                             *
       //   Resulting queue handle is HObj                            *
       //                                                             *
       //**************************************************************

       //**************************************************************
       //                                                             *
       //   Create object descriptor for source queue                 *
       //                                                             *
       //**************************************************************
       MQOD.ODON = Queue;

       // Open options are Browse and Fail-of-quiescing
       Options = OOBRW + OOFIQ;
       MQOPEN(HConn : MQOD : Options : HObj : OpenCode : Reason);

       // report reason, if any; stop if failed
       If Reason <> RCNONE;
         //  "MQOPEN ended with reason code ...."
         MQCall = 'MQOPEN';
         ReasonCode = %CHAR(Reason);
         Except MQFailure;
       EndIf;

       If OpenCode = CCFAIL;
         Message = 'Unable to open queue for browse';
         Except DspMessage;
       EndIf;

       //**************************************************************
       //                                                             *
       //   Get messages from message queue                           *
       //                                                             *
       //**************************************************************
       //  initial loop condition based on result of MQOPEN
       CompletionCode = OpenCode;

       //  start of loop to browse all the messages
       DoW CompletionCode <> CCFAIL;
         // GET options are to Browse next message, without waiting ...
         MQGMO.GMOPT = GMBRWN + GMNWT;

         //      ... and Accept-truncated-message
         MQGMO.GMOPT += GMATM;

         //      ... and convert application data
         MQGMO.GMOPT += GMCONV;

         //  MsgId and CorrelId are selectors that must be cleared
         //  to get messages in sequence, and they are set each MQGET
         MQMD.MDMID = MINONE;
         MQMD.MDCID = CINONE;

         //  clear buffer because MQGET only fills to length of message
         Buffer = *BLANKS;

         MQGET(HConn : HObj : MQMD : MQGMO : BufferLength : %ADDR(Buffer) :
               MessageLength : CompletionCode : Reason);

         // display each message received
         If CompletionCode <> CCFAIL;
           // display name of resolved queue before first one
           If I = 0;
             Message = 'Messages for ' + MQGMO.GMRQN;
             Except DspMessage;
           EndIf;

           I += 1;
           MessageNumber = %CHAR(I);
           Except Text;
         EndIf;

         // report reason (from MQGET), if any
         Select;
           When Reason = RCNONE;

           When Reason = RC2033;
             Message = 'no more messages';
             Except DspMessage;

           When MessageLength > BufferLength;
             Message = '  --- truncated';
             Except DspMessage;

           Other;
             // "MQGET ended with reason code ...."
             MQCall = 'MQGET ';
             ReasonCode = %CHAR(Reason);
             Except MQFailure;
         EndSl;
       EndDo; //  end DO-while loop; MQI failure causes loop to end

       //**************************************************************
       //                                                              *
       //   Close the source queue (if it was opened)                  *
       //                                                              *
       //**************************************************************
       // test if queue was opened
       If OpenCode <> CCFAIL;
         //   close it with no options if it was
         Options = CONONE;
         MQCLOSE(HConn : HObj : Options : CompletionCode : Reason);

         // report reason (from MQCLOSE), if any
         If Reason <> RCNONE;
           //  "MQCLOSE ended with reason code ...."
           MQCall = 'MQCLOSE';
           ReasonCode = %CHAR(Reason);
           Except MQFailure;
         EndIf;
       EndIf;

       //**************************************************************
       //                                                             *
       // Disconnect from queue manager (if not previously connected) *
       //                                                             *
       //**************************************************************
       If ConnReason <> RC2002;
         MQDISC(HConn : OpenCode : Reason);

         // report reason and stop if it failed
         If OpenCode = CCFAIL;
           //  "MQDISC ended with reason code ...."
           MQCall = 'MQDISC';
           ReasonCode = %CHAR(Reason);
           Except MQFailure;
         EndIf;
       EndIf;

       // "Sample AMQ3GBR4 end"
       Status = 'end';
       Except Sample;

       *INLR = *ON;
      /End-Free
      //**************************************************************
     OQSYSPRT   E            Sample         1
     O                                              'Sample AMQ3GBR4 '
     O                       Status
     O          E            DspMessage     1
     O                       Message
     O          E            Text           1
     O                       MessageNumber
     O                                              ' <'
     O                       Buffer
     O                                              '>'
     O          E            MQFailure      1
     O                       MQCall
     O                                              ' ended with reason code '
     O                       ReasonCode
      //**************************************************************
      //                                                             *
      // END OF AMQ3GBR4                                             *
      //                                                             *
      //**************************************************************
