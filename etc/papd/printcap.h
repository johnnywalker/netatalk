/*
 * $Id: printcap.h,v 1.3 2001-06-25 20:13:45 rufustfirefly Exp $
 */

#ifndef PAPD_PRINTCAP_H
#define PAPD_PRINTCAP_H 1

#include <sys/cdefs.h>

int getprent __P(( register char *, register char * ));
int pnchktc __P(( char * ));
int pgetflag __P(( char * ));
void endprent __P(( void ));
int pgetent __P(( char *, char *, char * ));
int pgetnum __P(( char * ));
int pnamatch __P(( char * ));

#endif /* PAPD_PRINTCAP_H */