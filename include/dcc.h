#ifndef _DCC_H_
#define _DCC_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WIN32
#define DCCCALL WINAPI
#else
#define DCCCALL 
#endif

#define MODE_BLOCK		0
#define MODE_NONBLOCK	1

#define MODE_UDP	0
#define MODE_TCP	1
#define IMEI_LEN	15
#define DTU_NAME	15
#define DCC_ID_LEN	3
#define MAX_MSG_LEN 1492
#define USER_NAME_LEN 16
#define PASSWD_MD5_LEN 16

#define DC_MSG_DATA		0x00
#define DC_MSG_ONLINE	0x01
#define DC_MSG_OFFLINE	0x02
#define DC_MSG_GETSTATUS	0x03
#define DC_MSG_SENDRESULT	0x04
#define DC_MSG_STATUSRESULT	0x05
#define DC_MSG_LISTEN_DTU	0x06
#define DC_MSG_DCC_ID		0x07
#define DC_MSG_DISC_DTU		0x08
#define DC_MSG_ADD_DTU		0x09
#define DC_MSG_AT_CMD		0x0A
#define DC_MSG_AT_DATA		0x0B
#define DC_MSG_ATDATA_RES	0x0C
#define DC_MSG_NEED_AUTH	0x0D
#define DC_MSG_RENAME_DTU	0x0E
#define DC_MSG_DEL_DTU		0x0F

#define DC_MSG_AUTH			0x50
#define DC_MSG_AUTH_RESULT	0x51
#define DC_MSG_VIDEO_START	0x52
#define DC_MSG_VIDEO_STOP	0x53
#define DC_MSG_VIDEO_GET	0x54
#define DC_MSG_VIDEO_ATTRIBUTES	0x55
#define DC_MSG_VIDEO_SET	0x56

#define SENDRESULT_SUCCESS		0x00
#define SENDRESULT_NOSUCHDTU	0x01
#define SENDRESULT_OFFLINE		0x02
#define SENDRESULT_CONGESTED 	0x03
#define SENDRESULT_NORIGHT 		0x04

#define STATUSRESULT_NOSUCHDTU	0x00
#define STATUSRESULT_ONLINE		0x01
#define STATUSRESULT_OFFLINE	0x02
#define STATUSRESULT_NORIGHT	0x03

#define AUTHRESULT_PASSED		0x00
#define AUTHRESULT_FAILED		0x01

struct dc_msg {
	char imei[IMEI_LEN+1];
	char name[DTU_NAME+1];
	unsigned char msg_type;		/* DC_MSG_DATA, DC_MSG_ONLINE, DC_MSG_OFFLINE */
	unsigned char reserved;
	unsigned short msg_len;		
	unsigned char msg_body[MAX_MSG_LEN];
};

int DCCCALL dcc_init(
							int mode, 
							unsigned short port, 
							const char *dc_host, 
							unsigned short dc_port, 
							int block);
int DCCCALL dcc_close(int dcc_hdl);
int DCCCALL dcc_msg_send(int dcc_hdl, struct dc_msg *msg);
int DCCCALL dcc_msg_receive(int dcc_hdl, struct dc_msg *msg);
int DCCCALL dcc_msg_send_auth(int dcc_hdl, char *user, char *passwd);
int DCCCALL dcc_get_local_ip(int dcc_hdl);
#ifdef __cplusplus
}
#endif

#endif
