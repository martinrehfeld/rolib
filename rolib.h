/**********************************************************************
                             R O L I B
                      Remote Operations Library

	Datei:		rolib.h
	Version:	1.12
	Datum:		96/11/28 11:45:12

                        (c) CR by Herlitz AG

 ********************************************************************** 

	Funktion:	Globale Header-Definitionen fuer die ROLIB

	Autor:		Destailleur, Lubitz
*/

#ifndef _RO_ROLIB_H
#define _RO_ROLIB_H

#ifdef TANDEM
#include "unixincl.system"
#else
#include "system.h"
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#ifdef BS2000
#include <sys.types.h>
#include <netdb.h>
#include <sys.socket.h>
#include <netinet.in.h>
#include <arpa.inet.h>
#else
#ifdef TANDEM
#include <types.h>
#include <fcntl.h>
#include <talh>
#include <cextdecs(AWAITIOX)>
#include <cextdecs(SETMODE)>
#include <cextdecs(CANCELREQ)>
#include <cextdecs(CLOSE)>
#include <cextdecs(DELAY)>
#include <cextdecs(FILE_GETINFO_)>
#include "$system.ztcpip.inh"
#include "$system.ztcpip.netdbh"
#include "$system.ztcpip.socketh"
#else
#ifdef WINNT
#include <winsock.h>
#include <malloc.h>
#include <sys/types.h>
#include <fcntl.h>
#else
#include <netdb.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#endif
#endif
#endif


#define RO_MAXLISTENQUEUE 5
#define RO_MAXCONNECTIONS 120

#define SOC_ACCEPT accept
#define SOC_BIND bind
#define SOC_CONNECT connect
#define SOC_LISTEN listen
#define SOC_SELECT select
#define SOC_SHUTDOWN shutdown
#define SOC_SOCKET socket
#define SOC_RECV recv
#define SOC_SEND send
#ifdef BS2000
#define SOC_CLOSE soc_close
#define SOC_IOCTL soc_ioctl
#define SOC_READ soc_read
#define SOC_WRITE soc_write
#else
#ifdef TANDEM
#define SOC_CLOSE CLOSE
#else
#ifdef WINNT
#define SOC_CLOSE closesocket
#else
#define SOC_CLOSE close
#endif
#endif
#ifdef WINNT
#define SOC_IOCTL ioctlsocket
#else
#define SOC_IOCTL ioctl
#endif
#define SOC_READ read
#define SOC_WRITE write
#endif

#ifdef WINNT
#define sleep(s) Sleep(s*1000)
#endif

/*
 * Strukturdefinitionen
 */

struct ro_application_info
{
	FILE *logfile;
	char providerdir[255];
	char servicedir[255];
	int debuglevel;
	unsigned short port;
#define RO_ASCII  0
#define RO_EBCDIC 1
	int charset;
#ifdef TANDEM
	int status2;						/* TANDEM: accept_nw pending ? */
#endif
};

struct ro_connparams {
	int mode;
#define RO_MODE_TRANSPARENT 1
#define RO_MODE_NONTRANSPARENT 0
	char escape;
#define RO_DEFAULT_ESC '^'
};

struct ro_connection
{
	int sock;
	int status;
#define RO_CONNECTION_CLOSED          0
#define RO_CONNECTION_CLOSE_REQUESTED 1
#define RO_CONNECTION_REQUESTED       2
#define RO_CONNECTION_ESTABLISHED     3
	unsigned long message_nr;
	char providername[16];
	char servicename[16];
	int remote_port;
	int level;
	struct ro_connparams params;
#ifdef TANDEM
	int status2;						/* TANDEM: recv_nw pending ? */
#define RO_RECV_PENDING               1
#define RO_PORT_CLEAR                 2
#endif
};

struct ro_connect_rq {
	char myprovidername[15];
	char myservicename[15];
	char providername[15];
	char servicename[15];
	int level;
#define RO_APPLICATION_LEVEL 0
#define RO_ADMIN_LEVEL 1
	struct ro_connparams roc;
	unsigned long id;
};

struct ro_connect_cf {
	unsigned long id;
};

struct ro_connect_rj {
	int reason;
	char *errtext;
	unsigned long id;
};

struct ro_disconn_rq {
	unsigned long id;
};

struct ro_disconn_cf {
	unsigned long id;
};

struct ro_disconn_im {
	int reason;
};

struct ro_invoke_rq {
	int op;
	int version;
	unsigned long id;
	char *in;
	unsigned long inlen;
};

struct ro_invoke_rs {
	int op;
	int version;
	unsigned long id;
	char *result;
	unsigned long resultlen;
};

struct ro_invoke_rj {
	int op;
	int version;
	int reason;
	unsigned long id;
	char *data;
	unsigned long datalen;
};

struct ro_event {
	unsigned short event_typ;
#define RO_CONNECT_REQUEST         1
#define RO_CONNECT_CONFIRMATION    2
#define RO_CONNECT_REJECT          3
#define RO_DISCONNECT_REQUEST      4
#define RO_DISCONNECT_CONFIRMATION 5
#define RO_DISCONNECT_IMMEDIATE    6
#define RO_INVOKE_REQUEST          7
#define RO_INVOKE_RESULT           8
#define RO_INVOKE_REJECT           9
#define RO_TIMEOUT                97
#define RO_SHUTDOWN               98
#define RO_INTERNAL_EVENT         99
	int cd;
	union {
		struct ro_connect_rq rocrq;
		struct ro_connect_cf roccf;
		struct ro_connect_rj rocrj;
		struct ro_disconn_rq rodrq;
		struct ro_disconn_cf rodcf;
		struct ro_disconn_im rodim;
		struct ro_invoke_rq roirq;
		struct ro_invoke_rs roirs;
		struct ro_invoke_rj roirj;
	} event;
};

struct ro_indication {
	int code;
	char text[255];
};

struct ro_error {
	int status;
	char *text;
};

#define RO_ERR_INITMESS 1
#define RO_ERR_PORTINVALID 2
#define RO_ERR_PORTRESERVED 3
#define RO_ERR_INITSOCK 4
#define RO_ERR_GETPROVIDER 5
#define RO_ERR_CREATESOCK 6
#define RO_ERR_BINDSOCK 7
#define RO_ERR_LISTENSOCK 8
#define RO_ERR_ACCEPTSOCK 9
#define RO_ERR_RECVSOCK 10
#define RO_ERR_SENDMESS 11
#define RO_ERR_SELECTSOCK 12
#define RO_ERR_WAITEVENT 13
#define RO_ERR_TOOMANYCON 14
#define RO_ERR_CONNSOCK 15
#define RO_ERR_CONNALLOC 16
#define RO_ERR_CONNDIRSERV 17
#define RO_ERR_QRYDIRSERV 18
extern struct ro_error ro_errors[];

struct ro_provider {
	char name[16];
	struct sockaddr_in sin;
	char charset[7];
};

struct ro_service {
	char name[16];
	unsigned short port;
};


/*
 * Allgemeine Definitionen
 */
#ifndef WINNT
typedef enum { FALSE = 0, TRUE = 1 } BOOLEAN;
#endif


/*
 * Symbolische Konstanten fuer mask bei ro_setconnparams, 
 * ggf. durch | verbinden
 */
#define RO_CP_SETMODE    0x0001	/* Verbindungs-Modus setzen */
#define RO_CP_SETESCAPE  0x0002	/* Escape-Character setzen */


/*
 * Symbolische Konstanten fuer die Return-Codes der ROLIB-Funktionen
 */
enum { RO_ERROR = 0, RO_OK = 1, RO_WARNING = 2 };


/*
 * Symbolische Konstanten fuer die OP-Codes von reservierten Operationen
 */
#define RO_STD_OP_PING 2
#define RO_STD_VERSION_PING 1
#define RO_STD_OP_SHUTDOWN 5
#define RO_STD_VERSION_SHUTDOWN 1

#define RO_STD_OP_SERVICEQUERY 512
#define RO_STD_VERSION_SERVICEQUERY 1
#define RO_STD_OP_WRITE 520
#define RO_STD_VERSION_WRITE 1

/*
 * Funktionsprototypen
 */
int ro_init(char *myprovidername, char *providerdir, char *servicedir,
            char *logfilename, int debuglevel, int port, 
			struct ro_indication *roi);
int ro_release(struct ro_indication *roi);
int ro_nw1_wait_event(int cd,
#ifndef TANDEM
					 fd_set *fdread,
					 int *maxfd,
#endif
					 struct ro_indication *roi);
int ro_nw2_wait_event(
#ifdef TANDEM
					  unsigned short ret_socket,
#else
					  int fdcount, fd_set *fdread,
#endif
                      long maxlen,
					  struct ro_event *roe,
					  struct ro_indication *roi);
int ro_wait_event(int secs, int cd, long maxlen, struct ro_event *roe,
				  struct ro_indication *roi);
int ro_direct_connect(struct sockaddr_in *netaddr,
					  struct ro_connect_rq *rocrq, int *cd,
					  struct ro_indication *roi);
int ro_connect(struct ro_connect_rq *rocrq, int *cd, struct ro_indication *roi);
int ro_co_confirm(int cd, struct ro_connect_cf *roccf,
                  struct ro_indication *roi);
int ro_co_reject(int cd, struct ro_connect_rj *rocrj,
                 struct ro_indication *roi);
int ro_disconnect(int cd, struct ro_disconn_rq *rodrq,
                  struct ro_indication *roi);
int ro_dc_immediate(int cd, struct ro_disconn_im *rodim,
                    struct ro_indication *roi);
int ro_dc_confirm(int cd, struct ro_disconn_cf *rodcf,
				  struct ro_indication *roi);
int ro_setconnparams(int cd, struct ro_connparams *roc, int mask,
					 struct ro_indication *roi);
int ro_request(int cd, struct ro_invoke_rq *roirq, struct ro_indication *roi);
int ro_result(int cd, struct ro_invoke_rs *roirs, struct ro_indication *roi);
int ro_reject(int cd, struct ro_invoke_rj *roirj, struct ro_indication *roi);
struct ro_connection *ro_getconnection(int cd);
int ro_enterconnection(int cd, struct ro_connection *roc);
struct ro_service *ro_getservicebyname(char *name);
struct ro_provider *ro_getproviderbyname(char *name);
void ro_setdebuglevel(int lvl);
int ro_getdebuglevel(void);

#ifndef HAS_BSTRING
void bzero(char *p, int len);
void bcopy(char *s, char *d, int len);
#endif

#ifndef HAS_GETOPT
int getopt(int nargc, char **nargv, char *ostr);
#endif

void dprintf(int lvl, char *format, ...);
void lbzero(char *p, unsigned long len);
void lbcopy(char *s, char *d, unsigned long len);

#endif

/******************************* EOF ********************************/
