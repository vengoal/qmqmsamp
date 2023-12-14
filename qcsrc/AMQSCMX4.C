/* %Z% %W% %I% %E% %U% */
 /********************************************************************/
 /*                                                                  */
 /* Program name: AMQSCMX4                                           */
 /*                                                                  */
 /* Description: Sample C program for a channel message exit         */
 /*                                                                  */
 /*   <copyright                                                     */
 /*   notice="lm-source-program"                                     */
 /*   pids="5724-H72,"                                               */
 /*   years="1993,2016"                                              */
 /*   crc="3429011576" >                                             */
 /*   Licensed Materials - Property of IBM                           */
 /*                                                                  */
 /*   5724-H72,                                                      */
 /*                                                                  */
 /*   (C) Copyright IBM Corp. 1993, 2016 All Rights Reserved.        */
 /*                                                                  */
 /*   US Government Users Restricted Rights - Use, duplication or    */
 /*   disclosure restricted by GSA ADP Schedule Contract with        */
 /*   IBM Corp.                                                      */
 /*   </copyright>                                                   */
 /*                                                                  */
 /********************************************************************/
 /*                                                                  */
 /* Function:                                                        */
 /*                                                                  */
 /*   AMQSCMX4 is a sample C program for a channel message exit,     */
 /*   for use with IBM MQ for IBM i. When installed as the           */
 /*   message exit for either the sender or receiver channels        */
 /*   (or both), it prints the MsgId (in hex and character form),    */
 /*   and the date and time for each message.                        */
 /*                                                                  */
 /*   The output is logged in the file which is named in the         */
 /*   message exit user data field (MSGUSRDATA) on the channel       */
 /*   definition.                                                    */
 /*                                                                  */
 /*   If MSGUSRDATA is left blank, the log records will be written   */
 /*   by default to QGPL/AMQCHLLOG.                                  */
 /*                                                                  */
 /*   If the file name in the user data is invalid, a message will   */
 /*   be sent to the QSYSOPR message queue, and the channel will not */
 /*   start.                                                         */
 /*                                                                  */
 /*   The file name in the user data MUST be qualified with a        */
 /*   library name, or it will be created in QTEMP for the           */
 /*   MQ channel job, and output will be lost when the channel ends. */
 /*                                                                  */
 /********************************************************************/
 /*                                                                  */
 /* Compilation guidance:                                            */
 /*                                                                  */
 /*   You must ensure that AMQSCMX4 is compiled in accordance with   */
 /*   the defined procedures in the IBM MQ Intercommunication        */
 /*   publication (SC33-1872) for iSeries channel exits:             */
 /*                                                                  */
 /*     -- The program must be made threadsafe. The program must be  */
 /*        bound to the threaded library QMQM/LIBMQM_R.              */
 /*                                                                  */
 /*     -- The program must be associated with the *CALLER           */
 /*        activation group.                                         */
 /*                                                                  */
 /*     -- The program must be enabled to work with teraspace        */
 /*        storage.                                                  */
 /*                                                                  */
 /*   AMQSCMX4 takes the parameters defined for MQCHANNELEXIT        */
 /*   in the IBM MQ Distributed Queue Management Guide               */
 /*   (SC33-1139)                                                    */
 /*                                                                  */
 /********************************************************************/
 /*                                                                  */
 /* Example usage instructions for IBM i:                            */
 /*                                                                  */
 /*   1. Stop the IBM MQ channel which requires the message          */
 /*      exit.                                                       */
 /*                                                                  */
 /*   2. Use IBM MQ command CHGMQMCHL to modify the message          */
 /*      exit attributes:                                            */
 /*                                                                  */
 /*         MSGEXIT     = QMQM/AMQSCMX4                              */
 /*         MSGUSRDATA  = 'QMQMSAMP/CHLLOG'                          */
 /*                                                                  */
 /*   3. Start and stop the channel. A file named CHLLOG should be   */
 /*      created in library QMQMSAMP. This file should contain at    */
 /*      least two message data entries.                             */
 /*                                                                  */
 /********************************************************************/
     /* includes */
 #include <cmqc.h>              /* For MQI datatypes                 */
 #include <cmqxc.h>             /* For MQI exit-related definitions  */
 #include <stdio.h>
 #include <string.h>
 #include <stdlib.h>
 #include <time.h>
 #include <ctype.h>

 int  main  (int argc, char * argv[])
 {
   PMQCXP   ChannelParms;
   PMQCD    ChannelDef  ;
   MQLONG   DataLen      ;
   MQLONG   AgentBuffLen ;
   PMQBYTE  pAgentBuff   ;
   MQLONG   ExitBuffLen  ;
   MQPTR    ExitBuffAddr ;
   time_t   curtime;
   FILE     *outfile;
   char     filename[MQ_EXIT_DATA_LENGTH];
   int      i,
            count;

   if (argc != 8)
   {
     fprintf( stdout,
              "\nChannel exit- ERROR wrong number of parameters\n");
   }

   ChannelParms  = (PMQCXP)  argv[1];
   ChannelDef    = (PMQCD)   argv[2];
   DataLen       = (MQLONG) *argv[3];
   AgentBuffLen  = (MQLONG) *argv[4];
   pAgentBuff    = (PMQBYTE) argv[5];
   ExitBuffLen   = (MQLONG) *argv[6];
   ExitBuffAddr  = (MQPTR)  *argv[7];

    /******************************************************/
    /* Work out which file name to use (and strip spaces) */
    /******************************************************/
    for (i=0;                             /* Strip leading spaces */
         i < MQ_EXIT_DATA_LENGTH &&
         ChannelDef->MsgUserData &&
         isspace(*(ChannelDef->MsgUserData+i));
         i++);
    for ( count=0;                        /* Copy file name       */
          i < MQ_EXIT_DATA_LENGTH &&
          ChannelDef->MsgUserData &&
          ! isspace(*(ChannelDef->MsgUserData + i));
          filename[count++] = *(ChannelDef->MsgUserData + i++) );
    if (count == 0)
    {
      strcpy ( filename, "QGPL/AMQCHLLOG");
    }
    else
    {
      filename[count] = '\0';
    }
    /****************************************************/
    /* switch on ExitReason to see why exit was invoked */
    /****************************************************/
    switch(ChannelParms->ExitReason)
    {

      /******************************/
      /* Initialization             */
      /******************************/
      case MQXR_INIT:
      {
         /***************************************************/
         /* Open the log file                               */
         /***************************************************/
         outfile = fopen( filename, "ab+, lrecl=132" );
         if (outfile == (FILE*)0)
         {
           char errmsg[200];
           sprintf(errmsg,"sndmsg msg(\'Channel exit: %10.10s failed"
                          " to open file: %s\') tousr(QSYSOPR)",
                           ChannelDef->MsgExit,
                           ChannelDef->MsgUserData);
           system(errmsg);
           ChannelParms->ExitResponse = MQXCC_CLOSE_CHANNEL;
           return(0);
         }
         fprintf( outfile, "%10.10s ** Initialisation call **  "
                           "Channel type %d.  Channel name: %s",
                  ChannelDef->MsgExit, ChannelDef->ChannelType,
                  ChannelDef->ChannelName);
         fclose(outfile);
         break;
      }

      /****************************/
      /* Process a message        */
      /****************************/
      case MQXR_MSG:
      {
        char hexbuffer[60] ,
             textbuffer[30],
             tmpchar;

        memset( hexbuffer, '\0', sizeof(hexbuffer));
        memset( textbuffer, '\0', sizeof(textbuffer));

        for ( count = 0, i=0; i < MQ_MSG_ID_LENGTH ; i++)
        {
          tmpchar = *(((PMQXQH)pAgentBuff)->MsgDesc.MsgId + i);
          sprintf( (hexbuffer + count), "%02X", tmpchar);
          sprintf( (textbuffer + i), "%c",
                   isprint(tmpchar) ? tmpchar : '.');
          count += 2;
          if ( ! ((i+1) % 4) )
          {
            *(hexbuffer + count) = ' ';
            count +=1;
          }
        }

        /* Print message Id and timestamp */
        outfile = fopen( filename, "ab+, lrecl=132" );
        curtime = time(NULL);
        fprintf( outfile, "  MsgId: \"%24.24s\"< %s > %s",
                 textbuffer, hexbuffer,  ctime( &curtime ));
        fclose(outfile);
        break;
      }

      /****************************/
      /* Termination              */
      /****************************/
      case MQXR_TERM:
      {
         outfile = fopen( filename, "ab+, lrecl=132" );
         fprintf( outfile, "%10.10s ** Termination call **  "
                  "Channel type %d.  Channel name: %s",
                  ChannelDef->MsgExit, ChannelDef->ChannelType,
                  ChannelDef->ChannelName);
         fclose(outfile);
         break;
      }

      /**************************************/
      /* Default - unrecognised reason code */
      /**************************************/
      default:
      {
         outfile = fopen( filename, "ab+, lrecl=132" );
         fprintf( outfile, "%10.10s ** Reason code error ** "
                  "Check the channel definition.  This exit should "
                  "only be named in the MSGEXIT fields.",
                  ChannelDef->MsgExit);
         fclose(outfile);
         break;
      }

    }

    ChannelParms->ExitResponse = MQXCC_OK;
  }

 /********************************************************************/
 /*                                                                  */
 /* END OF AMQSCMX4                                                  */
 /*                                                                  */
 /********************************************************************/
