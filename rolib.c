/**********************************************************************
                             R O L I B
                      Remote Operations Library

	Datei:		rolib.c
	Version:	1.9
	Datum:		96/11/28 11:44:59

                        (c) CR by Herlitz AG

 ********************************************************************** 

	Funktion:	Standardisierte Online-Kommunikation ueber TCP-Sockets.

	Autor:		Destailleur, Lubitz
*/

static char V_rolib[]="@(#)rolib.c	1.9 vom 96/11/28  (c) CR by Herlitz AG";

#ifdef TANDEM
#include "unixincl.rolib"
#include "unixincl.messages"
#else
#include "rolib.h"
#include "messages.h"
#include <sys/time.h>
#endif

#ifndef HAS_GETOPT
int	opterr = 1,
	optind = 1,
	optopt,
	optreset;
char *optarg;

#define	BADCH	(int)'?'
#define	BADARG	(int)':'
#define	EMSG	""
#endif

struct ro_error ro_errors[] = {
	{ RO_OK, "Kein Fehler" },
	{ RO_ERROR,   "Messages-Modul konnte nicht initialisiert werden" },
	{ RO_ERROR,   "Port ungueltig (>65535)" },
	{ RO_WARNING, "Port im reservierten Bereich (<5000)" },
	{ RO_ERROR,   "Socket-Bibliothek konnte nicht initialisiert werden" },
	{ RO_ERROR,   "Providerdirectory ungueltig oder Provider nicht gefunden" },
	{ RO_ERROR,   "Socket konnte nicht kreiert werden" },
	{ RO_ERROR,   "Socket konnte nicht gebunden werden" },
	{ RO_ERROR,   "Fuer Socket konnte keine Listen-Queue eingerichtet werden" },
	{ RO_ERROR,   "Verbindung konnte auf Socket nicht angenommen werden" },
	{ RO_ERROR,   "Daten konnten nicht vom Socket gelesen werden" },
	{ RO_WARNING, "Send-Message fehlgeschlagen (Non-Fatal)" },
	{ RO_ERROR,   "Select/AWAITIO fehlgeschlagen" },
	{ RO_ERROR,   "Fehler bei ro_wait_event" },
	{ RO_ERROR,   "Zu viele offene Verbindungen" },
	{ RO_ERROR,   "Socket konnte nicht verbunden werden" },
	{ RO_ERROR,   "Verbindung konnte nicht eingetragen werden" },
	{ RO_ERROR,   "Verbindung zum Directory-Server fehlgeschlagen" },
	{ RO_ERROR,   "Remote Operation am Directory-Server fehlgeschlagen" }
};

static struct ro_connection *ro_connections = NULL;
static struct ro_application_info ro_app;

static int listen_socket = -1;
static struct sockaddr_in sin;
static int namelen;

static int open_connections = 0;
static int allocated_connections = 0;

/*
 *	 ro_wait_event: public-vars
 */
static int lcd;
#ifdef TANDEM
static struct sockaddr_in from;
static char rec_buf[2+1];

/**** HJG - sockinfo */
 static short sockinfo ( short sock , char * comment )
 {
  static short err , lasterr ;
  err = FILE_GETINFO_ ( sock , &lasterr ) ;
  if (err == 0 ) err = lasterr ;
  if (err != 0 )
   dprintf (6,"rolib.sockinfo %s : sock %d , err %d \n" , comment, sock , err );
  return err ;
 }
/**** HJG - FILE_GETINFO_ */
#endif

#ifndef HAS_BSTRING
void bzero(char *p, int len)
{
	while(len--)
		*p++ = '\0';
}

void bcopy(char *s, char *d, int len)
{
	while(len--)
		*d++ = *s++;
}
#endif

void lbzero(char *p, unsigned long len)
{
	while(len--)
		*p++ = '\0';
}

void lbcopy(char *s, char *d, unsigned long len)
{
	while(len--)
		*d++ = *s++;
}

#ifndef HAS_GETOPT
int getopt(int nargc, char **nargv, char *ostr)
{
	char *__progname = nargv[0];
	static char *place = EMSG;
	char *oli;

	if(optreset || !*place)
	{
		optreset = 0;
		if(optind >= nargc || *(place = nargv[optind]) != '-') 
		{
			place = EMSG;
			return (-1);
		}
		if(place[1] && *++place == '-') 
		{
			++optind;
			place = EMSG;
			return (-1);
		}
	}
	if((optopt = (int) *place++) == (int)':' ||
	    !(oli = strchr(ostr, optopt))) {
		if (optopt == (int)'-')
			return (-1);
		if (!*place)
			++optind;
		if (opterr && *ostr != ':')
			(void)fprintf(stderr,
			    "%s: illegal option -- %c\n", __progname, optopt);
		return (BADCH);
	}
	if (*++oli != ':') {
		optarg = NULL;
		if (!*place)
			++optind;
	}
	else {
		if (*place)
			optarg = place;
		else if (nargc <= ++optind) {
			place = EMSG;
			if (*ostr == ':')
				return (BADARG);
			if (opterr)
				(void)fprintf(stderr,
				    "%s: option requires an argument -- %c\n",
				    __progname, optopt);
			return (BADCH);
		}
	 	else
			optarg = nargv[optind];
		place = EMSG;
		++optind;
	}
	return (optopt);
}
#endif 

void dprintf(int lvl, char *format, ...)
{
	static BOOLEAN write_timestamp = TRUE;
	char timestamp[20];
	time_t currtime;
	va_list args;

	va_start(args, format);
	if((lvl < ro_app.debuglevel || lvl == 0) && ro_app.logfile != (FILE *) NULL)
	{
		if(write_timestamp)
		{
			currtime = time(NULL);
			strftime(timestamp,20,"%d.%m.%y %H:%M:%S",localtime(&currtime));
			fprintf(ro_app.logfile,"%s - ",timestamp);
			write_timestamp = FALSE;
		}
		if(format[strlen(format)-1] == '\n')
			write_timestamp = TRUE;
		vfprintf(ro_app.logfile,format,args);
#ifdef HAS_FASTFLUSH
		fflush(ro_app.logfile); 
#endif
	}
	va_end(args);
}

void ro_setdebuglevel(int lvl)
{
	ro_app.debuglevel = lvl;
}

int ro_getdebuglevel(void)
{
	return ro_app.debuglevel;
}

static int add_connection(int sock, int status, struct ro_connparams *roc)
{
	int cd = 0;
	struct ro_connection *new_connections;
	BOOLEAN found_unused = FALSE;
	struct sockaddr addr;
	int addrlen = sizeof(addr);
	struct sockaddr_in *ptr = (struct sockaddr_in *) &addr;

	while(cd < allocated_connections)
	{
		if(ro_connections[cd].status == RO_CONNECTION_CLOSED)
		{
			found_unused = TRUE;
			break;
		}
		cd++;
	}
	if(!found_unused)
	{
		if(ro_connections)
			new_connections = (struct ro_connection *) realloc(ro_connections,
						sizeof(struct ro_connection)*(++allocated_connections));
		else
			new_connections = (struct ro_connection *) malloc(
						sizeof(struct ro_connection)*(++allocated_connections));
		if(new_connections == NULL)
			return -1;
		ro_connections = new_connections;
	}
	open_connections++;
	ro_connections[cd].sock = sock;
	ro_connections[cd].status = status;
	ro_connections[cd].params = *roc;
	ro_connections[cd].providername[0] = '\0';
	ro_connections[cd].servicename[0] = '\0';
#ifdef TANDEM
	dprintf(1,"Add connection: socket = %d, cd = %d\n",sock,cd);
	ro_connections[cd].status2 = RO_PORT_CLEAR;
	getpeername_nw(sock,&addr,&addrlen,0); 
	sockinfo(sock, "add_connection.getpeername_nw");
	AWAITIOX(&sock);
#else
	getpeername(sock,&addr,&addrlen); 
#endif
	ro_connections[cd].remote_port = ptr->sin_port;
	return cd;
}

static int ro_error(struct ro_indication *roi, int code, char *text)
{
	strcpy(roi->text,text);
	roi->code = code;
	dprintf(ro_errors[code].status == RO_ERROR ? 4 : 5,
			"ro_error: Status = %s, Code = %d - %s (%s)\n",
			ro_errors[code].status == RO_ERROR ? "RO_ERROR" : "RO_WARNING",
			code, ro_errors[code].text, text);
	return ro_errors[code].status;
}

static int s_message(struct ro_connection *roc,
			         struct ro_application_info *roa,
			         struct ro_event *roe,
					 struct ro_indication *roi)
{
	int ret;

#ifdef TANDEM
	if(roc->status2 == RO_RECV_PENDING)
		CANCELREQ(roc->sock);
#endif
	ret = send_message(roc,roa,roe,roi);
#ifdef TANDEM
	if(roc->status2 == RO_RECV_PENDING)
		recv_nw(roc->sock,&rec_buf[2],1,MSG_PEEK,1);
	sockinfo(roc->sock, "s_message");
#endif
	return ret;
}

int ro_init(char *myprovidername, char *providerdir, char *servicedir,
			char *logfilename, int debuglevel, int port,
			struct ro_indication *roi)
{
	char providername[16];
	struct ro_provider *provider;
	int ret = RO_OK;

	if(logfilename != (char *) NULL)
		ro_app.logfile = fopen(logfilename,"a");
	else
		ro_app.logfile = (FILE *) NULL;

	if(!message_init())
		return ro_error(roi,RO_ERR_INITMESS,"");
	if(port < 5000 && port != 0)
		ret = ro_error(roi,RO_ERR_PORTRESERVED,"");
	else if(port > 65535)
		return ro_error(roi,RO_ERR_PORTINVALID,"");

	ro_app.debuglevel = debuglevel;
	ro_app.port = port;
#ifdef TANDEM
	ro_app.status2 = RO_PORT_CLEAR;
#endif
	if(providerdir)
		strcpy(ro_app.providerdir,providerdir);
	else
		strcpy(ro_app.providerdir,RO_PROVIDERDIR_FILENAME);
	if(servicedir)
		strcpy(ro_app.servicedir,servicedir);
	else
		strcpy(ro_app.servicedir,RO_SERVICEDIR_FILENAME);

#ifdef WINNT
	{                                                                       
		WORD wVersionRequested;                                             
		WSADATA wsaData;                                                    
		int err;                                                            

		wVersionRequested = MAKEWORD(1,1);                                  
		err = WSAStartup(wVersionRequested,&wsaData);                       
		if(err != 0)                                                        
			return ro_error(roi,RO_ERR_INITSOCK,"Winsock-Initialisierung");
	}                                                                       
#endif

	strncpy(providername,myprovidername,15);
	strtok(providername," \t");
dprintf(6,"ro_init: looking for local provider :%s:\n",providername);
	if((provider = ro_getproviderbyname(providername)) == NULL)
		return ro_error(roi,RO_ERR_GETPROVIDER,"Lokaler Provider");
	ro_app.charset = strcmp(provider->charset,"EBCDIC") ? 
					 RO_ASCII : RO_EBCDIC;

	if(ro_app.port != 0)
	{
#ifdef TANDEM
		if((listen_socket = 
			socket_nw(AF_INET,SOCK_STREAM,IPPROTO_TCP,0x200,1)) < 0)
			return ro_error(roi,RO_ERR_CREATESOCK,strerror(errno));
		sockinfo(listen_socket, "init.socket_nw.listen");
		AWAITIOX(&listen_socket);
		SETMODE(listen_socket,30,3);  /* Allow nowait, complete in any order */
#else
		if((listen_socket = SOC_SOCKET(AF_INET,SOCK_STREAM,IPPROTO_TCP)) < 0)
			return ro_error(roi,RO_ERR_CREATESOCK,strerror(errno));
#endif

		namelen = sizeof(sin);
		bzero((char *) &sin,namelen);
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = htonl(INADDR_ANY);
		sin.sin_port = (unsigned short) htons(ro_app.port);
#ifdef TANDEM
		if(bind_nw(listen_socket,(struct sockaddr *) &sin, namelen, 1) < 0)
			return ro_error(roi,RO_ERR_BINDSOCK,strerror(errno));
		sockinfo(listen_socket, "init.bind_nw.listen");
		AWAITIOX(&listen_socket);
#else
		if(SOC_BIND(listen_socket,(struct sockaddr *) &sin, namelen) < 0)
			return ro_error(roi,RO_ERR_BINDSOCK,strerror(errno));
#endif

		if(SOC_LISTEN(listen_socket,RO_MAXLISTENQUEUE) < 0)
			return ro_error(roi,RO_ERR_LISTENSOCK,strerror(errno));
	}
dprintf(6,"ro_init: complete\n");
	return ret;
}

int ro_release(struct ro_indication *roi)
{
	int i;

	for(i = 0; i < allocated_connections; i++)
		if(ro_connections[i].status != RO_CONNECTION_CLOSED)
		{
#ifdef TANDEM
			CANCELREQ(ro_connections[i].sock);
			ro_connections[i].status2 = RO_PORT_CLEAR;
#endif
			SOC_CLOSE(ro_connections[i].sock);
			ro_connections[i].status = RO_CONNECTION_CLOSED;
		}
	if(ro_app.port != 0)
	{
#ifdef TANDEM
		CANCELREQ(listen_socket);
		ro_app.status2 = RO_PORT_CLEAR;
#endif
		SOC_CLOSE(listen_socket);
		ro_app.port = 0;
	}
dprintf(6,"ro_release: complete\n");
	fclose(ro_app.logfile);
	return RO_OK;
}

int ro_nw1_wait_event(int cd,
#ifndef TANDEM
                     fd_set *fdread,
                     int *maxfd,
#endif
					 struct ro_indication *roi)
{
	int i;

dprintf(5,"ro_wait_event entered\n");
	lcd = cd;
#ifdef TANDEM
	{
		int flen = sizeof(from);

		if(cd == -1 && ro_app.port != 0 && ro_app.status2 == RO_PORT_CLEAR)
			if(accept_nw(listen_socket,(struct sockaddr *)&from,&flen,1) < 0)
				return ro_error(roi,RO_ERR_ACCEPTSOCK,strerror(errno));
			else
				ro_app.status2 = RO_RECV_PENDING;
		sockinfo(listen_socket, "ev1.accept_nw.listen");
	}
#else
	*maxfd = 0;
	FD_ZERO(fdread);
	if(cd == -1 && ro_app.port != 0)
		FD_SET(listen_socket,fdread);
#endif
	if(cd >= 0)
	{
#ifdef TANDEM
		if(ro_connections[cd].status2 == RO_PORT_CLEAR)
			if(recv_nw(ro_connections[cd].sock,&rec_buf[2],1,MSG_PEEK,1) < 0)
				return ro_error(roi,RO_ERR_RECVSOCK,strerror(errno));
			else
				ro_connections[cd].status2 = RO_RECV_PENDING;
		sockinfo(ro_connections[cd].sock, "nw1.recv_nwp.cd_sock");
#else
		FD_SET(ro_connections[cd].sock,fdread);
		*maxfd = ro_connections[cd].sock;
#endif
	}
	else
	{
		for(i = 0; i < allocated_connections; i++)
			if(ro_connections[i].status != RO_CONNECTION_CLOSED)
			{
#ifdef TANDEM
				if(ro_connections[i].status2 == RO_PORT_CLEAR)
					if(recv_nw(ro_connections[i].sock,&rec_buf[2],1,MSG_PEEK,1)
					   < 0)
						return ro_error(roi,RO_ERR_RECVSOCK,strerror(errno));
				ro_connections[i].status2 = RO_RECV_PENDING;
#else
				FD_SET(ro_connections[i].sock,fdread);
				*maxfd = ro_connections[i].sock > *maxfd ?
						 ro_connections[i].sock : *maxfd;
#endif
			}
	}
#ifndef TANDEM
	*maxfd = *maxfd > listen_socket ? *maxfd+1 : listen_socket+1;
#endif
	return RO_OK;
}

int ro_nw2_wait_event(
#ifdef TANDEM
                      unsigned short ret_socket,
#else
                      int fdcount, fd_set *fdread,
#endif
					  long maxlen, struct ro_event *roe, 
					  struct ro_indication *roi)
{
	static int last_cd;
	int loop_count;
	int i;
	int ecd = -1;
	unsigned short newsock;
	BOOLEAN found_event = FALSE;
	BOOLEAN application_event = FALSE;
	struct ro_connparams roc;

#ifdef TANDEM
	if(ret_socket != listen_socket)
		for(i = 0; i < allocated_connections; i++)
			if(ro_connections[i].sock == ret_socket)
			{
				if(ro_connections[i].status2 == RO_RECV_PENDING)
					ecd = i;
				break;
			}
	if(ret_socket == listen_socket &&
	   ro_app.status2 == RO_RECV_PENDING) /* Incoming connection */
	{
dprintf(5,"ro_wait_event: accepting...\n");
		if((newsock = socket_nw(AF_INET,SOCK_STREAM,IPPROTO_TCP,0x200,1)) < 0)
			return ro_error(roi,RO_ERR_CREATESOCK,strerror(errno));
		AWAITIOX(&newsock);
		SETMODE(newsock,30,3);	/* Allow nowait, complete in any order */
		ro_app.status2 = RO_PORT_CLEAR;
		if(accept_nw2(newsock,(struct sockaddr_in *) &from,1) < 0)
			return ro_error(roi,RO_ERR_ACCEPTSOCK,strerror(errno));
		AWAITIOX(&newsock);
		add_connection(newsock,RO_CONNECTION_REQUESTED,&roc);
	}
	else if (ecd >= 0)					/* Incoming data */
	{
dprintf(5,"ro_wait_event: incoming data: ro_connections[%d]\n",ecd);
		found_event = TRUE;
	}
	if(lcd < 0)
	{
		for(i = 0; i < allocated_connections; i++)
			if(ro_connections[i].status != RO_CONNECTION_CLOSED &&
			   ro_connections[i].status2 == RO_RECV_PENDING)
			{
				CANCELREQ(ro_connections[i].sock);
				ro_connections[i].status2 = RO_PORT_CLEAR;
			}
		if(ro_app.port != 0 && ro_app.status2 == RO_RECV_PENDING)
		{
			CANCELREQ(listen_socket);
			ro_app.status2 = RO_PORT_CLEAR;
		}
	}
	else if(ro_connections[lcd].status2 == RO_RECV_PENDING)
	{
		CANCELREQ(ro_connections[lcd].sock);
		ro_connections[lcd].status2 = RO_PORT_CLEAR;
	}
#else
dprintf(5,"ro_wait_event: selected...\n");
	if(fdcount > 0)
	{
		if(ro_app.port != 0)
			if(FD_ISSET(listen_socket,fdread))
			{	/* Incoming connection */
	dprintf(5,"ro_wait_event: accepting...\n");
				if((newsock = SOC_ACCEPT(listen_socket,(struct sockaddr *) &sin,
										 &namelen)) < 0)
					return ro_error(roi,RO_ERR_ACCEPTSOCK,strerror(errno));
				add_connection(newsock,RO_CONNECTION_REQUESTED,&roc);
				fdcount--;
			}
		last_cd++;
		loop_count = 0;
		while(!found_event && fdcount)	/* Incoming data */
		{
			for(i = last_cd; i < allocated_connections; i++)
				if(ro_connections[i].status != RO_CONNECTION_CLOSED)
					if(FD_ISSET(ro_connections[i].sock,fdread))
					{
						ecd = last_cd = i;
						found_event = TRUE;
						fdcount--;
						goto while_loop;
					}
			last_cd = 0;
		while_loop:
			if(++loop_count == 2)
				break;
		}
	}
#endif

	if(!found_event)	/* No event or internal event received */
	{
		roe->event_typ = RO_INTERNAL_EVENT;
		return RO_OK;
	}
dprintf(4,"ro_wait_event: receiving event...\n");
#ifdef TANDEM
	ro_connections[ecd].status2 = RO_PORT_CLEAR;
#endif
	if(receive_message(ro_connections,ecd,&ro_app,roe,maxlen,roi) == RO_ERROR) 
	{
		dprintf(4,"Error receiving message.\n");

		roe->cd = ecd;
		roe->event_typ = RO_DISCONNECT_IMMEDIATE;
	}
dprintf(4,"ro_wait_event: processing event %d.\n",roe->event_typ);
	switch(roe->event_typ)
	{
		case RO_CONNECT_REQUEST:
			if(open_connections > RO_MAXCONNECTIONS)
			{
				struct ro_indication roi;
				struct ro_connect_rj rocrj;

				rocrj.reason = 0;
				rocrj.errtext = "too many connections";
				rocrj.id = roe->event.rocrq.id;
				ro_co_reject(roe->cd,&rocrj,&roi);
				SOC_CLOSE(ro_connections[roe->cd].sock);
				ro_connections[roe->cd].status = RO_CONNECTION_CLOSED;
				open_connections--;
			}
			ro_connections[roe->cd].status = RO_CONNECTION_REQUESTED;
			ro_connections[roe->cd].level = roe->event.rocrq.level;
			ro_connections[roe->cd].params = roe->event.rocrq.roc;
			strncpy(ro_connections[roe->cd].providername,
					roe->event.rocrq.myprovidername,15);
			strncpy(ro_connections[roe->cd].servicename,
					roe->event.rocrq.myservicename,15);
			ro_connections[roe->cd].providername[15]
				= ro_connections[roe->cd].servicename[15] = '\0';

			if(!(application_event = (BOOLEAN)
				(roe->event.rocrq.level == RO_APPLICATION_LEVEL)))
			{
				struct ro_indication roi;
				struct ro_connect_cf roccf;

				roccf.id = roe->event.rocrq.id;
				ro_co_confirm(roe->cd,&roccf,&roi);
			}
			break;
		case RO_CONNECT_CONFIRMATION:
			application_event = TRUE;
			ro_connections[roe->cd].status = RO_CONNECTION_ESTABLISHED;
			break;
		case RO_CONNECT_REJECT:
			application_event = TRUE;
			SOC_CLOSE(ro_connections[roe->cd].sock);
			ro_connections[roe->cd].status = RO_CONNECTION_CLOSED;
			break;
		case RO_DISCONNECT_REQUEST:
			application_event = (BOOLEAN)
				(ro_connections[roe->cd].level == RO_APPLICATION_LEVEL);
			if(application_event)
				ro_connections[roe->cd].status = RO_CONNECTION_CLOSE_REQUESTED;
			else
			{
				struct ro_indication roi;
				struct ro_disconn_cf rodcf;

				rodcf.id = roe->event.rodrq.id;
				ro_dc_confirm(roe->cd,&rodcf,&roi);
			}
			break;
		case RO_DISCONNECT_CONFIRMATION:
			application_event = TRUE;
			SOC_CLOSE(ro_connections[roe->cd].sock);
			ro_connections[roe->cd].status = RO_CONNECTION_CLOSED;
			open_connections--;
			break;
		case RO_DISCONNECT_IMMEDIATE:
			application_event = (BOOLEAN)
				(ro_connections[roe->cd].level == RO_APPLICATION_LEVEL);
			SOC_CLOSE(ro_connections[roe->cd].sock);
			ro_connections[roe->cd].status = RO_CONNECTION_CLOSED;
			open_connections--;
			break;
		case RO_INVOKE_REQUEST:
			application_event = (BOOLEAN)
				(ro_connections[roe->cd].level == RO_APPLICATION_LEVEL);
			if(!application_event)
				switch(roe->event.roirq.op)
				{
					case RO_STD_OP_PING:
						/* ro_invoke_rq and ro_invoke_rs must have same
						 * structure!!! */
						roe->event_typ = RO_INVOKE_RESULT;
						roe->event.roirs.version = RO_STD_VERSION_PING;
						if(!s_message(&ro_connections[roe->cd],
										 &ro_app,roe,roi))
							return ro_error(roi,RO_ERR_SENDMESS,
											  "RO_STD_OP_PING");
						break;
					case RO_STD_OP_SHUTDOWN:
						roe->event_typ = RO_SHUTDOWN;
						application_event = TRUE;
						break;
					case RO_STD_OP_SERVICEQUERY:
						{
							struct ro_indication roi;
							struct ro_invoke_rs roirs;
							struct ro_service *service;
							char servicename[16];
							char serviceport[6];

							strncpy(servicename,roe->event.roirq.in,
									roe->event.roirq.inlen > 15 ? 15 :
									roe->event.roirq.inlen);
							servicename[roe->event.roirq.inlen > 15 ? 15 :
							            roe->event.roirq.inlen] = '\0';
							service = ro_getservicebyname(servicename);
							sprintf(serviceport,"%05d",
									service == NULL ? 0 : service->port);
							roirs.op = roe->event.roirq.op;
							roirs.version = RO_STD_VERSION_SERVICEQUERY;
							roirs.id = roe->event.roirq.id;
							roirs.result = serviceport;
							roirs.resultlen = strlen(serviceport);
							ro_result(roe->cd,&roirs,&roi);
						}
						break;
					default: /* Unknown admin operation */
						{
							struct ro_indication roi;
							struct ro_invoke_rj roirj;

							roirj.op = roe->event.roirq.op;
							roirj.version = roe->event.roirq.version;
							roirj.reason = 0;
							roirj.id = roe->event.roirq.id;
							roirj.data = NULL;
							roirj.datalen = 0;
							ro_reject(roe->cd,&roirj,&roi);
						}
				}
			break;
		case RO_INVOKE_RESULT:
			application_event = TRUE;
			break;
		case RO_INVOKE_REJECT:
			application_event = TRUE;
			break;
	}
	if(!application_event)	/* No application event received */
		roe->event_typ = RO_INTERNAL_EVENT;
	return RO_OK;
}

int getMillisekunden()
{
	struct timeval microTime;
	struct timeval *microTimeptr=&microTime;
	
	long sekunden;
	long sekundenHelp;
	long microSekunden;
	
	gettimeofday(microTimeptr, NULL);
	
	sekunden = microTimeptr->tv_sec;
	microSekunden = microTimeptr->tv_usec;
	sekundenHelp = sekunden / 86400;
	sekunden = sekunden - (sekundenHelp * 86400); 
	sekunden = (sekunden * 1000) + (microSekunden / 1000);
	
	return(sekunden);
}


int ro_wait_event(int secs, int cd, long maxlen, struct ro_event *roe,
				  struct ro_indication *roi)
{
#ifndef TANDEM
	int fdcount;
	struct timeval timeout;
	fd_set fdread;
	int maxfd;
	int millisec;
#else
	unsigned short ret_socket;
#endif

	millisec = getMillisekunden();
	if(ro_nw1_wait_event(cd,
#ifndef TANDEM
                     &fdread,
                     &maxfd,
#endif
					 roi) == RO_ERROR)
		return ro_error(roi,RO_ERR_WAITEVENT,"ro_nw1_wait_event");
#ifdef TANDEM
	{
		long addr, tag;
		short count, segment;

		if(cd >= 0)
			ret_socket = ro_connections[cd].sock;
		else
			ret_socket = -1;
		if(AWAITIOX(&ret_socket,&addr,&count,&tag,(long) secs,&segment)
				 == CCL)
		{
			dprintf(5,"ro_wait_event: AWAITIOX failed, socket=%d\n",ret_socket);
			return ro_error(roi,RO_ERR_SELECTSOCK,strerror(errno));
		}
		/* TIMEOUT abpruefen !!! */
	}
#else
	bzero((char *) &timeout, sizeof(timeout));
	timeout.tv_sec = secs;
dprintf(5,"ro_wait_event: select\n");
	fdcount = SOC_SELECT(maxfd, &fdread, (fd_set *) NULL, 
					 (fd_set *) NULL,
					 secs == -1 ? (struct timeval *) NULL : &timeout);
dprintf(6,"ro_wait_event: Wartezeit auf Event: %d ms.\n",getMillisekunden()- millisec);
	millisec = getMillisekunden();
	if(fdcount < 0)
		return ro_error(roi,0,strerror(errno));
	else if(fdcount == 0)
	{
		roe->event_typ = RO_TIMEOUT;
		return RO_OK;
	}
#endif
	if(ro_nw2_wait_event(
#ifdef TANDEM
					  ret_socket,
#else
					  fdcount, &fdread,
#endif
					  maxlen,roe, 
					  roi) == RO_ERROR)
	
		return ro_error(roi,RO_ERR_WAITEVENT,"ro_nw2_wait_event");

dprintf(6,"ro_wait_event: Übertragungszeit des Events: %d ms.\n",getMillisekunden() - millisec);

	return RO_OK;
}

int ro_direct_connect(struct sockaddr_in *netaddr,
					  struct ro_connect_rq *rocrq, int *cd,
                      struct ro_indication *roi)
{
	int sock;
	struct ro_event roe;

dprintf(6,"ro_direct_connect: start\n");
	if(open_connections >= RO_MAXCONNECTIONS)
		return ro_error(roi,RO_ERR_TOOMANYCON,"");

#ifdef TANDEM
	if((sock = socket_nw(AF_INET,SOCK_STREAM,IPPROTO_TCP,0x200,1)) < 0)
		return ro_error(roi,RO_ERR_CREATESOCK,strerror(errno));
	AWAITIOX(&sock);
	SETMODE(sock,30,3);		/* Allow nowait, complete in any order */
#else
	if((sock = SOC_SOCKET(AF_INET,SOCK_STREAM,IPPROTO_TCP)) < 0)
		return ro_error(roi,RO_ERR_CREATESOCK,strerror(errno));
#endif
	
dprintf(6,"ro_direct_connect: connecting to %s(%d)\n", inet_ntoa(netaddr->sin_addr),ntohs(netaddr->sin_port));
#ifdef TANDEM
	if(connect_nw(sock,(struct sockaddr *) netaddr, 
				   sizeof(struct sockaddr_in),1) < 0)
	{
		SOC_CLOSE(sock);
		return ro_error(roi,RO_ERR_CONNSOCK,strerror(errno));
	}
	if(AWAITIOX(&sock) == CCL)
	{
		SOC_CLOSE(sock);
		return ro_error(roi,RO_ERR_SELECTSOCK,"AWAITIOX (connect)");
	}
#else
	if(SOC_CONNECT(sock,(struct sockaddr *) netaddr, 
				   sizeof(struct sockaddr_in)) < 0)
	{
		SOC_CLOSE(sock);
		return ro_error(roi,RO_ERR_CONNSOCK,strerror(errno));
	}
#endif

	if((*cd = add_connection(sock,RO_CONNECTION_REQUESTED, &(rocrq->roc))) < 0)
		return ro_error(roi,RO_ERR_CONNALLOC,"");

	ro_connections[*cd].level = rocrq->level;
	roe.event_typ = RO_CONNECT_REQUEST;
	roe.cd = *cd;
	roe.event.rocrq = *rocrq;
dprintf(6,"ro_direct_connect: sending connect-request\n");
	return s_message(&ro_connections[roe.cd],&ro_app,&roe,roi);
}

struct ro_connection *ro_getconnection(int cd)
{
	static struct ro_connection roc;

	if(cd >= allocated_connections || cd < 0)
		return (struct ro_connection *) NULL;
	roc = ro_connections[cd];
	return &roc;
}

int ro_enterconnection(int cd, struct ro_connection *roc)
{
	if(cd == -1)
		cd = add_connection(roc->sock, roc->status, &(roc->params));
	if(cd < allocated_connections && cd > 0)
		ro_connections[cd] = *roc;
	return cd;
}

struct ro_provider *ro_getproviderbyname(char *providername)
{
	FILE *dir;
	int i;
	char name[16], lbuf[255], *field;
	struct hostent *host;
	static struct ro_provider provider;

	bzero(name, sizeof(name));
	strncpy(name,providername,15);
	strtok(name," ");
	bzero((char *) &provider,sizeof(struct ro_provider));

	if((dir = fopen(ro_app.providerdir,"r")) == NULL)
	{
dprintf(1,"ro_getproviderbyname: open-error (%s / %s)\n",ro_app.providerdir,strerror(errno));
		return (struct ro_provider *) NULL;
	}
	while(fgets(lbuf,sizeof(lbuf),dir))
	{
		strtok(lbuf," \t\n");
		if(!strcmp(lbuf,name))
			break;
	}
	if(feof(dir))
	{
		fclose(dir);
dprintf(6,"ro_getproviderbyname: EOF by %s in %s\n",name,ro_app.providerdir);
		return (struct ro_provider *) NULL;
	}
	fclose(dir);

	for(i = 1; (field = strtok(NULL," \t\n")); i++)
	{
		switch(i)
		{
			case 1:		/* IP-adress */
				provider.sin.sin_family = AF_INET;
				if((host = gethostbyname(field)) == NULL)
					provider.sin.sin_addr.s_addr = inet_addr(field);
				else
					provider.sin.sin_addr.s_addr = 
						inet_addr(inet_ntoa(*(struct in_addr *)(host->h_addr)));
				break;
			case 2:		/* TCP-port */
				provider.sin.sin_port = htons((unsigned short) atoi(field));
				break;
			case 3:		/* character set */
				strncpy(provider.charset,field,6);
				provider.charset[6] = '\0';
				break;
		}
	}
	strncpy(provider.name,name,15);
	provider.name[15] = '\0';

	return &provider;
}

struct ro_service *ro_getservicebyname(char *servicename)
{
	FILE *dir;
	int i;
	char name[16], lbuf[255], *field;
	struct servent *serv;
	static struct ro_service service;

	bzero((char *) &service,sizeof(struct ro_service));
	bzero(name, sizeof(name));
	strncpy(name,servicename,15);
	strtok(name," ");

	dir = fopen(ro_app.servicedir,"r");
	while(fgets(lbuf,sizeof(lbuf),dir))
	{
		strtok(lbuf," \t\n");
		if(!strcmp(lbuf,name))
			break;
	}
	if(feof(dir))
	{
		fclose(dir);
		return (struct ro_service *) NULL;
	}
	fclose(dir);

	for(i = 1; (field = strtok(NULL," \t\n")); i++)
	{
		switch(i)
		{
			case 1:		/* TCP-Port */
				if((serv = getservbyname(field,"tcp")) == NULL)
					service.port = atoi(field);
				else
					service.port = ntohs(serv->s_port);
				break;
		}
	}
	strncpy(service.name,name,15);
	service.name[15] = '\0';

	return &service;
}

int ro_connect(struct ro_connect_rq *rocrq, int *cd, struct ro_indication *roi)
{
	int lcd;
	int status;
	char event_buffer[1024];
	char serviceport[6];
	struct sockaddr_in netaddr;
	struct ro_event *roe = (struct ro_event *) event_buffer;
	struct ro_provider *provider;
	struct ro_connect_rq lrocrq;
	struct ro_invoke_rq roirq;
	struct ro_disconn_im rodim;

dprintf(6,"ro_connect: start\n");
dprintf(6,"ro_connect: looking for remote provider :%s:\n",rocrq->providername);
	if((provider = ro_getproviderbyname(rocrq->providername)) == NULL)
		return ro_error(roi,RO_ERR_GETPROVIDER,"entfernter Provider");
	lrocrq = *rocrq;
	lrocrq.level = RO_ADMIN_LEVEL;
	lrocrq.roc.escape = RO_DEFAULT_ESC;
	lrocrq.roc.mode = RO_MODE_NONTRANSPARENT;
	strncpy(lrocrq.servicename,"dirserv        ",15);
dprintf(6,"ro_connect: connecting to dirserv\n");
	if((status = ro_direct_connect(&(provider->sin),&lrocrq,&lcd,roi)) != RO_OK)
		return status;
	status = ro_wait_event(-1,lcd,sizeof(event_buffer),roe,roi);
	if(status == RO_ERROR || roe->event_typ != RO_CONNECT_CONFIRMATION)
		return ro_error(roi,RO_ERR_CONNDIRSERV,"");
	roirq.op = RO_STD_OP_SERVICEQUERY;
	roirq.version = RO_STD_VERSION_SERVICEQUERY;
	roirq.id = 1;
	roirq.in = rocrq->servicename;
	roirq.inlen = strlen(rocrq->servicename);
dprintf(6,"ro_connect: requesting servicequery\n");
	if(ro_request(lcd,&roirq,roi) == RO_ERROR)
		return ro_error(roi,RO_ERR_QRYDIRSERV,"SERVICEQUERY - request");

	status = ro_wait_event(-1,lcd,sizeof(event_buffer),roe,roi);
	if(status == RO_ERROR || roe->event_typ != RO_INVOKE_RESULT)
	{
dprintf(4,"ro_connect: Service query failed\n");
		return ro_error(roi,RO_ERR_QRYDIRSERV,"SERVICEQUERY - wait");
	}
	strncpy(serviceport,roe->event.roirs.result,roe->event.roirs.resultlen);
	serviceport[roe->event.roirs.resultlen] = '\0';
	rodim.reason = 0;
dprintf(6,"ro_connect: disconnecting from dirserv\n");
	ro_dc_immediate(lcd, &rodim, roi);
	netaddr.sin_family = AF_INET;
	netaddr.sin_port = htons((unsigned short) atoi(serviceport));
	netaddr.sin_addr = provider->sin.sin_addr;
	if(netaddr.sin_port == 0)
		return ro_error(roi,RO_ERR_QRYDIRSERV,"SERVICEQUERY - result=0");

dprintf(6,"ro_connect: connecting to requested service\n");
	return ro_direct_connect(&netaddr,rocrq,cd,roi);
}

int ro_co_confirm(int cd, struct ro_connect_cf *roccf,
                  struct ro_indication *roi)
{
	struct ro_event roe;

	roe.event_typ = RO_CONNECT_CONFIRMATION;
	roe.cd = cd;
	roe.event.roccf = *roccf;
	ro_connections[cd].status = RO_CONNECTION_ESTABLISHED;
	return s_message(&ro_connections[roe.cd],&ro_app,&roe,roi);
}

int ro_co_reject(int cd, struct ro_connect_rj *rocrj,
                 struct ro_indication *roi)
{
	int ret;
	struct ro_event roe;

	roe.event_typ = RO_CONNECT_REJECT;
	roe.cd = cd;
	roe.event.rocrj = *rocrj;
	open_connections--;
	ret = s_message(&ro_connections[roe.cd],&ro_app,&roe,roi);
	SOC_CLOSE(ro_connections[cd].sock);
	ro_connections[cd].status = RO_CONNECTION_CLOSED;
	return ret;
}

int ro_disconnect(int cd, struct ro_disconn_rq *rodrq,
                  struct ro_indication *roi)
{
	struct ro_event roe;

	roe.event_typ = RO_DISCONNECT_REQUEST;
	roe.cd = cd;
	roe.event.rodrq = *rodrq;
	ro_connections[cd].status = RO_CONNECTION_CLOSE_REQUESTED;
	return s_message(&ro_connections[roe.cd],&ro_app,&roe,roi);
}

int ro_dc_immediate(int cd, struct ro_disconn_im *rodim,
                  struct ro_indication *roi)
{
	int ret;
	struct ro_event roe;

	roe.event_typ = RO_DISCONNECT_IMMEDIATE;
	roe.cd = cd;
	roe.event.rodim = *rodim;
	open_connections--;
	ret = s_message(&ro_connections[roe.cd],&ro_app,&roe,roi);
	SOC_CLOSE(ro_connections[cd].sock);
	ro_connections[cd].status = RO_CONNECTION_CLOSED;
	return ret;
}

int ro_dc_confirm(int cd, struct ro_disconn_cf *rodcf,
							struct ro_indication *roi)
{
	int ret;
	struct ro_event roe;

	roe.event_typ = RO_DISCONNECT_CONFIRMATION;
	roe.cd = cd;
	roe.event.rodcf = *rodcf;
	ret = s_message(&ro_connections[roe.cd],&ro_app,&roe,roi);
	open_connections--;
	SOC_CLOSE(ro_connections[cd].sock);
	ro_connections[cd].status = RO_CONNECTION_CLOSED;
	return ret;
}

int ro_setconnparams(int cd, struct ro_connparams *roc, int mask,
					 struct ro_indication *roi)
{
	if(mask & RO_CP_SETMODE)
		ro_connections[cd].params.mode = roc->mode;
	if(mask & RO_CP_SETESCAPE)
		ro_connections[cd].params.escape = roc->escape;
	return RO_OK;
}

int ro_request(int cd, struct ro_invoke_rq *roirq, struct ro_indication *roi)
{
	struct ro_event roe;

	roe.event_typ = RO_INVOKE_REQUEST;
	roe.cd = cd;
	roe.event.roirq = *roirq;
	return s_message(&ro_connections[roe.cd],&ro_app,&roe,roi);
}

int ro_result(int cd, struct ro_invoke_rs *roirs, struct ro_indication *roi)
{
	struct ro_event roe;

	roe.event_typ = RO_INVOKE_RESULT;
	roe.cd = cd;
	roe.event.roirs = *roirs;
	return s_message(&ro_connections[roe.cd],&ro_app,&roe,roi);
}

int ro_reject(int cd, struct ro_invoke_rj *roirj, struct ro_indication *roi)
{
	struct ro_event roe;

	roe.event_typ = RO_INVOKE_REJECT;
	roe.cd = cd;
	roe.event.roirj = *roirj;
	return s_message(&ro_connections[roe.cd],&ro_app,&roe,roi);
}

/******************************* EOF ********************************/
