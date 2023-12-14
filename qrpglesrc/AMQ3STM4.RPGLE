     H
      ****************************************************************
      *                                                              *
      * Program name: AMQ3STM4                                       *
      *                                                              *
      * Description: Sample RPG program that sets properties of a    *
      *              message handle using MQSETMP and puts it to a   *
      *              message queue.                                  *
      *                                                              *
      *   <copyright                                                 *
      *   notice="lm-source-program"                                 *
      *   pids="5724-H72,"                                           *
      *   years="2011,2012"                                          *
      *   crc="2189574124" >                                         *
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
      *   AMQ3STM4 is a sample RPG program to set properties of a    *
      *   message handle and puts it to a message queue, and is an   *
      *   example of the use of MQSETMP.                             *
      *                                                              *
      *    Program logic:                                            *
      *         MQCONN to the target queue manager                   *
      *         MQOPEN target queue for OUTPUT                       *
      *         for each line in the input data array,               *
      *         .  MQCRTMH create message handle                     *
      *         .  for each line in the properties array,            *
      *            .   MQSETMP set the message property on the       *
      *                message handle                                *
      *         .  MQPUT datagram message with text line as data     *
      *         .  MQDLTMH delete message handle                     *
      *         MQCLOSE target queue                                 *
      *         MQDISC from the queue manager                        *
      *                                                              *
      ****************************************************************
      *                                                              *
      *                                                              *
      *                                                              *
      *   Exceptions signaled:  none                                 *
      *   Exceptions monitored: none                                 *
      *                                                              *
      *   AMQ3STM4 has 2 parameters - (1) name of the target queue   *
      *                               (2) name of the queue manager  *
      *                                                              *
      *   Note: The second parameter is optional                     *
      *                                                              *
      ****************************************************************

      // note, sample uses defaults where it can

     FQSYSPRT   O    F  132        PRINTER

      // Test data used as input
     D Data            S             60    DIM(3) CTData
     D NumProperties   C                   CONST(2)
     D Properties      S             60    DIM(NumProperties) CTData
     D PropertyNames   S             20    DIM(NumProperties) CTData

      // Buffer length
     D BufferLength    S             10I 0

      // Options
     D Options         S             10I 0

      // Handles
     D HConn           S             10I 0
     D HObj            S             10I 0
     D HMsg            S             20I 0

      // Completion codes
     D OpenCode        S             10I 0
     D CompletionCode  S             10I 0

      // Reasons
     D Reason          S             10I 0
     D ConnReason      S             10I 0

     D PropName        DS                  Qualified
     D/COPY CMQCHRVG

     D PropertyName    S             40A   Varying
     D PropertyValue   S             40A   Varying
     D PropertyLength  S             10I 0

      // Loop Counters
     D I               S             10I 0 INZ(1)
     D J               S             10I 0 INZ(1)

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
     D/COPY CMQPMOG

     D MQCMHO          DS                  Qualified
     D/COPY CMQCMHOG

     D MQDMHO          DS                  Qualified
     D/COPY CMQDMHOG

     D MQPD            DS                  Qualified
     D/COPY CMQPDG

     D MQSMPO          DS                  Qualified
     D/COPY CMQSMPOG

      // Optional parameters
     D Qmgr            S             48A

      // Output fields
     D Status          S              5A
     D MQCall          S              7A
     D ReasonCode      S              4A
     D Message         S             50A

      // program parameters are: 48-character target queue name
      //                         48-character queue manager name (optional)
     D Main            PR                  ExtPgm('AMQ3STM4')
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

       //***************************************************************
       //                                                              *
       //    use parameter as the name of the target queue             *
       //                                                              *
       //***************************************************************
       MQOD.ODON = Queue;

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

       // message is string type
       MQMD.MDFMT = FMSTR;

       //   Start loop to put messages
       DoW CompletionCode <> CCFAIL AND I <= %ELEM(Data);

           MQCMHO.CMOPT = CMVAL;

           MQCRTMH(HConn : MQCMHO : HMsg : CompletionCode : Reason);

           // report reason, if any; stop if failed
           If Reason <> RCNONE;
             // "MQCRTMH ended with reason code ...."
             MQCall = 'MQCRTMH';
             ReasonCode = %CHAR(Reason);
             Except MQFailure;

             // Skip to the next iteration of the loop (causing end)
             Iter;
           EndIf;

           J = 1;

           DoW J <= %ELEM(Properties);
             PropertyName = %TRIMR(PropertyNames(J));
             PropertyValue = %TRIMR(Properties(J));

             PropName.VCHRP = %ADDR(PropertyName) + 2;
             PropName.VCHRL = %LEN(PropertyName);

             MQSETMP(HConn : HMsg : MQSMPO : PropName : MQPD : TYPSTR :
                     %LEN(PropertyValue) : %ADDR(PropertyValue) + 2 :
                     CompletionCode : Reason);

             // report reason, if any; stop if failed
             If Reason <> RCNONE;
               // "MQSETMP ended with reason code ...."
               MQCall = 'MQSETMP';
               ReasonCode = %CHAR(Reason);
               Except MQFailure;

               // Skip to the next iteration of the loop (causing end)
               Iter;
             EndIf;

             J += 1;
           EndDo;

           // report reason, if any; stop if failed
           If Reason <> RCNONE;
             Iter;
           EndIf;


           MQPMO.PMVER = PMVER3;
           MQPMO.PMOMH = HMsg;

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

           MQDLTMH(Hconn : HMsg : MQDMHO : CompletionCode : Reason);
           // report reason, if any; stop if failed
           If Reason <> RCNONE;
             // "MQDLTMH ended with reason code ...."
             MQCall = 'MQDLTMH';
             ReasonCode = %CHAR(Reason);
             Except MQFailure;
           EndIf;

           I += 1;
       EndDo;

       //***************************************************************
       //                                                              *
       //   Close the target queue (if it was opened)                  *
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

       // "Sample AMQ3STM4 end"
       Status = 'end';
       Except Sample;

       *INLR = *ON;
      /End-Free
      //***************************************************************
     OQSYSPRT   E            Sample         1
     O                                              'Sample AMQ3STM4 '
     O                       Status
     O          E            DspMessage     1
     O                       Message
     O          E            MQFailure      1
     O                       MQCall
     O                                              ' ended with reason code '
     O                       ReasonCode
      //***************************************************************
      //                                                              *
      // END OF AMQ3STM4                                              *
      //                                                              *
      //***************************************************************
      //***
      //  Sample input follows
      //***
**CTDATA Data
This is the first message added to the queue.
This is the second message.
This is the final message.
**CTDATA PropertyNames
MessageFormat
UserID
**CTDATA Properties
XML
DummyID
