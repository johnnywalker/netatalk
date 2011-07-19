# Makefile.am for etc/uams/

SUBDIRS = uams_krb4

#
# conditionally build some modules
#

UAMS_GENERIC = libuams_guest.la libuams_passwd.la

# these are complex: check if DHX and DHX2 module should be build

if USE_DHX
if HAVE_OPENSSL
UAMS_DHX = libuams_dhx_passwd.la
if BUILD_PAM
UAMS_DHX_PAM = libuams_dhx_pam.la
endif
endif
endif

if USE_DHX2
if HAVE_LIBGCRYPT
UAMS_DHX2 = libuams_dhx2_passwd.la
if BUILD_PAM
UAMS_DHX2_PAM = libuams_dhx2_pam.la
endif
endif
endif

# these are simple, though some the last three depend on OpenSSL

if BUILD_PAM
UAMS_PAM = libuams_pam.la
endif

if USE_RANDNUM
UAMS_RANDNUM = libuams_randnum.la
endif

if USE_PGP
UAMS_PGP = libuams_pgp.la
endif

if USE_GSSAPI
UAMS_GSSAPI = libuams_gss.la
endif

# links

#
# source files
#

libuams_guest_la_SOURCES      = uams_guest.c
libuams_randnum_la_SOURCES    = uams_randnum.c
libuams_passwd_la_SOURCES     = uams_passwd.c
libuams_pam_la_SOURCES        = uams_pam.c
libuams_pgp_la_SOURCES        = uams_pgp.c
libuams_dhx_passwd_la_SOURCES = uams_dhx_passwd.c
libuams_dhx_pam_la_SOURCES    = uams_dhx_pam.c uams.h
libuams_dhx2_passwd_la_SOURCES	= uams_dhx2_passwd.c
libuams_dhx2_pam_la_SOURCES	= uams_dhx2_pam.c uams.h
libuams_gss_la_SOURCES   = uams_gss.c

#
# flags
#

# these should be sorted out, applying both to AM_CFLAGS is senseless
AM_CFLAGS = @SSL_CFLAGS@ @LIBGCRYPT_CFLAGS@

libuams_pam_la_CFLAGS               = @PAM_CFLAGS@
libuams_dhx_pam_la_CFLAGS           = @SSL_CFLAGS@ @PAM_CFLAGS@
libuams_dhx2_pam_la_CFLAGS          = @LIBGCRYPT_CFLAGS@ @PAM_CFLAGS@
libuams_gss_la_CFLAGS   	    = @GSSAPI_CFLAGS@

libuams_guest_la_LDFLAGS            = -static
libuams_randnum_la_LDFLAGS          = -static @SSL_LIBS@
libuams_passwd_la_LDFLAGS           = -static @CRYPT_LIBS@
libuams_pam_la_LDFLAGS              = -static @PAM_LIBS@
libuams_pgp_la_LDFLAGS              = -static @SSL_LIBS@
libuams_dhx_passwd_la_LDFLAGS       = -static @CRYPT_LIBS@ @SSL_LIBS@
libuams_dhx_pam_la_LDFLAGS          = -static @CRYPT_LIBS@ @SSL_LIBS@ @PAM_LIBS@
libuams_dhx2_passwd_la_LDFLAGS	    = -static @CRYPT_LIBS@ @LIBGCRYPT_LIBS@
libuams_dhx2_pam_la_LDFLAGS	    = -static @LIBGCRYPT_LIBS@ @PAM_LIBS@
libuams_gss_la_LDFLAGS   	    = -static @GSSAPI_LIBS@ 

#
# module compilation
#

uamsdir = @UAMS_PATH@
noinst_LTLIBRARIES =		\
	$(UAMS_GENERIC)		\
	$(UAMS_PGP)		\
	$(UAMS_PAM)		\
	$(UAMS_RANDNUM)		\
	$(UAMS_DHX)		\
	$(UAMS_DHX_PAM)		\
	$(UAMS_DHX2)		\
	$(UAMS_DHX2_PAM)	\
	$(UAMS_GSSAPI)          \
        libuams.la

libuams_la_SOURCES =            \
        dummy.c

libuams_la_LIBADD =             \
	$(UAMS_GENERIC)		\
	$(UAMS_PGP)		\
	$(UAMS_PAM)		\
	$(UAMS_RANDNUM)		\
	$(UAMS_DHX)		\
	$(UAMS_DHX_PAM)		\
	$(UAMS_DHX2)		\
	$(UAMS_DHX2_PAM)	\
	$(UAMS_GSSAPI)          

libuams_la_LDFLAGS = -static
