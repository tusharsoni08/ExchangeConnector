Exchange Connector (Unified Inbox)
===================================

Programming Language
---------------------
	- C/C++

Required Dependencies
---------------------
	- Boost: sudo apt-get install libboost-all-dev
	- OpenSSL: sudo apt-get install libssl-dev


Compilation
--------------------
	g++ -DWITH_OPENSSL -DDEBUG -o ewsClient sendMsg.cpp soapC.cpp soapExchangeServiceBindingProxy.cpp stdsoap2.cpp ./gsoap/custom/duration.c -lssl -lcrypto


Run Binaries
-------------
	./ewsClient


Log Files
----------
	1. Request logs from UE to Connector:
		- ue_connector_log.txt

	2. Request(SENT) logs from Connector to Exchange Server:
		- SENT.log

	3. Response(RECV) logs from Exchange Server to Connector:
		- RECV.log

	4. *(Don't use it) Only for Internal details (Hardware-level) of SOAP request/response
		- TEST.log


Configuration File (config.txt)
--------------------------------
	- Details about PORT number of Connector and QUEUE_SIZE of the request from UE to Connector. 
	- Connector is concurrently processing request from queue. 
	- If queue is full then Connector will refuse the further request from UE.

	NOTE: Please make sure in editing config.txt file, that Do not change anything in formatting. You are only allowed to change the values. Do not enter new line or space. Otherwise program will failed to read file correctly.


Figure for communication
-------------------------

	UE <--------------------------> Exchange_Connector <------------------------> Exchange_Server


Contact
--------
	For any help please contact me: 
	E-mail: soni.tushar93@gmail.com or tushar.soni@unifiedinbox.com


