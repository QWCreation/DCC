#ifndef _GENERAL_H 
#define _GENERAL_H
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
/* for log output */
#include "log.h"

#define BUFS 4096
#define LOGMSGLN 9
enum erl {DBML=0,INFL,SYSL,DBEL,WANL,DBGL};

#define DDBML DBML,__FILE__,__LINE__		/* level 0:Program output massage */
#define DINFL INFL,__FILE__,__LINE__		/* level 1:Program output massage */
#define DSYSL SYSL,__FILE__,__LINE__		/* level 2:System error           */
#define DDBEL DBEL,__FILE__,__LINE__		/* level 3:DB error               */
#define DWANL WANL,__FILE__,__LINE__		/* level 4:Warnning               */
#define DDBGL DBGL,__FILE__,__LINE__		/* level 5:Debug massage          */

#define NOMAL_END						0
#define ABNOMAL_END						9
#define ABEND_END						-1

#define TAG_END							8
#define TAG_START						5
#define TEMPERATURE_INDEX				11

#define MSG_BODY_SIZE					2000
#define MSG_TAG_SIZE					12

#define MSG_DSG							0		/*message is dsg's type*/
#define MSG_NOT_DSG1					1		/*message is dsg's type and the massage's lenth less than tag_end*/
#define MSG_NOT_DSG2					2		/*message is dsg's type and the massage's lenth less than temperature*/

int set_envionment ( const char *filename );
int print_msg( int lev,const char *,int,const char *,... );
//void print_menu();
void print_count( int dtype );



#endif

