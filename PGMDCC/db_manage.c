#include "db_manage.h"

// global paraments
extern char *db_ip;
extern char *db_user;
extern char *db_passwd;

//MYSQL my_connection;
static MYSQL         *conn_ptr;

static MYSQL_STMT    *stmt_tran_ptr = NULL;
static MYSQL_STMT	 *stmt_tran_update_ptr = NULL;
static MYSQL_STMT    *stmt_data_ptr = NULL;

static T_TRANSDUCER  st_trsd;
static T_TRANSDUCER  st_trsd_update;
static T_DATA        st_data;

static MYSQL_BIND    slt_trans_bind[SEL_TRANS_FIELD_CNT];
static unsigned long slt_trans_len[SEL_TRANS_FIELD_CNT];

static MYSQL_BIND	 upd_trans_bind[UPD_TRANS_FIELD_CNT];
static unsigned long upd_trans_len[UPD_TRANS_FIELD_CNT];

static MYSQL_BIND    ins_data_bind[INS_DATA_FIELD_CNT];
static unsigned long ins_data_len[INS_DATA_FIELD_CNT];

static int init_stmt_transducer();
static int select_from_transducer( const char *tagname );
static int init_stmt_data();
static int init_stmt_trans_update();
static int update_transducer( int flag, const char *tagname );
static int my_chg_stmt_execute( MYSQL_STMT* *stmt );
static void set_currenttime_sql(MYSQL_TIME *sqltm);
static int cover_msg_struct( const struct dc_msg *msg, pMYDCCMSG dccmsg );
static void mem_reset_zero();
static int db_start_status = 0;
static int close_statement_db( MYSQL_STMT* *stmt );

int db_main_process( const struct dc_msg *msg )
{
	int rc = DB_FOUND;			//#define DB_FOUND				1

	mem_reset_zero();
	
	MYDCCMSG  dccmsg;
	rc = cover_msg_struct( msg, &dccmsg );
	
	strcpy( st_trsd.imei, msg->imei );

	rc = select_from_transducer( msg->imei );
	// return err when db error occured;
	if( rc == DB_ABEND ){
		return rc;
	}
	else if ( rc == DB_NOT_FOUND ){		/* the No.of imei not in transducer */
		return rc;
	}
	
	strcpy( st_data.imei, msg->imei );	
	ins_data_len[0] = strlen( st_data.imei );
	strcpy( st_data.data, dccmsg.msg_body );
	ins_data_len[1] = strlen( st_data.data ) - 1;	/*ommit the last bit of 0 */
	set_currenttime_sql( &st_data.time );

	LOG_NOTICE("msg_body = %s", st_data.data);
	if( ins_data_len[1] <= 20){
		rc = my_chg_stmt_execute( &stmt_data_ptr);	/* save data only when it's the alarm data sent by PLC */
	}
	else{
		rc = DB_NOT_INSERT;
	}
	return rc ;
}

/*===========================================================================*/
/*  DESCRIPTION :      									                     */
/* 		Connect to DB and init the statments						 		 */
/* 	Return      :					  										 */
/*		DB_ABEND    : Error occurt											 */
/*		DB_NOMAL_END: Execute successful									 */
/*																			 */
/* 	Parameter   :					  										 */
/* 						  													 */
/*===========================================================================*/
int init_connect_db( const int status )
{
	if( (db_start_status > 0) && (db_start_status != status) ){
		LOG_NOTICE( "DB start settiing has been reset! Reconnect to DB!" );
	}

	if( !conn_ptr )
	{
		conn_ptr = mysql_init(NULL);
		conn_ptr = mysql_real_connect( conn_ptr, db_ip, db_user, 
								db_passwd, "",0,NULL,CLIENT_FOUND_ROWS) ;

		if( !conn_ptr )
		{
			if ( mysql_errno( conn_ptr ) )
			{
				LOG_ERROR( "DB Connection error %d: %s",
							   mysql_errno(conn_ptr),mysql_error(conn_ptr) );
			}
			return DB_ABEND;
		}
	}

	if( init_stmt_transducer() != DB_NOMAL_END ){			
		return DB_ABEND;
	}

	if( status & CTRL_RECORD )
	{
		if( init_stmt_data() != DB_NOMAL_END ){
			return DB_ABEND;
		}
	}
	else{
		close_statement_db( &stmt_data_ptr );
	}
	
	db_start_status = status;
	return DB_NOMAL_END;
}

/*===========================================================================*/
/*  DESCRIPTION		: disconnect the  connection of DB		                 */
/* 						  													 */
/*===========================================================================*/
int dis_connect_db()
{
	int rc = close_statement_db( &stmt_tran_ptr );
	if( rc ){
		return rc;
	}

	rc = close_statement_db( &stmt_data_ptr );
	if( rc ){
		return rc;
	}

	mysql_close( conn_ptr );	
	conn_ptr = NULL;
	LOG_NOTICE( "DB disconnected successfuly!" );

	return rc;
}

int close_statement_db( MYSQL_STMT* *stmt )
{
	int rc = 0;
	if( *stmt ){
		rc = mysql_stmt_close( *stmt );
		if ( rc )
		{
		  	LOG_ERROR( "Failed while closing the statement. errno: %d , msg:%s",
		  	 		   rc, mysql_stmt_error( *stmt ));
			return rc;
		}
		*stmt = NULL;
	}
	return rc;
}

/*===========================================================================*/
/*  DESCRIPTION :      									                     */
/* 		prepare the pretreatment for select from table transducer			 */
/* 	Return      :					  										 */
/*		DB_ABEND    : Error occurt											 */
/*		DB_NOMAL_END: Execute successful									 */
/*																			 */
/* 	Parameter   :					  										 */
/* 						  													 */
/*===========================================================================*/
int init_stmt_transducer()
{
	LOG_DEBUG( "Init tansducer statement process started..." );
	if( !stmt_tran_ptr ){
		stmt_tran_ptr = mysql_stmt_init( conn_ptr );
		if ( !stmt_tran_ptr )
		{
			LOG_ERROR( "SQL STMT init SQL_SELECT_TRANSDUCER faild!" );
			return DB_ABEND;
		}
		int rc = mysql_stmt_prepare( stmt_tran_ptr, SQL_SELECT_TRANSDUCER, strlen(SQL_SELECT_TRANSDUCER) );
		if ( rc )
		{
			LOG_ERROR( "MYSQL Prepare SQL_SELECT_TRANSDUCER failed! errno:%d msg:%s",
		  						rc, mysql_stmt_error(stmt_tran_ptr) );
			return DB_ABEND;
		}
 
		/* Get the parameter count from the statement */
		int param_count= mysql_stmt_param_count(stmt_tran_ptr);
				 
		if ( param_count != SEL_TRANS_FIELD_CNT ) /* validate parameter count */
		{
			LOG_ERROR( "Invalid parameter's count(%d) for SQL_SELECT_TRANSDUCER!", param_count );
			return DB_ABEND;
		}
		
		/* Bind the data for all 1 parameters */		 
		memset( slt_trans_bind, 0, sizeof(slt_trans_bind) );
		
		slt_trans_bind[0].buffer_type = MYSQL_TYPE_VAR_STRING;
		slt_trans_bind[0].buffer= (char *)st_trsd.imei;
		slt_trans_bind[0].buffer_length= DB_IMEI_LEN;
		slt_trans_bind[0].is_null = 0;
		slt_trans_bind[0].length = &slt_trans_len[0];
		 
		/* Bind the buffers */
		rc = mysql_stmt_bind_param( stmt_tran_ptr, slt_trans_bind );
		if ( rc )
		{
			LOG_ERROR( "MYSQL bind SQL_SELECT_TRANSDUCER parameters failed! errno:%d msg:%s",
							 rc, mysql_stmt_error( stmt_tran_ptr) );
			return DB_ABEND;
		}
	}
	return DB_NOMAL_END;
}

/*===========================================================================*/
/*  DESCRIPTION :      									                     */
/* 		select the reviced imei from table transducer						 */
/* 	Return      :					  										 */
/*		DB_ABEND    : Error occurt											 */
/*		DB_FOUND    : Data is conformable									 */
/*		DB_NOT_FOUND: Data is not  successful								 */
/*																			 */
/* 	Parameter   :					  										 */
/* 						  													 */
/*===========================================================================*/
int select_from_transducer( const char *tagname )
{
	slt_trans_len[0] = strlen( st_trsd.imei);
	//slt_trans_len[1] = strlen( st_trsd.tag);	
	
	MYSQL_BIND    res_trans_bind[RES_TRANS_FIELD_CNT];
	MYSQL_RES     *prepare_meta_result;
	
	//MYSQL_TIME    ts;
	my_bool       is_null[RES_TRANS_FIELD_CNT];
	unsigned long res_trans_len[RES_TRANS_FIELD_CNT];
	
	/* Fetch result set meta information */
	prepare_meta_result = mysql_stmt_result_metadata(stmt_tran_ptr );
	if (!prepare_meta_result)
	{
		LOG_ERROR( "MYSQL stmt resultmetadata returned no meta information. errno:%d msg:%s",
						   mysql_stmt_errno( stmt_tran_ptr ),  mysql_stmt_error( stmt_tran_ptr ) );
		return DB_ABEND;
	}

	/* Execute the SELECT query */
	int rc = mysql_stmt_execute( stmt_tran_ptr );
	if ( rc )
	{
		LOG_ERROR( "SQL SQL_SELECT_TRANSDUCER STMT execute faild! errno:%d:,msg:%s",
				           rc,  mysql_stmt_error( stmt_tran_ptr ) );		
		mysql_free_result(prepare_meta_result);
		return DB_ABEND;
	}

	/* Bind the result buffers for all columns before fetching them */
	memset(res_trans_bind, 0, sizeof(res_trans_bind));
	
	/* TRANSDUCER_ID COLUMN */
	res_trans_bind[0].buffer_type= MYSQL_TYPE_VAR_STRING;
	res_trans_bind[0].buffer= (char *)st_trsd.transducer_id;
	res_trans_bind[0].buffer_length= sizeof(st_trsd.transducer_id);
	res_trans_bind[0].is_null= &is_null[0];
	res_trans_bind[0].length= &res_trans_len[0];
	 
	/* LOCATION_ID COLUMN */
	res_trans_bind[1].buffer_type= MYSQL_TYPE_VAR_STRING;
	res_trans_bind[1].buffer= (char *)st_trsd.location_id;
	res_trans_bind[1].buffer_length= sizeof(st_trsd.location_id);
	res_trans_bind[1].is_null= &is_null[1];
	res_trans_bind[1].length= &res_trans_len[1];

	/* TAG_NO COLUMN */
	res_trans_bind[2].buffer_type= MYSQL_TYPE_LONG;
	res_trans_bind[2].buffer= (char *)&st_trsd.tag_no;
	res_trans_bind[2].is_null= &is_null[2];
	res_trans_bind[2].length= &res_trans_len[2];

	res_trans_bind[3].buffer_type= MYSQL_TYPE_VAR_STRING;
	res_trans_bind[3].buffer= (char *)st_trsd.tag;
	res_trans_bind[3].buffer_length= sizeof(st_trsd.tag);
	res_trans_bind[3].is_null= &is_null[3];
	res_trans_bind[3].length= &res_trans_len[3];

	/* FLAG COLUMN */
	res_trans_bind[4].buffer_type= MYSQL_TYPE_LONG;
	res_trans_bind[4].buffer= (char *)&st_trsd.flag;
	res_trans_bind[4].is_null= &is_null[4];
	res_trans_bind[4].length= &res_trans_len[4];
	 
	/* Bind the result buffers */
	rc = mysql_stmt_bind_result(stmt_tran_ptr, res_trans_bind);
	if ( rc )
	{
		LOG_ERROR( "MYSQL bind TANSDUCER's result parameters failed! errno:%d msg:%s",
							 rc, mysql_stmt_error( stmt_tran_ptr) );
		mysql_free_result(prepare_meta_result);
		return DB_ABEND;
	}
	 
	/* Now buffer all results to client */
	rc = mysql_stmt_store_result(stmt_tran_ptr);
	if ( rc  )
	{
		LOG_ERROR( "MYSQL store TANSDUCER's result parameters failed! errno:%d msg:%s",
							 rc, mysql_stmt_error( stmt_tran_ptr) );
		mysql_free_result(prepare_meta_result);
		return DB_ABEND;
	}

	int found_rows = mysql_stmt_num_rows( stmt_tran_ptr );
	if( found_rows < ONE_ROWS_ONLY ){
	 	LOG_ERROR( "MYSQL store TANSDUCER's not enough. Result(%d) rows has been found.",
			found_rows );
		mysql_free_result(prepare_meta_result);
		return DB_NOT_FOUND;
	}
	
	while ( !mysql_stmt_fetch(stmt_tran_ptr) )
	{
		if ( strcmp( st_trsd.imei, tagname ) == 0 ){
			mysql_free_result( prepare_meta_result );
			return  DB_FOUND;
		}
	}
	/* Free the prepared result metadata */
	mysql_free_result( prepare_meta_result );

	return  DB_MANY_FOUND;
}


/*===========================================================================*/
/*  DESCRIPTION :      									                     */
/* 		prepare the pretreatment for insert into table data	 				 */
/* 	Return      :					  										 */
/*		DB_ABEND    : Error occurt											 */
/*		DB_NOMAL_END: Execute successful									 */
/*																			 */
/* 	Parameter   :					  										 */
/* 						  													 */
/*===========================================================================*/
int init_stmt_data()
{
	LOG_DEBUG( "Init data statement process started..." );
	if( !stmt_data_ptr ){
		stmt_data_ptr = mysql_stmt_init( conn_ptr );
		if ( !stmt_data_ptr )
		{
			LOG_ERROR( "SQL STMT init SQL_INSERT_DATA faild!" );
		  	return DB_ABEND;
		}
		int rc = mysql_stmt_prepare(stmt_data_ptr, SQL_INSERT_DATA, strlen(SQL_INSERT_DATA));
		if ( rc )
		{
			LOG_ERROR( "MYSQL Prepare SQL_INSERT_DATA failed! errno:%d msg:%s",
		  						rc, mysql_stmt_error(stmt_data_ptr));
			return DB_ABEND;
		}
		
		/* Get the parameter count from the statement */
		int param_count= mysql_stmt_param_count( stmt_data_ptr );

		if ( param_count != INS_DATA_FIELD_CNT ) /* validate parameter count */
		{
			LOG_ERROR( "Invalid parameter's count(%d) for SQL_INSERT_DATA!", param_count );
			return DB_ABEND;
		}
		
		/* Bind the data for all 3 parameters */		 
		memset( ins_data_bind, 0, sizeof(ins_data_bind) );
		
		ins_data_bind[0].buffer_type = MYSQL_TYPE_VAR_STRING;
		ins_data_bind[0].buffer= (char *)st_data.imei;
		ins_data_bind[0].buffer_length= DB_IMEI_LEN;
		ins_data_bind[0].is_null = 0;
		ins_data_bind[0].length = &ins_data_len[0];

		ins_data_bind[1].buffer_type = MYSQL_TYPE_VAR_STRING;
		ins_data_bind[1].buffer= (char *)st_data.data;
		ins_data_bind[1].buffer_length= DB_DATA_LEN;
		ins_data_bind[1].is_null = 0;
		ins_data_bind[1].length = &ins_data_len[1];

		ins_data_bind[2].buffer_type = MYSQL_TYPE_DATETIME;
		ins_data_bind[2].buffer= (char *)&st_data.time;
		ins_data_bind[2].is_null = 0;
		ins_data_bind[2].length = &ins_data_len[2];

		/* Bind the buffers */
		rc = mysql_stmt_bind_param( stmt_data_ptr, ins_data_bind );
		if ( rc )
		{
			LOG_ERROR( "MYSQL bind SQL_INSERT_DATA parameters failed! errno:%d msg:%s",
							 rc, mysql_stmt_error( stmt_data_ptr) );
			return DB_ABEND;
		}
	}
	return DB_NOMAL_END;
}

/*===========================================================================*/
/*  DESCRIPTION :      									                     */
/* 		prepare the statement for updating table transducer's flag			 */
/* 		(2016/5/25)					 	 									 */
/* 	Return      :					  										 */
/*		DB_ABEND    : Error occured											 */
/*		DB_NOMAL_END: Execute successful									 */
/*																			 */
/* 	Parameter   :					  										 */
/* 									 	 									 */
/*===========================================================================*/
int init_stmt_trans_update()
{
	LOG_DEBUG( "Init tansducer flag update statement process started..." );
	if( !stmt_tran_update_ptr ){
		stmt_tran_update_ptr = mysql_stmt_init( conn_ptr );
		if ( !stmt_tran_update_ptr )
		{
			LOG_ERROR( "SQL STMT init SQL_UPDATE_TRANSDUCER_FLAG faild!" );
			return DB_ABEND;
		}
		int rc = mysql_stmt_prepare( stmt_tran_update_ptr, SQL_UPDATE_TRANSDUCER_FLAG, strlen(SQL_UPDATE_TRANSDUCER_FLAG) );
		if ( rc )
		{
			LOG_ERROR( "MYSQL Prepare SQL_UPDATE_TRANSDUCER_FLAG failed! errno:%d msg:%s",
		  						rc, mysql_stmt_error(stmt_tran_update_ptr) );
			return DB_ABEND;
		}
 
		/* Get the parameter count from the statement */
		int param_count= mysql_stmt_param_count(stmt_tran_update_ptr);
				 
		if ( param_count != UPD_TRANS_FIELD_CNT ) /* validate parameter count */
		{
			LOG_ERROR( "Invalid parameter's count(%d) for SQL_UPDATE_TRANSDUCER_FLAG!", param_count );
			return DB_ABEND;
		}
		
		/* Bind the data for all 2 parameters */		 
		memset( upd_trans_bind, 0, sizeof(upd_trans_bind) );
		
		upd_trans_bind[0].buffer_type = MYSQL_TYPE_LONG;
		upd_trans_bind[0].buffer= (char *)&st_trsd_update.flag;
		upd_trans_bind[0].is_null = 0;
		upd_trans_bind[0].length = &upd_trans_len[0];
		
		upd_trans_bind[1].buffer_type = MYSQL_TYPE_VAR_STRING;
		upd_trans_bind[1].buffer= (char *)st_trsd_update.imei;
		upd_trans_bind[1].buffer_length= DB_IMEI_LEN;
		upd_trans_bind[1].is_null = 0;
		upd_trans_bind[1].length = &upd_trans_len[1];
		
		/* Bind the buffers */
		rc = mysql_stmt_bind_param( stmt_tran_update_ptr, upd_trans_bind );
		if ( rc )
		{
			LOG_ERROR( "MYSQL bind SQL_UPDATE_TRANSDUCER_FLAG parameters failed! errno:%d msg:%s",
							 rc, mysql_stmt_error( stmt_tran_update_ptr) );
			return DB_ABEND;
		}
	}
	return DB_NOMAL_END;
}

/*===========================================================================*/
/*  DESCRIPTION :      									                     */
/* 		update table transducer's flag by IMEI								 */
/* 		(2016/5/25)					 	 									 */
/* 	Return      :					  										 */
/*		DB_ABEND    : Error occured											 */
/*		DB_NOMAL_END: Execute successful									 */
/*																			 */
/* 	Parameter   :					  										 */
/* 		FLAG	: status of DTU ( 1:online; 2:offline )						 */
/* 		IMEI	: IMEI of the DTU being updated								 */
/* 									 	 									 */
/*===========================================================================*/
int update_transducer( int flag, const char *tagname );
{
	strcpy( st_trsd_update.flag, flag );
	upd_trans_len[0] = strlen( st_trsd_update.flag);
	strcpy( st_trsd_update.imei, msg->imei );	
	upd_trans_len[1] = strlen( st_trsd_update.imei);
	
	/* Execute the UPDATE statement */
	int rc = mysql_stmt_execute( stmt_tran_update_ptr );
	if ( rc )
	{
		LOG_ERROR( "SQL SQL_UPDATE_TRANSDUCER_FLAG STMT execute faild! errno:%d:,msg:%s",
				           rc,  mysql_stmt_error( stmt_tran_update_ptr ) );		
		return DB_ABEND;
	}

	return  DB_NOMAL_END;
}

/*===========================================================================*/
/*  DESCRIPTION :      									                     */
/* 		set recived massages to dccmsg struct				  				 */
/* 	Return      :					  										 */
/*		DB_ABEND    : Error occurt											 */
/*		DB_NOMAL_END: Execute successful									 */
/*																			 */
/* 	Parameter   :					  										 */
/* 			stmt(I)	: the stmt pointer to SQL pretreatment	 				 */
/* 						  													 */
/*===========================================================================*/
int my_chg_stmt_execute( MYSQL_STMT* *stmt )
{
	//if pretreatment pointer is effective,Run the SQL streatment
	if( stmt )
	{
		if ( mysql_stmt_execute( *stmt ) )
		{
			LOG_ERROR( "SQL STMT execute faild! errno:%d:,msg:%s",
				       mysql_stmt_errno(*stmt),  mysql_stmt_error(*stmt) );
			return DB_ABEND;
		}

		/* Get the total number of affected rows */
		int affected_rows= mysql_stmt_affected_rows(*stmt);
		
		/* validate affected rows less than 1 rows,error*/
		if ( affected_rows < 1 ) 
		{
			LOG_ERROR( "Invalid affected rows(%lu) by MySQL", (unsigned long)affected_rows);
			return DB_ABEND;
		}
	}
	else{
		LOG_ERROR( "Invalid MYSQL_STMT pointor by MySQL");
		return DB_ABEND;
	}
	return DB_NOMAL_END;
}

/*===========================================================================*/
/*  DESCRIPTION :      									                     */
/* 		set recived massages to dccmsg struct				  				 */
/* 	Return      :					  										 */
/*																			 */
/* 	Parameter   :					  										 */
/* 			sqltm(I/O)	: format tm struct to MYSQL_TIME struct for program	 */
/* 						  													 */
/*===========================================================================*/
void set_currenttime_sql(MYSQL_TIME *sqltm)
{
	time_t ltime;
    struct tm *tm;
    time(&ltime);
    tm = localtime(&ltime);
    tm->tm_mon = tm->tm_mon + 1;
    
	memset( sqltm, '\0', sizeof(sqltm) );
	sqltm->year= tm->tm_year + 1900; 
	sqltm->month= tm->tm_mon;
	sqltm->day= tm->tm_mday;
	sqltm->hour= tm->tm_hour;
	sqltm->minute= tm->tm_min;
	sqltm->second= tm->tm_sec;
	sqltm->second_part= 0;
	sqltm->neg= 0;
}

/*===========================================================================*/
/*  DESCRIPTION :      									                     */
/* 		set recived massages to dccmsg struct				  				 */
/* 	Return      :					  										 */
/* 			MSG_DSG     : the dsg's massage type							 */
/* 			MSG_NOT_DSG1: the massage is not dsg's type 					 */
/*						  and can't get transducer's tag information		 */
/* 			MSG_NOT_DSG2: the massage is not dsg's type 					 */
/*						  and can't get temperatuer's tag information		 */
/* 	Parameter   :					  										 */
/* 			msg(I)		: recive the massages from the tansducers			 */
/* 			dccmsg(I/O)	: format the massages to the struct for program		 */
/* 						  													 */
/*===========================================================================*/
int cover_msg_struct( const struct dc_msg *msg, pMYDCCMSG dccmsg )
{
	/* initialize the return's value is be DSG's massage's type*/
	int rc = MSG_DSG;

	/* initialize the dccmsg struct */
	memset( dccmsg->msg_body, '\0', sizeof(dccmsg->msg_body) );
	memset( dccmsg->msg_tag, '\0', sizeof(dccmsg->msg_tag) );
	dccmsg->temperature = 0.0;

	if( msg->msg_len < TAG_END ) {
		rc = MSG_NOT_DSG1;
	}
	if ( msg->msg_len < TEMPERATURE_INDEX ) {
		rc = MSG_NOT_DSG2;
	}
	int ix = 0;
	char ascii[4] = {'\0'}; 
	for( ix = 0 ; ix <= msg->msg_len; ix++ )
	{		
		sprintf( ascii,"%d", msg->msg_body[ix]);

		// if the data's length is less than db's field length then strcat
		if( ( strlen( dccmsg->msg_body) + strlen(ascii) ) 
			                                        < MSG_BODY_SIZE ) {
			strcat( dccmsg->msg_body, ascii );
			// the transducer's tag is been recorded in msg_body's TAG_START(5)~TAG_END(8) 
			if( (ix >= TAG_START) && (ix <= TAG_END) ){
				strcat( dccmsg->msg_tag, ascii );
			}
			// the temperatuer's been recorded in msg_body's TEMPERATURE_INDEX(11) 
			if( ix == TEMPERATURE_INDEX ){
				dccmsg->temperature = atoi(ascii);
			}
		}
	}
	return rc;
}

/*===========================================================================*/
/*  DESCRIPTION :      									                     */
/* 		initialize all the structs(data/temperatuer/tansducer) to null		 */
/* 	Return      :					  										 */
/* 																			 */
/* 	Parameter   :					  										 */
/* 						  													 */
/*===========================================================================*/
void mem_reset_zero()
{
	memset( st_data.data, '\0', sizeof(st_data.data));
	memset( st_data.imei, '\0', sizeof(st_data.imei));

	memset( st_trsd.transducer_id, '\0', sizeof(st_trsd.transducer_id) );
	memset( st_trsd.location_id, '\0', sizeof(st_trsd.location_id) );
	st_trsd.tag_no = 0;
	memset( st_trsd.tag, '\0', sizeof(st_trsd.tag) );
	memset( st_trsd.memo, '\0', sizeof(st_trsd.memo) );
	memset( st_trsd.imei, '\0', sizeof(st_trsd.imei) );
	st_trsd.flag = 0;
}
