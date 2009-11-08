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
 * \file	modules.c
 * \brief	module handling
 * \author	Bjoern Biesenbach <bjoern at bjoern-b dot de>
*/

#include "modules.h"
#include "config.h"

static gchar *buildModulePath(gchar *name)
{
    static gchar path[1024];

    g_sprintf(path,"./plugins/%s/lib%s.la",name,name);
    if(fileExists(path))
        return path;
    else
    {
        g_debug("modules.c: %s does not exist",path);
        return NULL;
    }
}

static GList *createModuleListFromString(gchar **module_names)
{
    struct _had_module *module; 
    int i;
    GList *module_list;

    module_list = NULL;

    if(module_names)
    {
        while(module_names[i])
        {
            module = g_malloc0(sizeof(struct _had_module));
            g_strlcpy(module->name, module_names[i],sizeof(module->name));
            module->loaded = FALSE;
            g_sprintf(module->full_name,"mod_%s",module->name);
            module->depencies = hadConfigGetStringList(module->full_name, "deps");
            module_list = g_list_prepend(module_list, module);
            g_debug("modules: %s to load", module_names[i]);
            i++;
        }
    }
    g_strfreev(module_names);
    return module_list;
}

struct _had_module *getModuleFromName(gchar *name)
{
    guint num_modules;
    guint i;
    struct _had_module *module;

    num_modules = g_list_length(had_modules);

    for(i=0;i<num_modules;i++)
    {  
        module = g_list_nth_data(had_modules, i);
        if(!g_strcmp0(module->name, name))
            return module;
    }
    return NULL;
}



void loadModule(gpointer data, gpointer user_data)
{
    struct _had_module *module;
    gchar *module_path;
    int i;
    
    if(!data)
        return;

    module = (struct _had_module*)data;

    if(module->loaded)
        return;

    g_debug("modules.c: trying to load %s", module->name);
    
    if(module->depencies)
    {
        while(module->depencies[i])
        {
            g_debug("modules.c: trying to resolve depency %s for module %s",
                module->depencies[i], module->name);
            loadModule(getModuleFromName(module->depencies[i]), NULL);
            i++;
        }
    }
    module_path = buildModulePath(module->name);
    if(module_path)
    {
        module->module = g_module_open(module_path, G_MODULE_BIND_LAZY);
        return;
    }
}

gboolean loadModules()
{
    g_list_foreach(had_modules, loadModule, NULL);
}

void hadModulesInit(void)
{
    had_modules = createModuleListFromString(hadConfigGetModuleNamesToLoad());
    loadModules();
}

