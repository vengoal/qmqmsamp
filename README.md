# qmqmsamp
IBM i (AS/400) QMQMSAMP Library Sample program 

## qrpglesrc ILE RPG samples description
https://github.com/vengoal/qmqmsamp/blob/main/qrpglesrc/MBRLIST.txt
<ul>
  <li>MQ Copy files for ILE RPG/400, they are supplied as members of file QRPGLESRC in library QMQM.</li>
<li>To do this for ILE RPG/400, you can use the typical IBM i commands, CRTRPGMOD and CRTPGM.

After creating your *MODULE, you need to specify BNDSRVPGM(QMQM/LIBMQM) in the CRTPGM command. This includes the various IBM MQ procedures in your program.

Make sure that the library containing the copy files (QMQM) is in the library list when you perform the compilation.</li>
</ul>

## qcbllesrc ILE COBOL samples description
https://github.com/vengoal/qmqmsamp/blob/main/qcbllesrc/MBRLIST.txt
<ul>
  <li>The COBOL copy files containing the named constants and structure definitions for use with the MQI are contained in the source physical file QMQM/QCBLLESRC.</li>
  <li><a href="https://www.ibm.com/docs/en/ibm-mq/9.3?topic=i-preparing-cobol-programs-in">Preparing COBOL programs in IBM i</a>
  To do this for ILE COBOL, you can use the typical IBM i commands, CRTCBLMOD and CRTPGM.
    
  After creating your *MODULE, you need to specify BNDSRVPGM(QMQM/AMQ0STUB) in the CRTPGM command. This includes the various IBM MQ procedures in your program.

Make sure that the library containing the copy files (QMQM) is in the library list when you perform the compilation.</li>
</ul>

## qcsrc C samples description
https://github.com/vengoal/qmqmsamp/blob/main/qcsrc/MBRLIST.txt
<ul>
  <li>The C include files containing the named constants and structure definitions for use with the MQI are contained in the source physical file QMQM/H.</li>
  <li><a href="https://www.ibm.com/docs/en/ibm-mq/9.3?topic=i-preparing-c-programs-in">Preparing C programs in IBM i</a></li>
  <li>CRTCMOD and CRTPGM with BNDSRVPGM(QMQM/LIBMQM)</li>
</ul>

## qclsrc CL samples description
https://github.com/vengoal/qmqmsamp/blob/main/qclsrc/MBRLIST.txt

## Reference
<ul>
  <li><a href="https://www.ibm.com/docs/en/ibm-mq/9.3?topic=reference-i-application-programming-ilerpg" target="_blank"> IBM i Application Programming Reference (ILE/RPG)</a></li>
  <li><a href="https://public.dhe.ibm.com/software/integration/library/books/amqwak00.pdf">Application Programming Reference (ILE RPG) PDF</a></li>
  <li><a href="https://www.ibm.com/docs/en/ibm-mq/9.3?topic=application-building-your-procedural-i">Building your procedural application on IBM i</a></li>
</ul>
