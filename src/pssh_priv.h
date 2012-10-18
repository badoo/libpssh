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
 * $Id: pssh_priv.h,v 1.19 2008/05/05 15:01:27 rushba Exp $
 */
#ifndef PSSH_PRIV_H
#define PSSH_PRIV_H

#include "pssh.h"
#include "pssh_config.h"

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <libssh2.h>

#define PSSH_HOSTADDR_LEN    100
#define PSSH_EXEC_ALLOC_SIZE 8192

#define TAILQ_FIRST(head) ((head)->tqh_first)
#define	TAILQ_NEXT(elm, field)	((elm)->field.tqe_next)
#ifndef TAILQ_FOREACH
#define	TAILQ_FOREACH(var, head, field)					\
	for ((var) = ((head)->tqh_first);				\
		(var);							\
		(var) = ((var)->field.tqe_next))
#endif

typedef enum {
    PSSH_STAT_ERROR,
    PSSH_STAT_NONE,
    PSSH_STAT_WAIT_RESOLVE,
    PSSH_STAT_RESOLVED,
    PSSH_STAT_WAIT_CONN,
    PSSH_STAT_DONE_CONN,
    PSSH_STAT_WAIT_SESS,
    PSSH_STAT_WAIT_AUTH,
    PSSH_STAT_CONNECTED
} pssh_int_stat_t;

TAILQ_HEAD(pssh_sessions_t, pssh_sess_entry);

struct pssh_sess_entry {
    char                   hostaddr_str[PSSH_HOSTADDR_LEN];
    int                    hostaddr;
    int                    port;
    int                    sock;
    int                    is_reported;
    pssh_int_stat_t        stat;
    LIBSSH2_SESSION       *ssh_sess;
    int                    ssh_sess_started;
    char                   ev_up;
    struct event          *ev;
    short                  ev_type;
    struct pssh_session_t *self_sess;
    TAILQ_ENTRY(pssh_sess_entry) entries;
};

struct pssh_session_t {
    char                   *username;
    char                   *public_key;
    char                   *priv_key;
    char                   *password;
    int                     timeout;
    struct event           *timeout_event;
    int                     is_timeout;
    time_t                  last_connect;
    struct pssh_sessions_t *sessions;
    struct pssh_sess_entry *pssh_curr_server;
    struct event_base      *ev_base;
    struct evdns_base      *evdns_base;
    int                     opts;
};

/*
 * Task's structures.
 */
typedef enum {
    PSSH_TASK_STAT_NONE,
    PSSH_TASK_STAT_ERROR,
    PSSH_TASK_STAT_START,
    PSSH_TASK_STAT_CHANNEL_READY,
    PSSH_TASK_STAT_DATA_TRANSFER,
    PSSH_TASK_STAT_CMD_RUNNING,
    PSSH_TASK_STAT_STREAMS_READED,
    PSSH_TASK_STAT_DONE
} pssh_task_int_stat_t;

typedef enum {
    PSSH_CD_TO_SERV,
    PSSH_CD_FROM_SERV
} pssh_copy_direction_t;

typedef struct {
    pssh_copy_direction_t  dir;
    char                   l_fn[MAXPATHLEN+1]; /* local file name */
    char                   r_fn[MAXPATHLEN+1]; /* remote file name */
    char                  *data; /* file content */
    int                    tcnt; /* amount transferred data */
    struct stat            st;  /* file info */
} pssh_copy_task_t;

typedef struct {
    char *data;
    int   alloc;
    int   len;
    int   eof;
} pssh_exec_stream_t;

typedef struct {
    char  cmd[MAXPATHLEN+1];
    pssh_exec_stream_t out_stream;
    pssh_exec_stream_t err_stream;
    int   ret_code;
} pssh_exec_task_t;

TAILQ_HEAD(pssh_tasks_t, pssh_task_t);
struct pssh_task_t {
    char                    hostname[PSSH_HOSTADDR_LEN];
    int                     is_reported;
    struct pssh_sess_entry *sess_entry;
    pssh_task_type_t        type;
    pssh_task_int_stat_t    stat;
    LIBSSH2_CHANNEL        *channel;
    /* struct event           *ev; */
    /* char                    ev_up; */
    /* short                   ev_type; */
    union {
        pssh_copy_task_t    cp;
        pssh_exec_task_t    ex;
    } task;
    /* For sequence of task. Not yet ready. */
    TAILQ_ENTRY(pssh_task_t) next_task;
    TAILQ_ENTRY(pssh_task_t) entries;
};

struct pssh_task_list_t {
    struct pssh_session_t *sess;
    struct pssh_task_t *curr_task;      /* Pointer for pssh_task_first etc... */
    /* Counters for timeouts. */
    int done_cnt;
    int err_cnt;
    int skip_cnt;
    time_t stamp;
    struct pssh_tasks_t *tasks;
};

#endif
