/*
 * Copyright (c) 1990,1993 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <sys/param.h>
#include <sys/uio.h>
#include <atalk/logger.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <errno.h>
#include <sys/wait.h>

#include <atalk/adouble.h>

#include <netatalk/at.h>
#include <atalk/compat.h>
#include <atalk/dsi.h>
#include <atalk/atp.h>
#include <atalk/asp.h>
#include <atalk/afp.h>
#include <atalk/paths.h>
#include <atalk/util.h>
#include <atalk/server_child.h>
#include <atalk/server_ipc.h>

#include "globals.h"
#include "afp_config.h"
#include "status.h"
#include "fork.h"
#include "uam_auth.h"
#include "afp_zeroconf.h"

#ifdef TRU64
#include <sys/security.h>
#include <prot.h>
#include <sia.h>

static int argc = 0;
static char **argv = NULL;
#endif /* TRU64 */

unsigned char	nologin = 0;

struct afp_options default_options;
static AFPConfig *configs;
static server_child *server_children;
static sig_atomic_t reloadconfig = 0;

/* Two pointers to dynamic allocated arrays which store pollfds and associated data */
static struct pollfd *fdset;
static struct polldata *polldata;
static int fdset_size;          /* current allocated size */
static int fdset_used;          /* number of used elements */


#ifdef TRU64
void afp_get_cmdline( int *ac, char ***av)
{
    *ac = argc;
    *av = argv;
}
#endif /* TRU64 */

/* This is registered with atexit() */
static void afp_exit(void)
{
    if (parent_or_child == 0)
        /* Only do this in the parent */
        server_unlock(default_options.pidfile);
}


/* ------------------
   initialize fd set we are waiting for.
*/
static void fd_set_listening_sockets(void)
{
    AFPConfig   *config;

    for (config = configs; config; config = config->next) {
        if (config->fd < 0) /* for proxies */
            continue;
        fdset_add_fd(&fdset, &polldata, &fdset_used, &fdset_size, config->fd, LISTEN_FD, config);
    }
}
 
static void fd_reset_listening_sockets(void)
{
    AFPConfig   *config;

    for (config = configs; config; config = config->next) {
        if (config->fd < 0) /* for proxies */
            continue;
        fdset_del_fd(&fdset, &polldata, &fdset_used, &fdset_size, config->fd);
    }
    fd_set_listening_sockets();
}

/* ------------------ */
static void afp_goaway(int sig)
{

#ifndef NO_DDP
    asp_kill(sig);
#endif /* ! NO_DDP */

    if (server_children)
        server_child_kill(server_children, CHILD_DSIFORK, sig);

    switch( sig ) {

    case SIGTERM :
        LOG(log_note, logtype_afpd, "AFP Server shutting down on SIGTERM");
        AFPConfig *config;
        for (config = configs; config; config = config->next)
            if (config->server_cleanup)
                config->server_cleanup(config);
        server_unlock(default_options.pidfile);
        exit(0);
        break;

    case SIGUSR1 :
        nologin++;
        auth_unload();
        LOG(log_info, logtype_afpd, "disallowing logins");        
        break;

    case SIGHUP :
        /* w/ a configuration file, we can force a re-read if we want */
        reloadconfig = 1;
        break;

    default :
        LOG(log_error, logtype_afpd, "afp_goaway: bad signal" );
    }
    return;
}

static void child_handler(int sig _U_)
{
    int fd;
    int status, i;
    pid_t pid;
  
#ifndef WAIT_ANY
#define WAIT_ANY (-1)
#endif /* ! WAIT_ANY */

    while ((pid = waitpid(WAIT_ANY, &status, WNOHANG)) > 0) {
        for (i = 0; i < server_children->nforks; i++) {
            if ((fd = server_child_remove(server_children, i, pid)) != -1) {
                fdset_del_fd(&fdset, &polldata, &fdset_used, &fdset_size, fd);        
                break;
            }
        }

        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status))
                LOG(log_info, logtype_afpd, "child[%d]: exited %d", pid, WEXITSTATUS(status));
            else
                LOG(log_info, logtype_afpd, "child[%d]: done", pid);
        } else {
            if (WIFSIGNALED(status))
                LOG(log_info, logtype_afpd, "child[%d]: killed by signal %d", pid, WTERMSIG(status));
            else
                LOG(log_info, logtype_afpd, "child[%d]: died", pid);
        }
    }
}

int main(int ac, char **av)
{
    AFPConfig           *config;
    fd_set              rfds;
    void                *ipc;
    struct sigaction	sv;
    sigset_t            sigs;
    int                 ret;

#ifdef TRU64
    argc = ac;
    argv = av;
    set_auth_parameters( ac, av );
#endif /* TRU64 */

    /* Log SIGBUS/SIGSEGV SBT */
    fault_setup(NULL);

    /* Default log setup: log to syslog */
    setuplog("default log_note");

    afp_options_init(&default_options);
    if (!afp_options_parse(ac, av, &default_options))
        exit(EXITERR_CONF);

    /* Save the user's current umask for use with CNID (and maybe some 
     * other things, too). */
    default_options.save_mask = umask( default_options.umask );

    switch(server_lock("afpd", default_options.pidfile,
                       default_options.flags & OPTION_DEBUG)) {
    case -1: /* error */
        exit(EXITERR_SYS);
    case 0: /* child */
        break;
    default: /* server */
        exit(0);
    }
    atexit(afp_exit);

    /* install child handler for asp and dsi. we do this before afp_goaway
     * as afp_goaway references stuff from here. 
     * XXX: this should really be setup after the initial connections. */
    if (!(server_children = server_child_alloc(default_options.connections,
                            CHILD_NFORKS))) {
        LOG(log_error, logtype_afpd, "main: server_child alloc: %s", strerror(errno) );
        exit(EXITERR_SYS);
    }

    memset(&sv, 0, sizeof(sv));    
    /* linux at least up to 2.4.22 send a SIGXFZ for vfat fs,
       even if the file is open with O_LARGEFILE ! */
#ifdef SIGXFSZ
    sv.sa_handler = SIG_IGN;
    sigemptyset( &sv.sa_mask );
    if (sigaction(SIGXFSZ, &sv, NULL ) < 0 ) {
        LOG(log_error, logtype_afpd, "main: sigaction: %s", strerror(errno) );
        exit(EXITERR_SYS);
    }
#endif
    
    sv.sa_handler = child_handler;
    sigemptyset( &sv.sa_mask );
    sigaddset(&sv.sa_mask, SIGALRM);
    sigaddset(&sv.sa_mask, SIGHUP);
    sigaddset(&sv.sa_mask, SIGTERM);
    sigaddset(&sv.sa_mask, SIGUSR1);
    
    sv.sa_flags = SA_RESTART;
    if ( sigaction( SIGCHLD, &sv, NULL ) < 0 ) {
        LOG(log_error, logtype_afpd, "main: sigaction: %s", strerror(errno) );
        exit(EXITERR_SYS);
    }

    sv.sa_handler = afp_goaway;
    sigemptyset( &sv.sa_mask );
    sigaddset(&sv.sa_mask, SIGALRM);
    sigaddset(&sv.sa_mask, SIGTERM);
    sigaddset(&sv.sa_mask, SIGHUP);
    sigaddset(&sv.sa_mask, SIGCHLD);
    sv.sa_flags = SA_RESTART;
    if ( sigaction( SIGUSR1, &sv, NULL ) < 0 ) {
        LOG(log_error, logtype_afpd, "main: sigaction: %s", strerror(errno) );
        exit(EXITERR_SYS);
    }

    sigemptyset( &sv.sa_mask );
    sigaddset(&sv.sa_mask, SIGALRM);
    sigaddset(&sv.sa_mask, SIGTERM);
    sigaddset(&sv.sa_mask, SIGUSR1);
    sigaddset(&sv.sa_mask, SIGCHLD);
    sv.sa_flags = SA_RESTART;
    if ( sigaction( SIGHUP, &sv, NULL ) < 0 ) {
        LOG(log_error, logtype_afpd, "main: sigaction: %s", strerror(errno) );
        exit(EXITERR_SYS);
    }


    sigemptyset( &sv.sa_mask );
    sigaddset(&sv.sa_mask, SIGALRM);
    sigaddset(&sv.sa_mask, SIGHUP);
    sigaddset(&sv.sa_mask, SIGUSR1);
    sigaddset(&sv.sa_mask, SIGCHLD);
    sv.sa_flags = SA_RESTART;
    if ( sigaction( SIGTERM, &sv, NULL ) < 0 ) {
        LOG(log_error, logtype_afpd, "main: sigaction: %s", strerror(errno) );
        exit(EXITERR_SYS);
    }

    /* afpd.conf: not in config file: lockfile, connections, configfile
     *            preference: command-line provides defaults.
     *                        config file over-writes defaults.
     *
     * we also need to make sure that killing afpd during startup
     * won't leave any lingering registered names around.
     */

    sigemptyset(&sigs);
    sigaddset(&sigs, SIGALRM);
    sigaddset(&sigs, SIGHUP);
    sigaddset(&sigs, SIGUSR1);
#if 0
    /* don't block SIGTERM */
    sigaddset(&sigs, SIGTERM);
#endif
    sigaddset(&sigs, SIGCHLD);

    pthread_sigmask(SIG_BLOCK, &sigs, NULL);
    if (!(configs = configinit(&default_options))) {
        LOG(log_error, logtype_afpd, "main: no servers configured");
        exit(EXITERR_CONF);
    }
    pthread_sigmask(SIG_UNBLOCK, &sigs, NULL);

    /* Register CNID  */
    cnid_init();

    /* watch atp, dsi sockets and ipc parent/child file descriptor. */
    fd_set_listening_sockets();

    afp_child_t *child;

    /* trying to figure out why poll keeps returning */
    int intr_count = 0;

    /* wait for an appleshare connection. parent remains in the loop
     * while the children get handled by afp_over_{asp,dsi}.  this is
     * currently vulnerable to a denial-of-service attack if a
     * connection is made without an actual login attempt being made
     * afterwards. establishing timeouts for logins is a possible 
     * solution. */
    while (1) {
        LOG(log_maxdebug, logtype_afpd, "main: polling %i fds", fdset_used);
        pthread_sigmask(SIG_UNBLOCK, &sigs, NULL);
        ret = poll(fdset, fdset_used, -1);
        pthread_sigmask(SIG_BLOCK, &sigs, NULL);
        int saveerrno = errno;

        if (reloadconfig) {
            nologin++;
            auth_unload();

            LOG(log_info, logtype_afpd, "re-reading configuration file");
            for (config = configs; config; config = config->next)
                if (config->server_cleanup)
                    config->server_cleanup(config);

            /* configfree close atp socket used for DDP tickle, there's an issue
             * with atp tid. */
            configfree(configs, NULL);
            if (!(configs = configinit(&default_options))) {
                LOG(log_error, logtype_afpd, "config re-read: no servers configured");
                exit(EXITERR_CONF);
            }
            fd_reset_listening_sockets();
            nologin = 0;
            reloadconfig = 0;
            errno = saveerrno;
        }

        if (ret == 0) {
            /* this should never happen! */
            LOG(log_error, logtype_afpd, "main: poll finished?! this is not good!");
            break;
        }
        
        if (ret < 0) {
            if (errno == EINTR) {
                if (++intr_count > 100)
                {
                    LOG(log_error, logtype_afpd, "main: too many EINTR! (%d)", intr_count);
                    break;
                } else
                    continue;
            }
            LOG(log_error, logtype_afpd, "main: can't wait for input: %s", strerror(errno));
            break;
        }
        intr_count = 0;

        for (int i = 0; i < fdset_used; i++) {
            if (fdset[i].revents > 0) {
                if (fdset[i].revents & POLLIN) {
                    switch (polldata[i].fdtype) {
                    case LISTEN_FD:
                        LOG(log_debug, logtype_afpd, "main: IPC request for LISTEN_FD");
                        config = (AFPConfig *)polldata[i].data;
                        /* config->server_start is afp_config.c:dsi_start() for DSI */
                        if (child = config->server_start(config, configs, server_children)) {
                            /* Add IPC fd to select fd set */
                            fdset_add_fd(&fdset, &polldata, &fdset_used, &fdset_size, child->ipc_fds[0], IPC_FD, child);
                        }
                        break;
                    case IPC_FD:
                        LOG(log_debug, logtype_afpd, "main: IPC request for IPC_FD");
                        child = (afp_child_t *)polldata[i].data;
                        LOG(log_debug, logtype_afpd, "main: IPC request from child[%u]", child->pid);
                        if ((ret = ipc_server_read(server_children, child->ipc_fds[0])) == 0) {
                            fdset_del_fd(&fdset, &polldata, &fdset_used, &fdset_size, child->ipc_fds[0]);
                            close(child->ipc_fds[0]);
                            child->ipc_fds[0] = -1;
                        }
                        break;
                    default:
                        LOG(log_debug, logtype_afpd, "main: IPC request for unknown type");
                        break;
                    } /* switch */
                } else {
                    LOG(log_error, logtype_afpd, "main: unexpected poll event! (%d)", fdset[i].revents);
                } /* if */
            }  /* if */
        } /* for (i)*/
    } /* while (1) */

    return 0;
}
