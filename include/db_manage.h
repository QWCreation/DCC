#ifndef _DB_MANAGE_H 
#define _DB_MANAGE_H

#define  DEBUG 

#include <stdio.h>
#include "mysql.h"
#include "general.h"
#include "dcc.h"

#define CHAR_ARR_LEN				256

#define DB_DATA_LEN 				2000
#define DB_IMEI_LEN 				50
#define DB_LOCAL_LEN 				50
#define DB_MEMO_LEN 				200
#define DB_TAG_LEN 					50
#define DB_TRAN_LEN 				50


#define DB_FOUND					0
#define DB_MANY_FOUND				9
#define DB_NOT_FOUND				1
#define DB_ABEND					2
#define DB_NOMAL_END				3
#define DB_NOT_INSERT				4

#define  ONE_ROWS_ONLY				1
#define  SEL_TRANS_FIELD_CNT       	1
#define  RES_TRANS_FIELD_CNT       	5
#define  INS_DATA_FIELD_CNT        	3
#define  INS_TMPER_FIELD_CNT       	6
#define  UPD_TRANS_FIELD_CNT		2

#define CTRL_SHOW							1	/*00000001			1:show detailed informations on the destop when recive messages*/
#define CTRL_CHOSREV 						2	/*00000010			2:recive the data for all of transducers*/
#define CTRL_RECORD	 						4	/*00000100			4:recive the data only which in the database's table <WORKHEAT.TRANSDUCER>*/

#define SQL_SELECT_TRANSDUCER		"SELECT TRANSDUCER_ID,LOCATION_ID,TAG_NO,TAG,FLAG FROM alarmnotify.transducer WHERE IMEI = ?"    /*  AND TAG=?" */
#define SQL_INSERT_DATA				"INSERT INTO alarmnotify.data VALUES( null, ?, ?, ?)"
#define SQL_UPDATE_TRANSDUCER_FLAG	"UPDATE alarmnotify.transducer SET flag = ? WHERE IMEI = ?"

typedef struct {
	char		imei[DB_IMEI_LEN+1];
	char		data[DB_DATA_LEN+1];
	MYSQL_TIME	time;
} T_DATA,*pT_DATA;

typedef struct {
	char		transducer_id[DB_TRAN_LEN+1];
	char		location_id[DB_LOCAL_LEN+1];
	int			tag_no;
	char		tag[DB_TAG_LEN+1];
	char		imei[DB_IMEI_LEN+1];
	int			flag;
	char 		memo[DB_MEMO_LEN+1];
	MYSQL_TIME	upd_date;
} T_TRANSDUCER,*pT_TRANSDUCER;

typedef struct {
	char		transducer_id[DB_TRAN_LEN+1];
	char		location_id[DB_LOCAL_LEN+1];
	int			tag_no;
	MYSQL_TIME	recorddate;
	float		temperature;
	int			flag;
} T_TEMPERATURE,*pTEMPERATURE;

typedef struct{
	char  msg_body[MSG_BODY_SIZE+1];
	char  msg_tag[MSG_TAG_SIZE+1];
	float temperature;
} MYDCCMSG,*pMYDCCMSG;

int init_connect_db( const int status );
int query_sel_sql( const char* stringSql);
int query_chg_sql( const char* stringSql);
int set_query_string( char *SQL_STMTS, const char *fmt,...);
int db_main_process( const struct dc_msg *msg );
int update_transducer( const int flag, const struct dc_msg *msg );

void free_result();
int dis_connect_db();

#endif
