/**********************************************************************
                             R O L I B
                      Remote Operations Library

	Datei:		messages.h
	Version:	1.6
	Datum:		96/11/28 11:49:35

                        (c) CR by Herlitz AG

 ********************************************************************** 

	Funktion:	Header-Definitionen fuer die ROLIB-Message-Funktionen

	Autor:		Destailleur, Lubitz
*/

#ifndef _RO_MESSAGES_H
#define _RO_MESSAGES_H

#ifdef TANDEM
#include "unixincl.rolib"
#else
#include "rolib.h"
#endif

#ifndef WINNT
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#define RO_INIT_MSG(roe) \
{ \
	int j; j=msg_idx[roe->event_typ].von; \
	ro_messages[j++].datap=(void *)common_hd.ma; \
	ro_messages[j++].datap=(void *)&common_hd.len; \
	ro_messages[j++].datap=(void *)&conn->message_nr; \
	ro_messages[j++].datap=(void *)&roe->event_typ; \
	switch(roe->event_typ) { \
	case RO_CONNECT_REQUEST: \
		ro_messages[j++].datap=(void *)roe->event.rocrq.myprovidername; \
		ro_messages[j++].datap=(void *)roe->event.rocrq.myservicename; \
		ro_messages[j++].datap=(void *)roe->event.rocrq.providername; \
		ro_messages[j++].datap=(void *)roe->event.rocrq.servicename; \
		ro_messages[j++].datap=(void *)&roe->event.rocrq.roc.mode; \
		ro_messages[j++].datap=(void *)&roe->event.rocrq.roc.escape; \
		ro_messages[j++].datap=(void *)&roe->event.rocrq.level; \
		ro_messages[j++].datap=(void *)&roe->event.rocrq.id; \
		break; \
	case RO_CONNECT_CONFIRMATION: \
		ro_messages[j++].datap=(void *)&roe->event.roccf.id; \
		break; \
	case RO_CONNECT_REJECT: \
		ro_messages[j++].datap=(void *)&roe->event.rocrj.reason; \
		ro_messages[j++].datap=(void *)&roe->event.rocrj.id; \
		if (ro_buffer) \
			roe->event.rocrj.errtext=ro_buffer; \
		else \
			ro_messages[j].elen=strlen(roe->event.rocrj.errtext)+1; \
		ro_messages[j++].datap=(void *)roe->event.rocrj.errtext; \
		break; \
	case RO_DISCONNECT_REQUEST: \
		ro_messages[j++].datap=(void *)&roe->event.rodrq.id; \
		break; \
	case RO_DISCONNECT_CONFIRMATION: \
		ro_messages[j++].datap=(void *)&roe->event.rodcf.id; \
		break; \
	case RO_DISCONNECT_IMMEDIATE: \
		ro_messages[j++].datap=(void *)&roe->event.rodim.reason; \
		break; \
	case RO_INVOKE_REQUEST: \
		ro_messages[j++].datap=(void *)&roe->event.roirq.op; \
		ro_messages[j++].datap=(void *)&roe->event.roirq.version; \
		ro_messages[j++].datap=(void *)&roe->event.roirq.id; \
		if (ro_buffer) {\
			roe->event.roirq.in=ro_buffer; \
			roe->event.roirq.inlen=0; \
			ro_messages[j].elen=0; \
		}\
		else \
			ro_messages[j].elen=roe->event.roirq.inlen; \
		ro_messages[j].lenp=&roe->event.roirq.inlen; \
		ro_messages[j++].datap=(void *)roe->event.roirq.in; \
		break; \
	case RO_INVOKE_RESULT: \
		ro_messages[j++].datap=(void *)&roe->event.roirs.op; \
		ro_messages[j++].datap=(void *)&roe->event.roirs.version; \
		ro_messages[j++].datap=(void *)&roe->event.roirs.id; \
		if (ro_buffer) {\
			roe->event.roirs.result=ro_buffer; \
			roe->event.roirs.resultlen=0; \
			ro_messages[j].elen=0; \
		}\
		else \
			ro_messages[j].elen=roe->event.roirs.resultlen; \
		ro_messages[j].lenp=&roe->event.roirs.resultlen; \
		ro_messages[j++].datap=(void *)roe->event.roirs.result; \
		break; \
	case RO_INVOKE_REJECT: \
		ro_messages[j++].datap=(void *)&roe->event.roirj.op; \
		ro_messages[j++].datap=(void *)&roe->event.roirj.version; \
		ro_messages[j++].datap=(void *)&roe->event.roirj.reason; \
		ro_messages[j++].datap=(void *)&roe->event.roirj.id; \
		if (ro_buffer) {\
			roe->event.roirj.data=ro_buffer; \
			roe->event.roirs.resultlen=0; \
			ro_messages[j].elen=0; \
		} \
		else \
			ro_messages[j].elen=roe->event.roirj.datalen; \
		ro_messages[j].lenp=&roe->event.roirj.datalen; \
		ro_messages[j++].datap=(void *)roe->event.roirj.data; \
		break; \
	} \
	ro_messages[j].datap=(void *)common_hd.me; \
}

/*
 * Strukturdefinitionen
 */
struct common_header {
	char ma[2];
	unsigned long len;
	int nr;
	unsigned short eventnr;
	char me[2];
};

									/* Interne Struktur der Nachrichten */
struct msg_unit {           
	int             evt;            /* Event-Nr */
	int             flags;          /* Versorgen beim Send oder Receive */
	unsigned long   elen;           /* Externe Data-Laenge */
	int             ityp;           /* Interner Data-Typ */
	unsigned long   *lenp;          /* Pointer auf interne Feld-Laenge */
	void            *datap;         /* Pointer auf internes Feld */
};

struct index  {						/* Index auf Units */
	int von;
	int bis;
};

struct msg_unit_list {				/* Chained List Message Units */
	int len;
	char *val;
	int flags;
	struct msg_unit_list *next;
};

/*
 * Funktionsprototypen
 */
BOOLEAN message_init(void);
int send_message(struct ro_connection *conn,
				 struct ro_application_info *app, struct ro_event *roe,
				 struct ro_indication *roi);
int receive_message(struct ro_connection *conn, int cd,
					struct ro_application_info *app,
					struct ro_event *roe, unsigned long maxlen,
					struct ro_indication *roi);

#endif

/******************************* EOF ********************************/
