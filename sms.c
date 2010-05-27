/*
 * Copyright (C) 2008-2010 Bjoern Biesenbach <bjoern@bjoern-b.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*!
 * \file    sms.c
 * \brief   sending sms support
 * \author  Bjoern Biesenbach <bjoern at bjoern-b dot de>
*/

#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <string.h>
#include <glib.h>
#include <stdio.h>
#include "had.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "sms"

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


void sms(char *number, char *message)
{
    char buf[2048];
    char buf2[2048];
    char new_url[2048];
    struct curl_slist *headers=NULL;

    if(!config.sms_activated)
        return;

    g_assert(message);
    g_assert(number);

    if(strlen(message) > 160) {
        g_message("message too long (%d)",(int)strlen(message));
        message[160] = '\0';
    }

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

    sprintf(buf2, sms_string, number, message);
    headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: text/xml");
    sprintf(buf, "Content-length: %d", (int)strlen(buf2));
    headers = curl_slist_append(headers, buf);


    curl_easy_setopt(curlhandler, CURLOPT_POSTFIELDS, buf2);
    curl_easy_setopt(curlhandler, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curlhandler, CURLOPT_POSTFIELDSIZE, strlen(buf2));

    g_message("sms send %s to number %s",message, number);
    curl_easy_perform(curlhandler);
}
