/**********************************************************************
                             R O L I B
                      Remote Operations Library

	Datei:		messages.c
	Version:	1.5
	Datum:		96/11/28 11:47:59

                        (c) CR by Herlitz AG

 ********************************************************************** 

	Funktion:	Message-Funktionen fuer die ROLIB

	Autor:		Destailleur, Lubitz

 Message-Aufbau
 --------------
 Die einzelnen Messages sind in Units unterteilt, die einen C-Typ besitzen.
 Pro Message darf nur ein Unit vom Typ VCHAR oder DATA auftreten und zwar 
 an der vorletzten Stelle der Message !!!
 
 Send:  Entsprechend Aufbau der Message-Units ro_messages
        (Messages bestehen aus Units)
        - Speicherallokierung laut externe Ldnge der Message-Units-Strukturen 
        - Konvertierung der Eingabe-Daten in die Message-Unit->Daten
          unter Ber|cksichtigung des C-Typs, der UNIT_MAXLEN (ev. Zerlegung)
        - Escape-Bearbeitung der Units-Daten
        - Nach Berabeitung aller bettroffenen Message-Units versorgen folgender 
          Message-Felder:
        	  * MA
        	  * Message-Ldnge
        	  * Message-Nr
        	  * ME
        - Entladen und Senden |ber Socket der Unit-Liste mit Blvcken vom
          max. Ldnge TCP_MAXLEN
        - Freigabe des List-Speichers
        - Rueckgabe der Anzahl von gelesenen Bytes oder -1 by Fehler
        
 Rec.:  Entsprechend Aufbau der Message-units-Tabelle
        - Lesen aus Socket in der gemeinsamen Header-Laenge
        - Konvertieren und kopieren der Header-Daten (u.a. Message-Laenge)
        - Unitweise Lesen von Maximum TCP_MAXLEN Bytes und 
          kopieren direkt in die Ausgabe-Daten
        - Berechnen der Rest-Laenge fuer Unittyp _VCHAR oder _DATA
*/
 
static char V_messages[]="@(#)messages.c	1.5 vom 96/11/28  (c) CR by Herlitz AG";
 
#ifdef TANDEM
#include "unixincl.messages"
#else
#include "messages.h"
#endif
 
#define TCP_MAXLEN  1024
#define UNIT_MAXLEN 1024*4
 
#define _CHAR   0x01	    /* Interne C-Datentypen */
#define _VCHAR  0x02
#define _DATA   0x04
#define _USHORT 0x08
#define _INT    0x10
#define _ULONG  0x20
 
							/* Flags zur Steuerung der Bearbeitung */
#define _RCV    0x0001	    /* - Steuerung der Grund-Bearbeitung (I/O) */
#define _SND    0x0002	    
 
#define _MMA    0x0004	    /* - Gemeinsame Message units */
#define _LEN    0x0008
#define _MES    0x0010
#define _EVT    0x0020
#define _MME    0x0040
 
#define _ESC    0x0100
#define _ASC    0x0200		/* Immer ASCII im Message */
#define _COD    0x0400		/* Unveraendert, wenn transparent */
 
#define MAXEVENT 9
 
#define DEBUG_MSG_ERR 7
#define DEBUG_MSG_TCP 8
#define DEBUG_MSG_ALL 9

#define ERR_MSG_ALLOC 	100
#define ERR_MSG_TCP		101
#define ERR_MSG_ROLIB	102
 
#define _LINE "-----------------------------------------------------------\n"

static struct common_header common_hd;
 
static struct msg_unit ro_messages[] = 
{
	/* Gemeinsamer Header */
	{0, _SND| _MMA| _ASC, 2L, _CHAR,   NULL, common_hd.ma       },
	{0, _SND| _LEN| _ASC,10L, _ULONG,  NULL, &common_hd.len     },
	{0, _SND| _MES| _ASC, 6L, _ULONG,  NULL, &common_hd.nr      },
	{0, _SND| _EVT| _ASC, 2L, _USHORT, NULL, &common_hd.eventnr },
 
	/* Verbindungsaufforderung */
	{1, _SND| _MMA| _ASC, 2L, _CHAR,   NULL, common_hd.ma       },
	{1, _SND| _LEN| _ASC,10L, _ULONG,  NULL, &common_hd.len     },
	{1, _SND| _MES| _ASC, 6L, _ULONG,  NULL, NULL               },
	{1, _SND| _EVT| _ASC, 2L, _USHORT, NULL, NULL               },
	{1, _SND| _RCV| _ASC,15L, _CHAR,   NULL, NULL               },
	{1, _SND| _RCV| _ASC,15L, _CHAR,   NULL, NULL               },
	{1, _SND| _RCV| _ASC,15L, _CHAR,   NULL, NULL               },
	{1, _SND| _RCV| _ASC,15L, _CHAR,   NULL, NULL               },
	{1, _SND| _RCV| _ASC, 1L, _INT,    NULL, NULL               },
	{1, _SND| _RCV| _ASC, 1L, _CHAR,   NULL, NULL               },
	{1, _SND| _RCV| _ASC, 1L, _INT,    NULL, NULL               },
	{1, _SND| _RCV| _ASC, 6L, _ULONG,  NULL, NULL               },
	{1, _SND| _MME| _ASC, 2L, _CHAR,   NULL, common_hd.me       },
		
	/* Verbindungs-Bestaetigung */
	{2, _SND| _MMA| _ASC, 2L, _CHAR,   NULL, common_hd.ma       },
	{2, _SND| _LEN| _ASC,10L, _ULONG,  NULL, &common_hd.len     },
	{2, _SND| _MES| _ASC, 6L, _ULONG,  NULL, NULL               },
	{2, _SND| _EVT| _ASC, 2L, _USHORT, NULL, NULL               },
	{2, _SND| _RCV| _ASC, 6L, _ULONG,  NULL, NULL               },
	{2, _SND| _MME| _ASC, 2L, _CHAR,   NULL, common_hd.me       },
 
	/* Verbindungsabweisung */
	{3, _SND| _MMA| _ASC, 2L, _CHAR,   NULL, common_hd.ma       },
	{3, _SND| _LEN| _ASC,10L, _ULONG,  NULL, &common_hd.len     },
	{3, _SND| _MES| _ASC, 6L, _ULONG,  NULL, NULL               },
	{3, _SND| _EVT| _ASC, 2L, _USHORT, NULL, NULL               },
	{3, _SND| _RCV| _ASC, 4L, _INT,    NULL, NULL               },
	{3, _SND| _RCV| _ASC, 6L, _ULONG,  NULL, NULL               },
	{3, _SND| _RCV| _ASC, 0L, _VCHAR,  NULL, NULL               },
	{3, _SND| _MME| _ASC, 2L, _CHAR,   NULL, common_hd.me       },
 
	/* Disconnect Aufforderung */
	{4, _SND| _MMA| _ASC, 2L, _CHAR,   NULL, common_hd.ma       },
	{4, _SND| _LEN| _ASC,10L, _ULONG,  NULL, &common_hd.len     },
	{4, _SND| _MES| _ASC, 6L, _ULONG,  NULL, NULL               },
	{4, _SND| _EVT| _ASC, 2L, _USHORT, NULL, NULL               },
	{4, _SND| _RCV| _ASC, 6L, _ULONG,  NULL, NULL               },
	{4, _SND| _MME| _ASC, 2L, _CHAR,   NULL,  common_hd.me      },
 
	/* Disconnect  Bestaetigung */
	{5, _SND| _MMA| _ASC, 2L, _CHAR,   NULL, common_hd.ma       },
	{5, _SND| _LEN| _ASC,10L, _ULONG,  NULL, &common_hd.len     },
	{5, _SND| _MES| _ASC, 6L, _ULONG,  NULL, NULL               },
	{5, _SND| _EVT| _ASC, 2L, _USHORT, NULL, NULL               },
	{5, _SND| _RCV| _ASC, 6L, _ULONG,  NULL, NULL               },
	{5, _SND| _MME| _ASC, 2L, _CHAR,   NULL, common_hd.me       },
 
	/* Disconnect  Immediate */
	{6, _SND| _MMA| _ASC, 2L, _CHAR,   NULL, common_hd.ma       },
	{6, _SND| _LEN| _ASC,10L, _ULONG,  NULL, &common_hd.len     },
	{6, _SND| _MES| _ASC, 6L, _ULONG,  NULL, NULL               },
	{6, _SND| _EVT| _ASC, 2L, _USHORT, NULL, NULL               },
	{6, _SND| _RCV| _ASC, 6L, _ULONG,  NULL, NULL               },
	{6, _SND| _MME| _ASC, 2L, _CHAR,   NULL,  common_hd.me      },
 
	/* Invoke Request */
	{7, _SND| _MMA| _ASC, 2L, _CHAR,   NULL, common_hd.ma       },
	{7, _SND| _LEN| _ASC,10L, _ULONG,  NULL, &common_hd.len     },
	{7, _SND| _MES| _ASC, 6L, _ULONG,  NULL, NULL               },
	{7, _SND| _EVT| _ASC, 2L, _USHORT, NULL, NULL               },
	{7, _SND| _RCV| _ASC, 4L, _INT,    NULL, NULL               },
	{7, _SND| _RCV| _ASC, 2L, _INT,    NULL, NULL               },
	{7, _SND| _RCV| _ASC, 6L, _ULONG,  NULL, NULL               },
	{7, _SND| _RCV| _COD, 0L, _DATA,   NULL, NULL               },
	{7, _SND| _MME| _ASC, 2L, _CHAR,   NULL, common_hd.me       },
 
	/* Invoke Result */
	{8, _SND| _MMA| _ASC, 2L, _CHAR,   NULL, common_hd.ma       },
	{8, _SND| _LEN| _ASC,10L, _ULONG,  NULL, &common_hd.len     },
	{8, _SND| _MES| _ASC, 6L, _ULONG,  NULL, NULL               },
	{8, _SND| _EVT| _ASC, 2L, _USHORT, NULL, NULL               },
	{8, _SND| _RCV| _ASC, 4L, _INT,    NULL, NULL               },
	{8, _SND| _RCV| _ASC, 2L, _INT,    NULL, NULL               },
	{8, _SND| _RCV| _ASC, 6L, _ULONG,  NULL, NULL               },
	{8, _SND| _RCV| _COD, 0L, _DATA,   NULL, NULL               },
	{8, _SND| _MME| _ASC, 2L, _CHAR,   NULL, common_hd.me       },
 
	/* Invoke Reject */
	{9, _SND| _MMA| _ASC, 2L, _CHAR,   NULL, common_hd.ma       },
	{9, _SND| _LEN| _ASC,10L, _ULONG,  NULL, &common_hd.len     },
	{9, _SND| _MES| _ASC, 6L, _ULONG,  NULL, NULL               },
	{9, _SND| _EVT| _ASC, 2L, _USHORT, NULL, NULL               },
	{9, _SND| _RCV| _ASC, 4L, _INT,    NULL, NULL               },
	{9, _SND| _RCV| _ASC, 2L, _INT,    NULL, NULL               },
	{9, _SND| _RCV| _ASC, 4L, _INT,    NULL, NULL               },
	{9, _SND| _RCV| _ASC, 6L, _ULONG,  NULL, NULL               },
	{9, _SND| _RCV| _COD, 0L, _DATA,   NULL, NULL               },
	{9, _SND| _MME| _ASC, 2L, _CHAR,   NULL, common_hd.me       },
 
	/* Tab-EOF */
	{10, 0	 , 0L, _INT ,   NULL, NULL                          }
 
};	/* --> Ende Send-Message-Structure */
 
static int  ebcdic_ascii7[]  = 
{
/*	   0     1     2     3     4     5     6     7   */
/*	   8     9     A     B     C     D     E     F   */
	0x00, 0x01, 0x02, 0x03, 0x00, 0x09, 0x00, 0x7f,  /*  0 */
	0x00, 0x00, 0x00, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,

	0x10, 0x11, 0x12, 0x13, 0x00, 0x0a, 0x08, 0x00,  /*  1 */
	0x18, 0x19, 0x00, 0x00, 0x1c, 0x1d, 0x1e, 0x1f,

	0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x17, 0x1b,  /*  2 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x06, 0x07,

	0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x00, 0x04,  /*  3 */
	0x00, 0x00, 0x00, 0x00, 0x14, 0x15, 0x00, 0x1a,

	0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /*  4 */
	0x00, 0x24, 0x60, 0x2e, 0x3c, 0x28, 0x2b, 0xf6,

	0x26, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /*  5 */
	0x00, 0x00, 0x21, 0x24, 0x2a, 0x29, 0x3b, 0x00,

	0x2d, 0x2f, 0x40, 0x5b, 0x5d, 0x00, 0x00, 0x7e,  /*  6 */
	0x00, 0x00, 0x5e, 0x2c, 0x25, 0x5f, 0x3e, 0x3f,
 
/*	   0     1     2     3     4     5     6     7   */
/*	   8     9     A     B     C     D     E     F   */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /*  7 */
	0x00, 0x60, 0x3a, 0x23, 0x40, 0x27, 0x3d, 0x22,

	0x00, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,  /*  8 */
	0x68, 0x69, 0x00, 0x5b, 0x5c, 0x5d, 0x00, 0x00,

	0x00, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70,  /*  9 */
	0x71, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0x00, 0x7e, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,  /*  A */
	0x79, 0x7a, 0x00, 0x7b, 0x7c, 0x7d, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /*  B */
	0x00, 0x00, 0x00, 0xc4, 0xd6, 0xdc, 0x00, 0x00,

	0x7b, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,  /*  C */
	0x48, 0x49, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0x7d, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,  /*  D */
	0x51, 0x52, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0x5c, 0x00, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,  /*  E */
	0x59, 0x5a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,  /*  F */
	0x38, 0x39, 0x00, 0xe4, 0x00, 0xfc, 0x00, 0xdf
};
 
static int ascii7_ebcdic[]  = 
{
/*	   0     1     2     3     4     5     6     7   */
/*	   8     9     A     B     C     D     E     F   */
	0x00, 0x01, 0x02, 0x03, 0x37, 0x2d, 0x2e, 0x2f,  /*  0 */
	0x16, 0x05, 0x25, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,

	0x10, 0x11, 0x12, 0x13, 0x3c, 0x3d, 0x32, 0x26,  /*  1 */
	0x18, 0x19, 0x3f, 0x27, 0x1c, 0x1d, 0x1e, 0x1f,

	0x40, 0x5a, 0x7f, 0x7b, 0x5b, 0x6c, 0x50, 0x7d,  /*  2 */
	0x4d, 0x5d, 0x5c, 0x4e, 0x6b, 0x60, 0x4b, 0x61,

	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,  /*  3 */
	0xf8, 0xf9, 0x7a, 0x5e, 0x4c, 0x7e, 0x6e, 0x6f,

	0x7c, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,  /*  4 */
	0xc8, 0xc9, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6,

	0xd7, 0xd8, 0xd9, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6,  /*  5 */
	0xe7, 0xe8, 0xe9, 0xbb, 0xbc, 0xbd, 0x6a, 0x6d,

	0x4a, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,  /*  6 */
	0x88, 0x89, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96,

/*	   0     1     2     3     4     5     6     7   */
/*	   8     9     A     B     C     D     E     F   */
	0x97, 0x98, 0x99, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6,  /*  7 */
	0xa7, 0xa8, 0xa9, 0xfb, 0x4f, 0xfd, 0x67, 0x07,

	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /*  8 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /*  9 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /*  A */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /*  B */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, 0xbb, 0x00, 0x00, 0x00,  /*  C */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xbc, 0x00,  /*  D */
	0x00, 0x00, 0x00, 0x00, 0xbd, 0x00, 0x00, 0xff,

	0x00, 0x00, 0x00, 0x00, 0xfb, 0x00, 0x00, 0x00,  /*  E */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4f, 0x00,  /*  F */
	0x00, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x00, 0x00
};
 
static struct index msg_idx[MAXEVENT+1]; /* Index-Tabelle auf message Units */

BOOLEAN indexing (struct msg_unit structure[], struct index idx[])
{
	int i = 0,j;
	BOOLEAN rc=TRUE;
 
	while (structure[i].evt<MAXEVENT+1) {
		idx[structure[i].evt].von = j = i;
		while(structure[j].evt<MAXEVENT+1 && structure[i].evt==structure[j].evt)
			idx[structure[i].evt].bis = j++;
		i=j;
	}
			/* if (structure[j].evt<structure[i].evt)
				rc=FALSE; */
	return(rc);
}

BOOLEAN message_init() 
{
	if (!indexing(ro_messages, msg_idx)) {
		dprintf(DEBUG_MSG_ERR,"Messages.c: Index auf Message-Units nicht angelegt\n");
		return(FALSE);
	}
	return(TRUE);
}
		

/*
expand_esc(msg_unit_list *u, char esc) 
{
	int i;
	int nesc = 0;
 
	for (i=0;i<u->len; i++)
		if (u->val+i==esc)
			nesc++;
 
	if (u->val=realloc(u->val,u->len+nesc)==NULL) {
		dprintf(DEBUG_MSG_ERR, "Messages.c: Malloc failed to alloc %d Bytes\n",sizeof(struct msg_unit_list));
		return(-1);
	}
 
	i=0;
	while (i<u->len) {
		if (*(u->val+i)==esc) {
			for (j=u->len-1;j>i;j--)
				*(u->val+j+1)=*(u->val+j);
			(u->len)++;
			*(u->val+(++i))=esc;
		}
		i++;
	}
}
 
delete_esc(msg_unit_list *u, char esc) 
{
	int i = 0,j;
	boolean in_double_esc = 0;

	while (i<u->len) {
		if (*(u->val+i)==esc)
			if (in_double_esc) {
				for (j=i;j<u->len-1;j++)
					*(u->val+j)=*(u->val+j+1);
				u->len--;
				in_double_esc=FALSE;
			}
			else
				in_double_esc=TRUE;
		i++;
	}
}
*/
 
/**************************************************************************
 * - Erweitert die Liste um ein Element  (u)
 * - Allokiert den Data-Bereich u->val in der Laenge nalloc+1
 **************************************************************************/
struct msg_unit_list *alloc_msg_unit(int nalloc) 
{
	char *s;
	size_t n = sizeof(struct msg_unit_list);
	struct msg_unit_list *u;
			
						/* Alloc List-Element */ 
	u=(struct msg_unit_list *)malloc(n);     
 
	if (u==NULL) {
		dprintf(DEBUG_MSG_ERR, "Messages.c: Malloc failed to alloc %d Bytes\n",n);
		return(u);
	}
	else {
		n=nalloc+1;
		if ((s=(char *)malloc(n))==NULL) {
			dprintf(DEBUG_MSG_ERR, "Messages.c: Malloc failed to alloc %d Bytes\n",nalloc);
			return(NULL);
		}
		u->len=nalloc;		  /* Struktur aktualisieren */
		u->val=s;
		u->next=NULL;			   
	}	  
	return(u); 
}
 
int convert_char(int in, int tocode)
{       
	if (tocode==RO_ASCII)
		return(ebcdic_ascii7[in & 0xff]);
	else if (tocode==RO_EBCDIC)
		return(ascii7_ebcdic[in & 0xff]);
	else
		return(in & 0xff);
}
 
char *convert_str(char *s, int len, int tocode) 
{
	int i = 0;

	while (i<len) {
		*(s+i)=convert_char((int)*(s+i), tocode);
		i++;
	}
	return(s);
}
										
convert_code_send(struct msg_unit_list *u, int mycode, int mode) 
{
	if (mycode==RO_EBCDIC)
		if (u->flags & _ASC)
			convert_str(u->val, u->len, RO_ASCII);
		else if (u->flags & _COD)
			if (!mode)	      /* Nicht transparent */
				convert_str(u->val, u->len, RO_ASCII);
}
 
convert_code_recv(char *s, int len, int flags, int mycode, int mode) 
{
	if (mycode==RO_EBCDIC)
		if (flags & _ASC)
			convert_str(s, len, RO_EBCDIC);
		else if (flags & _COD)
			if (!mode)	      /* Nicht transparent */
				convert_str(s, len, RO_EBCDIC);
}
 
struct msg_unit_list *add_msg_unit(struct msg_unit *mes_unit, int nalloc, void *datap) 
{
	char format[15], *s;
	int  *int_val;
	unsigned short *short_val;
	unsigned long  *long_val;
	struct msg_unit_list *unit_l;
 
	if ((unit_l=alloc_msg_unit(nalloc)) == NULL)
		return NULL;
 
	s=format;
	if (mes_unit->ityp & _INT) {
		int_val=(int *)datap; 
		sprintf(s,"%05d",*int_val);  
		s=s+strlen(s)-nalloc;	 
		bcopy(s, unit_l->val, nalloc);
	}
	else if (mes_unit->ityp & _USHORT) {
		short_val=(unsigned short *)datap;
		sprintf(s,"%05u",*short_val);
		s=s+strlen(s)-nalloc;	  
		bcopy(s, unit_l->val, nalloc); 
	}
	else if (mes_unit->ityp & _ULONG) {
		long_val=(unsigned long *)datap;
		sprintf(s,"%010lu",*long_val);
		s=s+strlen(s)-nalloc;	  
		bcopy(s, unit_l->val, nalloc); 
	}
	else if (mes_unit->ityp & _CHAR) {
		bcopy((char *)datap,unit_l->val,nalloc);
	}
	else if (mes_unit->ityp & _VCHAR) {
		strcpy(unit_l->val,(char *)datap);
	}
	else if (mes_unit->ityp & _DATA) {
		bcopy((char *)datap,unit_l->val,nalloc);
	}
 
	if (mes_unit->ityp & _INT)
		dprintf(DEBUG_MSG_ALL, "\t(OUT)\tINT   :%d:\n", *int_val);
	else if (mes_unit->ityp & _USHORT) 
		dprintf(DEBUG_MSG_ALL, "\t(OUT)\tUSHORT:%u:\n", *short_val);
	else if (mes_unit->ityp & _ULONG)
		dprintf(DEBUG_MSG_ALL, "\t(OUT)\tLONG  :%lu:\n", *long_val);
	else if (mes_unit->ityp & _CHAR)
		dprintf(DEBUG_MSG_ALL, "\t(OUT)\tCHAR  :%*.*s:\n", nalloc, nalloc, unit_l->val);
	else if (mes_unit->ityp & _VCHAR)
		dprintf(DEBUG_MSG_ALL, "\t(OUT)\tVCHAR :%*.*s:\n", nalloc, nalloc, unit_l->val);
	else if (mes_unit->ityp & _DATA)
		dprintf(DEBUG_MSG_ALL, "\t(OUT)\tDATA  :%*.*s:\n",nalloc,nalloc,unit_l->val);
	return(unit_l);
}
		
unit_list_free (struct msg_unit_list *u) 
{
	struct msg_unit_list *udel;
 
	while (u) {
		udel=u;
		u=u->next;
		free(udel->val);
		free(udel); 
	}
}
 
int send_tcp_frame (int sd , char *tcp_frame, int nframe, struct ro_indication *roi) 
{
	int n = 0;
	int nwrite = 0;	/* schon geschrieben = 0 */
 
	dprintf(DEBUG_MSG_ALL, _LINE);
	dprintf(DEBUG_MSG_TCP, "\tTCP_FRAME zu senden(%d Bytes)\n",nframe);
	dprintf(DEBUG_MSG_TCP, "\t:%*.*s:\n",nframe,nframe,tcp_frame);

	while (n<nframe) 
	{
		if ((n%21)==0)
			dprintf(DEBUG_MSG_ALL, "\n\t:");
		dprintf(DEBUG_MSG_ALL, "%x:",(int)*(tcp_frame+n++) & 0xff);
	}; 
	dprintf(DEBUG_MSG_ALL,"\n");
 
	do {
#ifdef TANDEM
		n=send_nw(sd, &tcp_frame[nwrite], nframe-nwrite, 0, 1);
		if (n!=0) 
		{
			dprintf(0, "Messages.c: send_nw failed :%s:\n",strerror(errno));
			roi->code=ERR_MSG_TCP;
			sprintf(roi->text, "send_nw failed :%s:", strerror(errno));
			return(-1);
		}
		{
			struct send_nw_str *snw;
			int cc, tag;
			short err;

			if (AWAITIOX(&sd,(char *) &snw, &cc, &tag, -1L)!=CCE) 
			{
				roi->code=ERR_MSG_TCP;
				sprintf(roi->text, "AWAITIOX for send_nw failed:", strerror(err));
				if (FILE_GETINFO_(sd, &err)==0)
				{
					dprintf(0, "Messages.c: AWAITIOX for send_nw failed :%s:\n",strerror(err));
					strncat(roi->text, strerror(err),160);
				}
				return(-1);
			}
			n = snw->nb_sent;
		}
#else
		n=SOC_SEND(sd, &tcp_frame[nwrite], nframe-nwrite, 0);
#endif
		if (n==-1) {
			dprintf(DEBUG_MSG_TCP, "Messages.c: SOC_SEND failed :%s:\n",strerror(errno));
			roi->code=ERR_MSG_TCP;
			sprintf(roi->text, "SOC_SEND failed :%s:", strerror(errno));
			return(-1);
		}
		else if (n==0) {
			dprintf(DEBUG_MSG_TCP, "Messages.c: SOC_SEND EOF :%s:\n",strerror(errno));
			roi->code=ERR_MSG_TCP;
			sprintf(roi->text, "SOC_SEND EOF :%s:", strerror(errno));
			return(0);
		}

		nwrite += n;    /* schon auf Socket geschrieben */
	} while (nwrite < nframe);

	dprintf(DEBUG_MSG_ALL, _LINE);
	return (nwrite);	/* Bytes gesendet */
}
 
int send_tcp_message(struct ro_connection *conn, int mycode, struct msg_unit_list *unit_list, struct ro_indication *roi) 
{
	/* Chained List Message Units */
	struct msg_unit_list *unit_l;
	char dummy[2*TCP_MAXLEN+4]; 
	char *tcp_buf = dummy+4;
#define TCP_EMPTY  0
#define TCP_FILLED 1
#define TCP_FULL   2
	int buffer_status = TCP_EMPTY;
	int copy_len;
	unsigned long unit_pos, tcpbuf_pos = 0L;
	long nsent = 0L;
 
	/* Liste entladen, konvertieren und Message aufbauen */
	for (unit_l=unit_list;unit_l;unit_l=unit_l->next) {
		convert_code_send(unit_l, mycode, conn->params.mode);
	 
		unit_pos = 0L;
	 
		while (unit_pos < unit_l->len) {
			/* Vorhandener Platz fuer Daten im TCP-Buffer */
			copy_len = min(unit_l->len - unit_pos,TCP_MAXLEN - tcpbuf_pos);
	 
			if (copy_len==TCP_MAXLEN - tcpbuf_pos)
				buffer_status = TCP_FULL;
			else
				buffer_status = TCP_FILLED;
	 
			if (copy_len) {
				bcopy(unit_l->val+unit_pos,tcp_buf+tcpbuf_pos,copy_len);
				unit_pos += copy_len;
				tcpbuf_pos  += copy_len;
			}
	 
			if (buffer_status==TCP_FULL)
				if (send_tcp_frame(conn->sock, tcp_buf, tcpbuf_pos, roi)!=tcpbuf_pos)
					return(RO_ERROR);
				else {
					nsent += tcpbuf_pos;
					tcpbuf_pos=0L;
					buffer_status = TCP_EMPTY;
				}
		}
	}
 
	if (buffer_status!=TCP_EMPTY)
		if (send_tcp_frame(conn->sock, tcp_buf, tcpbuf_pos, roi)!=tcpbuf_pos)
				return(RO_ERROR);
		else {
			buffer_status = TCP_EMPTY;
			nsent += tcpbuf_pos;
		}
	unit_list_free(unit_list);
	/* Anzahl uebertragener Bytes steht in nsent */
	return(RO_OK);
}
 
update_send_len(struct msg_unit_list *u, unsigned long len) 
{
	while (u && !(u->flags & _LEN))
		u=u->next;
 
	if (u)
		sprintf(u->val,"%010lu",len);
	else
	{
		dprintf(DEBUG_MSG_ERR, "Messages.c: Message-Laenge nicht versorgt\n");
		return(-1);
	}
	return(0);
}
 
int send_message(struct ro_connection *conn, struct ro_application_info *ro_app, struct ro_event *roe, struct ro_indication *roi)
{
	char msg_esc;
	struct msg_unit_list *ro_send_list, *unitcur, *unitnxt;
 
	int ntoalloc, i;
	unsigned long nallocated;
 
	static void  *bufp;
	char  *ro_buffer;
 
	conn->message_nr++;
 
	msg_esc = roe->event_typ <= 3 ? RO_DEFAULT_ESC : conn->params.escape;
 
	common_hd.ma[0]=msg_esc; 
	common_hd.me[0]=msg_esc;
	common_hd.ma[1]='['; 
	common_hd.me[1]=']';
	common_hd.len=0L;
 
	ro_buffer=NULL;
	
	RO_INIT_MSG(roe)
 
	ro_send_list = NULL;
 
       	dprintf(DEBUG_MSG_ALL, _LINE);
	dprintf(DEBUG_MSG_TCP, "--> SEND-EVENT: %d\n",roe->event_typ);
 
	for (i=msg_idx[roe->event_typ].von; i<=msg_idx[roe->event_typ].bis; i++) {
		if (ro_messages[i].flags & _SND) {
 
			nallocated=0;
 
			bufp=ro_messages[i].datap;
 
			while (ro_messages[i].elen > nallocated) {
 
				ntoalloc = min(ro_messages[i].elen - nallocated, UNIT_MAXLEN);
 
				unitnxt=add_msg_unit(&ro_messages[i], ntoalloc, bufp);
 
				if (unitnxt) {
 
					nallocated += unitnxt->len;
					bufp = (char *)bufp + unitnxt->len;
 
					unitnxt->flags=ro_messages[i].flags;
 
					if (!ro_send_list)
						ro_send_list = unitcur = unitnxt;
					else {
						unitcur->next = unitnxt;
						unitcur=unitcur->next;
					}
 
				}
				else
				{
					roi->code=ERR_MSG_ALLOC;
					sprintf(roi->text,"Failed to alloc %d Bytes",ntoalloc);
					return(RO_ERROR);
				}
			}
			common_hd.len += nallocated;
		}
	}
 
	/* Ext. Message Laenge anpassen */
	if (update_send_len(ro_send_list,common_hd.len)<0)
	{
		roi->code=ERR_MSG_ALLOC;
		sprintf(roi->text,"Failed to update length");
		return(RO_ERROR);
	}
 
	dprintf(DEBUG_MSG_ERR, "\tMessage gebildet, %5lu Bytes\n",common_hd.len);
	
	return(send_tcp_message(conn, ro_app->charset, ro_send_list, roi));
}
 
int recv_tcp_frame(int sd, char *tcp_frame, int nlen, struct ro_indication *roi) 
{
	int nread = 0;			/* schon gelesen = 0 */
	int n;

	dprintf(DEBUG_MSG_ALL, _LINE);
 
	do {
#ifdef TANDEM
		n=recv_nw(sd, &tcp_frame[nread], nlen-nread, 0, 1);
		if (n!=0) 
		{
			dprintf(0, "Messages.c: recv_nw failed :%s:\n",strerror(errno));
			roi->code=ERR_MSG_TCP;
			sprintf(roi->text, "recv_nw failed :%s:", strerror(errno));
			return(-1);
		}
		{
			long buffer_addr;
			short err;
			if(AWAITIOX(&sd,&buffer_addr,&n) != CCE)
			{
				roi->code=ERR_MSG_TCP;
				sprintf(roi->text, "AWAITIOX auf recv_nw failed:");
				if (FILE_GETINFO_(sd, &err)==0)
				{
					dprintf(0, "Messages.c: AWAITIOX auf recv_nw failed :%s:\n",strerror(err));
					strncat(roi->text, strerror(err),160);
				}
				return(-1);
			}
		}
#else
		n=SOC_RECV(sd, &tcp_frame[nread], nlen-nread, 0);
#endif
		if (n==-1) 
		{
			dprintf(DEBUG_MSG_ERR, "Messages.c: SOC_RECV failed :%s:\n",strerror(errno));
			roi->code=ERR_MSG_TCP;
			sprintf(roi->text, "SOC_RECV failed :%s:", strerror(errno));
			return(-1);
		} 
		else if (n==0) 
		{
			dprintf(DEBUG_MSG_ERR, "Messages.c: SOC_RECV EOF :%s:\n",strerror(errno));
			roi->code=ERR_MSG_TCP;
			sprintf(roi->text, "SOC_RECV EOF :%s:", strerror(errno));
			return(0);
		} 
		dprintf(DEBUG_MSG_TCP, "\tTCP_FRAME received (%d Bytes):\t:%*.*s:\n",n,n,n,&tcp_frame[nread]);
		nread += n;     /* schon von Socket gelesen */

	} while (nread < nlen);

	dprintf(DEBUG_MSG_ALL, _LINE);
	return (nread);		 /* Bytes gelesen */
}
 
int move_recv_unit(char *recv_buf, struct msg_unit *mes_unit, void *datap, int len) 
{
	int copy_len;
	unsigned long *p_long;
	unsigned short *p_short;
	int *p_int;
	char conv[12];
	
	if (mes_unit->ityp & _CHAR) {
		copy_len=mes_unit->elen;
		bcopy(recv_buf, (char *)datap, copy_len);
	}
	else if (mes_unit->ityp & _DATA) {
		copy_len=len;
		bcopy(recv_buf, (char *)datap, copy_len);
		*mes_unit->lenp += copy_len;
	}
	else if (mes_unit->ityp & _VCHAR) {
		copy_len=strlen(recv_buf)+1;
		strcpy((char *)datap, recv_buf);
	}
	else if (mes_unit->ityp & _ULONG) {
		p_long=(unsigned long *)datap;
		copy_len=mes_unit->elen;
		strncpy(conv, recv_buf, copy_len); 
		conv[copy_len]='\0';
		*p_long=strtoul(conv,(char **)NULL,10);
	}
	else if (mes_unit->ityp & _USHORT) {
		p_short=(unsigned short *)datap;
		copy_len=mes_unit->elen;
		strncpy(conv, recv_buf, copy_len); 
		conv[copy_len]='\0';
		*p_short=(unsigned short)strtol(conv,(char **)NULL,10);
	}
	else if (mes_unit->ityp & _INT) {
		p_int=(int *)datap;
		copy_len=mes_unit->elen;
		strncpy(conv, recv_buf, copy_len); 
		conv[copy_len]='\0';
		*p_int=(int)strtol(conv,(char **)NULL,10);
	}
 
	if (mes_unit->ityp & _CHAR)
		dprintf(DEBUG_MSG_ALL, "\tCHAR  :%*.*s:\n",copy_len,copy_len,recv_buf);
	else if (mes_unit->ityp & _DATA)
		dprintf(DEBUG_MSG_ALL, "\tDATA  :%*.*s:\n",copy_len,copy_len,recv_buf);
	else if (mes_unit->ityp & _VCHAR)
		dprintf(DEBUG_MSG_ALL, "\tVCHAR :%*.*s:\n",copy_len,copy_len,recv_buf);
	else if (mes_unit->ityp & _ULONG)
		dprintf(DEBUG_MSG_ALL, "\tLONG  :%lu:\n",*p_long);
	else if (mes_unit->ityp & _USHORT)
		dprintf(DEBUG_MSG_ALL, "\tUSHORT:%d:\n",*p_short);
	else if (mes_unit->ityp & _INT)
		dprintf(DEBUG_MSG_ALL, "\tINT   :%d:\n",*p_int);
 
	return(copy_len);
}
 
int receive_message(struct ro_connection *conn, int cd, struct ro_application_info *ro_app, struct ro_event *roe, unsigned long maxlen, struct ro_indication *roi) 
{
#define MSG_HEADER_LEN 20
#define MSG_TRAILER_LEN 2
 
	char dummy[2*TCP_MAXLEN+4], *tcp_buf = dummy+4, *tcp_buf_cur, *ro_buffer;
	static void *bufp;
	int  h, i, n, ntoread, nread, nmoved;
	unsigned long rest_lge, unit_len;
 
	if (cd>=0) {
		roe->cd = cd;
		conn+=cd;
	}
	else 
	{
		dprintf(DEBUG_MSG_ERR,"Messages.c: Connection-Index wrong %d", cd);
		roi->code=ERR_MSG_ROLIB;
		sprintf(roi->text, "Connection-index wrong :%d:", cd);
		return(RO_ERROR);
	}
 		
	tcp_buf_cur=tcp_buf;

	ntoread = MSG_HEADER_LEN;

	if (ntoread)
		if ((nread=recv_tcp_frame(conn->sock, tcp_buf_cur, ntoread, roi))!=ntoread) 
		{
			 dprintf(DEBUG_MSG_ERR, "Messages.c: Header nicht empfangen\n");
			 roi->code=ERR_MSG_ROLIB;
			 sprintf(roi->text, "Header not received");
			 return(RO_ERROR);
		}
 
	nmoved = 0;
	tcp_buf_cur=tcp_buf;
	/* Header empfangen */
	for (h=msg_idx[0].von; h<=msg_idx[0].bis; h++) {
		convert_code_recv(tcp_buf_cur, ro_messages[h].elen, ro_messages[h].flags, ro_app->charset, conn->params.mode);

		dprintf(DEBUG_MSG_ALL, "\t(IN)\t:");
		n=0;
		while (n<ro_messages[h].elen) 
		{
			dprintf(DEBUG_MSG_ALL, "%x:",(int)*(tcp_buf_cur+n++) & 0xff);
		}
 
		bufp = ro_messages[h].datap;

		if ((nmoved = move_recv_unit(tcp_buf_cur, &ro_messages[h], bufp, ro_messages[h].elen))!=ro_messages[h].elen) 
		{
			dprintf(DEBUG_MSG_ERR, "Messages.c: Header Aufbau falsch (Index:%d)",h);
			roi->code=ERR_MSG_ROLIB;
			sprintf(roi->text, "Unexpected Header structure (Index %d)", h);
			return(RO_ERROR);
		}
		tcp_buf_cur += nmoved;
	}
	
	rest_lge=common_hd.len - MSG_HEADER_LEN;
	if (rest_lge > maxlen - sizeof(roe)) 
	{
		roi->code=ERR_MSG_ROLIB;
		sprintf(roi->text, "Message too long (%5lu)", rest_lge);
		dprintf(DEBUG_MSG_ERR, "Messages.c: Message zu lang (%5lu)",rest_lge);
		return(0L);
	}
 
	roe->event_typ=common_hd.eventnr;
 
	ro_buffer=(char *)roe + sizeof(struct ro_event);
 
	dprintf(DEBUG_MSG_ALL, "\n");
	dprintf(DEBUG_MSG_ALL, _LINE);
	dprintf(DEBUG_MSG_ALL, "--> RECEIVE-EVENT: %d\n",roe->event_typ);
 
	RO_INIT_MSG(roe)		/* Initialisierung von ro_messages */
 
	for (i=msg_idx[roe->event_typ].von+h; i<=msg_idx[roe->event_typ].bis; i++) 
	{
		bufp=ro_messages[i].datap;
 
		if (ro_messages[i].elen)       /* Unit-Laenge > 0: abziehen  */
		{
			unit_len=ro_messages[i].elen;
 
			if (unit_len<=rest_lge)
				rest_lge -= unit_len;
			else 
			{
				dprintf(DEBUG_MSG_ERR, "Messages.c: Falsche Laenge beim Receive\n");
				roi->code=ERR_MSG_ROLIB;
				sprintf(roi->text, "Message too short (%5lu)", rest_lge);
				return(RO_ERROR);
			}
		}
		else			    /* VCHAR oder DATA */
			unit_len=rest_lge - MSG_TRAILER_LEN;
 
		nmoved=0;
		while (unit_len > nmoved) 
		{
 
			ntoread = min(unit_len - nmoved,TCP_MAXLEN);
 
			nread=recv_tcp_frame(conn->sock, tcp_buf, ntoread, roi);

			convert_code_recv(tcp_buf, nread, ro_messages[i].flags, ro_app->charset, conn->params.mode); 
 
			if (ro_messages[i].flags & _RCV) 
			{
				if (move_recv_unit(tcp_buf,&ro_messages[i],bufp, nread)!=nread) 
				{
					dprintf(DEBUG_MSG_ERR, "Messages.c: Message Aufbau falsch (Index:%d)",i);
					roi->code=ERR_MSG_ROLIB;
					sprintf(roi->text, "Unexpected Header structure (Index %d)", i);
					return(RO_ERROR);
				}
				else
					bufp=(char *)bufp + nread;
			}
			nmoved +=nread;
		}
	}
	dprintf(DEBUG_MSG_TCP, "\n");
	dprintf(DEBUG_MSG_TCP, "\tMessage empfangen, %5lu Bytes\n",common_hd.len);
	dprintf(DEBUG_MSG_TCP, _LINE);
	/* Anzahl empfangener Bytes steht in common_hd.len */
	return(common_hd.len);
}
 
/******************************* EOF ********************************/
