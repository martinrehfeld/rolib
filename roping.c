/**********************************************************************
                             R O L I B
                      Remote Operations Library

	Datei:		%M%
	Version:	%I%
	Datum:		%E% %U%

                        (c) CR by Herlitz AG

 ********************************************************************** 

	Aufruf:		ro_ping [-p tcp-port ] [-h <Provider>] [-s <Service>]

	Funktion:	Testclient fuer ROLIB.

	Autor:		Destailleur, Lubitz
*/

static char V_ro_ping[]="%W% vom %D%  (c) CR by Herlitz AG";

#include <stdio.h>
#include "rolib.h"

#define DEFAULT_PORT 6666
#define EVENTBUFFERSIZE 16384

#define MYHOSTNAME    "europa         "
#define MYSERVICENAME "ro_ping        "

#define REMOTE_PROVIDER "europa"
#define REMOTE_SERVICE "dirserv"


struct ro_indication roi;


void usage(void)
{
	fprintf(stderr,"Usage: ro_ping [-p <TCP-Port>] [-h <Provider>] [-s <Service>]\n");
}

void app_err(int status, struct ro_indication *roi)
{
	switch(status)
	{
		case RO_ERROR:
			fprintf(stderr,"ro_ping: error %d: %s\n",roi->code,roi->text);
			exit(1);
			break;
		case RO_WARNING:
			fprintf(stderr,"ro_ping: warning %d: %s\n",roi->code,roi->text);
			break;
	}
}

void do_ping(int cd)
{
	static unsigned long id = 1;
	struct ro_invoke_rq roirq;
	int status;

	roirq.op = RO_STD_OP_PING;
	roirq.version = RO_STD_VERSION_PING;
	roirq.id = id++;
	roirq.in = "Hello, this is ro_ping speaking...";
	roirq.inlen = strlen(roirq.in);
	printf("Sending ping (%lu) with %lu bytes.\n",roirq.id,roirq.inlen);
	status = ro_request(cd,&roirq,&roi);
	app_err(status,&roi);
}

int main(int argc, char *argv[])
{
#ifndef BS2000
	extern char *optarg;
#endif
	int c;
	int cd;
	int status;
	char remote_provider[16], remote_service[16];
	int port = DEFAULT_PORT;
	struct ro_connect_rq rocrq;
	char event_buffer[EVENTBUFFERSIZE];
	struct ro_event *roe = (struct ro_event *) event_buffer;

	strncpy(remote_provider,REMOTE_PROVIDER,15);
	strncpy(remote_service,REMOTE_SERVICE,15);
	remote_provider[15] = remote_service[15] = '\0';

#ifndef BS2000
	while((c = getopt(argc, argv, "p:h:s:")) != -1)
	{
		switch(c)
		{
			case 'p':
				port = atoi(optarg);
				break;
			case 'h':
				strncpy(remote_provider,optarg,15);
				break;
			case 's':
				strncpy(remote_service,optarg,15);
				break;
			case '?':
				usage();
				exit(1);
		}
	}
#endif

	status = ro_init("roping.log",10,port,&roi);
	app_err(status,&roi);

	strncpy(rocrq.myhostname,MYHOSTNAME,15);
	strncpy(rocrq.myservicename,MYSERVICENAME,15);
	rocrq.level = RO_ADMIN_LEVEL;
	rocrq.roc.mode = RO_MODE_TRANSPARENT;
	rocrq.roc.escape = RO_DEFAULT_ESC;
	rocrq.id = 1;
dprintf(1,"ro_ping: Connecting...\n");
	status = ro_connect(remote_provider,remote_service,&rocrq,&cd,&roi);
	app_err(status,&roi);

dprintf(1,"ro_ping: Waiting...\n");
	while((status = ro_wait_event(-1, -1, sizeof(event_buffer),
						(struct ro_event *) event_buffer, &roi)) == RO_OK)
	{
		if(status != RO_OK)
		{
			app_err(status,&roi);
			break;
		}
		switch(roe->event_typ)
		{
			case RO_CONNECT_CONFIRMATION:
				cd = roe->cd;
				do_ping(cd);
				break;
			case RO_CONNECT_REJECT:
				puts("ro_ping: Connection rejected, exiting...");
				exit(1);
				break;
			case RO_INVOKE_RESULT:
				printf("Pong (%lu) with %lu bytes received.\n",
					   roe->event.roirs.id, roe->event.roirs.resultlen);
				sleep(1);
				do_ping(cd);
				break;
			case RO_INVOKE_REJECT:
				puts("ro_ping: Invocation rejected, exiting...");
				exit(1);
				break;
			default:
				dprintf(2,"Unhandled event received (%d), ignoring...\n",
				       roe->event_typ);
				break;
		}
	}
	ro_release(&roi);
	return 0;
}

/******************************* EOF ********************************/
