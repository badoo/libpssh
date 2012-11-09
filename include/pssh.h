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
 * $Id: pssh.h,v 1.15 2008/05/05 15:01:27 rushba Exp $
 */
#ifndef PSSH_H
#define PSSH_H

enum {
	PSSH_CONNECTED,
	PSSH_RUNNING,
	PSSH_TIMEOUT,
	PSSH_FAILED,
	PSSH_SUCCESS
};

typedef enum {
	PSSH_NONE,
	PSSH_ERROR,
	PSSH_INPROGRESS,
	PSSH_DONE
} pssh_stat_t;

/* Consts for opts in pssh_session_t (bit fields) */
#define PSSH_OPT_NO_SEARCH 1

typedef struct pssh_session_t pssh_session_t;
typedef struct pssh_task_list_t pssh_task_list_t;

/* connect funcs */
pssh_session_t *pssh_init(const char *username, const char *public_key_path, const char *priv_key_path,
			  const char *password, int opts);
int pssh_server_add(pssh_session_t * sess, const char *serv_name, int port);
struct pssh_sess_entry *pssh_server_first(pssh_session_t * sess);
struct pssh_sess_entry *pssh_server_next(pssh_session_t * sess);
pssh_stat_t pssh_stat(struct pssh_sess_entry *ent);
const char *pssh_stat_str(struct pssh_sess_entry *ent);
char *pssh_serv_name(struct pssh_sess_entry *ent);
int pssh_serv_port(struct pssh_sess_entry *ent);
int pssh_connect(pssh_session_t * sess, struct pssh_sess_entry **ent, int timeout);
void pssh_free(pssh_session_t * s);

/* exec funcs */
/* Task type */
typedef enum {
	PSSH_TASK_NONE,
	PSSH_TASK_ERROR,
	PSSH_TASK_INPROGRESS,
	PSSH_TASK_DONE
} pssh_task_stat_t;

typedef enum {
	PSSH_TASK_TYPE_COPY,
	PSSH_TASK_TYPE_EXEC
} pssh_task_type_t;

pssh_task_list_t *pssh_task_list_init(pssh_session_t * sess);
/* like scp l_fn srv:r_fn */
int pssh_cp_to_server(pssh_task_list_t * tl, const char *srv, const char *l_fn, const char *r_fn, int timeout_sec);
/* like scp srv:r_fn l_fn */
int pssh_cp_from_server(pssh_task_list_t * tl, const char *srv, const char *l_fn, const char *r_fn, int timeout_sec);
/* like ssh -T srv cmd */
int pssh_add_cmd(pssh_task_list_t * tl, const char *srv, const char *cmd, int timeout_sec);

/* Walk tasks. */
struct pssh_task_t *pssh_task_first(pssh_task_list_t * tl);
struct pssh_task_t *pssh_task_next(pssh_task_list_t * tl);

/* Interface for private members of pssh_task_t structure */
char *pssh_task_server_name(struct pssh_task_t *task);
pssh_task_stat_t pssh_task_stat(struct pssh_task_t *task);
int pssh_task_stdout_len(struct pssh_task_t *task);
char *pssh_task_stdout(struct pssh_task_t *task);
int pssh_task_stderr_len(struct pssh_task_t *task);
char *pssh_task_stderr(struct pssh_task_t *task);
int pssh_task_exit_status(struct pssh_task_t *task);
pssh_task_type_t pssh_task_type(struct pssh_task_t *task);
char *pssh_task_get_cmd(struct pssh_task_t *task);

/* start execute */
int pssh_exec(pssh_task_list_t * tl, struct pssh_task_t **t);

/* Cleanup */
void pssh_task_list_free(pssh_task_list_t * tl);

#endif
