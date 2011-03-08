/**********************************************************************
                             R O L I B
                      Remote Operations Library

	Datei:		dirserv.c
	Version:	1.6
	Datum:		96/05/09 14:56:08

                        (c) CR by Herlitz AG

 ********************************************************************** 

	Aufruf:		dirserv [-p <tcp-port>] [-l <logfile>] [-n <myname>] 
	                    [-d <debuglvl]

	Funktion:	Standard-Responder fuer ROLIB-Systeme: Directory-Server
				horcht defaultmaessig auf Port 5555, es sei denn, es
				wurde mit -p ein anderer Port festgelegt.

	Autor:		Destailleur, Lubitz
*/

static char V_dirserv[]="@(#)dirserv.c	1.6 vom 96/05/09  (c) CR by Herlitz AG";

#ifdef TANDEM
#include "unixincl.rolib"
#else
#include "rolib.h"
#endif

#define MYPROVIDERNAME "lokal          "

#define DEFAULT_DEBUGLEVEL 10
#define DEFAULT_PORT 5555
#define EVENTBUFFERSIZE 16*1024


struct ro_indication roi;


void usage(void)
{
	fprintf(stderr,"Usage: dirserv [-p <TCP-Port>] [-l <logfile>]\n"
				   "               [-n <myname>] [-d <debuglvl]\n");
}

void app_err(int status, struct ro_indication *roi)
{
	switch(status)
	{
		case RO_ERROR:
			dprintf(0,"dirserv: error %d: %s\n",roi->code,roi->text);
			fprintf(stderr,"dirserv: error %d: %s\n",roi->code,roi->text);
			exit(1);
			break;
		case RO_WARNING:
			dprintf(1,"dirserv: warning %d: %s\n",roi->code,roi->text);
			fprintf(stderr,"dirserv: warning %d: %s\n",roi->code,roi->text);
			break;
	}
}

int main(int argc, char *argv[])
{
	extern char *optarg;
	int c;
	int status;
	int debuglevel = DEFAULT_DEBUGLEVEL;
	int port = DEFAULT_PORT;
	char myprovidername[16];
	char logfilename[80];
	char event_buffer[EVENTBUFFERSIZE];
	struct ro_event *roe = (struct ro_event *) event_buffer;

	bzero(logfilename,sizeof(myprovidername));
	bzero(logfilename,sizeof(logfilename));
	strncpy(myprovidername,MYPROVIDERNAME,sizeof(myprovidername)-1);
	strncpy(logfilename,"dirserv.log",sizeof(logfilename)-1);

	while((c = getopt(argc, argv, "p:l:n:d:")) != -1)
	{
		switch(c)
		{
			case 'p':
				port = atoi(optarg);
				break;
			case 'l':
				strncpy(logfilename,optarg,sizeof(logfilename)-1);
				break;
			case 'n':
				strncpy(myprovidername,optarg,sizeof(myprovidername)-1);
				break;
			case 'd':
				debuglevel = atoi(optarg);
				break;
			case '?':
				usage();
				exit(1);
		}
	}

	dprintf(2,"dirserv: Initialising ROLIB\n");
	status = ro_init(myprovidername,NULL,NULL,logfilename,debuglevel,port,&roi);
	app_err(status,&roi);

	dprintf(2,"dirserv: Waiting for events\n");
	while((status = ro_wait_event(-1, -1, sizeof(event_buffer),
						(struct ro_event *) event_buffer, &roi)) != RO_ERROR)
	{
		dprintf(2,"dirserv: Application Event %d received.\n",roe->event_typ);
		switch(roe->event_typ)
		{
			case RO_CONNECT_REQUEST:
				{                                        
					struct ro_connect_rj rocrj;          

					rocrj.reason = 0;
					rocrj.errtext = 
						  "Dieser Responder nimmt keine Verbindungen an!";
					rocrj.id = roe->event.rocrq.id;      
					ro_co_reject(roe->cd,&rocrj,&roi);
				}
				break;
		}
	}
	app_err(status,&roi);
	ro_release(&roi);
	return status;
}

/******************************* EOF ********************************/
