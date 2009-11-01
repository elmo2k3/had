#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <signal.h>
#include "had.h"
#include "daemon.h"

int daemonize(void)
{
	pid_t pid;
	FILE *pid_file;
	if(fileExists(config.pid_file))
	{
		printf("%s exists. Maybe had is still running?\n",config.pid_file);
		exit(EXIT_FAILURE);
	}

	if(( pid = fork() ) != 0 )
		exit(EX_OSERR);

	if(setsid() < 0)
		exit(EX_OSERR);
	
	signal(SIGHUP, SIG_IGN);
	
	if(( pid = fork() ) != 0 )
		exit(EX_OSERR);

	umask(0);
	
	signal(SIGHUP, (void*)hadSignalHandler);

	pid_file = fopen(config.pid_file,"w");
	if(!pid_file)
	{
		printf("Could not write %s\n",config.pid_file);
		exit(EXIT_FAILURE);
	}
	fprintf(pid_file,"%d\n",(int)getpid());
	fclose(pid_file);

	if(config.verbosity >= 9)
		printf("My PID is %d\n",(int)getpid());


	
	freopen(config.logfile, "a", stdout);
	freopen(config.logfile, "a", stderr);

	/* write into file without buffer */
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
}

int killDaemon(int signal)
{
	FILE *pid_file = fopen(config.pid_file,"r");

	int pid;

	if(!pid_file)
	{
		printf("Could not open %s. Maybe had is not running?\n",config.pid_file);
		return(EXIT_FAILURE);
	}
	fscanf(pid_file,"%d",&pid);
	fclose(pid_file);

	kill(pid,signal);
	return EXIT_SUCCESS;
}

void hadSignalHandler(int signal)
{
//	if(signal == SIGTERM || signal == SIGINT)
//	{
//		if(config.daemonize)
//			unlink(config.pid_file);
//		networkThreadStop();
//		writeStateFile(config.statefile);
//		verbose_printf(0,"Shutting down\n");
//		exit(EXIT_SUCCESS);
//	}
//	else if(signal == SIGHUP)
//	{
//		struct _config configTemp;
//		memcpy(&configTemp, &config, sizeof(config));
//
//		verbose_printf(0,"Config reloaded\n");
//		loadConfig(HAD_CONFIG_FILE);
//
//		// check if ledmatrix should be turned off
//		if(configTemp.led_matrix_activated != config.led_matrix_activated)
//		{
//			if(config.led_matrix_activated)
//			{
//				if(!ledIsRunning() && (hadState.relais_state & 4))
//				{
//					pthread_create(&threads[2],NULL,(void*)&ledMatrixThread,NULL);
//					pthread_detach(threads[2]);
//				}
//				hadState.ledmatrix_user_activated = 1;
//			}
//			else
//				stopLedMatrixThread();
//		}
//
//	}
}
