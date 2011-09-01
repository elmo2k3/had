/*
 * Copyright (C) 2007-2010 Bjoern Biesenbach <bjoern@bjoern-b.de>
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
 * \file    had.c
 * \brief   main file for had
 * \author  Bjoern Biesenbach <bjoern at bjoern-b dot de>
*/

#include <stdio.h>
#include <unistd.h>
#include <sys/signal.h>
#include <mysql/mysql.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sysexits.h>
#include <glib.h>

#include "base_station.h"
#include "database.h"
#include "had.h"
#include "mpd.h"
#include "listen.h"
#include "client.h"
#include "config.h"
#include "led_routines.h"
#include "sms.h"
#include "version.h"
#include "hr20.h"
#include "rfid_tag_reader.h"
#include "statefile.h"
#include "security.h"
#include "voltageboard.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "had"

/*! thread variable array for network, mpd and ledmatrix */
GMainLoop *had_main_loop;

/*! big array for the last measured temperatures */
int16_t lastTemperature[9][9];
/*! big array for the last measured voltages at the rf temperature modules */
int16_t lastVoltage[9];

time_t time_had_started;

static int killDaemon(int signal);
static void hadSignalHandler(int signal);
static void had_check_parameters(int argc, char **argv);
static void had_check_daemonize(void);
static void had_print_version(void);
static void had_load_state(void);
static void had_init_hr20(void);
static void had_init_base_station(void);

int main(int argc, char* argv[])
{
    signal(SIGINT, (void*)hadSignalHandler);
    signal(SIGTERM, (void*)hadSignalHandler);
    struct RfidTagReader *tag_reader;
    GError *error = NULL;

    g_thread_init(NULL);

    time_had_started = time(NULL);

    ledMatrixInit();
    readConfig();
    had_check_parameters(argc,argv);
    had_check_daemonize();
    g_log_set_default_handler(had_log_handler, NULL);
    had_print_version();
    had_load_state();
    client_manager_init();
    listen_global_init(&error);

    if(error) {
        if(config.daemonize) unlink(config.pid_file);
        g_error("%s",error->message);
        g_error_free(error);
        return EXIT_FAILURE;
    }
    ledMatrixStart();
    mpdInit();
    had_init_hr20();
    had_init_base_station();
    voltageboard_init();

    if(config.rfid_activated) {
        tag_reader = rfid_tag_reader_new(config.rfid_port);
        if(tag_reader == NULL) {
            if(config.daemonize) unlink(config.pid_file);
            g_error("Error opening %s for tagreader",config.rfid_port);
            return EXIT_FAILURE;
        }
        rfid_tag_reader_set_callback(tag_reader, security_tag_handler);
    }

    had_main_loop = g_main_loop_new(NULL,FALSE);

    g_main_loop_run(had_main_loop);

    listen_global_finish();
    return 0;
}

static void hadSignalHandler(int signal)
{
    if(signal == SIGTERM || signal == SIGINT)
    {
        g_main_loop_quit(had_main_loop);
        if(config.daemonize)
            unlink(config.pid_file);
        //networkThreadStop();
        writeStateFile(config.statefile);
        g_message("Shutting down");
    }
    else if(signal == SIGHUP)
    {
        struct _config configTemp;
        memcpy(&configTemp, &config, sizeof(config));

        g_message("Config reloaded");
        readConfig();
    }
}

static int killDaemon(int signal)
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

static void had_check_parameters(int argc, char **argv)
{
    if(argc > 2)
    {
        printUsage();
        exit(EXIT_FAILURE);
    }
    
    if(argc >1)
    {
        if(!strcmp(argv[1],"--help"))
        {
            printUsage();
            exit(EXIT_SUCCESS);
        }

        if(!strcmp(argv[1],"-k"))
        {
            exit(killDaemon(SIGTERM));
        }

        /* reload config */
        if(!strcmp(argv[1],"-r"))
            exit(killDaemon(SIGHUP));
    }
}

static void had_check_daemonize(void)
{
    pid_t pid;
    FILE *pid_file;
    
//  freopen("/dev/null", "a", stderr);

    if(config.daemonize)
    {
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

        freopen(config.logfile, "a", stdout);
//      freopen("/dev/null", "a", stderr);

        /* write into file without buffer */
        setvbuf(stdout, NULL, _IONBF, 0);
        setvbuf(stderr, NULL, _IONBF, 0);

    }
}

static void had_print_version(void)
{
#ifdef VERSION
    g_message("had %s started",VERSION);
#else
    g_message("had started");
#endif
}

static void had_load_state(void)
{
    if(loadStateFile(config.statefile))
    {
        g_debug("Statefile successfully read");
        //relaisP.port = hadState.relais_state;
    }
    else
    {
        g_debug("Statefile could not be read, using default values");
        memset(&hadState, 0, sizeof(hadState));
//      memset(&relaisP, 0, sizeof(relaisP));
        hadState.scrobbler_user_activated = config.scrobbler_activated;
        hadState.ledmatrix_user_activated = config.led_matrix_activated;
        hadState.beep_on_door_opened = 1;
        hadState.beep_on_window_left_open = 1;
    }
}

static void had_init_hr20(void)
{   
    hr20Init();
}

static void had_init_base_station(void)
{
    int16_t celsius,decicelsius;
    base_station_init();
    
    glcdP.backlight = 1;
#ifdef _NO_FFTW3_
    getLastTemperature(4,0,&celsius,&decicelsius);
#else
    getLastTemperature(3,1,&celsius,&decicelsius);
#endif
    lastTemperature[3][3] = (int16_t)celsius*10;

#ifdef _NO_FFTW3_
    getLastTemperature(4,1,&celsius,&decicelsius);
#else
    getLastTemperature(3,0,&celsius,&decicelsius);
#endif
    lastTemperature[3][0] = (int16_t)celsius*10;
    sendBaseLcdText("had wurde gestartet ... ");
}

