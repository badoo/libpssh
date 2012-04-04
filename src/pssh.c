/* Copyright (c) 2008-2009 Dmitry Novikov <rushba at eyelinkmedia dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms,
 * with or without modification, are permitted provided
 * that the following conditions are met:
 *
 *   Redistributions of source code must retain the above
 *   copyright notice, this list of conditions and the
 *   following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials
 *   provided with the distribution.
 *
 *   Neither the name of the copyright holder nor the names
 *   of any other contributors may be used to endorse or
 *   promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

/*
 * $Id: pssh.c,v 1.35 2008/05/12 11:35:50 rushba Exp $
 */
#include "debug.h"
#include "pssh_priv.h"
#include "xsys.h"

#include <sys/queue.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <event.h>
#include <evdns.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

/* static struct pssh_sess_entry *pssh_curr_server; */
/* Event handling. */
static void pssh_event_handler(int fd, short type, void *arg);
static int pssh_event_update(struct pssh_sess_entry *ent, int flags, int timeout);

static void pssh_fsm(struct pssh_sess_entry *ent);

#if 0
static void logfn(int is_warn, const char *msg)
{
	(void)is_warn;
	fprintf(stderr, "%s\n", msg);
}
#endif

static void pssh_dns_cb(int result, char type, int count, int ttl, void *addr, void *arg)
{				/* {{{ */
	struct pssh_sess_entry *ent = (struct pssh_sess_entry *)arg;
	int *iaddr = (int *)addr;
	char addr_str[100];

	if (result == DNS_ERR_NONE) {
		inet_ntop(AF_INET, addr, addr_str, sizeof(addr_str));
		ent->hostaddr = iaddr[0];
		ent->stat = PSSH_STAT_RESOLVED;

		/* Resolved, try auth. */
		ent->ev_up = 0;
		pssh_fsm(ent);
	} else {
		pssh_printf("%s: %s: %s(%d)!\n", __func__, ent->hostaddr_str, evdns_err_to_string(result), result);
		ent->stat = PSSH_STAT_ERROR;
		ent->hostaddr = -1;
	}
	(void)type;
	(void)count;
	(void)ttl;
}

/* }}} */

static int pssh_event_update(struct pssh_sess_entry *ent, int flags, int timeout)
{				/* {{{ */
	struct timeval tv;
	int ret;

	tv.tv_sec = timeout;
	tv.tv_usec = 0;
	event_set(ent->ev, ent->sock, flags, pssh_event_handler, (void *)ent);
	ret = event_add(ent->ev, &tv);
	if (ret == 0)
		ent->ev_up = 1;
	return ret;
}

/* }}} */

static int pssh_eagain_handle(struct pssh_sess_entry *ent, int timeout)
{				/* {{{ */
	int flag;
	if (libssh2_session_last_io(ent->ssh_sess) == LIBSSH2_LAST_IO_SEND) {
		flag = EV_WRITE;
	} else {
		flag = EV_READ;
	}
	return pssh_event_update(ent, flag, timeout);
}

/* }}} */

static void pssh_conn(struct pssh_sess_entry *ent, int timeout)
{				/* {{{ */
	struct sockaddr_in sin;
	int ret;
	struct timeval tv;

	tv.tv_sec = timeout;
	tv.tv_usec = 0;

	ent->sock = socket(AF_INET, SOCK_STREAM, 0);
	if (ent->sock == -1) {
		pssh_printf("socket ret -1: %d (%s)\n", errno, strerror(errno));
		pssh_printf("error in %s for %s\n", __func__, ent->hostaddr_str);
		ent->stat = PSSH_STAT_ERROR;
		return;
	}

	ret = fcntl(ent->sock, F_GETFL, 0);
	fcntl(ent->sock, F_SETFL, ret | O_NONBLOCK);

	sin.sin_family = AF_INET;
	sin.sin_port = htons(ent->port);
	memcpy(&(sin.sin_addr), &(ent->hostaddr), sizeof(struct in_addr));

	if (connect(ent->sock, (struct sockaddr *)(&sin), sizeof(struct sockaddr_in)) != 0) {
		if (errno != EINPROGRESS) {
			pssh_printf("%s: failed to connect: %d (%s)\n", ent->hostaddr_str, errno, strerror(errno));
			pssh_printf("error in %s for %s\n", __func__, ent->hostaddr_str);
			ent->stat = PSSH_STAT_ERROR;
		} else {
			ent->stat = PSSH_STAT_WAIT_CONN;
			memset(ent->ev, 0, sizeof(struct event));
			ret = pssh_event_update(ent, EV_WRITE, timeout);
			assert(ret == 0);
		}
	}
}

/* }}} */

pssh_session_t *pssh_init(const char *username, const char *public_key_path, const char *priv_key_path,
			  const char *password, int opts)
{				/* {{{ */
	struct pssh_session_t *session;
	int ret;

	/* if (getenv("SSH_AUTH_SOCK") == NULL) */
	/*     return NULL; */

	session = xmalloc(sizeof(struct pssh_session_t));
	memset(session, 0, sizeof(struct pssh_session_t));

	session->sessions = xmalloc(sizeof(struct pssh_sessions_t));
	memset(session->sessions, 0, sizeof(struct pssh_sessions_t));

	session->timeout_event = xmalloc(sizeof(struct event));
	memset(session->timeout_event, 0, sizeof(struct event));

	session->opts = opts;

	session->username = xstrdup(username);
	session->public_key = xstrdup(public_key_path);
	if (password)
		session->password = xstrdup(password);
	else
		session->password = xstrdup("");
	if (priv_key_path)
		session->priv_key = xstrdup(priv_key_path);
	else
		session->priv_key = NULL;

	session->ev_base = event_init();
	/* evdns_init(); */

	if (session->opts & PSSH_OPT_NO_SEARCH)
		ret = evdns_resolv_conf_parse(DNS_OPTION_NAMESERVERS | DNS_OPTION_MISC, "/etc/resolv.conf");
	else
		ret = evdns_resolv_conf_parse(DNS_OPTIONS_ALL, "/etc/resolv.conf");

	if (ret != 0) {
		pssh_printf("%s: evdns_resolv_conf_parse ret %d\n", __func__, ret);
		return NULL;
	}
	/* evdns_set_log_fn(logfn); */
	TAILQ_INIT(session->sessions);

	return session;
}

/* }}} */

int pssh_server_add(pssh_session_t * sess, const char *serv_name, int port)
{				/* {{{ */
	struct pssh_sess_entry *entry;

	entry = malloc(sizeof(struct pssh_sess_entry));
	assert(entry != NULL);
	memset(entry, 0, sizeof(struct pssh_sess_entry));

	strcpy(entry->hostaddr_str, serv_name);
	entry->port = port;
	entry->hostaddr = 0;
	entry->stat = PSSH_STAT_NONE;
	entry->sock = -1;
	entry->ev = malloc(sizeof(struct event));
	assert(entry->ev != NULL);
	entry->ev_up = 0;
	entry->ssh_sess = libssh2_session_init();
	assert(entry->ssh_sess != NULL);
	entry->self_sess = sess;
	libssh2_session_set_blocking(entry->ssh_sess, 0);

	if (sess->opts & PSSH_OPT_NO_SEARCH)
		evdns_resolve_ipv4(entry->hostaddr_str, DNS_QUERY_NO_SEARCH, pssh_dns_cb, entry);
	else
		evdns_resolve_ipv4(entry->hostaddr_str, 0, pssh_dns_cb, entry);

	TAILQ_INSERT_TAIL(sess->sessions, entry, entries);
	return 0;
}

/* }}} */

static void pssh_fsm(struct pssh_sess_entry *ent)
{				/* {{{ */
	int timeout;
	int conn_ret;
	int ret;
	socklen_t conn_ret_len;
	struct pssh_session_t *sess = ent->self_sess;

	assert(ent != NULL);

	timeout = sess->timeout;
	switch (ent->stat) {
	case PSSH_STAT_RESOLVED:
		pssh_printf("%s: PSSH_STAT_RESOLVED\n", ent->hostaddr_str);
		pssh_conn(ent, timeout);
		break;
	case PSSH_STAT_WAIT_CONN:
		pssh_printf("%s: PSSH_STAT_WAIT_CONN\n", ent->hostaddr_str);
		if (ent->ev_type & EV_WRITE) {
			conn_ret_len = sizeof(conn_ret);
			ret = getsockopt(ent->sock, SOL_SOCKET, SO_ERROR, &conn_ret, &conn_ret_len);
			if ((ret == 0) && (conn_ret == 0)) {
				ent->stat = PSSH_STAT_DONE_CONN;
			} else {
				pssh_printf("%s: %d: %s: PSSH_STAT_ERROR\n", __FILE__, __LINE__, ent->hostaddr_str);
				ent->stat = PSSH_STAT_ERROR;
			}
		}
		pssh_event_update(ent, EV_WRITE, timeout);
		break;
	case PSSH_STAT_DONE_CONN:
		pssh_printf("%s: PSSH_STAT_DONE_CONN\n", ent->hostaddr_str);
		ret = libssh2_session_startup(ent->ssh_sess, ent->sock);
		switch (ret) {
		case 0:
			ent->ssh_sess_started = 1;
			ent->stat = PSSH_STAT_WAIT_AUTH;
			pssh_event_update(ent, EV_WRITE, timeout);
			break;
		case LIBSSH2_ERROR_EAGAIN:
			ret = pssh_eagain_handle(ent, timeout);
			assert(ret == 0);
			break;
		default:
			pssh_printf("%s: %d: %s: PSSH_STAT_ERROR\n", __FILE__, __LINE__, ent->hostaddr_str);
			ent->stat = PSSH_STAT_ERROR;
			break;
		}
		break;
	case PSSH_STAT_WAIT_AUTH:
		pssh_printf("%s: PSSH_STAT_WAIT_AUTH\n", ent->hostaddr_str);
		ret =
		    libssh2_userauth_publickey_fromfile(ent->ssh_sess, sess->username, sess->public_key, sess->priv_key,
							sess->password);
		switch (ret) {
		case 0:
			ent->stat = PSSH_STAT_CONNECTED;
			ent->self_sess->last_connect = time(0);
			break;
		case LIBSSH2_ERROR_EAGAIN:
			pssh_printf("%s: PSSH_STAT_WAIT_AUTH: EAGAIN\n", ent->hostaddr_str);
			ret = pssh_eagain_handle(ent, timeout);
			assert(ret == 0);
			break;
		default:
			pssh_printf("%s: libssh2_userauth_publickey_fromfile ret %d (%d)\n",
				    ent->hostaddr_str, ret, libssh2_session_last_errno(ent->ssh_sess));
			ent->stat = PSSH_STAT_ERROR;
			break;
		}
		break;
	case PSSH_STAT_ERROR:
	case PSSH_STAT_CONNECTED:
		pssh_printf("%s: error || connected\n", ent->hostaddr_str);
		break;
	default:
		pssh_printf("%s: stat %d\n", ent->hostaddr_str, ent->stat);
		break;
	}
	return;
}

/* }}} */

static void pssh_event_handler(int fd, short type, void *arg)
{				/* {{{ */
	time_t is_time;
	struct pssh_sess_entry *ent = (struct pssh_sess_entry *)arg;
	assert(ent != NULL);

/*     pssh_printf("%s: %s\n", __func__, ent->hostaddr_str); */

	if (type & EV_TIMEOUT) {
		pssh_printf("%s: %s: timeout! now %ld, last %ld\n",
			    __func__, ent->hostaddr_str, time(0), ent->self_sess->last_connect);
		if (ent->self_sess->last_connect != 0) {
			is_time = time(0) - ent->self_sess->last_connect;
			if (is_time >= ent->self_sess->timeout) {
				ent->self_sess->is_timeout = 1;
				return;
			}
		} else {
			ent->self_sess->last_connect = time(0);
		}
	}

	ent->ev_type = type;
	ent->ev_up = 0;
	pssh_fsm(ent);

	(void)fd;
	return;
}

/* }}} */

static void pssh_shutdown_events(pssh_session_t * sess)
{				/* {{{ */
	int ret;
	struct pssh_sess_entry *entry;
	TAILQ_FOREACH(entry, sess->sessions, entries) {
		if (entry->ev_up) {
			ret = event_del(entry->ev);
			entry->ev_up = 0;
		}
/*         pssh_printf("%s: ret %d\n", __func__, ret); */
	}
}

/* }}} */

int pssh_connect(pssh_session_t * sess, struct pssh_sess_entry **e, int timeout)
{				/* {{{ */
	int ret;
	int need_return = 0;
	struct pssh_sess_entry *entry;
	int tot_cnt, err_cnt, ok_cnt;
	struct timeval tv;

	sess->timeout = timeout;
	tv.tv_usec = 0;
	tv.tv_sec = timeout;

	for (;;) {
		tot_cnt = err_cnt = ok_cnt = 0;
		TAILQ_FOREACH(entry, sess->sessions, entries) {
			tot_cnt++;
			if (sess->is_timeout) {
				sess->last_connect = 0;
				sess->is_timeout = 0;
				ret = PSSH_TIMEOUT;
				*e = NULL;
				pssh_shutdown_events(sess);
				evdns_shutdown(0);
				need_return = 1;
				break;
			}

			if (entry->stat == PSSH_STAT_CONNECTED) {
				if (entry->is_reported == 0) {
					entry->is_reported = 1;
					need_return = 1;
					*e = entry;
					ret = PSSH_CONNECTED;
					break;
				} else {
					ok_cnt++;
				}
			} else if (entry->stat == PSSH_STAT_ERROR) {
				err_cnt++;
			}
		}

		if (need_return) {
			/* Ugly hack */
			if (ret == PSSH_TIMEOUT) {
				TAILQ_FOREACH(entry, sess->sessions, entries) {
					if (entry->stat != PSSH_STAT_CONNECTED)
						entry->stat = PSSH_STAT_ERROR;
				}
			}

			break;
		}

		/* pssh_printf("tot_cnt %d, err_cnt %d, ok_cnt %d\n", */
		/*             tot_cnt, err_cnt, ok_cnt); */

		if ((err_cnt + ok_cnt) == tot_cnt) {
			sess->last_connect = 0;
			sess->is_timeout = 0;
			evdns_shutdown(0);
			if (err_cnt != 0)
				ret = PSSH_FAILED;
			else
				ret = PSSH_SUCCESS;
			*e = NULL;
			break;
		}

		event_loop(EVLOOP_ONCE);

	}
	return ret;
}

/* }}} */

struct pssh_sess_entry *pssh_server_first(pssh_session_t * sess)
{				/* {{{ */
	sess->pssh_curr_server = TAILQ_FIRST(sess->sessions);
	return sess->pssh_curr_server;
}

/* }}} */

struct pssh_sess_entry *pssh_server_next(pssh_session_t * sess)
{				/* {{{ */
	if (sess && sess->pssh_curr_server) {
		sess->pssh_curr_server = TAILQ_NEXT(sess->pssh_curr_server, entries);
		return sess->pssh_curr_server;
	}
	return NULL;
}

/* }}} */

char *pssh_serv_name(struct pssh_sess_entry *ent)
{				/* {{{ */
	if (ent != NULL)
		return ent->hostaddr_str;
	return NULL;
}

/* }}} */

pssh_stat_t pssh_stat(struct pssh_sess_entry * ent)
{				/* {{{ */
	pssh_stat_t st;
	switch (ent->stat) {
	case PSSH_STAT_NONE:
		st = PSSH_NONE;
		break;
	case PSSH_STAT_ERROR:
		st = PSSH_ERROR;
		break;
	case PSSH_STAT_CONNECTED:
		st = PSSH_DONE;
		break;
	default:
		st = PSSH_INPROGRESS;
		break;
	}
	return st;
}

/* }}} */

int pssh_serv_port(struct pssh_sess_entry *ent)
{				/* {{{ */
	return ent->port;
}

/* }}} */

const char *pssh_stat_str(struct pssh_sess_entry *ent)
{				/* {{{ */
	static const char *stat_str[] = {
		"PSSH_STAT_ERROR",
		"PSSH_STAT_NONE",
		"PSSH_STAT_WAIT_RESOLVE",
		"PSSH_STAT_RESOLVED",
		"PSSH_STAT_WAIT_CONN",
		"PSSH_STAT_DONE_CONN",
		"PSSH_STAT_WAIT_SESS",
		"PSSH_STAT_WAIT_AUTH",
		"PSSH_STAT_CONNECTED"
	};

	return stat_str[ent->stat];
}

/* }}} */

void pssh_free(pssh_session_t * s)
{				/* {{{ */
	struct pssh_sess_entry *entry;
	while (s->sessions->tqh_first != NULL) {
		entry = s->sessions->tqh_first;
		TAILQ_REMOVE(s->sessions, s->sessions->tqh_first, entries);
		if (entry->ssh_sess_started)
			libssh2_session_disconnect(entry->ssh_sess, "Aloha, Hawai!");
		libssh2_session_free(entry->ssh_sess);
		free(entry->ev);
		free(entry);
	}

	free(s->sessions);
	free(s->public_key);
	free(s->username);
	free(s->priv_key);
	free(s->password);
	free(s->timeout_event);

	/* evdns_shutdown(0); */
	event_base_free(s->ev_base);
	free(s);
}

/* }}} */
