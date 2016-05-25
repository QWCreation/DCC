#ifndef _PROGRAM_DCC_H
#define _PROGRAM_DCC_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <limits.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <linux/limits.h>
#include "db_manage.h"

// SYSTEM ENV
#define SH_START						"SH_START"
#define	EXPORT_WORK_DIR					"EXPORT_WORK_DIR"
#define	EXPORT_CONFIG_DIR				"EXPORT_CONFIG_DIR"
#define	EXPORT_LOG_DIR					"EXPORT_LOG_DIR"

//default paraments
const char *default_work_dir = "/prod/dsg/run/bin";
const char *default_config_dir = ".";
const char *default_log_dir = "/prod/dsg/run/log";

const char *mode_string[] = {"UDP", "TCP"};

// global paraments
int  g_dcc_hdl = 0;
char *dc_ip;
char *dc_user;
char *dc_passwd;

char *db_ip;
char *db_user;
char *db_passwd;

unsigned short dc_port = 0;
unsigned short local_port = 0;
char *work_dir;
char *config_dir;
char *log_dir;

int mode = 1;
LogLevel log_lev = LL_NOTICE;

int system_init();
void receive_data( struct dc_msg *msg );
void connect_dc(void);
void dis_connect_dc(void);
void menu_loop(void);

#endif
