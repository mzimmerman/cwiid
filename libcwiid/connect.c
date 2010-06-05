/* Copyright (C) 2007 L. Donnie Smith <donnie.smith@gatech.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include "cwiid_internal.h"

pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER;
static int wiimote_id = 0;

/* TODO: Turn this onto a macro on next major so version */
cwiid_wiimote_t *cwiid_open(bdaddr_t *bdaddr, int flags)
{
	return cwiid_open_timeout( bdaddr, flags, DEFAULT_TIMEOUT );
}

cwiid_wiimote_t *cwiid_open_timeout(bdaddr_t *pbdaddr, int flags, int timeout)
{
	struct sockaddr_l2 remote_addr;
	int ctl_socket = -1, int_socket = -1;
	struct wiimote *wiimote = NULL;
   int ret;
   bdaddr_t bdaddr;

	/* If BDADDR_ANY is given, find available wiimote */
	if ((pbdaddr == NULL) || bacmp( pbdaddr, BDADDR_ANY) == 0) {
		if (cwiid_find_wiimote( &bdaddr, timeout)) {
			goto ERR_HND;
		}
		sleep(1);
	}
   else {
      bacpy( &bdaddr, pbdaddr );
   }

	/* Connect to Wiimote */
	/* Control Channel */
	memset( &remote_addr, 0, sizeof(remote_addr) );
	remote_addr.l2_family = AF_BLUETOOTH;
   bacpy( &remote_addr.l2_bdaddr, &bdaddr );
	remote_addr.l2_psm    = htobs(CTL_PSM);
   ctl_socket = socket( AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP );
   if (ctl_socket < 0) {
		cwiid_err(NULL, "Socket creation error (control socket)");
		goto ERR_HND;
	}
   ret = connect( ctl_socket, (struct sockaddr *) &remote_addr, sizeof(remote_addr) );
   if (ret < 0) {
		cwiid_err(NULL, "Socket connect error (control socket)");
		goto ERR_HND;
	}

	/* Interrupt Channel */
	remote_addr.l2_psm    = htobs(INT_PSM);
   int_socket = socket( AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP );
   if (int_socket < 0) {
		cwiid_err(NULL, "Socket creation error (interrupt socket)");
		goto ERR_HND;
	}
   ret = connect( int_socket, (struct sockaddr *) &remote_addr, sizeof(remote_addr) );
   if (ret < 0) {
		cwiid_err(NULL, "Socket connect error (interrupt socket)");
		goto ERR_HND;
	}

   wiimote = cwiid_new( ctl_socket, int_socket, flags );
   if (wiimote == NULL) {
		/* Raises its own error */
		goto ERR_HND;
	}

	return wiimote;

ERR_HND:
	/* Close Sockets */
	if (ctl_socket != -1) {
		if (close(ctl_socket)) {
			cwiid_err(NULL, "Socket close error (control socket)");
		}
	}
	if (int_socket != -1) {
		if (close(int_socket)) {
			cwiid_err(NULL, "Socket close error (interrupt socket)");
		}
	}
	return NULL;
}

cwiid_wiimote_t *cwiid_listen(int flags)
{
	struct sockaddr_l2 local_addr;
	struct sockaddr_l2 remote_addr;
	socklen_t socklen;
	int ctl_server_socket = -1, int_server_socket = -1,
	    ctl_socket = -1, int_socket = -1;
	struct wiimote *wiimote = NULL;

	/* Connect to Wiimote */
	/* Control Channel */
	memset( &local_addr, 0, sizeof(local_addr) );
	local_addr.l2_family = AF_BLUETOOTH;
	local_addr.l2_bdaddr = *BDADDR_ANY;
	local_addr.l2_psm = htobs(CTL_PSM);
	if ((ctl_server_socket =
	  socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP)) == -1) {
		cwiid_err(NULL, "Socket creation error (control socket)");
		goto ERR_HND;
	}
	if (bind(ctl_server_socket, (struct sockaddr *)&local_addr,
	         sizeof local_addr)) {
		cwiid_err(NULL, "Socket bind error (control socket)");
		goto ERR_HND;
	}
	if (listen(ctl_server_socket, 1)) {
		cwiid_err(NULL, "Socket listen error (control socket)");
		goto ERR_HND;
	}

	/* Interrupt Channel */
	local_addr.l2_psm = htobs(INT_PSM);
	if ((int_server_socket =
	  socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP)) == -1) {
		cwiid_err(NULL, "Socket creation error (interrupt socket)");
		goto ERR_HND;
	}
	if (bind(int_server_socket, (struct sockaddr *)&local_addr,
	         sizeof local_addr)) {
		cwiid_err(NULL, "Socket bind error (interrupt socket)");
		goto ERR_HND;
	}
	if (listen(int_server_socket, 1)) {
		cwiid_err(NULL, "Socket listen error (interrupt socket)");
		goto ERR_HND;
	}

	/* Block for Connections */
	if ((ctl_socket = accept(ctl_server_socket, (struct sockaddr *)&remote_addr, &socklen)) < 0) {
		cwiid_err(NULL, "Socket accept error (control socket)");
		goto ERR_HND;
	}
	if ((int_socket = accept(int_server_socket, (struct sockaddr *)&remote_addr, &socklen)) < 0) {
		cwiid_err(NULL, "Socket accept error (interrupt socket)");
		goto ERR_HND;
	}

	/* Close server sockets */
	if (close(ctl_server_socket)) {
		cwiid_err(NULL, "Socket close error (control socket)");
	}
	if (close(int_server_socket)) {
		cwiid_err(NULL, "Socket close error (interrupt socket)");
	}

	if ((wiimote = cwiid_new(ctl_socket, int_socket, flags)) == NULL) {
		/* Raises its own error */
		goto ERR_HND;
	}

	return wiimote;

ERR_HND:
	/* Close Sockets */
	if (ctl_server_socket != -1) {
		if (close(ctl_server_socket)) {
			cwiid_err(NULL, "Socket close error (control server socket)");
		}
	}
	if (int_server_socket != -1) {
		if (close(int_server_socket)) {
			cwiid_err(NULL, "Socket close error (interrupt server socket)");
		}
	}
	if (ctl_socket != -1) {
		if (close(ctl_socket)) {
			cwiid_err(NULL, "Socket close error (control socket)");
		}
	}
	if (int_socket != -1) {
		if (close(int_socket)) {
			cwiid_err(NULL, "Socket close error (interrupt socket)");
		}
	}

	return NULL;
}

cwiid_wiimote_t *cwiid_new(int ctl_socket, int int_socket, int flags)
{
	struct wiimote *wiimote = NULL;
   int i;
	char mesg_pipe_init = 0,
        mesg_mutex_init = 0, state_mutex_init = 0, write_mutex_init = 0,
        rpt_mutex_init = 0, router_mutex_init = 0, router_cond_init = 0,
	     router_thread_init = 0;
	void *pthread_ret;

	/* Allocate wiimote */
   wiimote = malloc( sizeof(*wiimote) );
   if (wiimote == NULL) {
		cwiid_err(NULL, "Memory allocation error (cwiid_wiimote_t)");
		goto ERR_HND;
	}

	/* Clear memory. */
	memset( wiimote, 0, sizeof(*wiimote) );

   /* Set sockets and flags. */
	wiimote->ctl_socket = ctl_socket;
	wiimote->int_socket = int_socket;
	wiimote->flags      = flags;

	/* Global Lock, Store and Increment wiimote_id */
	if (pthread_mutex_lock(&global_mutex)) {
		cwiid_err(NULL, "Mutex lock error (global mutex)");
		goto ERR_HND;
	}
	wiimote->id = wiimote_id++;
	if (pthread_mutex_unlock(&global_mutex)) {
		cwiid_err(wiimote, "Mutex unlock error (global mutex) - "
		                   "deadlock warning");
		goto ERR_HND;
	}

	/* Create pipes */
	if (pipe( wiimote->mesg_pipe )) {
		cwiid_err(wiimote, "Pipe creation error (mesg pipe)");
		goto ERR_HND;
	}
	mesg_pipe_init = 1;

	/* Setup blocking */
	if (fcntl( wiimote->mesg_pipe[1], F_SETFL, O_NONBLOCK) ) {
		cwiid_err(wiimote, "File control error (mesg write pipe)");
		goto ERR_HND;
	}
	if (wiimote->flags & CWIID_FLAG_NONBLOCK) {
		if (fcntl( wiimote->mesg_pipe[0], F_SETFL, O_NONBLOCK) ) {
			cwiid_err(wiimote, "File control error (mesg read pipe)");
			goto ERR_HND;
		}
	}

   /* Init conditionals. */
   if (pthread_cond_init( &wiimote->router_cond, NULL )) {
      cwiid_err( wiimote, "Conditional initialization error (router_cond)");
      goto ERR_HND;
   }
   router_cond_init = 1;

	/* Init mutexes */
	if (pthread_mutex_init(&wiimote->state_mutex, NULL)) {
		cwiid_err(wiimote, "Mutex initialization error (state mutex)");
		goto ERR_HND;
	}
	state_mutex_init = 1;
	if (pthread_mutex_init(&wiimote->write_mutex, NULL)) {
		cwiid_err(wiimote, "Mutex initialization error (rpt mutex)");
		goto ERR_HND;
	}
	write_mutex_init = 1;
	if (pthread_mutex_init(&wiimote->rpt_mutex, NULL)) {
		cwiid_err(wiimote, "Mutex initialization error (rpt mutex)");
		goto ERR_HND;
	}
	rpt_mutex_init = 1;
	if (pthread_mutex_init(&wiimote->mesg_mutex, NULL)) {
		cwiid_err(wiimote, "Mutex initialization error (mesg mutex)");
		goto ERR_HND;
	}
	mesg_mutex_init = 1;
	if (pthread_mutex_init(&wiimote->router_mutex, NULL)) {
		cwiid_err(wiimote, "Mutex initialization error (status mutex)");
		goto ERR_HND;
	}
	router_mutex_init = 1;

   /* Stuff to set before router thread. */
   wiimote->router_rpt_wait      = RPT_NULL;
   wiimote->router_rpt_buf       = NULL;
   wiimote->router_rpt_process   = 1;
	wiimote->mesg_callback        = NULL;
	memset(&wiimote->state, 0, sizeof wiimote->state);

   /* Prepare for wait. */
   rpt_wait_start( wiimote );

	/* Launch interrupt socket listener and dispatch threads */
	if (pthread_create(&wiimote->router_thread, NULL,
	                   (void *(*)(void *))&router_thread, wiimote)) {
		cwiid_err(wiimote, "Thread creation error (router thread)");
		goto ERR_HND;
	}
	router_thread_init = 1;

   /* Wait for the wiimote to first send a status report. */
   rpt_wait( wiimote, RPT_STATUS, NULL, 1 );
   rpt_wait_end( wiimote );

#if 0
   /* Update status. */
	cwiid_set_led( wiimote, CWIID_LED1_ON | CWIID_LED2_ON | CWIID_LED3_ON | CWIID_LED4_ON );
#endif

	return wiimote;

ERR_HND:
	if (wiimote) {
		/* Close threads */
		if (router_thread_init) {
			pthread_cancel(wiimote->router_thread);
			if (pthread_join(wiimote->router_thread, &pthread_ret)) {
				cwiid_err(wiimote, "Thread join error (router thread)");
			}
			else if (!((pthread_ret == PTHREAD_CANCELED) &&
			         (pthread_ret == NULL))) {
				cwiid_err(wiimote, "Bad return value from router thread");
			}
		}

		/* Close Pipes */
		if (mesg_pipe_init) {
			if (close(wiimote->mesg_pipe[0]) || close(wiimote->mesg_pipe[1])) {
				cwiid_err(wiimote, "Pipe close error (mesg pipe)");
			}
		}

      /* Destroy conditionals. */
      if (router_cond_init) {
         if (pthread_cond_destroy( &wiimote->router_cond )) {
            cwiid_err( wiimote, "Conditional destroy error (router_cond)" );
         }
      }

		/* Destroy Mutexes */
		if (state_mutex_init) {
			if (pthread_mutex_destroy(&wiimote->state_mutex)) {
				cwiid_err(wiimote, "Mutex destroy error (state mutex)");
			}
		}
		if (write_mutex_init) {
			if (pthread_mutex_destroy(&wiimote->write_mutex)) {
				cwiid_err(wiimote, "Mutex destroy error (rpt mutex)");
			}
		}
		if (rpt_mutex_init) {
			if (pthread_mutex_destroy(&wiimote->rpt_mutex)) {
				cwiid_err(wiimote, "Mutex destroy error (rpt mutex)");
			}
		}
		if (mesg_mutex_init) {
			if (pthread_mutex_destroy(&wiimote->mesg_mutex)) {
				cwiid_err(wiimote, "Mutex destroy error (mesg mutex)");
			}
		}
		if (router_mutex_init) {
			if (pthread_mutex_destroy(&wiimote->router_mutex)) {
				cwiid_err(wiimote, "Mutex destroy error (mesg mutex)");
			}
		}
		free(wiimote);
	}
	return NULL;
}

int cwiid_close(cwiid_wiimote_t *wiimote)
{
	void *pthread_ret;

	/* Stop rumbling, otherwise wiimote continues to rumble for
	   few seconds after closing the connection! There should be no
	   need to check if stopping fails: we are closing the connection
	   in any case. */
	if (wiimote->state.rumble) {
		cwiid_set_rumble(wiimote, 0);
	}

	/* Cancel and join router_thread and router_thread */
	if (pthread_cancel(wiimote->router_thread)) {
		/* if thread quit abnormally, would have printed it's own error */
	}
	if (pthread_join(wiimote->router_thread, &pthread_ret)) {
		cwiid_err(wiimote, "Thread join error (router thread)");
	}
	else if (!((pthread_ret == PTHREAD_CANCELED) || (pthread_ret == NULL))) {
		cwiid_err(wiimote, "Bad return value from router thread");
	}

	if (wiimote->mesg_callback) {
      pthread_mutex_lock( &wiimote->mesg_mutex );
		if (cancel_mesg_callback(wiimote)) {
			/* prints it's own errors */
		}
      pthread_mutex_unlock( &wiimote->mesg_mutex );
	}

	/* Close sockets */
	if (close(wiimote->int_socket)) {
		cwiid_err(wiimote, "Socket close error (interrupt socket)");
	}
	if (close(wiimote->ctl_socket)) {
		cwiid_err(wiimote, "Socket close error (control socket)");
	}
	/* Close Pipes */
	if (close(wiimote->mesg_pipe[0]) || close(wiimote->mesg_pipe[1])) {
		cwiid_err(wiimote, "Pipe close error (mesg pipe)");
	}
   /* Destroy conditionals. */
   if (pthread_cond_destroy( &wiimote->router_cond )) {
      cwiid_err( wiimote, "Conditional destroy error (router_cond)" );
   }
	/* Destroy mutexes */
	if (pthread_mutex_destroy(&wiimote->state_mutex)) {
		cwiid_err(wiimote, "Mutex destroy error (state)");
	}
	if (pthread_mutex_destroy(&wiimote->write_mutex)) {
		cwiid_err(wiimote, "Mutex destroy error (write)");
	}
	if (pthread_mutex_destroy(&wiimote->rpt_mutex)) {
		cwiid_err(wiimote, "Mutex destroy error (rpt)");
	}
	if (pthread_mutex_destroy(&wiimote->mesg_mutex)) {
		cwiid_err(wiimote, "Mutex destroy error (mesg)");
	}
	if (pthread_mutex_destroy(&wiimote->router_mutex)) {
		cwiid_err(wiimote, "Mutex destroy error (router)");
	}

	free(wiimote);

	return 0;
}
