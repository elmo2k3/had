#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <string.h>
#include "had.h"

static size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp);

static const char *identify_string = 
"<?xml version=\"1.0\"?>\n\
<methodCall>\n\
<methodName>samurai.ClientIdentify</methodName>\n\
<params>\n\
<param><value><struct>\n\
<member><name>ClientName</name><value><string>bjoerns sms program</string></value></member>\n\
</struct>\n\
</value>\n\
</param>\n\
</params>\n\
</methodCall>";

static const char *sms_string =
"<?xml version=\"1.0\"?>\n\
<methodCall>\n\
<methodName>samurai.SessionInitiate</methodName>\n\
<params>\n\
<param><value><struct>\n\
<member><name>RemoteUri</name><value><string>sip:%s@sipgate.net</string></value></member>\n\
<member><name>TOS</name><value><string>text</string></value></member>\n\
<member><name>Content</name><value><string>%s</string></value></member>\n\
</struct>\n\
</value>\n\
</param>\n\
</params>\n\
</methodCall>";

static const char *url = "https://%s:%s@samurai.sipgate.net/RPC2";

static size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
	return size*nmemb;
}


void sms(char *message)
{
	char buf[2048];
	char buf2[2048];
	char new_url[2048];

	struct curl_slist *headers=NULL;
	headers = curl_slist_append(headers, "Content-Type: text/xml");

	sprintf(new_url, url, config.sipgate_user, config.sipgate_pass);

	sprintf(buf, "Content-length: %d", (int)strlen(identify_string));
	headers = curl_slist_append(headers, buf);
  
	CURL *curlhandler = curl_easy_init();
	curl_easy_setopt(curlhandler, CURLOPT_POSTFIELDS, identify_string);
	curl_easy_setopt(curlhandler, CURLOPT_URL, new_url);
	curl_easy_setopt(curlhandler, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curlhandler, CURLOPT_POSTFIELDSIZE, strlen(identify_string));
	curl_easy_setopt(curlhandler, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(curlhandler, CURLOPT_WRITEFUNCTION, write_data);
	
	curl_slist_free_all(headers);

	sprintf(buf2, sms_string, config.cellphone, message);
	headers = NULL;
	headers = curl_slist_append(headers, "Content-Type: text/xml");
	sprintf(buf, "Content-length: %d", (int)strlen(buf2));
	headers = curl_slist_append(headers, buf);


	curl_easy_setopt(curlhandler, CURLOPT_POSTFIELDS, buf2);
	curl_easy_setopt(curlhandler, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curlhandler, CURLOPT_POSTFIELDSIZE, strlen(buf2));

	verbose_printf(0,"sms send %s\n",message);
//	printf(buf);
	curl_easy_perform(curlhandler);
}
