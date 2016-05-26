#include "program_dcc.h"
#include <curses.h>

/* curses libary's define start*/
#define MAINBOX_WIDTH	COLS
#define MAINBOX_HIGH	(LINES-2)
#define SUBBOX_WIDTH	(COLS-2) 
#define MENUBOX_HIGH	9 
#define CTRLBOX_HIGH	3
#define MSGBOX_HIGH		(MAINBOX_HIGH-MENUBOX_HIGH-CTRLBOX_HIGH-2)
#define CNTBOX_WIDTH	10
#define CNTBOX_HIGH		1
#define MYSQL_RECONNECT 7 * 3600                  // 7 Hours
/* curses libary's define end*/

static struct dc_msg msg;
static time_t lasttime;

int system_init()
{
	char *shell_start;
	if (( shell_start = getenv(SH_START)) == NULL){
		char config_file[PATH_MAX];
		sprintf( config_file, "%s/dtu.cfg", default_config_dir );
		// Set envionment variables for connect to dc(socket) and db
		if( set_envionment( config_file ) != 0 )
		{
			return 10;
		}
	}

	log_init(log_lev, "DCC", ".");

	//
	char *ch_dc_port = getenv( "DC_PORT" );
	if( ch_dc_port == NULL || ch_dc_port == "" ){
		LOG_ERROR( "Getting DC Port from envionment failed!" );
		return 11; 
	}
	else{
		dc_port = atoi( ch_dc_port );
	}

	//
	char *ch_local_port = getenv( "LOCAL_PORT" );
	if( ch_local_port == NULL || ch_local_port == "" ){
		LOG_ERROR( "Getting Local Port from envionment failed!" );
		return 12; 
	}
	else{
		local_port = atoi( ch_local_port );
	}

	//
	char *ch_mode = getenv( "MODE" );
	if( ch_mode == NULL || ch_mode == "" ){
		LOG_ERROR( "Getting DC mode from envionment failed!" );
		return 13;
	}
	else{
		mode = atoi( ch_mode );
	}
	
	//
	dc_ip = getenv( "DC_IP" );
	if( dc_ip == NULL || dc_ip == "" ){
		LOG_ERROR( "Getting DC_IP from envionment failed!" );
		return 14; 
	}

	dc_user = getenv( "DC_USER" );
	if( dc_user == NULL || dc_user == "" ){
		LOG_ERROR( "Getting DC_USER from envionment failed!" );
		return 15; 
	}

	//
	dc_passwd = getenv( "DC_PASSWD" );
	if( dc_passwd == NULL || dc_passwd == "" ){
		LOG_ERROR( "Getting DC_PASSWD from envionment failed!" );
		return 16; 
	}
	
	//
	db_ip = getenv( "DB_HOST" );
	if( db_ip == NULL || db_ip == "" ){
		LOG_ERROR( "Getting DB_HOST from envionment failed!" );
		return 17; 
	}
	//
	db_user = getenv( "DB_USER" );
	if( db_user == NULL || db_user == "" ){
		LOG_ERROR( "Getting DB_USER from envionment failed!" );
		return 18; 
	}
	
	//
	db_passwd = getenv( "DB_PASSWD" );
	if( db_passwd == NULL || db_passwd == "" ){
		LOG_ERROR( "Getting DB_PASSWD from envionment failed!" );
		return 19; 
	}

	return 0;	
}

void receive_data(struct dc_msg *msg)
{
	int ret;

	while ((ret = dcc_msg_receive(g_dcc_hdl, msg)) > 0) {
		
		LOG_DEBUG("msg_type = %i", msg->msg_type);

		switch (msg->msg_type) {
		case DC_MSG_DATA:
			db_main_process( msg );
			break;
		case DC_MSG_ONLINE:
			update_transducer( DC_MSG_ONLINE, msg );
			break;
		case DC_MSG_OFFLINE:
			update_transducer( DC_MSG_OFFLINE, msg );
			break;
		default:
			break;
		}
	}

	if (ret < 0) {
		LOG_TRACE( "the DC connection was destroyed: %d!", ret);
		dcc_close(g_dcc_hdl);
		g_dcc_hdl = 0;
		connect_dc();
	}
}

void connect_dc(void)
{
	if (g_dcc_hdl > 0)
		return ;

	g_dcc_hdl = dcc_init(mode, local_port, dc_ip, dc_port, MODE_NONBLOCK);
	if (g_dcc_hdl <= 0) {
		g_dcc_hdl = 0;
		LOG_ERROR( "Failed to connect to DC %s:%d, mode: %s!", 
					dc_ip, dc_port, mode_string[mode]);
	}

	dcc_msg_send_auth(g_dcc_hdl, dc_user, dc_passwd);
}


void dis_connect_dc(void)
{
	if (g_dcc_hdl <= 0)
		return ;

	dcc_close(g_dcc_hdl);
	g_dcc_hdl = 0;
	LOG_TRACE( "DC connection closed successfully!" );
}

int main(int argc, char ** argv)
{
	int started_flg = 0;
	int start_status = CTRL_SHOW | CTRL_CHOSREV | CTRL_RECORD;

	if ( system_init() )
	{
		fprintf(stderr, "System initialization failed!" );
		return 21;
	}

	LOG_TRACE("%s", "System Init Done.");

	if( 0 == started_flg ){
		if( init_connect_db( start_status ) == DB_NOMAL_END ){

				LOG_TRACE("%s", "DB Connected.");

				time( &lasttime);
				connect_dc();
				if( g_dcc_hdl > 0 )
				{
					started_flg = 1;
					LOG_TRACE("%s", "DC Connected.");
				}
				else{
					dis_connect_db();
					LOG_ERROR( "DB connection failed!" );
					return 0;
				}
		}
		else{
			LOG_ERROR( "DB connection failed!" );
			return 0;
		}
	}
	else
	{
		LOG_NOTICE( "System Listening has already been started!" );
		return 0;
	}

	LOG_NOTICE("%s", "Data Receiving Started...");

	while (g_dcc_hdl > 0) {
		time_t newtime;
		double dftime = difftime( newtime, lasttime );

		//the connection to mysql over of 7 hours,reconnect 
		if( dftime > MYSQL_RECONNECT )
		{
			dis_connect_db();

			//try for 3 times!
			int i = 0;
			for( i = 0; i < 3; i++ )
			{
				if( init_connect_db( start_status ) != DB_NOMAL_END ){
					LOG_ERROR( "DB reconnect failed! Retried [%d] times!", i );
					sleep(10);
					continue;
				}
				else{
					time( &lasttime );
					break;
				}
			}
			// all of 3 times are failed
			if( 4 == i )
			{
				break;
			}
		}
		timeout(0) ;
		receive_data( &msg );
	}

	if( started_flg ){
		started_flg = 0;
		dis_connect_dc();
		dis_connect_db();
	}

	return 0;
}
