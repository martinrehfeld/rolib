# =============================================================================
#                               R O L I B
#                       Remote Operations Library
# -----------------------------------------------------------------------------
# Datei     : Makefile
# Version   : 1.6
# Datum     : 96/05/09 14:57:24
#
# Autor     : Destailleur, Lubitz
# -----------------------------------------------------------------------------
#                          (c) CR by Herlitz AG
# =============================================================================
GCC=/usr/local/bin/gcc                       
INCLLIB=-I/usr/sap/HZE/rfc/rfcsdk/include  -I. 
LIBS=-L/usr/sap/HZE/rfc/rfcsdk/lib -L. -lrfc -lsocket -lnsl -ldl  
CFLAGS=-g
INSTALLBASE=/usr/local/rolib/
INSTALLBIN=bin
INSTALLLIB=lib
INSTALLINCLUDE=include
INSTALL=/usr/ucb/install

#all:	dirserv roping roping_nw roecho roshut librolib.a
all:	librolib.a cleanup


rolib.o: rolib.h system.h
	$(GCC) $(CFLAGS) -c -o $(@F) rolib.c  
	
messages.o: rolib.h messages.h system.h
	$(GCC) $(CFLAGS) -c -o $(@F) messages.c  

librolib.a: rolib.o messages.o
	ar -rc librolib.a rolib.o messages.o
	$(RANLIB) librolib.a
	
	

#
#	Programm(e) installieren
#
install:
	$(INSTALL) -g root -m 755 -o root dirserv dirserv.run roping roecho $(INSTALLBASE)$(INSTALLBIN)
	$(INSTALL) -g root -m 755 -o root rolib.h messages.h system.h $(INSTALLBASE)$(INSTALLINCLUDE)
	$(INSTALL) -g root -m 755 -o root librolib.a $(INSTALLBASE)$(INSTALLLIB)

cleanup: 
	rm -f *.o *.err 

# ------------------------------- EOF -------------------------------------
