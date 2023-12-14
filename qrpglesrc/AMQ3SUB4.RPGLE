     H
      ****************************************************************
      *                                                              *
      * Program name: AMQ3SUB4                                       *
      *                                                              *
      * Description: Sample ILE RPG program that subscribes and gets *
      *              messages from a topic (example using MQSUB).    *
      *              A managed destination is used                   *
      *                                                              *
      *   <copyright                                                 *
      *   notice="lm-source-program"                                 *
      *   pids="5724-H72,"                                           *
      *   years="2011,2012"                                          *
      *   crc="3655313366" >                                         *
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
      *   AMQ3SUB4 is a sample RPG program to subscribe to a         *
      *   topic and get messages from the managed destination        *
      *   queueand                                                   *
      *                                                              *
      *      -- subscribe non-durably to the topic named in the      *
      *         program parameter                                    *
      *                                                              *
      *      -- calls MQGET repeatedly to get messages from the      *
      *         managed destination queue and displays the message   *
      *                                                              *
      *      -- writes a message for each MQI reason other than      *
      *         RCNONE; stops if there is a MQI completion           *
      *         code of CCFAIL                                       *
      *                                                              *
      *    Program logic:                                            *
      *         MQSUB topic for input                                *
      *         while no MQI failures,                               *
      *         .  MQGET next message                                *
      *         .  print the result                                  *
      *         MQCLOSE the topic                                    *
      *                                                              *
      *                                                              *
      ****************************************************************
      *                                                              *
      *                                                              *
      *                                                              *
      *   Exceptions signaled:  none                                 *
      *   Exceptions monitored: none                                 *
      *                                                              *
      *   AMQ3SUB4 has 2 parameters - (1) name of the target topic   *
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

      // Subscription handle
     D HSub            S             10I 0

      // Completion codes
     D SubscribeCode   S             10I 0
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

      // Subscription Descriptor
     D MQSD            DS                  Qualified
     D/COPY CMQSDG

      // Message Descriptor
     D MQMD            DS                  Qualified
     D/COPY CMQMDG

      // Get message options
     D MQGMO           DS                  Qualified
     D/COPY CMQGMOG

      // Optional parameters
     D Qmgr            S             48A

      // Output fields
     D Status          S              5A
     D MQCall          S              7A
     D ReasonCode      S              4A
     D Message         S            100A

      // note, sample uses defaults where it can
      // program parameters are: 48-character target topic name
      //                         48-character queue manager name (optional)
     D Main            PR                  ExtPgm('AMQ3SUB4')
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
       //   Open the named topic (and fail if MQM is quiescing)        *
       //   is quiescing); exclusive or shared use of the queue is     *
       //   controlled by the queue definition here                    *
       //                                                              *
       //   Resulting queue handle is HObj                             *
       //                                                              *
       //***************************************************************

       MQSD.SDOSCHRP = %ADDR(Topic);
       MQSD.SDOSCHRL = %LEN(%TRIMR(Topic));

       // options are create, non-durable, fail-if-quiescing and managed
       MQSD.SDOPT = SOCRT + SONDUR + SOFIQ + SOMAN;

       MQSUB(HConn : MQSD : HObj : HSub : SubscribeCode : Reason);

       // report reason, if any; stop if failed
       If Reason <> RCNONE;
         // "MQSUB ended with reason code ...."
         MQCall = 'MQSUB';
         ReasonCode = %CHAR(Reason);
         Except MQFailure;
       EndIf;

       If SubscribeCode = CCFAIL;
         Message = 'Unable to subscribe to topic';
         Except DspMessage;
       EndIf;

       // Set up Get Message Options

       // wait up to 30 seconds for next message
       MQGMO.GMOPT = GMWT;
       MQGMO.GMWI = 30000;

       // also convert message data and do not get under syncpoint
       MQGMO.GMOPT += GMCONV + GMNSYP;

       // initial loop condition is based on result of MQSUB
       CompletionCode = SubscribeCode;

       // start of loop to read all the messages
       DoW CompletionCode <> CCFAIL;

         // MQGET sets Encoding and CodedCharSet to values in messages returned
         // so these fields should be reset to default values before every
         // call, as GMCONV is specified
         MQMD.MDENC = ENNAT;
         MQMD.MDCSI = CSQM;

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
       EndDo; // end DO-while loop; MQI failure causes loop to end

       //***************************************************************
       //                                                              *
       //   Close the subscription handle (if it was opened)           *
       //                                                              *
       //***************************************************************
       If SubscribeCode <> CCFAIL;
         Options = CONONE;
         MQCLOSE(HConn : HObj : Options :  CompletionCode : Reason);

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
         MQDISC(HConn : CompletionCode : Reason);

         // report reason and stop if it failed
         If CompletionCode = CCFAIL;
           // "MQDISC ended with reason code ...."
           MQCall = 'MQDISC';
           ReasonCode = %CHAR(Reason);
           Except MQFailure;
         EndIf;
       EndIf;

       // "Sample AMQ3SUB4 end"
       Status = 'end';
       Except Sample;

       *INLR = *ON;
      /End-Free
      //***************************************************************
     OQSYSPRT   E            Sample         1
     O                                              'Sample AMQ3SUB4 '
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
      // END OF AMQ3SUB4                                              *
      //                                                              *
      //***************************************************************
