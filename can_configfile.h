#ifndef __CONFIGFILE_H__
#define __CONFIGFILE_H__


char *can_config_get_node_name(int address); // static
char *can_config_get_relais_name(int address, int relais); // static
int can_config_get_show_in_ui(int address);

#endif

