#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define CONFIG_FILE_NAME "had_can.conf"

GKeyFile *key_file = NULL;

static int configfile_init()
{
    const gchar *search_dirs[3] = {"/etc",".",NULL};

    if(key_file)
        return 2;

    key_file = g_key_file_new();

    if(!g_key_file_load_from_dirs(key_file,CONFIG_FILE_NAME,search_dirs,NULL,0,NULL))
    {
        g_key_file_free(key_file);
        key_file = NULL;
        return 3;
    }

    return 0;
}

char *can_config_get_node_name(int address) // static
{
    static char node_name[255];
    char group_name[255];
    char key_name[255];
    char *temp_name;

    node_name[0] = '\0';
    configfile_init();

    if(!key_file)
        return NULL;

    sprintf(group_name,"node%d",address);
    temp_name = g_key_file_get_string(key_file,group_name, "name", NULL);

    if(temp_name)
    {
        strncpy(node_name,temp_name,sizeof(node_name));
        g_free(temp_name);
    }

    return node_name;
}

char *can_config_get_relais_name(int address, int relais) // static
{
    static char node_name[255];
    char group_name[255];
    char key_name[255];
    char *temp_name;

    node_name[0] = '\0';
    configfile_init();

    if(!key_file)
        return NULL;

    sprintf(group_name,"node%d",address);
    sprintf(key_name,"relais%d",relais);
    temp_name = g_key_file_get_string(key_file,group_name, key_name, NULL);

    if(temp_name)
    {
        strncpy(node_name,temp_name,sizeof(node_name));
        g_free(temp_name);
    }

    return node_name;
}

//int getDbNum(int module, int sensor)
//{
//    char key_name[255];
//
//    configfile_init();
//
//    if(!key_file)
//        return -1;
//        
//    sprintf(key_name,"sensor_%d_%d_db",module, sensor);
//
//    return g_key_file_get_integer(key_file,"sensors",key_name,NULL);
//}
//
//char *getDbValue(char *what, int db_num, int module, int sensor)
//{
//    static char value[255];
//    char key_name[255];
//    char *temp_name;
//
//    configfile_init();
//
//    if(!key_file || !what)
//        return NULL;
//        
//    sprintf(key_name,"database_%d_%s",db_num,what);
//
//    temp_name = g_key_file_get_string(key_file,"databases",key_name,NULL);
//    if(temp_name)
//    {
//        strncpy(value,temp_name,sizeof(value));
//        g_free(temp_name);
//    }
//    else
//        return NULL;
//
//    return value;
//}

