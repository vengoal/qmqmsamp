/* %Z% %W% %I% %E% %U% */
 /********************************************************************/
 /*                                                                  */
 /* Program name: AMQSSSLC                                           */
 /*                                                                  */
 /* Description: Sample C program that demonstrates how to specify   */
 /*              SSL/TLS connection information on MQCONNX.          */
 /*   <copyright                                                     */
 /*   notice="lm-source-program"                                     */
 /*   pids="5724-H72"                                                */
 /*   years="1994,2016"                                              */
 /*   crc="3231263071" >                                             */
 /*   Licensed Materials - Property of IBM                           */
 /*                                                                  */
 /*   5724-H72                                                       */
 /*                                                                  */
 /*   (C) Copyright IBM Corp. 1994, 2016 All Rights Reserved.        */
 /*                                                                  */
 /*   US Government Users Restricted Rights - Use, duplication or    */
 /*   disclosure restricted by GSA ADP Schedule Contract with        */
 /*   IBM Corp.                                                      */
 /*   </copyright>                                                   */
 /********************************************************************/
 /*                                                                  */
 /* Function:                                                        */
 /*                                                                  */
 /*   AMQSSSLC is a sample C program that demonstrates how to use    */
 /*   the MQCNO and MQSCO structures to supply SSL/TLS client        */
 /*   connection information on the MQCONNX call. This enables a     */
 /*   client MQI application to provide the definition of its client */
 /*   connection channel and SSL/TLS settings at run-time without a  */
 /*   client channel definition table.                               */
 /*                                                                  */
 /*   If a connection name is supplied, the program constructs a     */
 /*   client connection channel definition in an MQCD structure.     */
 /*                                                                  */
 /*   If the stem name of the key repository file is supplied,       */
 /*   the program constructs an MQSCO structure;                     */
 /*   if an OCSP responder URL is also supplied, the                 */
 /*   program constructs an authentication information record MQAIR  */
 /*   structure.                                                     */
 /*                                                                  */
 /*   The program then connects to the queue manager using MQCONNX.  */
 /*   It inquires and prints out the name of the queue manager to    */
 /*   which it connected.                                            */
 /*                                                                  */
 /*   This program is intended to be linked as an MQI client         */
 /*   application. However, it may be linked as a regular MQI        */
 /*   application. Then, it simply connects to a local queue         */
 /*   manager and ignores the client connection information.         */
 /*                                                                  */
 /*                                                                  */
 /********************************************************************/
 /*                                                                  */
 /*   AMQSSSLC accepts the following parameters, all of which are    */
 /*   optional:                                                      */
 /*                                                                  */
 /*      -m QmgrName        Name of the queue manager to connect to  */
 /*                                                                  */
 /*      -c ChannelName     Name of the channel to use               */
 /*                                                                  */
 /*      -x ConnName        Server connection name                   */
 /*                                                                  */
 /*   SSL/TLS parameters:                                            */
 /*                                                                  */
 /*      -k KeyReposStem    The stem name of the key repository      */
 /*                         file. This is the full path to the file  */
 /*                         without the .kdb suffix. For example:    */
 /*                           /home/user/client                      */
 /*                           C:\User\client                         */
 /*                                                                  */
 /*      -s CipherSpec      The SSL/TLS channel CipherSpec string    */
 /*                         corresponding to the SSLCIPH on the      */
 /*                         SVRCONN channel definition on the queue  */
 /*                         manager.                                 */
 /*                                                                  */
 /*      -l CertLabel       The certificate label to use for the     */
 /*                         secure connection. Must be lowercase.    */
 /*                                                                  */
 /*                                                                  */
 /*      -f                 Specifies that only FIPS 140-2 certified */
 /*                         algorithms must be used.                 */
 /*                                                                  */
 /*      -b                 Specifies that only Suite B compliant    */
 /*                         algorithms must be used. This is a comma */
 /*                         separated list of values:                */
 /*                                                                  */
 /*         NONE              Equivalent to MQ_SUITE_B_NONE          */
 /*         128_BIT           Equivalent to MQ_SUITE_B_128_BIT       */
 /*         192_BIT           Equivalent to MQ_SUITE_B_192_BIT       */
 /*                                                                  */
 /*      -p                 Specifies the certificate validation     */
 /*                         policy to use. This must be one of:      */
 /*         ANY               MQ_CERT_VAL_POLICY_ANY                 */
 /*         RFC5280           MQ_CERT_VAL_POLICY_RFC5280             */
 /*                                                                  */
 /*                                                                  */
 /*   OCSP certificate revocation parameters:                        */
 /*                                                                  */
 /*      -o URL             The OCSP Responder URL                   */
 /*                                                                  */
 /********************************************************************/
 #define USAGE  "Parameters:"                                         \
                "  [-m QMgr]\n"                                       \
                "  [-c ChlName -x ConnName]\n"                        \
                "  [-k KeyReposStem] [-s CipherSpec] [-l CertLabel]\n"\
                "  [-p any|rfc5280]\n"                                \
                "  [-f] [-b none|128_bit|192_bit[,...] ]\n"           \
                "  [-o OcspURL]\n"

 #include <stdio.h>
 #include <stdlib.h>
 #include <ctype.h>
 #include <string.h>

                                    /* includes for MQ               */
 #include <cmqc.h>                  /* For regular MQI definitions   */
 #include <cmqxc.h>                 /* For MQCD definition           */

#if (MQAT_DEFAULT != MQAT_WINDOWS_NT) && (MQAT_DEFAULT != MQAT_MVS)
  #include <strings.h>
  #ifndef stricmp
    #define stricmp strcasecmp
  #endif
 #endif

 #define OK 0
 #define FAIL 1

 typedef struct
 {
   char     * Alias;
   char     * String;
   MQLONG     Value;
 } IntListEntry;

 IntListEntry CertValPolicyTable[] = {
   {"any",      "MQ_CERT_VAL_POLICY_ANY",      MQ_CERT_VAL_POLICY_ANY},
   {"rfc5280",  "MQ_CERT_VAL_POLICY_RFC5280",  MQ_CERT_VAL_POLICY_RFC5280},
   {NULL,       NULL,                  -99}
};

 IntListEntry SuiteBTable[] = {
   {"none",     "MQ_SUITE_B_NONE",     MQ_SUITE_B_NONE},
   {"128_bit",  "MQ_SUITE_B_128_BIT",  MQ_SUITE_B_128_BIT},
   {"192_bit",  "MQ_SUITE_B_192_BIT",  MQ_SUITE_B_192_BIT},
   {NULL,       NULL,                  -99}
 };

 /* function prototypes of local functions */
 static int ProcessCommandLine(int argc,
                               char   **argv,
                               char   **pQMgrName,
                               char   **pChannelName,
                               char   **pConnName,
                               char   **pKeyReposStem,
                               char   **pCipherSpec,
                               char   **pCertificateLabel,
                               MQLONG  *pCertificateValPolicy,
                               MQLONG  *pFipsRequired,
                               MQLONG  *pSuiteB,
                               char   **pOcspUrl);

 static int GetIntListArgument(int               argc,
                               char            **argv,
                               int              *pThisArg,
                               IntListEntry     *pTable,
                               MQLONG           *pIntList,
                               int               MaxCount);

 static int GetStringArgument(int    argc,
                              char **argv,
                              int   *pThisArg,
                              char **pString);

#if (MQAT_DEFAULT == MQAT_OS400)
 #undef  strdup
 #define strdup mqstrdup

 char * mqstrdup(char *pString)
 {
   char     *pCopy;
   size_t    length;

   length = strlen(pString);

   pCopy = malloc(length + 1);
   if (pCopy == NULL)
   {
     return pCopy;
   }

   strcpy(pCopy, pString);
   return pCopy;
 }
#endif

 int main(int argc, char **argv)
 {

   /*   Declare MQI structures needed to establish a connection      */
   MQCNO    ConnectOptions = {MQCNO_DEFAULT};
                                    /* MQCONNX options               */
   MQCD     ClientConn     = {MQCD_CLIENT_CONN_DEFAULT};
                                    /* Client connection channel     */
                                    /* definition                    */
   MQSCO    SslConnOptions = {MQSCO_DEFAULT};
                                    /* SSL connection options        */
   MQAIR    AuthInfoRec    = {MQAIR_DEFAULT};
                                    /* SSL authentication info rec   */
      /** note, sample uses defaults where it can **/

   /*   Command line parameters from the user                        */
   char    *pQMgrName         = NULL;  /* queue manager name         */
   char    *pChannelName      = NULL;  /* channel name               */
   char    *pConnName         = NULL;  /* connection name            */
   char    *pKeyReposStem     = NULL;  /* SSL key-rep stem name      */
   char    *pCipherSpec       = NULL;  /* SSL CipherSpec string      */
   char    *pCertificateLabel = NULL;  /* Certificate label          */
   char    *pOcspUrl          = NULL;  /* SSL OCSP responder URL     */
   MQLONG   CertificateValPolicy = -1; /* Cert-validation policy     */
   MQLONG   FipsRequired         = 0;  /* FIPS required              */
   MQLONG   SuiteB[MQ_SUITE_B_SIZE] = { 0,0,0,0 }; /* Suite B        */

   /*   Values used for MQI calls once the client has connected      */
   MQCHAR   QMName[MQ_Q_MGR_NAME_LENGTH]; /* name of connection qmgr */
   MQHCONN  Hcon;                   /* connection handle             */
   MQOD     od = { MQOD_DEFAULT };  /* object descriptor             */
   MQHOBJ   Hobj;                   /* object handle                 */
   MQLONG   Selector;               /* selector for inquiry          */
   MQLONG   CompCode;               /* completion code               */
   MQLONG   OpenCode;               /* MQOPEN completion code        */
   MQLONG   Reason;                 /* reason code                   */
   MQLONG   CReason;                /* reason code for MQCONNX       */

   printf("Sample AMQSSSLC start\n");

   if (ProcessCommandLine(argc,
                          argv,
                          &pQMgrName,
                          &pChannelName,
                          &pConnName,
                          &pKeyReposStem,
                          &pCipherSpec,
                          &pCertificateLabel,
                          &CertificateValPolicy,
                          &FipsRequired,
                          &SuiteB[0],
                          &pOcspUrl) != OK)
   {
     printf(USAGE);
     exit(99);
   }

   if (pQMgrName != NULL)
   {
     strncpy(QMName, pQMgrName, MQ_Q_MGR_NAME_LENGTH);
     printf("Connecting to queue manager %-48.48s\n", pQMgrName);
   }
   else
   {
     QMName[0] = '\0';    /* default */
     printf("Connecting to the default queue manager\n");
   }

   /******************************************************************/
   /*                                                                */
   /*   Initialise the client channel definition if required         */
   /*                                                                */
   /******************************************************************/
   if (pConnName != NULL)
   {
     strncpy(ClientConn.ConnectionName,
             pConnName,
             MQ_CONN_NAME_LENGTH);

     if (pChannelName == NULL)
     {
       pChannelName = "SYSTEM.DEF.SVRCONN";
     }

     strncpy(ClientConn.ChannelName,
             pChannelName,
             MQ_CHANNEL_NAME_LENGTH);

     /* Point the MQCNO to the client connection definition */
     ConnectOptions.ClientConnPtr = &ClientConn;

     printf("Using the server connection channel %s\n",
            pChannelName);
     printf("on connection name %s.\n", pConnName);

     if (pCipherSpec != NULL)
     {
       /* SSL requires MQCD version 7 or later */
       ClientConn.Version = MQCD_VERSION_7;

       /* The MQCD must contain the SSL CipherSpec string */
       strncpy(ClientConn.SSLCipherSpec,
               pCipherSpec,
               MQ_SSL_CIPHER_SPEC_LENGTH);

       printf("Using SSL CipherSpec %s\n", pCipherSpec);
     }
   }
   else
   {
     printf("No client connection information specified.\n");
   }

   /******************************************************************/
   /*                                                                */
   /*   Initialise the SSL and certificate revocation settings if    */
   /*   required                                                     */
   /*                                                                */
   /******************************************************************/
   if (pKeyReposStem != NULL)
   {
     /* The MQSCO can contain a key repository location */
     strncpy(SslConnOptions.KeyRepository,
             pKeyReposStem,
             MQ_SSL_KEY_REPOSITORY_LENGTH);

     printf("Using SSL key repository stem %s\n", pKeyReposStem);

     /* Point the MQCNO to the SSL configuration options */
     ConnectOptions.SSLConfigPtr = &SslConnOptions;


     if (FipsRequired)
     {
       printf("FIPS required\n");
       SslConnOptions.FipsRequired = MQSSL_FIPS_YES;

       /* A version 2 MQSCO supports FipsRequired */
       SslConnOptions.Version = MQSCO_VERSION_2;
     }

     if (SuiteB[0])
     {
       SslConnOptions.EncryptionPolicySuiteB[0] = SuiteB[0];
       SslConnOptions.EncryptionPolicySuiteB[1] = SuiteB[1];
       SslConnOptions.EncryptionPolicySuiteB[2] = SuiteB[2];
       SslConnOptions.EncryptionPolicySuiteB[3] = SuiteB[3];

       printf("Suite B Encryption Policy: %d, %d, %d, %d\n",
              SslConnOptions.EncryptionPolicySuiteB[0],
              SslConnOptions.EncryptionPolicySuiteB[1],
              SslConnOptions.EncryptionPolicySuiteB[2],
              SslConnOptions.EncryptionPolicySuiteB[3]);

       /* A version 3 MQSCO supports Suite B encryption policy */
       SslConnOptions.Version = MQSCO_VERSION_3;
     }

     if (CertificateValPolicy >= 0)
     {
       SslConnOptions.CertificateValPolicy = CertificateValPolicy;

       printf("Certificate Validation Policy: %d\n",
              SslConnOptions.CertificateValPolicy);

       /* A version 4 MQSCO supports certificate validation policy */
       SslConnOptions.Version = MQSCO_VERSION_4;
     }

     if (pCertificateLabel != NULL)
     {
       /* The MQSCO can contain a certificate label */
       strncpy(SslConnOptions.CertificateLabel,
           pCertificateLabel, MQ_CERT_LABEL_LENGTH);

       printf("Certificate Label: %s\n", pCertificateLabel);

       /* A version 5 MQSCO supports certificate label */
       SslConnOptions.Version = MQSCO_VERSION_5;
     }

     if (pOcspUrl != NULL)
     {
       /* OCSP requires MQAIR version 2 or later */
       AuthInfoRec.Version = MQAIR_VERSION_2;
       AuthInfoRec.AuthInfoType = MQAIT_OCSP;

       strncpy(AuthInfoRec.OCSPResponderURL,
               pOcspUrl,
               MQ_AUTH_INFO_OCSP_URL_LENGTH);

       /* The MQSCO must point to the MQAIR */
       SslConnOptions.AuthInfoRecCount = 1;
       SslConnOptions.AuthInfoRecPtr = &AuthInfoRec;

       printf("Using OCSP responder URL %s\n", pOcspUrl);
     }
     else
     {
       printf("No OCSP configuration specified.\n");
     }
   }
   else
   {
     printf("No SSL configuration specified.\n");
   }

   /* We must specify MQCNO version 4 to ensure that both the client
      connection pointer and SSL configuration options are used */
   ConnectOptions.Version = MQCNO_VERSION_4;

   /******************************************************************/
   /*                                                                */
   /*   Connect to queue manager                                     */
   /*                                                                */
   /******************************************************************/
   MQCONNX(QMName,                 /* queue manager                  */
           &ConnectOptions,        /* options for connection         */
           &Hcon,                  /* connection handle              */
           &CompCode,              /* completion code                */
           &CReason);              /* reason code                    */

   /* report reason and stop if it failed     */
   if (CompCode == MQCC_FAILED)
   {
     printf("MQCONNX ended with reason code %d\n", CReason);
     exit( (int)CReason );
   }

   /******************************************************************/
   /*                                                                */
   /*   Open the queue manager object to find out its name           */
   /*                                                                */
   /******************************************************************/
   od.ObjectType = MQOT_Q_MGR;       /* open the queue manager object*/
   MQOPEN(Hcon,                      /* connection handle            */
          &od,                       /* object descriptor for queue  */
          MQOO_INQUIRE +             /* open it for inquire          */
            MQOO_FAIL_IF_QUIESCING,  /* but not if MQM stopping      */
          &Hobj,                     /* object handle                */
          &OpenCode,                 /* MQOPEN completion code       */
          &Reason);                  /* reason code                  */

   /* report reason, if any      */
   if (Reason != MQRC_NONE)
   {
     printf("MQOPEN ended with reason code %d\n", Reason);
   }

   if (OpenCode == MQCC_FAILED)
   {
     printf("Unable to open queue manager for inquire\n");
   }

   /******************************************************************/
   /*                                                                */
   /*   Inquire the name of the queue manager                        */
   /*                                                                */
   /******************************************************************/
   if (OpenCode != MQCC_FAILED)
   {
     Selector = MQCA_Q_MGR_NAME;

     MQINQ(Hcon,                     /* connection handle            */
           Hobj,                     /* object handle for q manager  */
           1,                        /* inquire only one selector    */
           &Selector,                /* the selector to inquire      */
           0,                        /* no integer attributes needed */
           NULL,                     /* so no buffer supplied        */
           MQ_Q_MGR_NAME_LENGTH,     /* inquiring a q manager name   */
           QMName,                   /* the buffer for the name      */
           &CompCode,                /* MQINQ completion code        */
           &Reason);                 /* reason code                  */

     if (Reason == MQRC_NONE)
     {
       printf("Connection established to queue manager %-48.48s\n",
              QMName);
     }
     else
     {
       /* report reason, if any */
       printf("MQINQ ended with reason code %d\n", Reason);
     }
   }

   /******************************************************************/
   /*                                                                */
   /*   Close the queue manager object (if it was opened)            */
   /*                                                                */
   /******************************************************************/
   if (OpenCode != MQCC_FAILED)
   {
     MQCLOSE(Hcon,                    /* connection handle           */
             &Hobj,                   /* object handle               */
             MQCO_NONE,               /* no close options            */
             &CompCode,               /* completion code             */
             &Reason);                /* reason code                 */

     /* report reason, if any     */
     if (Reason != MQRC_NONE)
     {
       printf("MQCLOSE ended with reason code %d\n", Reason);
     }
   }

   /******************************************************************/
   /*                                                                */
   /*   Disconnect from MQM if not already connected                 */
   /*     - on OS/400, the calling job may be connected before the   */
   /*       sample was run and then it should not disconnect         */
   /*                                                                */
   /******************************************************************/
   if (CReason != MQRC_ALREADY_CONNECTED)
   {
     MQDISC(&Hcon,                   /* connection handle            */
            &CompCode,               /* completion code              */
            &Reason);                /* reason code                  */

     /* report reason, if any     */
     if (Reason != MQRC_NONE)
     {
       printf("MQDISC ended with reason code %d\n", Reason);
     }
   }

   /******************************************************************/
   /*                                                                */
   /* END OF AMQSSSLC                                                */
   /*                                                                */
   /******************************************************************/
   printf("Sample AMQSSSLC end\n");
   return(0);
 }


 /********************************************************************/
 /* Get the parameters specified on the command line                 */
 /********************************************************************/
 static int ProcessCommandLine(int      argc,
                               char   **argv,
                               char   **pQMgrName,
                               char   **pChannelName,
                               char   **pConnName,
                               char   **pKeyReposStem,
                               char   **pCipherSpec,
                               char   **pCertificateLabel,
                               MQLONG  *pCertificateValPolicy,
                               MQLONG  *pFipsRequired,
                               MQLONG  *pSuiteB,
                               char   **pOcspUrl)
 {
   int      c;                      /* current character             */
   int      argno = 1;              /* Number of current argument    */
   int      rc = OK;                /* return code - OK or FAIL      */

   /******************************************************************/
   /* A single parameter of a question mark gets the usage message   */
   /******************************************************************/
   if ((argc > 1) && (strcmp(argv[1], "?") == 0))
   {
     rc = FAIL;
   }

   /******************************************************************/
   /* Now get whatever we were given from the command line           */
   /******************************************************************/
   while ((rc == OK) && (argno < argc))
   {
     /****************************************************************/
     /* We are at the start of an argument, check for the flag       */
     /* introduction character                                       */
     /****************************************************************/
     if (argv[argno][0] == '-')
     {
       /**************************************************************/
       /* Get the flag, folding to lower case                        */
       /**************************************************************/
       c = tolower(argv[argno][1]);

       if (c == 'm')                           /* Queue manager name */
       {
         rc = GetStringArgument(argc, argv, &argno, pQMgrName);
       }
       else if (c == 'c')                            /* Channel name */
       {
         rc = GetStringArgument(argc, argv, &argno, pChannelName);
       }
       else if (c == 'x')                  /* Server connection name */
       {
         rc = GetStringArgument(argc, argv, &argno, pConnName);
       }
       else if (c == 'k')                 /* SSL key repository stem */
       {
         rc = GetStringArgument(argc, argv, &argno, pKeyReposStem);
       }
       else if (c == 's')                          /* SSL CipherSpec */
       {
         rc = GetStringArgument(argc, argv, &argno, pCipherSpec);
       }
       else if (c == 'l')                       /* Certificate label */
       {
         rc = GetStringArgument(argc, argv, &argno, pCertificateLabel);
       }
       else if (c == 'f')                          /* FIPS required  */
       {
         *pFipsRequired = MQSSL_FIPS_YES;
         argno ++;
       }
       else if (c == 'b')                          /* Suite B        */
       {
         rc = GetIntListArgument(argc, argv, &argno, SuiteBTable,
                                 pSuiteB, MQ_SUITE_B_SIZE);
       }
       else if (c == 'p')           /* Certificate validation policy */
       {
         rc = GetIntListArgument(argc, argv, &argno, CertValPolicyTable,
                                 pCertificateValPolicy, 1);
       }
       else if (c == 'o')                      /* OCSP responder URL */
       {
         rc = GetStringArgument(argc, argv, &argno, pOcspUrl);
       }
       else
       {
         rc = FAIL;
       }
     }
     else
     {
       /**************************************************************/
       /* Have reached end of flag parameters                        */
       /**************************************************************/
       break;
     }
   }

   /******************************************************************/
   /* Check the dependencies between arguments                       */
   /******************************************************************/
   if (rc == OK)
   {
     if ((*pConnName == NULL) && (*pChannelName != NULL))
     {
       rc = FAIL;
     }
   }

   return rc;
 }


 /********************************************************************/
 /* Get the next integer list argument. It may follow the flag       */
 /* character in the current argument or be in the next argument     */
 /* without a flag.                                                  */
 /********************************************************************/
 static int GetIntListArgument(int               argc,
                               char            **argv,
                               int              *pThisArg,
                               IntListEntry     *pTable,
                               MQLONG           *pIntList,
                               int               MaxCount)
 {
   char    *pchCurrent;             /* Ptr to current character      */
   char    *pchToken;               /* Ptr to next token             */
   int      argpos = 2;             /* Position within argument      */
   int      i;                      /* Input array index counter     */
   int      count  = 0;             /* Output array index counter    */
   int      found  = 0;             /* Did we find the argument?     */
   int      rc = OK;                /* Return code                   */

   /******************************************************************/
   /* Set up local variables                                         */
   /******************************************************************/
   pchCurrent = &(argv[*pThisArg][argpos]);

   /******************************************************************/
   /* If we are at the end of an argument...                         */
   /******************************************************************/
   if (*pchCurrent == '\0')
   {
     /****************************************************************/
     /* ... move on to the next one, if there is one                 */
     /****************************************************************/
     if (*pThisArg < argc - 1)
     {
       (*pThisArg)++;
       argpos = 0;
       pchCurrent = argv[*pThisArg];
     }
     else
     {
       rc = FAIL;
     }
   }

   /******************************************************************/
   /* Get the string from the current position but catch strings     */
   /* starting with the flag introduction characters                 */
   /******************************************************************/
   if (rc == OK)
   {
     if ((argpos == 0) && (*pchCurrent == '-'))
     {
       rc = FAIL;
     }
     else
     {
       /**************************************************************/
       /* Make a copy of the string so that we can modify it using   */
       /* strtok().                                                  */
       /**************************************************************/
       pchCurrent = strdup(pchCurrent);

       if (pchCurrent)
       {
         (*pThisArg)++;
       }
       else
       {
         printf("strdup() failed for argument %d\n", *pThisArg);
         rc = FAIL;
       }
     }
   }

   /******************************************************************/
   /* Process each element in the comma-separated list.              */
   /******************************************************************/
   if (rc == OK)
   {
     pchToken = strtok(pchCurrent, ",");

     while ((pchToken != NULL) && (rc == OK))
     {
       found = 0;

       for (i = 0; (pTable[i].Alias != NULL) && (count < MaxCount); i++)
       {
         if ((stricmp(pchToken, pTable[i].Alias) == 0) ||
             (stricmp(pchToken, pTable[i].String) == 0))
         {
           *pIntList = pTable[i].Value;
            pIntList ++;
            count ++;
            found = 1;
            break;
         }
       }

       if (found == 0)
       {
         printf("Unknown integer list value: '%s'\n", pchToken);
         rc = FAIL;
       }

       pchToken = strtok(NULL, ",");
     }

     /* Release duplicate string memory */
     free(pchCurrent);
   }

   return rc;
 }

 /********************************************************************/
 /* Get the next string argument. It may follow the flag character   */
 /* in the current argument or be in the next argument without a     */
 /* flag.                                                            */
 /********************************************************************/
 static int GetStringArgument(int     argc,
                              char  **argv,
                              int    *pThisArg,
                              char  **pString)
 {
   char    *pchCurrent;             /* Ptr to current character      */
   int      argpos = 2;             /* Position within argument      */
   int      rc = OK;                /* Return code                   */

   /******************************************************************/
   /* Set up local variables                                         */
   /******************************************************************/
   pchCurrent = &(argv[*pThisArg][argpos]);


   /******************************************************************/
   /* If we are at the end of an argument...                         */
   /******************************************************************/
   if (*pchCurrent == '\0')
   {
     /****************************************************************/
     /* ... move on to the next one, if there is one                 */
     /****************************************************************/
     if (*pThisArg < argc - 1)
     {
       (*pThisArg)++;
       argpos = 0;
       pchCurrent = argv[*pThisArg];
     }
     else
     {
       rc = FAIL;
     }
   }

   /******************************************************************/
   /* Get the string from the current position but catch strings     */
   /* starting with the flag introduction characters                 */
   /******************************************************************/
   if (rc == OK)
   {
     if ((argpos == 0) && (*pchCurrent == '-'))
     {
       rc = FAIL;
     }
     else
     {
       /**************************************************************/
       /* Since the string argument ends at the end of its argument  */
       /* in the argument vector, move on to the next one            */
       /**************************************************************/
       *pString = pchCurrent;
       (*pThisArg)++;
     }
   }

   return rc;
 }

