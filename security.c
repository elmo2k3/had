/*
 * Copyright (C) 2010 Bjoern Biesenbach <bjoern@bjoern-b.de>
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

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "security"

#include <glib.h>
#include <string.h>
#include <stdio.h>
#include "base_station.h"
#include "rfid_tag_reader.h"
#include "sms.h"
#include "had.h"
#include "misc.h"

#define SECURITY_TAG_ACTIVATE "0183C3637E"
#define SECURITY_TAG_DEACTIVATE "0108A2072F"
#define SECURITY_TAG_DEACTIVATE2 "01087214A1"

static int activated = 0; //later this could be read from statefile or so
static guint activate_source = 0;
static guint alarm_source = 0;

int security_is_active(void)
{
    return activated;
}

static gboolean activate_security(gpointer data)
{
    activated = 1;
    activate_source = 0;
    base_station_beep(2,50,50);
    if(config.switch_off_with_security) {
        g_debug("switching everything off");
        base_station_everything_off();
    }
    g_debug("activating security");
    return FALSE;
}

static void deactivate_security(void)
{
    if(activate_source) {
        g_debug("removing activate_source");
        g_source_remove(activate_source);
    }
    if(alarm_source) {
        g_debug("removing alarm_source");
        g_source_remove(alarm_source);
    }
    if(config.switch_off_with_security && !is_daylight()) {
        for(int i = 0; i < 3; i++)
        {
            hadState.rgbModuleValues[i].red = 255;
            hadState.rgbModuleValues[i].green = 255 ;
            hadState.rgbModuleValues[i].blue = 0;
        }
        setCurrentRgbValues();
    }
    activate_source = 0;
    alarm_source = 0;
    activated = 0;
    g_debug("deactivating security");
}

static gboolean door_alarm(gpointer data)
{
    alarm_source = 0;
    g_debug("sending sms because door was opened");
    sms(config.cellphone, "HAD: Wohnungstür in Abwesenheit geöffnet!!!");
    return FALSE;
}

void security_main_door_opening(gchar *number)
{
    gchar sms_string[160];
    
    if(number) {
        if(!strcmp(number,"01626824519")) {
            g_debug("the master himself is opening the door");
            return;
        }
    }
    if(security_is_active() && config.sms_on_main_door) {
        if(number) {
            snprintf(sms_string, 160, "HAD: Haustür während Abwesenheit geöffnet von %s",
                number);
        } else {
            snprintf(sms_string, 160, "HAD: Haustür während Abwesenheit geöffnet von unbekannt");
        }
        sms(config.cellphone, sms_string);
    }
}

void security_door_opened(void)
{
    if(activated) {
        if(!alarm_source) {
            alarm_source = g_timeout_add_seconds(config.security_time_before_alarm
                ,door_alarm, NULL);
        }
        base_station_beep(2,100,100);
    }
}

void security_tag_handler(struct RfidTagReader *tag_reader)
{
    gchar tag[11];  

    strncpy(tag, rfid_tag_reader_last_tag(tag_reader), 11);

    if (!strcmp(tag, SECURITY_TAG_ACTIVATE)) { // activate
        if(!activate_source) {
            activate_source = g_timeout_add_seconds(
                config.security_time_to_active, activate_security, NULL);
        }
        base_station_beep(1,50,0);
    } else if (!strcmp(tag, SECURITY_TAG_DEACTIVATE) || 
                !strcmp(tag, SECURITY_TAG_DEACTIVATE2)) {
        deactivate_security();
        base_station_beep(2,50,50);
    } else {
        g_message("unknown tag scanned: %s",tag);
        base_station_beep(1,50,0);
    }
}


