/*
 * Copyright (C) 2007-2009 Bjoern Biesenbach <bjoern@bjoern-b.de>
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
* \file	config.c
* \brief	config file handling
* \author	Bjoern Biesenbach <bjoern at bjoern-b dot de>
*/

#include <string.h>
#include <glib.h>
#include <stdio.h>
#include "config.h"

static GKeyFile *had_key_file;
static gchar *found_path;

gint hadConfigGetInt(gchar *group, gchar *key, gint default_int)
{
    GError *err = NULL;
    gint int_to_return;

    if(!key || !group || !strlen(key) || !strlen(group) || !had_key_file)
    {
        g_debug("config: error in argument");
        return default_int;
    }

    int_to_return = g_key_file_get_integer(had_key_file, group, key, &err);
    if(err)
    {
        if(err->code == G_KEY_FILE_ERROR_KEY_NOT_FOUND ||
            err->code == G_KEY_FILE_ERROR_GROUP_NOT_FOUND) // key not found so insert default value
        {
            g_key_file_set_integer(had_key_file, group, key, default_int);
        }
        else
        {
            g_debug("config: error reading %s,%s: %s",group,key,err->message);
        }
        g_error_free(err);
        return default_int;
    }
    else
        return int_to_return;
}

gboolean hadConfigGetBool(gchar *group, gchar *key, gboolean default_bool)
{
    GError *err = NULL;
    gboolean bool_to_return;

    if(!key || !group || !strlen(key) || !strlen(group) || !had_key_file)
    {
        g_debug("config: error in argument");
        return default_bool;
    }

    bool_to_return = g_key_file_get_boolean(had_key_file, group, key, &err);
    if(err)
    {
        if(err->code == G_KEY_FILE_ERROR_KEY_NOT_FOUND ||
            err->code == G_KEY_FILE_ERROR_GROUP_NOT_FOUND) // key not found so insert default value
        {
            g_key_file_set_boolean(had_key_file, group, key, default_bool);
        }
        else
        {
            g_debug("config: error reading %s,%s: %s",group,key,err->message);
        }
        g_error_free(err);
        return default_bool;
    }
    else
        return bool_to_return;
}

gchar *hadConfigGetString(gchar *group, gchar *key, gchar *default_string)
{
    GError *err = NULL;
    gchar *string_to_return;

    if(!key || !group || !strlen(key) || !strlen(group) || !had_key_file)
    {
        g_debug("config: error in argument");
        return default_string;
    }

    string_to_return = g_key_file_get_string(had_key_file, group, key, &err);
    if(err)
    {
        if(err->code == G_KEY_FILE_ERROR_KEY_NOT_FOUND || 
            err->code == G_KEY_FILE_ERROR_GROUP_NOT_FOUND) // key not found so insert default value
        {
            g_key_file_set_string(had_key_file, group, key, default_string);
        }
        else
        {
            g_debug("config: error reading %s,%s: %s",group,key,err->message);
        }
        g_error_free(err);
        return default_string;
    }
    else
        return default_string;
}

void hadConfigSetInt(gchar *group, gchar *key, int int_to_set)
{
    if(!key || !group || !strlen(key) || !strlen(group) || !had_key_file)
    {
        g_debug("config: error in argument");
        return;
    }
    g_key_file_set_integer(had_key_file, group, key, int_to_set);
}

void hadConfigSetBool(gchar *group, gchar *key, int bool_to_set)
{
    if(!key || !group || !strlen(key) || !strlen(group) || !had_key_file)
    {
        g_debug("config: error in argument");
        return;
    }
    g_key_file_set_boolean(had_key_file, group, key, bool_to_set);
}

void hadConfigSetString(gchar *group, gchar *key, gchar *string_to_set)
{
    if(!key || !group || !strlen(key) || !strlen(group) || !had_key_file)
    {
        g_debug("config: error in argument");
        return;
    }
    g_key_file_set_string(had_key_file, group, key, string_to_set);
}

void hadConfigInit(void)
{
    GError *err = NULL;

    had_key_file = g_key_file_new();
    if(!g_key_file_load_from_dirs(had_key_file, "had.conf", config_search_dirs,
        &found_path, G_KEY_FILE_KEEP_COMMENTS, &err))
    {
        g_debug("config: error finding file %s", err->message);
        g_error_free(err);
    }
    else
    {
        g_debug("config: loaded %s", found_path);
    }
    return;
}

void hadConfigUnload(void)
{
    FILE *fp;
    gsize data_length;
    gchar *data;
    
    if(had_key_file)
    {
        fp = fopen(found_path,"w");
        if(fp)
        {
            data = g_key_file_to_data(had_key_file, &data_length, NULL);
            if(fwrite(data,1, data_length, fp) != data_length)
                g_debug("config: wrote less bytes than necesarry");
            fclose(fp);
            g_free(data);
        }
        g_key_file_free(had_key_file);
        if(found_path)
            g_free(found_path);
    }
    return;
}

