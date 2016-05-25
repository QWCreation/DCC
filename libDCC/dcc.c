#ifdef WIN32
#include <windows.h>
#define EWOULDBLOCK WSAEWOULDBLOCK
#endif
#ifdef LINUX
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#define SOCKET int
#define INVALID_SOCKET (-1)
#define closesocket close
void Sleep(int mseconds)
{
	struct timeval delay;

	delay.tv_sec = (int) mseconds / 1000;
	delay.tv_usec = (mseconds % 1000) * 1000;

	select (0, 0, 0, 0, &delay);
}

#endif

#include "dcc.h"
#include "md5.h"

#define HDR_ID_LEN 4
#define MSG_LEN_OFFSET (HDR_ID_LEN+IMEI_LEN+1+DTU_NAME+1+2)
#define MSG_LEN_LEN 2
#define MSG_BODY_OFFSET (MSG_LEN_OFFSET+MSG_LEN_LEN)
#define SOCK_RX_BUF 4096
#define MAX_AUTH_WAIT	100
#define AUTH_RANDOM_LEN 4

#define MODE_HOST_IP 0
#define MODE_HOST_DNS 1

static const unsigned char g_hdr_id[4] = {0x7e, 0x7e, 0x7e, 0x7e};
struct dcc_ctrl {
	SOCKET listen_sock;
	int socket_mode;
	char rx_buf[SOCK_RX_BUF];
	int rx_offset;
	struct sockaddr_in dc_addr;
};

#define TRACE 

/*
 * decode the packet from DC
 * packet: the packet received from DC by net layer
*/
static int dcc_msg_decoder(unsigned char *packet, struct dc_msg *msg)
{
	unsigned char *ptr;

	memset(msg, 0, sizeof(struct dc_msg));
	ptr = packet;

	/* skip the header id */
	if (memcmp(ptr, g_hdr_id, sizeof(g_hdr_id)) != 0)
		return -2;	
	else
		ptr += sizeof(g_hdr_id);

	/* get imei */
	memcpy(msg->imei, ptr, IMEI_LEN+1);
	ptr += IMEI_LEN+1;
	/* get name */
	memcpy(msg->name, ptr, DTU_NAME+1);
	ptr += DTU_NAME+1;
	/* get msg type */
	msg->msg_type = *ptr;
	ptr += 1;
	/* skip the reserved one byte */
	ptr += 1;	
	/* get msg len */
	msg->msg_len = ntohs(*((unsigned short*)ptr));
	ptr += 2;

	if (msg->msg_len > MAX_MSG_LEN) {
		/* msg body too big */
		return -1;
	}

	/* get msg body */
	memcpy(msg->msg_body, ptr, msg->msg_len);	

	return 0;
}

static int dcc_msg_encoder(unsigned char *packet, struct dc_msg *msg)
{
	int len = 0;
	unsigned char *ptr;

	if (msg->msg_len > MAX_MSG_LEN) {
		/* msg body too big */
		return -1;
	}

	memset(packet, 0, sizeof(packet));
	ptr = packet;

	/* set packet header id */
	memcpy(ptr, g_hdr_id, sizeof(g_hdr_id));
	ptr += sizeof(g_hdr_id);
	len += sizeof(g_hdr_id);

	/* set imei */
	memcpy(ptr, msg->imei, IMEI_LEN+1);
	ptr += IMEI_LEN+1;
	len += IMEI_LEN+1;
	/* set name */
	memcpy(ptr, msg->name, DTU_NAME+1);
	ptr += DTU_NAME+1;
	len += DTU_NAME+1;
	/* set msg type */
	*ptr = msg->msg_type;
	ptr += 1;
	len += 1;
	/* skip the reserved one byte */
	ptr += 1;	
	len += 1;	
	/* set msg len */
	*((unsigned short*)ptr) = htons(msg->msg_len);
	ptr += 2;
	len += 2;
	/* set msg body */
	memcpy(ptr, msg->msg_body, msg->msg_len);
	len += msg->msg_len;

	return len;
}

/*
 * to receive udp pakcet from specified sock 
 */ 
static int dcc_udp_receive(struct dcc_ctrl *dcc, struct dc_msg *msg)
{
	int err;
	int ret = 0;
	int n = 0;
	int size = 0;
	struct sockaddr_in name;
	SOCKET listen_sock = dcc->listen_sock;

	size = sizeof(name);
	n = recvfrom(listen_sock, dcc->rx_buf, SOCK_RX_BUF, 0, (struct sockaddr*)&name, &size);
	if (n < 0) {
#ifdef WIN32
		err = WSAGetLastError();
#endif
#ifdef LINUX
		err = errno;
#endif
		if (err == EWOULDBLOCK)
			ret = 0;			/* no data available, try again */
		else
			ret = -1;
	}
	else if (n == 0) {
		//TRACE("socket %d is closed\n", listen_sock);
		ret = -1;
	}
	else {
		//TRACE("receive %d bytes from %s:%d\n", 
		//		n, inet_ntoa(name.sin_addr), ntohs(name.sin_port));
		if (memcmp(dcc->rx_buf, g_hdr_id, sizeof(g_hdr_id)) == 0) {
			ret = dcc_msg_decoder((unsigned char*)dcc->rx_buf, msg);
			if (ret < 0)
				ret = 0;
			else
				ret = n;
		}
		else 
			ret = 0;		/* not valid msg */
	}

	return ret;
}

/*
 * start to receive udp pakcet on src_addr:port
 * return value: sock or error code (<0)
 */ 
static int dcc_udp_start(unsigned short port)
{
	SOCKET sock;
	struct sockaddr_in local;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		//TRACE("failed to open udp socket: %d\n", WSAGetLastError());
		return -2;
	}
	
	memset(&local, 0, sizeof(local));
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = htonl(INADDR_ANY);
	local.sin_port = htons(port);
	
	if (bind(sock, (struct sockaddr*)&local, sizeof(local)) < 0) {
		//TRACE("failed to bind udp socket: %d\n", WSAGetLastError());
		return -3;
	}
	
	return sock;
}

/*
 * stop to receive udp packet on the sock
 */
static int dcc_udp_close(struct dcc_ctrl *dcc)
{
	closesocket(dcc->listen_sock);
	dcc->listen_sock = INVALID_SOCKET;

	return 0;
}

static int dcc_udp_send(struct dcc_ctrl *dcc, 
			struct sockaddr_in *addr, 
			unsigned char *buf, 
			unsigned short len)
{
	int err;
	int n;
	int ret;
	SOCKET listen_sock = dcc->listen_sock;
	
	if ((n = sendto(listen_sock, (char*)buf, len, 0, (struct sockaddr *)addr, 
							sizeof(struct sockaddr_in))) < 0) {
#ifdef WIN32
		err = WSAGetLastError();
#endif
#ifdef LINUX
		err = errno;
#endif
		if (err == EWOULDBLOCK)
			ret = 0; 
		else
			ret = n;
	}
	else
		ret = n;

	return ret;
}

static int packet_builder(char *buf, int *len, struct dc_msg *msg) 
{
	unsigned short idx = 0;
	unsigned short pkt_len;

	for (idx = 0; idx < *len; idx ++) {
		if (memcmp(buf+idx, g_hdr_id, sizeof(g_hdr_id)) == 0)
			break;
	}
	if (idx >= *len) {
		/* no vaild packet header found */
		if (*len > MAX_MSG_LEN) {
			/* clear the buf */
			*len = 0;	
		}
		return -1;
	}

	memcpy(&pkt_len, buf+idx+MSG_LEN_OFFSET, sizeof(pkt_len));
	pkt_len = ntohs(pkt_len);

	if (pkt_len > MAX_MSG_LEN) {
		//TRACE("packet len is error: %d!\n", pkt_len);
		/* clear the buf */
		*len = 0;
		return -1;
	}
	if ((*len - idx) >= (pkt_len+MSG_BODY_OFFSET)) {
		dcc_msg_decoder((unsigned char*)(buf+idx), msg);
		idx += pkt_len+MSG_BODY_OFFSET;
		if (idx < *len)
			memcpy(buf, buf + idx, *len - idx);
		*len -= idx;
		return idx;
	}
	else	/* not a whole packet */
		return 0;
}

/*
 * to receive tcp pakcet from specified sock 
 */ 
static int dcc_tcp_receive(struct dcc_ctrl *dcc, struct dc_msg *msg)
{
	int err;
	int ret;
	int n = 0;
	SOCKET listen_sock = dcc->listen_sock;

	ret = packet_builder(dcc->rx_buf, &dcc->rx_offset, msg);
	if (ret > 0)
		return ret;

	n = recv(listen_sock, dcc->rx_buf+dcc->rx_offset, SOCK_RX_BUF-dcc->rx_offset, 0);
	if (n < 0) {
#ifdef WIN32
		err = WSAGetLastError();
#endif
#ifdef LINUX
		err = errno;
#endif
		if (err == EWOULDBLOCK)
			ret = 0;		/* no data available, try again */
	}
	else if (n == 0) {
		//TRACE("socket %d is closed\n", listen_sock);
		ret = -1;
	}
	else {
		//TRACE("receive %d bytes\n", n);
		dcc->rx_offset += n;
		ret = packet_builder(dcc->rx_buf, &dcc->rx_offset, msg);
		if (ret <= 0)
			ret = 0;
	}

	return ret;
}

static int dcc_tcp_start(struct sockaddr_in *svr_addr)
{
	int sock = INVALID_SOCKET;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) {
		return -2;
	}
	
	if(connect(sock, (struct sockaddr*)svr_addr, sizeof(struct sockaddr_in)) < 0) {
		//TRACE("failed to connect to %s:%d!\n", inet_ntoa(svr_addr->sin_addr), 
		//				ntohs(svr_addr->sin_port));
		closesocket(sock);
		return -3;
	}

	return sock;
}

static int dcc_tcp_send(struct dcc_ctrl *dcc, unsigned char *buf, int len)
{
	int err;
	int n;
	SOCKET listen_sock = dcc->listen_sock;

	if ((n = send(listen_sock, (const char*)buf, len, 0)) < 0) {
#ifdef WIN32
		err = WSAGetLastError();
#endif
#ifdef LINUX
		err = errno;
#endif
		if (err == EWOULDBLOCK)
			return 0;
		else
			return -1;
	}
	else {
		if (n < len) {
			//TRACE("only part of data is sent, want to send %d, sent %d\n", len, n);
		}
		return n;
	}
}

static int dcc_tcp_close(struct dcc_ctrl *dcc) 
{
	closesocket(dcc->listen_sock);
	dcc->listen_sock = INVALID_SOCKET;

	return 0;
}

static int decide_host_mode(const char *host_name)
{
	char *p = (char *)host_name;

	while (*p != '\0') {
		if ((*p < '0' || *p > '9') && *p != '.') {
			return MODE_HOST_DNS;
		}
		p ++;
	}

	return MODE_HOST_IP;
}

static unsigned int dcc_gethostbyname(const char *target)
{
	struct hostent *hp;
	struct sockaddr_in *to;
	struct sockaddr whereto;
	struct in_addr host_in_addr;
	
	to = (struct sockaddr_in *)&whereto;
	hp = gethostbyname(target);
	if (!hp) {
		//TRACE("failed to get ip for %s, error: %d!\n", target, WSAGetLastError());
		return 0;
	}
	to->sin_family = hp->h_addrtype;
	if (hp->h_length > (int)sizeof(to->sin_addr)) {
		hp->h_length = sizeof(to->sin_addr);
	}
	memcpy(&to->sin_addr, hp->h_addr, hp->h_length);
	host_in_addr.s_addr = to->sin_addr.s_addr;

	return 	ntohl(to->sin_addr.s_addr);
}

/* init the socket to communicate with DC
 * mode: MODE_UDP or MODE_TCP
 * port: if mode is MODE_UDP, port is the local port to receive data from dc;
 *		 if mode is MODE_TCP, it is ignored;
 * dc_host: the dc ip address or host name.
 * dc_port: the dc listen port.
 * block: MODE_BLOCK or MODE_NONBLOCK
 * return value: if successfully, the returned value is positive, or failed.	
 */
int DCCCALL dcc_init(int mode, 
							unsigned short port, 
							const char *dc_host, 
							unsigned short dc_port, 
							int block)
{
	int sock;
	struct dcc_ctrl *dcc;
	unsigned int dc_ip;
	unsigned long flag = 1;
#ifdef WIN32
	int err;
	WSADATA wsaData;

	err = WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

	if (decide_host_mode(dc_host) == MODE_HOST_DNS) {
		dc_ip = dcc_gethostbyname(dc_host);
		if (dc_ip == 0)
			return -1;
		else
			dc_ip = htonl(dc_ip);
	}
	else
		dc_ip = inet_addr(dc_host);

	dcc = (struct dcc_ctrl *) malloc (sizeof(struct dcc_ctrl));
	if (dcc == NULL)
		return 0;
	else
		memset(dcc, 0, sizeof(struct dcc_ctrl));
	
	memset(&dcc->dc_addr, 0, sizeof(struct sockaddr_in));
	dcc->dc_addr.sin_family = AF_INET;
	dcc->dc_addr.sin_addr.s_addr = dc_ip;
	dcc->dc_addr.sin_port = htons(dc_port);

	if (dcc->dc_addr.sin_addr.s_addr == INADDR_NONE || dcc->dc_addr.sin_port == 0) {
		//TRACE("invalid dc ip address or port\n");
		return -1;
	}

	if (mode == MODE_UDP) {
		sock = dcc_udp_start(port);	
	}
	else if (mode == MODE_TCP) {
		sock = dcc_tcp_start(&dcc->dc_addr);
	}

	if (sock > 0) {
		dcc->socket_mode = mode;
	}
	else {
		free(dcc);	
		return 0;
	}

	/* ret is the socket to listen */
	if (block == MODE_NONBLOCK) {
#ifdef WIN32
		ioctlsocket(sock, FIONBIO, &flag);
#endif
#ifdef LINUX
		flag = fcntl(sock, F_GETFL, 0);
		fcntl(sock, F_SETFL, flag | O_NONBLOCK);
#endif
	}

	dcc->listen_sock = sock;
	return (int)dcc;
}

/* 
 * close the socket to communicate with DC
 */
int DCCCALL dcc_close(int dcc_hdl)
{
	struct dcc_ctrl *dcc = (struct dcc_ctrl *)dcc_hdl;

	if (dcc == NULL || dcc->listen_sock == 0)
		return -1;

	if (dcc->socket_mode == MODE_UDP)
	{
		dcc_udp_close(dcc);
	}
	else if (dcc->socket_mode == MODE_TCP)
	{
		dcc_tcp_close(dcc);	
	}

	free(dcc);
	/* dcc_hdl is not valid any more! */

#ifdef WIN32
	WSACleanup();
#endif

	return 0;
}

/* encode the packet and send to DC
 * msg: the message to send to DC
 * return value:	ret > 0, successfully
 *					ret = 0, try again
 *					ret < 0, failed, need to re-init socket
 */
int DCCCALL dcc_msg_send(int dcc_hdl, struct dc_msg *msg)
{
	unsigned char buf[2048];
	unsigned short len = 0;
	int ret;
	struct dcc_ctrl *dcc = (struct dcc_ctrl *)dcc_hdl;

	if (dcc == NULL || dcc->listen_sock == 0)
		return -1;

	len = dcc_msg_encoder(buf, msg);

	if (dcc->socket_mode == MODE_UDP)
	{
		ret = dcc_udp_send(dcc, &dcc->dc_addr, buf, len);
	}
	else if (dcc->socket_mode == MODE_TCP)
	{
		int try_count = 0;
		int offset = 0;

		while (1) {
			ret = dcc_tcp_send(dcc, buf + offset, len - offset);
			if (ret > 0) {
				offset += ret;
				if (offset == len) { // finished
					break;
				}
			}
			else if (ret == 0) {
				// BLOCKED
				try_count ++;
				if (try_count > 100) {
					break;
				}
				Sleep(100);				
			}
			else // failed
				break;
		}

		ret = offset;
	}
	else
		ret = -1;

	if (ret > 0 && ret < len)	/* only pary of data sent out, we think it failed */
		ret = 0;
	
	return ret;
}

/* receive the packet and decode to the struct dc_msg
 * msg: the message to contain the msg
 * return value:	ret > 0, successfully
 *					ret = 0, try again
 *					ret < 0, failed, need to re-init socket
 */
int DCCCALL dcc_msg_receive(int dcc_hdl, struct dc_msg *msg)
{
	int ret; 
	struct dcc_ctrl *dcc = (struct dcc_ctrl *)dcc_hdl;

	if (dcc == NULL || dcc->listen_sock == 0)
		return -1;

	if (dcc->socket_mode == MODE_UDP)
	{
		ret = dcc_udp_receive(dcc, msg);
	}
	else if (dcc->socket_mode == MODE_TCP)
	{
		ret = dcc_tcp_receive(dcc, msg);
	}
	else
		ret = -1;

	return ret;
}

int DCCCALL dcc_msg_send_auth(int dcc_hdl, char *user, char *passwd)
{
	int i;
	int ret;
	struct dc_msg msg;
	struct xMD5Context context;
	int pass_len;
	unsigned char pass[128];

	for (i = 0; i < 100; i ++) 
	{
		ret = dcc_msg_receive(dcc_hdl, &msg);
		if (ret < 0)
			return -1;
		else if (ret == 0)
			Sleep(100);
		else if (msg.msg_type != DC_MSG_NEED_AUTH)
			continue;
		else
			break;
	}

	if (i == MAX_AUTH_WAIT)
		return -1;
	else if (msg.msg_type != DC_MSG_NEED_AUTH || msg.msg_len < AUTH_RANDOM_LEN)
		return -1;

	memcpy(pass, &msg.msg_body[0], AUTH_RANDOM_LEN);
	strncpy(pass+AUTH_RANDOM_LEN, passwd, strlen(passwd));
	pass_len = AUTH_RANDOM_LEN + strlen(passwd);

	memset(&msg, 0, sizeof(msg));
	msg.msg_type = DC_MSG_AUTH;
	msg.msg_len = USER_NAME_LEN + PASSWD_MD5_LEN;
	memcpy(msg.msg_body, user, 
			strlen(user) > (USER_NAME_LEN - 1) ? (USER_NAME_LEN - 1) : strlen(user));
	xMD5Init(&context);
	xMD5Update(&context, pass, pass_len);
	xMD5Final(msg.msg_body+USER_NAME_LEN, &context);

	if (dcc_msg_send(dcc_hdl, &msg) < 0)
		return -1;

	for (i = 0; i < 100; i ++) 
	{
		ret = dcc_msg_receive(dcc_hdl, &msg);
		if (ret < 0)
			return -1;
		else if (ret == 0)
			Sleep(100);
		else if (msg.msg_type != DC_MSG_AUTH_RESULT)
			continue;
		else
			break;
	}

	if (i == MAX_AUTH_WAIT)
		return -1;
	if (msg.msg_type == DC_MSG_AUTH_RESULT && msg.msg_body[0] == AUTHRESULT_PASSED)
		return 0;
	else
		return -1;
}

int DCCCALL dcc_get_local_ip(int dcc_hdl)
{
	int local_ip;
	int len;
	struct sockaddr_in addr;
	struct dcc_ctrl *dcc = (struct dcc_ctrl *)dcc_hdl;

	if (dcc == NULL || dcc->listen_sock == 0)
		return -1;

	len = sizeof(struct sockaddr_in);
	if (getsockname(dcc->listen_sock, (struct sockaddr*)&addr, &len) < 0) {
		return 0;
	}

	local_ip = ntohl(addr.sin_addr.s_addr);
	return local_ip;
}
