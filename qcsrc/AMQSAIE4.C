/* %Z% %W% %I% %E% %U% */
/******************************************************************************/
/*                                                                            */
/* Program name: AMQSAIE4.C                                                   */
/*                                                                            */
/* Description:  Sample C program to demonstrate a basic event monitor        */
/*               using the MQ Admininstration Interface (MQAI).               */
/*   <copyright                                                               */
/*   notice="lm-source-program"                                               */
/*   pids="5724-H72,"                                                         */
/*   years="1999,2016"                                                        */
/*   crc="289459092" >                                                        */
/*   Licensed Materials - Property of IBM                                     */
/*                                                                            */
/*   5724-H72,                                                                */
/*                                                                            */
/*   (C) Copyright IBM Corp. 1999, 2016 All Rights Reserved.                  */
/*                                                                            */
/*   US Government Users Restricted Rights - Use, duplication or              */
/*   disclosure restricted by GSA ADP Schedule Contract with                  */
/*   IBM Corp.                                                                */
/*   </copyright>                                                             */
/******************************************************************************/
/*                                                                            */
/* Function:                                                                  */
/*    AMQSAIE4 is a sample C program that demonstrates how to write a simple  */
/*    event monitor using the mqGetBag call and other MQAI calls.             */
/*                                                                            */
/*    The name of the event queue to be monitored is passed as a parameter    */
/*    to the program. This would usually be one of the system event queues:-  */
/*            SYSTEM.ADMIN.QMGR.EVENT        Queue Manager events             */
/*            SYSTEM.ADMIN.PERFM.EVENT       Performance events               */
/*            SYSTEM.ADMIN.CHANNEL.EVENT     Channel events                   */
/*            SYSTEM.ADMIN.LOGGER.EVENT      Logger events                    */
/*                                                                            */
/*    To monitor the queue manager event queue or the performance event queue,*/
/*    the attributes of the queue manager needs to be changed to enable       */
/*    these events. For more information about this, see Part 1 of the        */
/*    Programmable System Management book. The queue manager attributes can   */
/*    be changed using either MQSC commands or the MQAI interface.            */
/*    Channel events are enabled by default.                                  */
/*                                                                            */
/* Program logic                                                              */
/*    Connect to the Queue Manager.                                           */
/*    Open the requested event queue with a wait interval of 30 seconds.      */
/*    Wait for a message, and when it arrives get the message from the queue  */
/*    and format it into an MQAI bag using the mqGetBag call.                 */
/*    There are many types of event messages and it is beyond the scope of    */
/*    this sample to program for all event messages. Instead the program      */
/*    prints out the contents of the formatted bag.                           */
/*    Loop around to wait for another message until either there is an error  */
/*    or the wait interval of 30 seconds is reached.                          */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* AMQSAIE4 has 2 parameters - the name of the event queue to be monitored    */
/*                           - the queue manager name (optional)              */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/* Includes                                                                   */
/******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <cmqc.h>                         /* MQI                              */
#include <cmqcfc.h>                       /* PCF                              */
#include <cmqbc.h>                        /* MQAI                             */

/******************************************************************************/
/* Macros                                                                     */
/******************************************************************************/
#define Int64 "ll"

/******************************************************************************/
/* Function prototypes                                                        */
/******************************************************************************/
void CheckCallResult(MQCHAR *, MQLONG , MQLONG);
void GetQEvents(MQHCONN, MQCHAR *);
int PrintBag(MQHBAG);
int PrintBagContents(MQHBAG, int);

/******************************************************************************/
/* Function: main                                                             */
/******************************************************************************/
int main(int argc, char *argv[])
{
   MQHCONN hConn;                          /* handle to connection            */
   MQCHAR QMName[MQ_Q_MGR_NAME_LENGTH+1]=""; /* default QM name               */
   MQLONG reason;                          /* reason code                     */
   MQLONG connReason;                      /* MQCONN reason code              */
   MQLONG compCode;                        /* completion code                 */

   /***************************************************************************/
   /* First check the required parameters                                     */
   /***************************************************************************/
   printf("Sample Event Monitor (times out after 30 secs)\n");
   if (argc < 2)
   {
     printf("Required parameter missing - event queue to be monitored\n");
     exit(99);
   }

   /**************************************************************************/
   /* Connect to the queue manager                                           */
   /**************************************************************************/
   if (argc > 2)
     strncpy(QMName, argv[2], (size_t)MQ_Q_MGR_NAME_LENGTH);
   MQCONN(QMName, &hConn, &compCode, &connReason);

   /***************************************************************************/
   /* Report the reason and stop if the connection failed                     */
   /***************************************************************************/
   if (compCode == MQCC_FAILED)
   {
      CheckCallResult("MQCONN", compCode, connReason);
      exit( (int)connReason);
   }

   /***************************************************************************/
   /* Call the routine to open the event queue and format any event messages  */
   /* read from the queue.                                                    */
   /***************************************************************************/
   GetQEvents(hConn, argv[1]);

   /***************************************************************************/
   /* Disconnect from the queue manager if not already connected              */
   /***************************************************************************/
   if (connReason != MQRC_ALREADY_CONNECTED)
   {
      MQDISC(&hConn, &compCode, &reason);
      CheckCallResult("MQDISC", compCode, reason);
   }

   return 0;

}


/******************************************************************************/
/*                                                                            */
/* Function: CheckCallResult                                                  */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Input Parameters:  Description of call                                     */
/*                    Completion code                                         */
/*                    Reason code                                             */
/*                                                                            */
/* Output Parameters: None                                                    */
/*                                                                            */
/* Logic: Display the description of the call, the completion code and the    */
/*        reason code if the completion code is not successful                */
/*                                                                            */
/******************************************************************************/
void  CheckCallResult(char *callText, MQLONG cc, MQLONG rc)
{
   if (cc != MQCC_OK)
         printf("%s failed: Completion Code = %d : Reason = %d\n", callText, cc, rc);

}


/******************************************************************************/
/*                                                                            */
/* Function: GetQEvents                                                       */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Input Parameters:  Handle to the queue manager                             */
/*                    Name of the event queue to be monitored                 */
/*                                                                            */
/* Output Parameters: None                                                    */
/*                                                                            */
/* Logic:   Open the event queue.                                             */
/*          Get a message off the event queue and format the message into     */
/*          a bag.                                                            */
/*          A real event monitor would need to be programmed to deal with     */
/*          each type of event that it receives from the queue. This is       */
/*          outside the scope of this sample, so instead, the contents of     */
/*          the bag are printed.                                              */
/*          The program waits for 30 seconds for an event message and then    */
/*          terminates if no more messages are available.                     */
/*                                                                            */
/******************************************************************************/
void GetQEvents(MQHCONN hConn, MQCHAR *qName)
{
   MQLONG openReason;                       /* MQOPEN reason code             */
   MQLONG reason;                           /* reason code                    */
   MQLONG compCode;                         /* completion code                */
   MQHOBJ eventQueue;                       /* handle to event queue          */
   MQHBAG eventBag = MQHB_UNUSABLE_HBAG;    /* event bag to receive event msg */
   MQOD   od = {MQOD_DEFAULT};              /* Object Descriptor              */
   MQMD   md = {MQMD_DEFAULT};              /* Message Descriptor             */
   MQGMO  gmo = {MQGMO_DEFAULT};            /* get message options            */
   MQLONG bQueueOK = 1;                     /* keep reading msgs while true   */

   /***************************************************************************/
   /* Create an Event Bag in which to receive the event.                      */
   /* Exit the function if the create fails.                                  */
   /***************************************************************************/
   mqCreateBag(MQCBO_USER_BAG, &eventBag, &compCode, &reason);
   CheckCallResult("Create event bag", compCode, reason);
   if (compCode !=MQCC_OK)
      return;

   /***************************************************************************/
   /* Open the event queue chosen by the user                                 */
   /***************************************************************************/
   strncpy(od.ObjectName, qName, (size_t)MQ_Q_NAME_LENGTH);
   MQOPEN(hConn, &od, MQOO_INPUT_AS_Q_DEF+MQOO_FAIL_IF_QUIESCING, &eventQueue,
          &compCode, &openReason);
   CheckCallResult("Open event queue", compCode, openReason);

   /***************************************************************************/
   /* Set the GMO options to control the action of the get message from the   */
   /* queue.                                                                  */
   /***************************************************************************/
   gmo.WaitInterval = 30000;               /* 30 second wait for message      */
   gmo.Options = MQGMO_WAIT
                 | MQGMO_FAIL_IF_QUIESCING
                 | MQGMO_NO_SYNCPOINT
                 | MQGMO_CONVERT;
   gmo.Version = MQGMO_VERSION_2;          /* Avoid need to reset Message ID  */
   gmo.MatchOptions = MQMO_NONE;           /* and Correlation ID after every  */
                                           /* mqGetBag                        */

   /***************************************************************************/
   /* If open fails, we cannot access the queue and must stop the monitor.    */
   /***************************************************************************/
   if (compCode != MQCC_OK)
      bQueueOK = 0;

   /***************************************************************************/
   /* Main loop to get an event message when it arrives                       */
   /***************************************************************************/
   while (bQueueOK)
   {
     printf("\nWaiting for an event\n");

     /*************************************************************************/
     /* Get the message from the event queue and convert it into the event    */
     /* bag.                                                                  */
     /*************************************************************************/
     mqGetBag(hConn, eventQueue, &md, &gmo, eventBag, &compCode, &reason);

     /*************************************************************************/
     /* If get fails, we cannot access the queue and must stop the monitor.   */
     /*************************************************************************/
     if (compCode != MQCC_OK)
     {
        bQueueOK = 0;
        /*********************************************************************/
        /* If get fails because no message available then we have timed out, */
        /* so report this, otherwise report an error.                        */
        /*********************************************************************/
        if (reason == MQRC_NO_MSG_AVAILABLE)
        {
           printf("No more messages\n");
        }
        else
        {
           CheckCallResult("Get bag", compCode, reason);
        }
     }

     /*************************************************************************/
     /* Event message read - Print the contents of the event bag              */
     /*************************************************************************/
     else
     {
       if ( PrintBag(eventBag) )
           printf("\nError found while printing bag contents\n");

     }  /* end of msg found */
   } /* end of main loop */

   /***************************************************************************/
   /* Close the event queue if successfully opened                            */
   /***************************************************************************/
   if (openReason == MQRC_NONE)
   {
      MQCLOSE(hConn, &eventQueue, MQCO_NONE, &compCode, &reason);
      CheckCallResult("Close event queue", compCode, reason);
   }

   /***************************************************************************/
   /* Delete the event bag if successfully created.                           */
   /***************************************************************************/
   if (eventBag != MQHB_UNUSABLE_HBAG)
   {
      mqDeleteBag(&eventBag, &compCode, &reason);
      CheckCallResult("Delete the event bag", compCode, reason);
   }

} /* end of GetQEvents */

/******************************************************************************/
/*                                                                            */
/* Function: PrintBag                                                         */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Input Parameters:  Bag Handle                                              */
/*                                                                            */
/* Output Parameters: None                                                    */
/*                                                                            */
/* Returns:           Number of errors found                                  */
/*                                                                            */
/* Logic: Calls PrintBagContents to display the contents of the bag.          */
/*                                                                            */
/******************************************************************************/
int PrintBag(MQHBAG dataBag)
{
    int errors;

    printf("\n");
    errors = PrintBagContents(dataBag, 0);
    printf("\n");

    return errors;
}

/******************************************************************************/
/*                                                                            */
/* Function: PrintBagContents                                                 */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Input Parameters:  Bag Handle                                              */
/*                    Indentation level of bag                                */
/*                                                                            */
/* Output Parameters: None                                                    */
/*                                                                            */
/* Returns:           Number of errors found                                  */
/*                                                                            */
/* Logic: Count the number of items in the bag                                */
/*        Obtain selector and item type for each item in the bag.             */
/*        Obtain the value of the item depending on item type and display the */
/*        index of the item, the selector and the value.                      */
/*        If the item is an embedded bag handle then call this function again */
/*        to print the contents of the embedded bag increasing the            */
/*        indentation level.                                                  */
/*                                                                            */
/******************************************************************************/
int PrintBagContents(MQHBAG dataBag, int indent)
{
   /***************************************************************************/
   /* Definitions                                                             */
   /***************************************************************************/
   #define LENGTH 500                       /* Max length of string to be read*/
   #define INDENT 4                         /* Number of spaces to indent     */
                                            /* embedded bag display           */

   /***************************************************************************/
   /* Variables                                                               */
   /***************************************************************************/
   MQLONG  itemCount;                       /* Number of items in the bag     */
   MQLONG  itemType;                        /* Type of the item               */
   int     i;                               /* Index of item in the bag       */
   MQCHAR  stringVal[LENGTH+1];             /* Value if item is a string      */
   MQBYTE  byteStringVal[LENGTH];           /* Value if item is a byte string */
   MQLONG  stringLength;                    /* Length of string value         */
   MQLONG  ccsid;                           /* CCSID of string value          */
   MQINT32 iValue;                          /* Value if item is an integer    */
   MQINT64 i64Value;                        /* Value if item is a 64-bit      */
                                            /* integer                        */
   MQLONG  selector;                        /* Selector of item               */
   MQHBAG  bagHandle;                       /* Value if item is a bag handle  */
   MQLONG  reason;                          /* reason code                    */
   MQLONG  compCode;                        /* completion code                */
   MQLONG  trimLength;                      /* Length of string to be trimmed */
   int     errors = 0;                      /* Count of errors found          */
   char    blanks[] = "                       "; /* Blank string used to      */
                                                 /* indent display            */

   /***************************************************************************/
   /* Count the number of items in the bag                                    */
   /***************************************************************************/
   mqCountItems(dataBag, MQSEL_ALL_SELECTORS, &itemCount, &compCode, &reason);

   if (compCode != MQCC_OK)
      errors++;
   else
   {
      printf("%.*sHandle:%d ", indent, blanks, dataBag);
      printf("%.*sSize:%d\n", indent, blanks, itemCount);
      printf("%.*sIndex: Selector: Value:\n", indent, blanks);
   }

   /***************************************************************************/
   /* If no errors found, display each item in the bag                        */
   /***************************************************************************/
   if (!errors)
   {
      for (i = 0; i < itemCount; i++)
      {
          /********************************************************************/
          /* First inquire the type of the item for each item in the bag      */
          /********************************************************************/
          mqInquireItemInfo(dataBag,             /* Bag handle                */
                            MQSEL_ANY_SELECTOR,  /* Item can have any selector*/
                            i,                   /* Index position in the bag */
                            &selector,           /* Actual value of selector  */
                                                 /* returned by call          */
                            &itemType,           /* Actual type of item       */
                                                 /* returned by call          */
                            &compCode,           /* Completion code           */
                            &reason);            /* Reason Code               */

          if (compCode != MQCC_OK)
             errors++;

          switch(itemType)
          {
          case MQITEM_INTEGER:
               /***************************************************************/
               /* Item is an integer. Find its value and display its index,   */
               /* selector and value.                                         */
               /***************************************************************/
               mqInquireInteger(dataBag,         /* Bag handle                */
                                MQSEL_ANY_SELECTOR, /* Allow any selector     */
                                i,               /* Index position in the bag */
                                &iValue,         /* Returned integer value    */
                                &compCode,       /* Completion code           */
                                &reason);        /* Reason Code               */

               if (compCode != MQCC_OK)
                  errors++;
               else
                  printf("%.*s  %-2d     %-4d     (%d)\n",
                          indent, blanks, i, selector, iValue);
               break;

          case MQITEM_INTEGER64:
               /***************************************************************/
               /* Item is a 64-bit integer. Find its value and display its    */
               /* index, selector and value.                                  */
               /***************************************************************/
               mqInquireInteger64(dataBag,       /* Bag handle                */
                                  MQSEL_ANY_SELECTOR, /* Allow any selector   */
                                  i,             /* Index position in the bag */
                                  &i64Value,     /* Returned integer value    */
                                  &compCode,     /* Completion code           */
                                  &reason);      /* Reason Code               */

               if (compCode != MQCC_OK)
                  errors++;
               else
                  printf("%.*s  %-2d     %-4d     (%"Int64"d)\n",
                          indent, blanks, i, selector, i64Value);
               break;

          case MQITEM_STRING:
               /***************************************************************/
               /* Item is a string. Obtain the string in a buffer, prepare    */
               /* the string for displaying and display the index, selector,  */
               /* string and Character Set ID.                                */
               /***************************************************************/
               mqInquireString(dataBag,          /* Bag handle                */
                               MQSEL_ANY_SELECTOR, /* Allow any selector      */
                               i,                /* Index position in the bag */
                               LENGTH,           /* Maximum length of buffer  */
                               stringVal,        /* Buffer to receive string  */
                               &stringLength,    /* Actual length of string   */
                               &ccsid,           /* Coded character set id    */
                               &compCode,        /* Completion code           */
                               &reason);         /* Reason Code               */

               /***************************************************************/
               /* The call can return a warning if the string is too long for */
               /* the output buffer and has been truncated, so only check     */
               /* explicitly for call failure.                                */
               /***************************************************************/
               if (compCode == MQCC_FAILED)
                   errors++;
               else
               {
                  /************************************************************/
                  /* Remove trailing blanks from the string and terminate with*/
                  /* a null. First check that the string should not have been */
                  /* longer than the maximum buffer size allowed.             */
                  /************************************************************/
                  if (stringLength > LENGTH)
                     trimLength = LENGTH;
                  else
                     trimLength = stringLength;
                  mqTrim(trimLength, stringVal, stringVal, &compCode, &reason);
                  printf("%.*s  %-2d     %-4d     '%s' %d\n",
                          indent, blanks, i, selector, stringVal, ccsid);
               }
               break;

          case MQITEM_BYTE_STRING:
               /***************************************************************/
               /* Item is a byte string. Obtain the byte string in a buffer,  */
               /* prepare the byte string for displaying and display the      */
               /* index, selector and string.                                 */
               /***************************************************************/
               mqInquireByteString(dataBag,      /* Bag handle                */
                                   MQSEL_ANY_SELECTOR, /* Allow any selector  */
                                   i,            /* Index position in the bag */
                                   LENGTH,       /* Maximum length of buffer  */
                                   byteStringVal, /* Buffer to receive string */
                                   &stringLength, /* Actual length of string  */
                                   &compCode,    /* Completion code           */
                                   &reason);     /* Reason Code               */

               /***************************************************************/
               /* The call can return a warning if the string is too long for */
               /* the output buffer and has been truncated, so only check     */
               /* explicitly for call failure.                                */
               /***************************************************************/
               if (compCode == MQCC_FAILED)
                   errors++;
               else
               {
                  printf("%.*s  %-2d     %-4d     X'",
                         indent, blanks, i, selector);

                  for (i = 0 ; i < stringLength ; i++)
                     printf("%02X", byteStringVal[i]);

                  printf("'\n");
               }
               break;

          case MQITEM_BAG:
               /***************************************************************/
               /* Item is an embedded bag handle, so call the PrintBagContents*/
               /* function again to display the contents.                     */
               /***************************************************************/
               mqInquireBag(dataBag,             /* Bag handle                */
                            MQSEL_ANY_SELECTOR,  /* Allow any selector        */
                            i,                   /* Index position in the bag */
                            &bagHandle,          /* Returned embedded bag hdle*/
                            &compCode,           /* Completion code           */
                            &reason);            /* Reason Code               */

               if (compCode != MQCC_OK)
                  errors++;
               else
               {
                  printf("%.*s  %-2d     %-4d     (%d)\n", indent, blanks, i,
                          selector, bagHandle);
                  if (selector == MQHA_BAG_HANDLE)
                     printf("%.*sSystem Bag:\n", indent+INDENT, blanks);
                  else
                     printf("%.*sGroup Bag:\n", indent+INDENT, blanks);
                  PrintBagContents(bagHandle, indent+INDENT);
               }
               break;

          default:
               printf("%.*sUnknown item type", indent, blanks);
          }
      }
   }
   return errors;
}

