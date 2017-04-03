/**
 * Created By: Tushar Soni
 * An Exchange Connector(Unified Inbox): Send/Receive Messages from Exchange Server
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <errno.h>
#include <boost/foreach.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/exception/all.hpp>
#include "soapExchangeServiceBindingProxy.h"
#include "ExchangeServiceBinding.nsmap"
using namespace std;

//#define PORT 8000
//#define QUEUE_SIZE 100

/* Write request log from UE to Exchange Connector */
ofstream fs;

/* exchange connector function for sending message */
bool sendMessage(string str, int &http_err_code)
{
	/* Read JSON Data from HTTP request */
	stringstream ss;
	ss << str;

	boost::property_tree::ptree pt;
	string userID, password, receiver, subject, body;
    
    try
    {
    	boost::property_tree::read_json(ss, pt);

	    string uri = pt.get<std::string>("uri");
	    string s = uri.substr(11);
	    userID = s.substr(0, s.find(":"));
	    if(userID.find("%40") != string::npos){
	    	userID.replace(userID.find("%40"), 3, "@");
	    }
	    password = s.substr(1 + s.find(":"), s.find("@exchange.com") - (1 + s.find(":")));

	    BOOST_FOREACH(boost::property_tree::ptree::value_type &e, pt.get_child("message")){
	    	if(e.first == "receivers"){
	    		BOOST_FOREACH(boost::property_tree::ptree::value_type &e1, e.second){
	    				receiver = e1.second.get<string>("address");
	    		}
	    	}
	    	if(e.first == "subject"){
	    		subject = e.second.data();
	    	}
	    	if(e.first == "parts"){
	    		BOOST_FOREACH(boost::property_tree::ptree::value_type &e2, e.second){
	    				body = e2.second.get<string>("data");
	    		}
	    	}
	    }
	}
	catch(std::exception const&  ex)
	{
	    fs << "Send Message JSON format is wrong. " <<  ex.what() << endl;
	}


    /* Connection with Exchange Server */
	ExchangeServiceBindingProxy *proxy = new ExchangeServiceBindingProxy();

	soap *pSoap = proxy->soap;
	proxy->soap_endpoint = "https://outlook.office365.com/ews/exchange.asmx"; // Fix end-point for exchange web services
	pSoap->userid = userID.c_str();
	pSoap->passwd = password.c_str();

	soap_ssl_init(); /* init OpenSSL (just once) */
	if (soap_ssl_client_context(pSoap, SOAP_SSL_DEFAULT | SOAP_SSL_SKIP_HOST_CHECK,
								"./openssl/client.pem", /* keyfile: required only when client must authenticate to server (see SSL docs on how to obtain this file) */
								"passowrd", /* password to read the key file (not used with GNUTLS) */
								"./openssl/cacerts.pem",  /* cacert file to store trusted certificates (needed to verify server) */
								NULL, /* capath to directory with trusted certificates */
								NULL /* if randfile!=NULL: use a file with random data to seed randomness */
	))
	{
		soap_print_fault(pSoap, stderr);
		exit(1);
	}

	// Run Time Flags: The input-mode and output-mode flags for inbound and outbound message processing
	pSoap->mode = SOAP_IO_DEFAULT | SOAP_IO_KEEPALIVE;  /* in&out: attempt to keep socket connections alive (open) */
	pSoap->omode = SOAP_XML_NOTYPE;  /* out: disable xsi:type attributes */


	/**
	 * CreateItem REQUEST
	 */

	ews__CreateItemType* createItem = new ews__CreateItemType();

	ns1__MessageDispositionType mdt = ns1__MessageDispositionType__SendAndSaveCopy;
	createItem->MessageDisposition = &mdt;

	createItem->SavedItemFolderId = new ns1__TargetFolderIdType();
	createItem->SavedItemFolderId->__union_TargetFolderIdType = 2;
	createItem->SavedItemFolderId->union_TargetFolderIdType.DistinguishedFolderId = new ns1__DistinguishedFolderIdType();
	createItem->SavedItemFolderId->union_TargetFolderIdType.DistinguishedFolderId->Id = ns1__DistinguishedFolderIdNameType__sentitems;


	std::string recipientEmail = receiver.c_str(); // "dynamo@duib.onmicrosoft.com"
	std::string iClass = "IPM.Note";
	bool boolval = false;

	createItem->Items = new ns1__NonEmptyArrayOfAllItemsType();
	createItem->Items->__size_NonEmptyArrayOfAllItemsType = 1;
	createItem->Items->__union_NonEmptyArrayOfAllItemsType = new __ns1__union_NonEmptyArrayOfAllItemsType();
	createItem->Items->__union_NonEmptyArrayOfAllItemsType->__union_NonEmptyArrayOfAllItemsType = 2;
	createItem->Items->__union_NonEmptyArrayOfAllItemsType->union_NonEmptyArrayOfAllItemsType.Message = new ns1__MessageType();

	createItem->Items->__union_NonEmptyArrayOfAllItemsType->union_NonEmptyArrayOfAllItemsType.Message->ItemClass = &iClass;
	createItem->Items->__union_NonEmptyArrayOfAllItemsType->union_NonEmptyArrayOfAllItemsType.Message->Subject = &subject;
	createItem->Items->__union_NonEmptyArrayOfAllItemsType->union_NonEmptyArrayOfAllItemsType.Message->Body = new ns1__BodyType();
	createItem->Items->__union_NonEmptyArrayOfAllItemsType->union_NonEmptyArrayOfAllItemsType.Message->Body->BodyType = ns1__BodyTypeType__Text;
	createItem->Items->__union_NonEmptyArrayOfAllItemsType->union_NonEmptyArrayOfAllItemsType.Message->Body->__item = body;

	createItem->Items->__union_NonEmptyArrayOfAllItemsType->union_NonEmptyArrayOfAllItemsType.Message->ToRecipients = new ns1__ArrayOfRecipientsType();
	createItem->Items->__union_NonEmptyArrayOfAllItemsType->union_NonEmptyArrayOfAllItemsType.Message->ToRecipients->__size_ArrayOfRecipientsType = 1;
	createItem->Items->__union_NonEmptyArrayOfAllItemsType->union_NonEmptyArrayOfAllItemsType.Message->ToRecipients->__union_ArrayOfRecipientsType = new __ns1__union_ArrayOfRecipientsType();
	createItem->Items->__union_NonEmptyArrayOfAllItemsType->union_NonEmptyArrayOfAllItemsType.Message->ToRecipients->__union_ArrayOfRecipientsType->__union_ArrayOfRecipientsType = 1;
	createItem->Items->__union_NonEmptyArrayOfAllItemsType->union_NonEmptyArrayOfAllItemsType.Message->ToRecipients->__union_ArrayOfRecipientsType->union_ArrayOfRecipientsType.Mailbox = new ns1__EmailAddressType();
	createItem->Items->__union_NonEmptyArrayOfAllItemsType->union_NonEmptyArrayOfAllItemsType.Message->ToRecipients->__union_ArrayOfRecipientsType->union_ArrayOfRecipientsType.Mailbox->EmailAddress = &recipientEmail;

	createItem->Items->__union_NonEmptyArrayOfAllItemsType->union_NonEmptyArrayOfAllItemsType.Message->IsRead = &boolval;


	/**
	 * CreateItem RESPONSE
	 */

	__ews__CreateItemResponse createItemRes;


	if( proxy->CreateItem(createItem, createItemRes) == SOAP_OK){
		proxy->destroy(); // delete data and release memory
		return true;
    } else {
      	//proxy->soap_stream_fault(std::cerr);
      	http_err_code = pSoap->error;
      	proxy->destroy(); // delete data and release memory
      	return false;
    }

	//proxy->soap_stream_fault(std::cerr);
//    proxy->destroy(); // delete data and release memory
}


/* Test Add Connection */

bool testConnection(string str, int &http_err_code){

	/* Read JSON Data from HTTP request */
	stringstream ss;
	ss << str;

	boost::property_tree::ptree pt;
	string userID, password;

	try
	{
	    boost::property_tree::read_json(ss, pt);

	    string uri = pt.get<std::string>("uri");
	    string s = uri.substr(11);
	    userID = s.substr(0, s.find(":"));
	    password = s.substr(1 + s.find(":"), s.find("@exchange.com") - (1 + s.find(":")));
	}
	catch(std::exception const&  ex)
	{
	    fs << "Add Connection JSON format is wrong. " <<  ex.what() << endl;
	}


    /* Connection with Exchange Server */
	ExchangeServiceBindingProxy *proxy = new ExchangeServiceBindingProxy();

	soap *pSoap = proxy->soap;
	proxy->soap_endpoint = "https://outlook.office365.com/ews/exchange.asmx"; // Fix end-point for exchange web services
	pSoap->userid = userID.c_str();
	pSoap->passwd = password.c_str();

	soap_ssl_init(); /* init OpenSSL (just once) */
	if (soap_ssl_client_context(pSoap, SOAP_SSL_DEFAULT | SOAP_SSL_SKIP_HOST_CHECK,
								"./openssl/client.pem", /* keyfile: required only when client must authenticate to server (see SSL docs on how to obtain this file) */
								"passowrd", /* password to read the key file (not used with GNUTLS) */
								"./openssl/cacerts.pem",  /* cacert file to store trusted certificates (needed to verify server) */
								NULL, /* capath to directory with trusted certificates */
								NULL /* if randfile!=NULL: use a file with random data to seed randomness */
	))
	{
		soap_print_fault(pSoap, stderr);
		exit(1);
	}

	// Run Time Flags: The input-mode and output-mode flags for inbound and outbound message processing
	pSoap->mode = SOAP_IO_DEFAULT | SOAP_IO_KEEPALIVE;  /* in&out: attempt to keep socket connections alive (open) */
	pSoap->omode = SOAP_XML_NOTYPE;  /* out: disable xsi:type attributes */


	/*FindItem REQUEST*/

	ews__FindItemType* findItem = new ews__FindItemType();

	//Set the Maximum number of items should return in response to one
	int numMax = 1;
	findItem->__union_FindItemType = 1;
	findItem->union_FindItemType.IndexedPageItemView = new ns1__IndexedPageViewType();
	findItem->union_FindItemType.IndexedPageItemView->MaxEntriesReturned = &numMax;

	//The response shape of items. This identifies the properties that are returned in the response. Specifying this is required
	findItem->ItemShape = new ns1__ItemResponseShapeType();
	//findItem->ItemShape = soap_new_ns1__ItemResponseShapeType(proxy->soap);
	findItem->ItemShape->BaseShape = ns1__DefaultShapeNamesType__IdOnly;

	findItem->Traversal = ns1__ItemQueryTraversalType__Shallow;

	/*The folders from which to perform the search. Specifying this is required.*/
	ns1__DistinguishedFolderIdType* dfit = new ns1__DistinguishedFolderIdType();
	dfit->Id = ns1__DistinguishedFolderIdNameType__inbox;

	findItem->ParentFolderIds = new ns1__NonEmptyArrayOfBaseFolderIdsType();
	findItem->ParentFolderIds->__size_NonEmptyArrayOfBaseFolderIdsType = 1;
	findItem->ParentFolderIds->__union_NonEmptyArrayOfBaseFolderIdsType = new __ns1__union_NonEmptyArrayOfBaseFolderIdsType();
	findItem->ParentFolderIds->__union_NonEmptyArrayOfBaseFolderIdsType->__union_NonEmptyArrayOfBaseFolderIdsType = 2;
	findItem->ParentFolderIds->__union_NonEmptyArrayOfBaseFolderIdsType->union_NonEmptyArrayOfBaseFolderIdsType.DistinguishedFolderId = dfit;


	/*FindItem RESPONSE*/

	__ews__FindItemResponse findItemRes;

	if( proxy->FindItem(findItem, findItemRes) == SOAP_OK){
		proxy->destroy(); // delete data and release memory
		return true;
	}else{
		http_err_code = pSoap->error;
		proxy->destroy(); // delete data and release memory
		return false;
	}
}

/* Retrieve Message Content */
bool retrieveMsgContent(string str, int &http_err_code, string &resjson){

	/* Read JSON Data from HTTP request */
	stringstream ss;
	ss << str;

	boost::property_tree::ptree pt;
	string userID, password;
	int entriesperpage;
	int startindex;
	string messageid = "";

	try
	{
	    boost::property_tree::read_json(ss, pt);

	    string uri = pt.get<std::string>("uri");
	    string s = uri.substr(11);
	    userID = s.substr(0, s.find(":"));
	    password = s.substr(1 + s.find(":"), s.find("@exchange.com") - (1 + s.find(":")));

	    entriesperpage = pt.get<int>("entriesperpage", 1);
		startindex = pt.get<int>("startindex", 0);
		messageid = pt.get<std::string>("messageid", "");
	}
	catch(std::exception const&  ex)
	{
	    fs << "Error throw in JSON format of retrieve message content. " <<  ex.what() << endl;
	}


    /* Connection with Exchange Server */
	ExchangeServiceBindingProxy *proxy = new ExchangeServiceBindingProxy();

	soap *pSoap = proxy->soap;
	proxy->soap_endpoint = "https://outlook.office365.com/ews/exchange.asmx"; // Fix end-point for exchange web services
	pSoap->userid = userID.c_str();
	pSoap->passwd = password.c_str();

	soap_ssl_init(); /* init OpenSSL (just once) */
	if (soap_ssl_client_context(pSoap, SOAP_SSL_DEFAULT | SOAP_SSL_SKIP_HOST_CHECK,
								"./openssl/client.pem", /* keyfile: required only when client must authenticate to server (see SSL docs on how to obtain this file) */
								"passowrd", /* password to read the key file (not used with GNUTLS) */
								"./openssl/cacerts.pem",  /* cacert file to store trusted certificates (needed to verify server) */
								NULL, /* capath to directory with trusted certificates */
								NULL /* if randfile!=NULL: use a file with random data to seed randomness */
	))
	{
		soap_print_fault(pSoap, stderr);
		exit(1);
	}

	// Run Time Flags: The input-mode and output-mode flags for inbound and outbound message processing
	pSoap->mode = SOAP_IO_DEFAULT | SOAP_IO_KEEPALIVE;  /* in&out: attempt to keep socket connections alive (open) */
	pSoap->omode = SOAP_XML_NOTYPE;  /* out: disable xsi:type attributes */


	/*GetItem REQUEST*/

	ews__GetItemType* getItem = new ews__GetItemType();
	getItem->ItemShape = new ns1__ItemResponseShapeType();
	//getItem->ItemShape->BaseShape = ns1__DefaultShapeNamesType__IdOnly;
	getItem->ItemShape->BaseShape = ns1__DefaultShapeNamesType__Default;

	//get the ItemId from FindItem
	ns1__ItemIdType* ItmID = new ns1__ItemIdType();
	ItmID->Id = messageid;  /*Set the itemID of the desired item to retrieve from FindItem*/
	
	getItem->ItemIds = new ns1__NonEmptyArrayOfBaseItemIdsType();
	getItem->ItemIds->__size_NonEmptyArrayOfBaseItemIdsType = 1;
	getItem->ItemIds->__union_NonEmptyArrayOfBaseItemIdsType = new __ns1__union_NonEmptyArrayOfBaseItemIdsType();
	getItem->ItemIds->__union_NonEmptyArrayOfBaseItemIdsType->__union_NonEmptyArrayOfBaseItemIdsType = 1;
	getItem->ItemIds->__union_NonEmptyArrayOfBaseItemIdsType->union_NonEmptyArrayOfBaseItemIdsType.ItemId = ItmID;


	/*GetItem RESPONSE*/

	__ews__GetItemResponse getItemRes;

	if(proxy->GetItem(getItem, getItemRes) == SOAP_OK){

		string* subject;
		string* sender_name;
		string* sender_address;
		string* receiver_name;
		string* receiver_address;
		string body_data;
		int* size;
		time_t* sent_time;

		ns1__MessageType* messT = new ns1__MessageType();				
		ews__ItemInfoResponseMessageType* iirmt = new ews__ItemInfoResponseMessageType();
		
		iirmt = getItemRes.ews__GetItemResponse->ResponseMessages->__union_ArrayOfResponseMessagesType->union_ArrayOfResponseMessagesType.GetItemResponseMessage;
		messT = iirmt->Items->__union_ArrayOfRealItemsType->union_ArrayOfRealItemsType.Message;
		subject = messT->Subject;
		sender_name = messT->From->union_SingleRecipientType.Mailbox->Name;
		sender_address = messT->From->union_SingleRecipientType.Mailbox->EmailAddress;
		receiver_name = messT->ToRecipients->__union_ArrayOfRecipientsType->union_ArrayOfRecipientsType.Mailbox->Name;
		receiver_address = messT->ToRecipients->__union_ArrayOfRecipientsType->union_ArrayOfRecipientsType.Mailbox->EmailAddress;
		body_data = messT->Body->__item;
		size = messT->Size;
		sent_time = messT->DateTimeSent;

		try
		{
			boost::property_tree::ptree root;

		    //status
		    boost::property_tree::ptree status_exchange_node;
		    status_exchange_node.put("status", 200);
		    status_exchange_node.put("info", "OK");
		    boost::property_tree::ptree status_node;
		    status_node.add_child("exchange", status_exchange_node);
		    root.add_child("status", status_node);

		    //sender
		    boost::property_tree::ptree sender;
		    sender.put("connector", "exchange");
		    sender.put("name", *sender_name);
		    sender.put("address", *sender_address);
		    sender.put("uri", "");

		    //returnPath
		    boost::property_tree::ptree returnPath;
		    returnPath.put("connector", "exchange");
		    returnPath.put("address", "");
		    returnPath.put("uri", "");

		    //receivers
		    boost::property_tree::ptree receivers;
		    boost::property_tree::ptree tmp_node;
		    tmp_node.put("connector", "exchange");
		    tmp_node.put("name", *receiver_name);
		    tmp_node.put("address", *receiver_address);
		    tmp_node.put("uri", "");
		    receivers.push_back(std::make_pair("", tmp_node));

		    //parts
		    boost::property_tree::ptree parts;
		    boost::property_tree::ptree tmp_node1;
		    tmp_node1.put("id", "0");
		    tmp_node1.put("contentType", "text/html");
		    tmp_node1.put("type", "body");
		    tmp_node1.put("data", body_data);
		    tmp_node1.put("size", *size);
		    tmp_node1.put("sort", 0);
		    parts.push_back(std::make_pair("", tmp_node1));

		    //messages-exchange
		    boost::property_tree::ptree msg_exchange_node;
		    boost::property_tree::ptree tmp_node2;
		    tmp_node2.put("uri", "inbox/");
		    tmp_node2.push_back(std::make_pair("mid", messageid));
		    tmp_node2.put("timestamp", *sent_time);
		    tmp_node2.push_back(std::make_pair("sender", sender));
		    tmp_node2.push_back(std::make_pair("returnPath", returnPath));
		    tmp_node2.push_back(std::make_pair("receivers", receivers));
		    tmp_node2.put("subject", *subject);
		    tmp_node2.put("date", *sent_time);
		    tmp_node2.put("userAgent", "");
		    tmp_node2.put("headers", "null");
		    tmp_node2.push_back(std::make_pair("parts", parts));
		    msg_exchange_node.push_back(std::make_pair("", tmp_node2));

		    //messages
		    boost::property_tree::ptree messages;
		    messages.add_child("exchange", msg_exchange_node);

		    root.add_child("messages", messages);

		    stringstream sstm;
		    boost::property_tree::write_json(sstm, root);

		    resjson = sstm.str();		   	
		}
		catch(std::exception const&  ex)
		{
		    fs << "Error: Retrieve Message Content parsing to JSON failed. " <<  ex.what() << endl;
		}

		proxy->destroy(); // delete data and release memory
		return true;
	}else{

    	std::cout <<  "Error occurs to Get Item response (i.e. retrieveMsgContent)" << std::endl;
      	proxy->soap_stream_fault(std::cerr); 
      	proxy->destroy(); // delete data and release memory
		return false;
    }

}


/* Retrieve Message Ids */
bool retrieveMsgIds(string str, int &http_err_code){

	/* Read JSON Data from HTTP request */
	stringstream ss;
	ss << str;

	boost::property_tree::ptree pt;
	string userID, password;
	int entriesperpage;
	int startindex;
	string messageid = "";

	try
	{
	    boost::property_tree::read_json(ss, pt);

	    string uri = pt.get<std::string>("uri");
	    string s = uri.substr(11);
	    userID = s.substr(0, s.find(":"));
	    password = s.substr(1 + s.find(":"), s.find("@exchange.com") - (1 + s.find(":")));

	    entriesperpage = pt.get<int>("entriesperpage", 1);
		startindex = pt.get<int>("startindex", 0);
		messageid = pt.get<std::string>("messageid", "");
	}
	catch(std::exception const&  ex)
	{
	    fs << "Error throw in JSON format of retrieve message Ids. " <<  ex.what() << endl;
	}


    /* Connection with Exchange Server */
	ExchangeServiceBindingProxy *proxy = new ExchangeServiceBindingProxy();

	soap *pSoap = proxy->soap;
	proxy->soap_endpoint = "https://outlook.office365.com/ews/exchange.asmx"; // Fix end-point for exchange web services
	pSoap->userid = userID.c_str();
	pSoap->passwd = password.c_str();

	soap_ssl_init(); /* init OpenSSL (just once) */
	if (soap_ssl_client_context(pSoap, SOAP_SSL_DEFAULT | SOAP_SSL_SKIP_HOST_CHECK,
								"./openssl/client.pem", /* keyfile: required only when client must authenticate to server (see SSL docs on how to obtain this file) */
								"passowrd", /* password to read the key file (not used with GNUTLS) */
								"./openssl/cacerts.pem",  /* cacert file to store trusted certificates (needed to verify server) */
								NULL, /* capath to directory with trusted certificates */
								NULL /* if randfile!=NULL: use a file with random data to seed randomness */
	))
	{
		soap_print_fault(pSoap, stderr);
		exit(1);
	}

	// Run Time Flags: The input-mode and output-mode flags for inbound and outbound message processing
	pSoap->mode = SOAP_IO_DEFAULT | SOAP_IO_KEEPALIVE;  /* in&out: attempt to keep socket connections alive (open) */
	pSoap->omode = SOAP_XML_NOTYPE;  /* out: disable xsi:type attributes */


	/*FindItem REQUEST*/

	ews__FindItemType* findItem = new ews__FindItemType();

	//Set the Maximum number of items should return in response with entriesperpage
	findItem->__union_FindItemType = 1;
	findItem->union_FindItemType.IndexedPageItemView = new ns1__IndexedPageViewType();
	findItem->union_FindItemType.IndexedPageItemView->MaxEntriesReturned = &entriesperpage;
	findItem->union_FindItemType.IndexedPageItemView->Offset = startindex;

	//The response shape of items. This identifies the properties that are returned in the response. Specifying this is required
	findItem->ItemShape = new ns1__ItemResponseShapeType();
	//findItem->ItemShape = soap_new_ns1__ItemResponseShapeType(proxy->soap);
	findItem->ItemShape->BaseShape = ns1__DefaultShapeNamesType__IdOnly;

	findItem->Traversal = ns1__ItemQueryTraversalType__Shallow;

	/*The folders from which to perform the search. Specifying this is required.*/
	ns1__DistinguishedFolderIdType* dfit = new ns1__DistinguishedFolderIdType();
	dfit->Id = ns1__DistinguishedFolderIdNameType__inbox;

	findItem->ParentFolderIds = new ns1__NonEmptyArrayOfBaseFolderIdsType();
	findItem->ParentFolderIds->__size_NonEmptyArrayOfBaseFolderIdsType = 1;
	findItem->ParentFolderIds->__union_NonEmptyArrayOfBaseFolderIdsType = new __ns1__union_NonEmptyArrayOfBaseFolderIdsType();
	findItem->ParentFolderIds->__union_NonEmptyArrayOfBaseFolderIdsType->__union_NonEmptyArrayOfBaseFolderIdsType = 2;
	findItem->ParentFolderIds->__union_NonEmptyArrayOfBaseFolderIdsType->union_NonEmptyArrayOfBaseFolderIdsType.DistinguishedFolderId = dfit;


	/*FindItem RESPONSE*/

	__ews__FindItemResponse findItemRes;

	if( proxy->FindItem(findItem, findItemRes) == SOAP_OK){
		//findItemRes.ews__FindItemResponse->ResponseMessages->__union_ArrayOfResponseMessagesType->union_ArrayOfResponseMessagesType.FindItemResponseMessage->RootFolder->union_FindItemParentType.Items->__union_ArrayOfRealItemsType[i].union_ArrayOfRealItemsType.Message->ItemId->Id;
		vector<string> ids_list;
		ns1__ArrayOfRealItemsType* all_items = findItemRes.ews__FindItemResponse->ResponseMessages->__union_ArrayOfResponseMessagesType->union_ArrayOfResponseMessagesType.FindItemResponseMessage->RootFolder->union_FindItemParentType.Items;
		if((*(findItemRes.ews__FindItemResponse->ResponseMessages->__union_ArrayOfResponseMessagesType->union_ArrayOfResponseMessagesType.FindItemResponseMessage->__ResponseMessageType_sequence->ResponseCode)) == (ews__ResponseCodeType__NoError)){
			for(int i=0; i < entriesperpage; i++){
				 ids_list.push_back(all_items->__union_ArrayOfRealItemsType[i].union_ArrayOfRealItemsType.Message->ItemId->Id);
				 cout << all_items->__union_ArrayOfRealItemsType[i].union_ArrayOfRealItemsType.Message->ItemId->Id << endl;
			}
		}else{
			cout << *(findItemRes.ews__FindItemResponse->ResponseMessages->__union_ArrayOfResponseMessagesType->union_ArrayOfResponseMessagesType.FindItemResponseMessage->__ResponseMessageType_sequence->ResponseCode) << endl;
			http_err_code = 400;
			proxy->destroy(); // delete data and release memory
			return false;
		}

		proxy->destroy(); // delete data and release memory
		return true;
	}else{
		http_err_code = pSoap->error;
		proxy->destroy(); // delete data and release memory
		return false;
	}


}



string error_code_def(int http_err_code){
	switch(http_err_code){
		case 201:
			return "201 Created";
		case 202:
			return "202 Accepted";
		case 203:
			return "203 Non-Authoritative Information";
		case 204:
			return "204 No Content";
		case 205:
			return "205 Reset Content";
		case 206:
			return "206 Partial Content";
		case 300:
			return "300 Multiple Choices";
		case 301:
			return "301 Moved Permanently";
		case 302:
			return "302 Found";
		case 303:
			return "303 See Other";
		case 304:
			return "304 Not Modified";
		case 305:
			return "305 Use Proxy";
		case 307:
			return "307 Temporary Redirect";
		case 400:
			return "400 Bad Request";
		case 401:
			return "401 Unauthorized";
		case 402:
			return "402 Payment Required";
		case 403:
			return "403 Forbidden";
		case 404:
			return "404 Not Found";
		case 405:
			return "405 Method Not Allowed";
		case 406:
			return "406 Not Acceptable";
		case 407:
			return "407 Proxy Authentication Required";
		case 408:
			return "408 Request Timeout";
		case 409:
			return "409 Conflict";
		case 410:
			return "410 Gone";
		case 411:
			return "411 Length Required";
		case 412:
			return "412 Precondition Failed";
		case 413:
			return "413 Request Entity Too Large";
		case 414:
			return "414 Request-URI Too Long";
		case 415:
			return "415 Unsupported Media Type";
		case 416:
			return "416 Requested Range Not Satisfiable";
		case 417:
			return "417 Expectation Failed";
		case 500:
			return "500 Internal Server Error";
		case 501:
			return "501 Not Implemented";
		case 502:
			return "502 Bad Gateway";
		case 503:
			return "503 Service Unavailable";
		case 504:
			return "504 Gateway Timeout";
		case 505:
			return "505 HTTP Version Not Supported";
		default:
			return "Out-of-range HTTP status code";
	}
}

/* Process each request */
void doProcess(int sock) {
    int n_rec;
    char buffer[5000];
    bzero(buffer,5000);

	n_rec = read(sock, buffer, 5000);

	fs << "\n\n\n" << endl;

	if (n_rec < 0) {
		fs << "ERROR reading from socket" << endl;
	    perror("ERROR reading from socket");
	    exit(1);
	}

	if(n_rec == 0){
	  	close(sock);
	}

	fs << buffer << endl;


	string buf_str, str_body, str_header;
	if(buffer != NULL){
		buf_str = buffer;
		str_header = buf_str.substr(0, buf_str.find("\r\n\r\n"));
		str_body = buf_str.substr(buf_str.find("\r\n\r\n"));
	}

	int n_send, http_err_code;
	if(str_header.find("/v2/test") != string::npos){
		fs << "\nWaiting for response from Connector for Add API call..." << endl;
		if(testConnection(str_body, http_err_code)){
			const char* success = "HTTP/1.1 200 OK\r\n\r\n{\"status\": 200, \"info\": \"OK\"}";
			n_send = write(sock, success, strlen(success));
			fs << "\nConnection is successfully added" << endl;
			fs << success << endl;
		}else{
			string str_code = error_code_def(http_err_code);
			string err_code;
			stringstream ss;
			ss << http_err_code;
			ss >> err_code;
			string tmp_fail = "HTTP/1.1 " + str_code + "\r\n\r\n{\"status\": " + err_code + ", \"info\": \"ERROR\"}";
			const char* fail = tmp_fail.c_str();
			n_send = write(sock, fail, strlen(fail));
			fs << "\nError in add connection is " + tmp_fail << endl;
			fs << fail << endl;
		}
	}else if(str_header.find("/v2/message/send") != string::npos){
		fs << "\nWaiting for response from Connector for Send API call..." << endl;
		if(sendMessage(str_body, http_err_code)){
			const char* success = "HTTP/1.1 200 OK\r\n\r\n{\"status\": 200, \"info\": \"OK\"}";
			n_send = write(sock, success, strlen(success));
			fs << "\nMessage is successfully sent" << endl;
			fs << success << endl;
		}else{
			string str_code = error_code_def(http_err_code);
			string err_code;
			stringstream ss;
			ss << http_err_code;
			ss >> err_code;
			string tmp_fail = "HTTP/1.1 " + str_code + "\r\n\r\n{\"status\": " + err_code + ", \"info\": \"ERROR\"}";
			const char* fail = tmp_fail.c_str();
			n_send = write(sock, fail, strlen(fail));
			fs << "\nError in sending message is " + tmp_fail << endl;
			fs << fail << endl;
		}
	}else if(str_header.find("/v2/message/retrieve") != string::npos){
		fs << "\nMessage retrieve API call to Exchange Server..." << endl;

		/* Read JSON Data from HTTP request */
		stringstream ss;
		ss << str_body;

		boost::property_tree::ptree pt;
		int entriesperpage;
		int startindex;
		string messageid = "";

		try
		{
		    boost::property_tree::read_json(ss, pt);

		    entriesperpage = pt.get<int>("entriesperpage", 0);
		    startindex = pt.get<int>("startindex", 0);
		    messageid = pt.get<std::string>("messageid", "");
		}
		catch(std::exception const&  ex)
		{
		    fs << "Error throw in JSON format of retrieve message. " <<  ex.what() << endl;
		}


		if(messageid != ""){
			string resjson;
			//fetch the content of msg via msgid
			if(retrieveMsgContent(str_body, http_err_code, resjson)){
				const char* success = resjson.c_str();
				n_send = write(sock, success, strlen(success));
				fs << "\nRetrieveMsgContent API call successful" << endl;
				fs << success << endl;
			}else{
				string str_code = error_code_def(http_err_code);
				string err_code;
				stringstream ss;
				ss << http_err_code;
				ss >> err_code;
				string tmp_fail = "HTTP/1.1 " + str_code + "\r\n\r\n{\"status\": " + err_code + ", \"info\": \"ERROR\"}";
				const char* fail = tmp_fail.c_str();
				n_send = write(sock, fail, strlen(fail));
				fs << "\nError in RetrieveMsgContent API call is " + tmp_fail << endl;
				fs << fail << endl;
			}
		}else{
			//fetch ids
			if(retrieveMsgIds(str_body, http_err_code)){
				const char* success = "HTTP/1.1 200 OK\r\n\r\n{\"status\": 200, \"info\": \"OK\"}";
				n_send = write(sock, success, strlen(success));
				fs << "\nretrieveMsgIds API call successful" << endl;
				fs << success << endl;
			}else{
				string str_code = error_code_def(http_err_code);
				string err_code;
				stringstream ss;
				ss << http_err_code;
				ss >> err_code;
				string tmp_fail = "HTTP/1.1 " + str_code + "\r\n\r\n{\"status\": " + err_code + ", \"info\": \"ERROR\"}";
				const char* fail = tmp_fail.c_str();
				n_send = write(sock, fail, strlen(fail));
				fs << "\nError in retrieveMsgIds API call is " + tmp_fail << endl;
				fs << fail << endl;
			}
		}

	}else{
		fs << "\nUnknown request from UE" << endl;
	}

	if (n_send < 0) {
		fs << "ERROR writing to socket" << endl;
	    perror("ERROR writing to socket");
	    exit(1);
	}

	//fs.close();

}

int main()
{
	/* open log file */
	fs.open("ue_connector_log.txt", std::ios_base::app);

	/* read config.txt */
	string line;
	int PORT, QUEUE_SIZE;

	fs << "\n\n\n\n\n================================================= New Connection #" << endl;

	stringstream ss;
	ifstream in;
	in.open("config.txt");
	if(in.fail()){
		fs  << "failed to open config.txt file" << endl;
		in.clear();
	}
	while(getline(in, line)){
		ss.clear();
		ss.str("");
		ss << line;

		if(line.find("PORT:") != string::npos){
			string tmp = line.substr(5);
			ss.clear();
			ss.str("");
			ss << tmp;
			ss >> PORT;
		}

		if(line.find("QUEUE_SIZE:") != string::npos){
			string tmp = line.substr(11);
			ss.clear();
			ss.str("");
			ss << tmp;
			ss >> QUEUE_SIZE;
		}
	}
	in.close();

	/* socket connection */
	int sockfd, newsockfd;
	socklen_t clientAddressLength;
	struct sockaddr_in serv_addr, cli_addr;
	int pid;

	/* First call to socket() function */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
    	fs << "ERROR opening socket" << endl;
        perror("ERROR opening socket");
        exit(1);
    }

    /* Initialize socket structure */
    bzero((char *) &serv_addr, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);

    /* Now bind the socket with host address and port using bind() call.*/
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    	fs << "ERROR on binding" << endl;
        perror("ERROR on binding");
        exit(1);
    }

    /* Now start listening for the clients, here
      * process will go in sleep mode and will wait
      * for the incoming connection
   */
    listen(sockfd, QUEUE_SIZE);
    clientAddressLength = sizeof(cli_addr);

	while (true) {
		/* parent process waiting to accept a new connection */
	    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clientAddressLength);

	    if (newsockfd < 0) {
	    	fs << "ERROR on accept" << endl;
	        perror("ERROR on accept");
	        exit(1);
	    }

	    /* Create child process */
	    pid = fork();

	    if (pid < 0) {
	    	fs << "ERROR on fork" << endl;
	        perror("ERROR on fork");
	        exit(1);
	    }

	    if (pid == 0) {
	        /* This is the client process */
	        close(sockfd);
	        doProcess(newsockfd);
	        exit(0);
	    }
	    else {
	        close(newsockfd);
	        if(waitpid(-1, NULL, WNOHANG) < 0){
	        	fs << "Failed to collect child process" << endl;
	        	perror("Failed to collect child process");
	        	exit(1);
	        }
	    }
	}

	fs.close();

   return 0;
}
