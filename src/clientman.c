/*
 * Copyright (C) 2010 gonzoj
 *
 * Please check the CREDITS file for further information.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <pthread.h>

#include "clientman.h"

#include "bncs.h"
#include "d2gs.h"
#include "internal.h"
#include "mcp.h"
#include "moduleman.h"
#include "packet.h"
#include "settings.h"
#include "gui.h"

#include <util/system.h>
//#include <util/types.h>

/* statistics */
int reconnects;

bool cm_fatal_error = FALSE;

static pthread_t bncs_engine_tid;
static pthread_t mcp_engine_tid;
static pthread_t d2gs_engine_tid;

static bool bncs_engine_cleanup = FALSE;
static bool mcp_engine_cleanup = FALSE;
static bool d2gs_engine_cleanup = FALSE;

static pthread_t cm_tid;

static pthread_mutex_t cm_continue_m;
static pthread_cond_t cm_continue_cv;

static bool cm_shutdown;

static bool cm_restart;

static pthread_mutex_t cm_manage_m;

typedef void * (*client_engine_thread_t)(void *);

void start_client_engine(client_engine_t client, void *arg) {
	if (is_module_thread()) {
		return;
	}

	switch(client) {

	case BNCS_CLIENT_ENGINE:
		if (pthread_self() == bncs_engine_tid) {
			return; // deadlock
		}

		pthread_mutex_lock(&cm_manage_m);

		if (bncs_engine_cleanup) {
			bncs_shutdown(); // do we really want to shutdown?
			pthread_join(bncs_engine_tid, NULL);
		}

		print("starting BNCS engine\n");

		bncs_engine_shutdown = FALSE; // added

		pthread_create(&bncs_engine_tid, NULL, (client_engine_thread_t) bncs_client_engine, arg);
		bncs_engine_cleanup = TRUE;

		pthread_mutex_unlock(&cm_manage_m);

		break;

	case MCP_CLIENT_ENGINE:
		if (pthread_self() == mcp_engine_tid) {
			return; // deadlock
		}

		pthread_mutex_lock(&cm_manage_m);

		if (mcp_engine_cleanup) {
			mcp_shutdown(); // do we really want to shutdown?
			pthread_join(mcp_engine_tid, NULL);
		}

		print("starting MCP engine\n");

		mcp_engine_shutdown = FALSE; // added

		pthread_create(&mcp_engine_tid, NULL, (client_engine_thread_t) mcp_client_engine, arg);
		mcp_engine_cleanup = TRUE;

		pthread_mutex_unlock(&cm_manage_m);

		break;

	case D2GS_CLIENT_ENGINE:
		if (pthread_self() == d2gs_engine_tid) {
			return; // deadlock
		}

		pthread_mutex_lock(&cm_manage_m);

		if (d2gs_engine_cleanup) {
			d2gs_shutdown(); // do we really want to shutdown?
			pthread_join(d2gs_engine_tid, NULL);
		}

		print("starting D2GS engine\n");

		d2gs_engine_shutdown = FALSE;

		pthread_create(&d2gs_engine_tid, NULL, (client_engine_thread_t) d2gs_client_engine, arg);
		d2gs_engine_cleanup = TRUE;

		pthread_mutex_unlock(&cm_manage_m);

		break;

	}
}

void stop_client_engine(client_engine_t client) {
	if (is_module_thread()) {
		return;
	}

	switch(client) {

	case BNCS_CLIENT_ENGINE:
		if (pthread_self() == bncs_engine_tid) {
			return; // deadlock
		}

		pthread_mutex_lock(&cm_manage_m);

		if (bncs_engine_cleanup) {
			print("stopping BNCS engine\n");

			bncs_shutdown();
			pthread_join(bncs_engine_tid, NULL);
			bncs_engine_cleanup = FALSE;
		}

		pthread_mutex_unlock(&cm_manage_m);

		break;

	case MCP_CLIENT_ENGINE:
		if (pthread_self() == mcp_engine_tid) {
			return; // deadlock
		}

		pthread_mutex_lock(&cm_manage_m);

		if (mcp_engine_cleanup) {
			print("stopping MCP engine\n");

			mcp_shutdown();
			pthread_join(mcp_engine_tid, NULL);
			mcp_engine_cleanup = FALSE;
		}

		pthread_mutex_unlock(&cm_manage_m);

		break;

	case D2GS_CLIENT_ENGINE:
		if (pthread_self() == d2gs_engine_tid) {
			return; // deadlock
		}

		pthread_mutex_lock(&cm_manage_m);

		if (d2gs_engine_cleanup) {
			print("stopping D2GS engine\n");

			d2gs_shutdown();
			pthread_join(d2gs_engine_tid, NULL);
			d2gs_engine_cleanup = FALSE;
		}

		pthread_mutex_unlock(&cm_manage_m);

		break;
	}

}

static int on_bncs_engine_shutdown(internal_packet_t *p) {
	if (*(int *)p->data == ENGINE_SHUTDOWN) {

	if (d2gs_get_client_status() == CLIENT_DISCONNECTED) {

		if (mcp_get_client_status() == CLIENT_CONNECTED) {

			if (!setting("BNCSDisconnect")->b_var) {

				stop_client_engine(MCP_CLIENT_ENGINE);

			} else {

				return FORWARD_PACKET;

			}
		}

		pthread_mutex_lock(&cm_continue_m);

		// signal to restart BNCS engine
		pthread_cond_signal(&cm_continue_cv);

		pthread_mutex_unlock(&cm_continue_m);

	}
	}

	return FORWARD_PACKET;
}

static int on_mcp_engine_shutdown(internal_packet_t *p) {
	if (*(int *)p->data == ENGINE_SHUTDOWN) {

	if (bncs_get_client_status() == CLIENT_DISCONNECTED && d2gs_get_client_status() == CLIENT_DISCONNECTED) {

		pthread_mutex_lock(&cm_continue_m);

		// signal to restart BNCS engine
		pthread_cond_signal(&cm_continue_cv);

		pthread_mutex_unlock(&cm_continue_m);

	}
	}

	return FORWARD_PACKET;
}

static int on_d2gs_engine_shutdown(internal_packet_t *p) {
	if (*(int *)p->data == ENGINE_SHUTDOWN) {

	if (bncs_get_client_status() == CLIENT_DISCONNECTED) {

		if (mcp_get_client_status() == CLIENT_CONNECTED) {

			if (!setting("BNCSDisconnect")->b_var) {

				stop_client_engine(MCP_CLIENT_ENGINE);

			} else {

				return FORWARD_PACKET;

			}
		}

		pthread_mutex_lock(&cm_continue_m);

		// signal to restart BNCS engine
		pthread_cond_signal(&cm_continue_cv);

		pthread_mutex_unlock(&cm_continue_m);
	}
	}

	return FORWARD_PACKET;
}

static int on_internal_fatal_error(internal_packet_t *p) {
	if (p->data) {

		error("internal fatal error (%s)... engines shutting down\n", p->data);

	} else {
		error("internal fatal error... engines shutting down\n");
	}

	//cm_shutdown = TRUE;

	//bncs_shutdown();
	//mcp_shutdown();
	//d2gs_shutdown();
	//stop_client_engine(D2GS_CLIENT_ENGINE);
	//stop_client_engine(MCP_CLIENT_ENGINE);
	//stop_client_engine(BNCS_CLIENT_ENGINE);

	//pthread_mutex_lock(&cm_continue_m);

	// signal to stop engines
	//pthread_cond_signal(&cm_continue_cv);

	//pthread_mutex_unlock(&cm_continue_m);
	
	cm_fatal_error = TRUE;

	return FORWARD_PACKET;
}

static int on_internal_request(internal_packet_t *p) {
	if (*(int *)p->data == CLIENT_RESTART) {
		print("client restart requested\n");
		print("restarting engines\n");

		pthread_mutex_lock(&cm_continue_m);

		cm_restart = TRUE;

		// signal to restart client engines
		pthread_cond_signal(&cm_continue_cv);

		pthread_mutex_unlock(&cm_continue_m);
	}

	return FORWARD_PACKET;
}

void * client_manager_thread(void *arg) {
	while (!cm_shutdown) {

		pthread_mutex_lock(&cm_continue_m);

		start_client_engine(BNCS_CLIENT_ENGINE, NULL);

		// wait for signal to restart BNCS engine
		pthread_cond_wait(&cm_continue_cv, &cm_continue_m);

		if (cm_restart) {
			cm_restart = FALSE;

			unregister_packet_handler(INTERNAL, BNCS_ENGINE_MESSAGE, (packet_handler_t) on_bncs_engine_shutdown);
			unregister_packet_handler(INTERNAL, MCP_ENGINE_MESSAGE, (packet_handler_t) on_mcp_engine_shutdown);
			unregister_packet_handler(INTERNAL, D2GS_ENGINE_MESSAGE, (packet_handler_t) on_d2gs_engine_shutdown);

			stop_client_engine(D2GS_CLIENT_ENGINE);
			stop_client_engine(MCP_CLIENT_ENGINE);
			stop_client_engine(BNCS_CLIENT_ENGINE);

			register_packet_handler(INTERNAL, BNCS_ENGINE_MESSAGE, (packet_handler_t) on_bncs_engine_shutdown);
			register_packet_handler(INTERNAL, MCP_ENGINE_MESSAGE, (packet_handler_t) on_mcp_engine_shutdown);
			register_packet_handler(INTERNAL, D2GS_ENGINE_MESSAGE, (packet_handler_t) on_d2gs_engine_shutdown);

			system_sh(setting("OnRestart")->s_var);
		}

		pthread_mutex_unlock(&cm_continue_m);

		if (!cm_shutdown) {
			reconnects++;
			sleep(setting("ReconnectDelay")->i_var);
		}
	}

	stop_client_engine(D2GS_CLIENT_ENGINE);
	stop_client_engine(MCP_CLIENT_ENGINE);
	stop_client_engine(BNCS_CLIENT_ENGINE);

	pthread_exit(NULL);
}

void start_client_manager() {
	register_packet_handler(INTERNAL, BNCS_ENGINE_MESSAGE, (packet_handler_t) on_bncs_engine_shutdown);
	register_packet_handler(INTERNAL, MCP_ENGINE_MESSAGE, (packet_handler_t) on_mcp_engine_shutdown);
	register_packet_handler(INTERNAL, D2GS_ENGINE_MESSAGE, (packet_handler_t) on_d2gs_engine_shutdown);
	register_packet_handler(INTERNAL, INTERNAL_FATAL_ERROR, (packet_handler_t) on_internal_fatal_error);

	register_packet_handler(INTERNAL, INTERNAL_REQUEST, (packet_handler_t) on_internal_request);

	cm_shutdown = FALSE;

	cm_restart = FALSE;

	pthread_mutex_init(&cm_continue_m, NULL);
	pthread_cond_init(&cm_continue_cv, NULL);

	pthread_mutex_init(&cm_manage_m, NULL);

	pthread_create(&cm_tid, NULL, client_manager_thread, NULL);
}

void stop_client_manager() {

	cm_shutdown = TRUE;

	unregister_packet_handler(INTERNAL, BNCS_ENGINE_MESSAGE, (packet_handler_t) on_bncs_engine_shutdown);
	unregister_packet_handler(INTERNAL, MCP_ENGINE_MESSAGE, (packet_handler_t) on_mcp_engine_shutdown);
	unregister_packet_handler(INTERNAL, D2GS_ENGINE_MESSAGE, (packet_handler_t) on_d2gs_engine_shutdown);
	unregister_packet_handler(INTERNAL, INTERNAL_FATAL_ERROR, (packet_handler_t) on_internal_fatal_error);

	unregister_packet_handler(INTERNAL, INTERNAL_REQUEST, (packet_handler_t) on_internal_request);

	//stop_client_engine(D2GS_CLIENT_ENGINE);
	//stop_client_engine(MCP_CLIENT_ENGINE);
	//stop_client_engine(BNCS_CLIENT_ENGINE);
	
	pthread_mutex_lock(&cm_continue_m);

	// signal to stop engines
	pthread_cond_signal(&cm_continue_cv);

	pthread_mutex_unlock(&cm_continue_m);

	pthread_join(cm_tid, NULL);

	//unregister_packet_handler(INTERNAL, BNCS_ENGINE_MESSAGE, (packet_handler_t) on_bncs_engine_shutdown);
	//unregister_packet_handler(INTERNAL, MCP_ENGINE_MESSAGE, (packet_handler_t) on_mcp_engine_shutdown);
	//unregister_packet_handler(INTERNAL, D2GS_ENGINE_MESSAGE, (packet_handler_t) on_d2gs_engine_shutdown);
	//unregister_packet_handler(INTERNAL, INTERNAL_FATAL_ERROR, (packet_handler_t) on_internal_fatal_error);

	pthread_mutex_destroy(&cm_continue_m);
	pthread_cond_destroy(&cm_continue_cv);

	pthread_mutex_destroy(&cm_manage_m);

	ui_add_statistics("reconnects: %i\n", reconnects);
}
