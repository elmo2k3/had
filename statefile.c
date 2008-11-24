#include <stdio.h>
#include "had.h"

int writeStateFile(char *filename)
{
	FILE *state_file = fopen(filename,"w");

	if(state_file)
	{
		fwrite(&hadState, sizeof(hadState), 1, state_file);
		fclose(state_file);
		return 1;
	}
	else
	{
		verbose_printf(0,"Could not write statefile!\n");
		return 0;
	}
}

int loadStateFile(char *filename)
{
	FILE *state_file = fopen(filename,"r");

	if(state_file)
	{
		fread(&hadState, sizeof(hadState), 1, state_file);
		fclose(state_file);
		return 1;
	}
	else
	{
		verbose_printf(0,"Count not read statefile!\n");
		return 0;
	}
}
	
	
