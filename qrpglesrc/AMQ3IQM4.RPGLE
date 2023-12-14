     H
      ****************************************************************
      *                                                              *
      * Program name: AMQ3IQM4                                       *
      *                                                              *
      * Description:  Sample RPG program that inquires properties of *
      *               a message handle, using MQINQMP, from a        *
      *               message queue.                                 *
      *                                                              *
      *   <copyright                                                 *
      *   notice="lm-source-program"                                 *
      *   pids="5724-H72,"                                           *
      *   years="2011,2012"                                          *
      *   crc="1880024320" >                                         *
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
      *    AMQ3IQM4 is a sample RPG program to inquire properties of *
      *    a message handle from a message queue, and is an example  *
      *    of the use of MQINQMP.                                    *
      *                                                              *
      *    Program logic:                                            *
      *         MQCONN to the target queue manager                   *
      *         MQOPEN queue for INPUT                               *
      *         MQCRTMH for message property calls                   *
      *         MQGET message, remove from queue                     *
      *         display the result                                   *
      *         no message available is failure, and ends            *
      *         otherwise, while no MQI failures                     *
      *            MQINQMP for next message property                 *
      *            display property name and value                   *
      *            (no properties available causes loop to end)      *
      *         MQDLTMH delete message handle                        *
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
      *   AMQ3IQM4 has 2 parameters - (1) name of the target queue   *
      *                               (2) name of the queue manager  *
      *                                                              *
      *   Note: The second parameter is optional                     *
      *                                                              *
      ****************************************************************

      // note, sample uses defaults where it can

     FQSYSPRT   O    F  132        PRINTER

      // Handles
     D HConn           S             10I 0
     D HObj            S             10I 0
     D HMsg            S             20I 0

      // Options
     D Options         S             10I 0

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

     D PropName        DS                  Qualified
     D/COPY CMQCHRVG

     D PropertyName    S             40A   Varying
     D PropNameMax     C                   %LEN(PropertyName)
     D PropertyValue   S             40A   Varying
     D PropValueMax    C                   %LEN(PropertyValue)
     D PropertyLength  S             10I 0
     D ActPropLength   S             10I 0
     D PropertyType    S             10I 0

      // inquire on all properties
      // also, consider 'user.%' to inquire on all user properties
      // or any other query string
     D PropQuery       S             10A   INZ('%') Varying

      // Declare MQI structures needed
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
     D/COPY CMQGMOG

     D MQCMHO          DS                  Qualified
     D/COPY CMQCMHOG

     D MQDMHO          DS                  Qualified
     D/COPY CMQDMHOG

     D MQPD            DS                  Qualified
     D/COPY CMQPDG

     D MQIMPO          DS                  Qualified
     D/COPY CMQIMPOG

      // Optional parameters
     D Qmgr            S             48A

      // Output fields
     D Status          S              5A
     D MQCall          S              7A
     D ReasonCode      S              4A
     D Message         S            100A

      // program parameters are: 48-character target queue name
      //                         48-character queue manager name (optional)
     D Main            PR                  ExtPgm('AMQ3IQM4')
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

       CompletionCode = OpenCode;

       //***************************************************************
       //                                                              *
       //   Create a message handle                                    *
       //                                                              *
       //***************************************************************
       If CompletionCode <> CCFAIL;
         MQCMHO.CMOPT = CMVAL;

         MQCRTMH(HConn : MQCMHO : HMsg : CompletionCode : Reason);

         // report reason, if any; stop if failed
         If Reason <> RCNONE;
           // "MQCRTMH ended with reason code ...."
           MQCall = 'MQCRTMH';
           ReasonCode = %CHAR(Reason);
           Except MQFailure;
         EndIf;
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

       // properties in handle
       MQGMO.GMOPT += GMPRIH;

       // Version 4 needed for message handle fields
       MQGMO.GMVER = GMVER4;
       MQGMO.GMMH = HMsg;

       MQGET(HConn : HObj : MQMD : MQGMO :  BufferLength : %ADDR(Buffer) :
             MessageLength : CompletionCode : Reason);

       // report reason, if any
       If Reason <> RCNONE;
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


       If Reason = RCNONE;
         // display each message received
         Except Text;

         PropertyType = TYPSTR;

         MQIMPO.IPOPT = IPINQN + // iterate over properties
                        IPCTYP + // convert type if necessary
                        IPCVAL;  // convert value into native CCSID

         PropName.VCHRP = %ADDR(PropQuery : *DATA);
         PropName.VCHRL = %LEN(PropQuery);

         MQIMPO.IPRETNAMCHRP = %ADDR(PropertyName : *DATA);
         MQIMPO.IPRETNAMVSBS = PropNameMax;

         DoW CompletionCode <> CCFAIL;

           %LEN(PropertyName) = PropNameMax;
           %LEN(PropertyValue) = PropValueMax;

           MQINQMP(HConn : HMsg : MQIMPO : PropName : MQPD : PropertyType :
                   PropValueMax : %ADDR(PropertyValue : *DATA) :
                   ActPropLength : CompletionCode : Reason);

            Select;
             When Reason = RCNONE;
               %LEN(PropertyValue) = ActPropLength;
               %LEN(PropertyName) = MQIMPO.IPRETNAMCHRL;

               Message = 'property name <' + PropertyName + '> value <' +
                         PropertyValue + '>';
               Except DspMessage;

             When Reason = RC2471;
               Message = 'no more properties';
               Except DspMessage;

             Other;
               MQCall = 'MQINQMP';
               ReasonCode = %CHAR(Reason);
               Except MQFailure;

               If ActPropLength > %LEN(PropertyValue);
                 Message = 'Length must be >= ' + %CHAR(ActPropLength);
                 Except DspMessage;
               EndIf;
           EndSl;
         EndDo;
       EndIf;

       //***************************************************************
       //                                                              *
       //  Delete the message handle                                   *
       //                                                              *
       //***************************************************************
       MQDLTMH(HConn : HMsg : MQDMHO : CompletionCode : Reason);

       // report reason, if any; stop if failed
       If Reason <> RCNONE;
         // "MQDLTMH ended with reason code ...."
         MQCall = 'MQDLTMH';
         ReasonCode = %CHAR(Reason);
         Except MQFailure;
       EndIf;

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

       // "Sample AMQ3IQM4 end"
       Status = 'end';
       Except Sample;

       *INLR = *ON;
      /End-Free
      //***************************************************************
     OQSYSPRT   E            Sample         1
     O                                              'Sample AMQ3IQM4 '
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
      // END OF AMQ3IQM4                                              *
      //                                                              *
      //***************************************************************
