     H
      ****************************************************************
      *                                                              *
      * Program name: AMQ3PUB4                                       *
      *                                                              *
      * Description: Sample ILE RPG program that puts messages       *
      *              to a TOPIC (example using MQPUT)                *
      *                                                              *
      *   <copyright                                                 *
      *   notice="lm-source-program"                                 *
      *   pids="5724-H72,"                                           *
      *   years="2011,2012"                                          *
      *   crc="1599515557" >                                         *
      *   Licensed Materials - Property of IBM                       *
      *                                                              *
      *   5724-H72,                                                  *
      *                                                              *
      *   (C) Copyright IBM Corp. 2011, 2012 All Rights Reserved.    *
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
      *   AMQ3PUB4 is a sample ILE RPG program to publish messages   *
      *   to a topic.                                                *
      *                                                              *
      *      -- sample message input is read using the ACCEPT        *
      *         verb, with a blank entry indicating the end of       *
      *         input; the program parameter identifies the          *
      *         target topic                                         *
      *         (So if this sample is run in an interactive job,     *
      *         lines would be read from the terminal)               *
      *                                                              *
      *      -- puts each text line in the input to the topic        *
      *         taking each line of text as the content              *
      *         of a message                                         *
      *                                                              *
      *      -- writes a message for each MQI reason other than      *
      *         RCNONE; stops if there is a MQI completion           *
      *         code of CCFAI                                        *
      *                                                              *
      *    Program logic:                                            *
      *         MQCONNect to target queue manager                    *
      *         MQOPEN target topic for OUTPUT/publishing            *
      *         display prompt for message(s)                        *
      *         while no MQI failures,                               *
      *         .  ACCEPT line of text                               *
      *         .  MQPUT message with text to target topic           *
      *         MQCLOSE target topic                                 *
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
      *   AMQ3PUB4 has 2 parameters - (1) name of the target topic   *
      *                               (2) name of the queue manager  *
      *                                                              *
      *   Note: The second parameter is optional                     *
      *                                                              *
      ****************************************************************

      // note, sample uses defaults where it can

     FQSYSPRT   O    F  132        PRINTER

      // Test data used as input
     D Data            S             60    DIM(3) CTData

      // Buffer length
     D BufferLength    S             10I 0 INZ(%LEN(Data))

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

      // Loop Counter
     D I               S             10I 0 INZ(1)

      // Declare MQI structures needed
      // MQI named constants
     D/COPY CMQG

      // Object Descriptor
     D MQOD            DS                  Qualified
     D/COPY CMQODG

      // Message Descriptor
     D MQMD            DS                  Qualified
     D/COPY CMQMDG

      // Put message options
     D MQPMO           DS                  Qualified
     I/COPY CMQPMOG

      // Optional parameters
     D Qmgr            S             48A

      // Output fields
     D Status          S              5A
     D MQCall          S              7A
     D ReasonCode      S              4A
     D Message         S             50A

      // program parameters are: 48-character target topic name
      //                         48-character queue manager name (optional)
     D Main            PR                  ExtPgm('AMQ3PUB4')
     D Topic                         48A
     D QmgrName                      48A   Options(*NOPASS)

     D Main            PI
     D Topic                         48A
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

       Message = 'Target topic is ' + Topic;
       Except DspMessage;

       //***************************************************************
       //                                                              *
       //   Connect to the queue manager                               *
       //                                                              *
       //***************************************************************
       MQCONN(Qmgr : HConn : CompletionCode : Reason);

       // report reason and stop if it failed
       If CompletionCode = CCFAIL;
         // "MQCONN ended with reason code ...."
         MQCall = 'MQCONN';
         ReasonCode = %CHAR(Reason);
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

       // open a Topic object
       MQOD.ODOT = OTTOP;

       // use version 4 of the MQOD, so we can use the long object name
       MQOD.ODVER = ODVER4;

       //***************************************************************
       //                                                              *
       //    use parameter as the name of the target queue             *
       //                                                              *
       //***************************************************************
       MQOD.ODOSCHRP = %ADDR(Topic);
       MQOD.ODOSCHRL = %LEN(%TRIMR(Topic));

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
         Message = 'Unable to open queue for output';
         Except DspMessage;
       EndIf;

       // initial loop condition is based on result of MQOPEN
       CompletionCode = OpenCode;

       // put options are Fail-if-quiescing and no syncpoint
       MQPMO.PMOPT = PMFIQ + PMNSYP;

       // message is string type
       MQMD.MDFMT = FMSTR;

       //   Start loop to put messages
       DoW CompletionCode <> CCFAIL AND I <= %ELEM(Data);

           BufferLength = %LEN(%TRIMR(Data(I)));
           MQPUT(HConn : HObj : MQMD : MQPMO :  BufferLength : %ADDR(Data(I)) :
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
       //   Close the target topic (if it was opened)                  *
       //                                                              *
       //***************************************************************
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

       // "Sample AMQ3PUB4 end"
       Status = 'end';
       Except Sample;

       *INLR = *ON;
      /End-Free
      //***************************************************************
     OQSYSPRT   E            Sample         1
     O                                              'Sample AMQ3PUB4 '
     O                       Status
     O          E            DspMessage     1
     O                       Message
     O          E            MQFailure      1
     O                       MQCall
     O                                              ' ended with reason code '
     O                       ReasonCode
      //***************************************************************
      //                                                              *
      // END OF AMQ3PUB4                                              *
      //                                                              *
      //***************************************************************
      //***
      //  Sample input follows
      //***
**
This is the first message added to the topic.
This is the second message.
This is the final message.
