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
 * $Id: task.c,v 1.25 2008/04/29 08:44:09 rushba Exp $
 */
#include "debug.h"
#include "pssh_priv.h"
#include "xsys.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <event.h>

#define PSSH_EAGAIN LIBSSH2_ERROR_EAGAIN

static void pssh_task_event_handler(int fd, short type, void *arg);

static struct pssh_sess_entry *pssh_find_ent(pssh_session_t *sess, const char *srv)
{
    struct pssh_sess_entry *ent;
    TAILQ_FOREACH(ent, sess->sessions, entries) {
        if (strcmp(ent->hostaddr_str, srv) == 0)
            return ent;
    }
    return NULL;
}

pssh_task_list_t *pssh_task_list_init(pssh_session_t *sess)
{
    struct pssh_task_list_t *tl = xmalloc(sizeof(struct pssh_task_list_t));
    tl->sess = sess;
    tl->tasks = xmalloc(sizeof(struct pssh_tasks_t));
    TAILQ_INIT(tl->tasks);

    return tl;
}

static int pssh_task_event_update(struct pssh_task_t *t, int flags, int timeout)
{
    struct timeval tv;

    if (t->sess_entry->ev_up == 0) {
        tv.tv_sec = timeout;
        tv.tv_usec = 0;
        event_set(t->sess_entry->ev, t->sess_entry->sock, flags, pssh_task_event_handler, t);
        t->sess_entry->ev_up = 1;
        return event_add(t->sess_entry->ev, &tv);
    }
    return 0;
}

static int pssh_task_eagain_handle(struct pssh_task_t *t, int timeout)
{
    int flag;
    if (libssh2_session_last_io(t->sess_entry->ssh_sess) == LIBSSH2_LAST_IO_SEND) {
        flag = EV_WRITE;
    } else {
        flag = EV_READ;
    }
    return pssh_task_event_update(t, flag, timeout);
}

/* Initialize common fields for cp & exec task. */
static struct pssh_task_t *pssh_init_common_task(pssh_session_t *sess, const char *srv, pssh_task_type_t type)
{
    struct pssh_task_t *t = xmalloc(sizeof(struct pssh_task_t));

    /* t->ev = xmalloc(sizeof(struct event)); */
    /* t->ev_up = 0; */
    strcpy(t->hostname, srv);
    t->type = type;
    t->stat = PSSH_TASK_STAT_NONE;
    t->is_reported = 0;
    t->sess_entry = pssh_find_ent(sess, srv);
    t->channel = NULL;
    return t;
}

static int check_zero_str(const char *s)
{
    if ((s == NULL) || (strlen(s) == 0))
        return -1;
    return 0;
}

static int pssh_add_cp_task(pssh_task_list_t *tl, const char *srv, const char *l_fn, const char *r_fn,
                            pssh_copy_direction_t dir)
{
    struct pssh_task_t *t;

    if (tl == NULL)
        return -1;

    if ((check_zero_str(srv) == -1) ||
        (check_zero_str(l_fn) == -1) ||
        (check_zero_str(r_fn) == -1))
        return -1;

    t = pssh_init_common_task(tl->sess, srv, PSSH_TASK_TYPE_COPY);
    memset(&t->task.cp, 0, sizeof(t->task.cp));
    t->task.cp.dir = dir;
    strcpy(t->task.cp.l_fn, l_fn);
    strcpy(t->task.cp.r_fn, r_fn);
    t->task.cp.data = NULL;
    TAILQ_INSERT_TAIL(tl->tasks, t, entries);
    return 0;
}

int pssh_cp_to_server(pssh_task_list_t *tl, const char *srv, const char *l_fn, const char *r_fn)
{
    return pssh_add_cp_task(tl, srv, l_fn, r_fn, PSSH_CD_TO_SERV);
}

int pssh_cp_from_server(pssh_task_list_t *tl, const char *srv, const char *l_fn, const char *r_fn)
{
    return pssh_add_cp_task(tl, srv, l_fn, r_fn, PSSH_CD_FROM_SERV);
}

int pssh_add_cmd(pssh_task_list_t *tl, const char *srv, const char *cmd)
{
    struct pssh_task_t *t;

    if (tl == NULL)
        return -1;
    if ((check_zero_str(srv) == -1) ||
        (check_zero_str(cmd) == -1))
        return -1;

    t = pssh_init_common_task(tl->sess, srv, PSSH_TASK_TYPE_EXEC);
    memset(&t->task.ex, 0, sizeof(pssh_exec_task_t));
    strcpy(t->task.ex.cmd, cmd);
    TAILQ_INSERT_TAIL(tl->tasks, t, entries);
    return 0;
}

/*
 * copy to remote server
 */
static int pssh_task_get_send_channel(struct pssh_task_t *task)
{
    int ret;
    int fd;
    pssh_copy_task_t *cp = &task->task.cp;
    int nread;

    if (cp->data == NULL) {
        ret = stat(cp->l_fn, &cp->st);
        if (ret == -1) {
            pssh_printf("can't stat ``%s'': %s (%d)\n", cp->l_fn, strerror(errno), errno);
            task->stat = PSSH_TASK_STAT_ERROR;
            return 0;
        }
        fd = open(cp->l_fn, O_RDONLY);
        if (fd == -1) {
            pssh_printf("can't open ``%s'': %s (%d)\n", cp->l_fn, strerror(errno), errno);
            task->stat = PSSH_TASK_STAT_ERROR;
            return 0;
        }
        cp->data = xmalloc(cp->st.st_size);
        
        nread = 0;
        while (nread < cp->st.st_size) {
            ret = read(fd, cp->data+nread, cp->st.st_size-nread);
            if (ret < 0) {
                pssh_printf("can't read ``%s'': %s (%d)\n", cp->l_fn, strerror(errno), errno);
                task->stat = PSSH_TASK_STAT_ERROR;
                return 0;
            }
            nread += ret;
        }
        cp->tcnt = 0;
        ret = close(fd);
    }

    task->channel = libssh2_scp_send(task->sess_entry->ssh_sess, cp->r_fn,
                                     0x1FF & cp->st.st_mode, cp->st.st_size);
    if (task->channel == NULL) {
        if (libssh2_session_last_errno(task->sess_entry->ssh_sess) != LIBSSH2_ERROR_EAGAIN) {
            pssh_printf("%s: lasrt_errno: %d\n", __func__, libssh2_session_last_errno(task->sess_entry->ssh_sess));
            task->stat = PSSH_TASK_STAT_ERROR;
            ret = 0;
        } else {
            ret = PSSH_EAGAIN;
        }
    } else {
        task->stat = PSSH_TASK_STAT_CHANNEL_READY;
        ret = 0;
    }

    return ret;
}

static int pssh_task_get_recv_channel(struct pssh_task_t *task)
{
    int ret = 0;
    pssh_copy_task_t *cp = &task->task.cp;

    task->channel = libssh2_scp_recv(task->sess_entry->ssh_sess, cp->r_fn, &cp->st);
    if (task->channel == NULL) {
        if (libssh2_session_last_errno(task->sess_entry->ssh_sess) != LIBSSH2_ERROR_EAGAIN) {
            pssh_printf("%s: lasrt_errno: %d\n", __func__, libssh2_session_last_errno(task->sess_entry->ssh_sess));
            task->stat = PSSH_TASK_STAT_ERROR;
        } else {
            ret = PSSH_EAGAIN;
        }
    } else {
        task->stat = PSSH_TASK_STAT_CHANNEL_READY;
    }
    return ret;
}

static int pssh_task_get_scp_channel(struct pssh_task_t *task)
{
    int ret;
    if (task->task.cp.dir == PSSH_CD_TO_SERV) {
        ret = pssh_task_get_send_channel(task);
    } else {
        ret = pssh_task_get_recv_channel(task);
    }
    return ret;
}

static int pssh_task_send_data(struct pssh_task_t *task)
{
    int ret;
    pssh_copy_task_t *cp = &task->task.cp;
    int len = cp->st.st_size - cp->tcnt;

    if (len > 8*1024)
        len = 8*1024;

    ret = libssh2_channel_write(task->channel, cp->data + cp->tcnt, len);
    pssh_printf("%s: %s: write %d\n", __func__, cp->l_fn, ret);
    if (ret < 0) {
        if (libssh2_session_last_errno(task->sess_entry->ssh_sess) != LIBSSH2_ERROR_EAGAIN) {
            pssh_printf("%s: lasrt_errno: %d\n", __func__, libssh2_session_last_errno(task->sess_entry->ssh_sess));
            task->stat = PSSH_TASK_STAT_ERROR;
            ret = 0;
        } else {
            ret = PSSH_EAGAIN;
        }
        return ret;
    }
    cp->tcnt += ret;

    if (cp->tcnt == cp->st.st_size) {
        /* Data transferred. */
        task->stat = PSSH_TASK_STAT_DATA_TRANSFER;
        ret = 0;
    }
    return 0;
    /* return ret; */
}

static int pssh_task_recv_data(struct pssh_task_t *task)
{
    int ret;
    pssh_copy_task_t *cp = &task->task.cp;

    if (cp->data == NULL) {
        cp->data = xmalloc(cp->st.st_size);
        cp->tcnt = 0;
    }

    ret = libssh2_channel_read(task->channel, cp->data + cp->tcnt, cp->st.st_size - cp->tcnt);
    if (ret == LIBSSH2_ERROR_EAGAIN) {
        ret = PSSH_EAGAIN;
    } else if (ret < 0) {
        task->stat = PSSH_TASK_STAT_ERROR;
        ret = 0;
    } else {
        cp->tcnt += ret;
        if (cp->tcnt == cp->st.st_size) {
            int fd = open(cp->l_fn, O_WRONLY|O_CREAT|O_TRUNC, cp->st.st_mode);
            if (fd == -1) {
                pssh_printf("open %s: %d (%s)\n", cp->l_fn, errno, strerror(errno));
                task->stat = PSSH_TASK_STAT_ERROR;
            } else if ((ret = write(fd, cp->data, cp->st.st_size)) != cp->st.st_size) {
                pssh_printf("write %d\n", ret);
                task->stat = PSSH_TASK_STAT_ERROR;
            } else if ((ret = close(fd)) == -1) {
                pssh_printf("close %d\n", ret);
                task->stat = PSSH_TASK_STAT_ERROR;
            } else {
                task->stat = PSSH_TASK_STAT_DATA_TRANSFER;
            }
        }
        ret = 0;
    }
    return ret;
}

static int pssh_task_transfer_data(struct pssh_task_t *task)
{
    int ret;
    if (task->task.cp.dir == PSSH_CD_TO_SERV) {
        ret = pssh_task_send_data(task);
    } else {
        ret = pssh_task_recv_data(task);
    }
    return ret;
}

static int pssh_task_cleanup_copy(struct pssh_task_t *task)
{
    pssh_copy_task_t *cp = &task->task.cp;

    assert(task->stat == PSSH_TASK_STAT_DATA_TRANSFER);

    /* Cleanup & shutdown */
    free(cp->data);
    cp->data = NULL;

    /* while (libssh2_channel_send_eof(task->channel) == LIBSSH2_ERROR_EAGAIN); */
    /* while (libssh2_channel_wait_eof(task->channel) == LIBSSH2_ERROR_EAGAIN); */
    /* while (libssh2_channel_wait_closed(task->channel) == LIBSSH2_ERROR_EAGAIN); */

    libssh2_channel_free(task->channel);
    task->channel = NULL;
    task->stat = PSSH_TASK_STAT_DONE;
    return 0;
}

static int pssh_task_run_copy_task(struct pssh_task_t *task)
{
    int ret = -1;
    switch (task->stat) {
    case PSSH_TASK_STAT_START:
        /* Init state. Try get channel for scp. */
        pssh_printf("%s: %s PSSH_TASK_STAT_START\n", task->task.cp.l_fn, task->hostname); 
        ret = pssh_task_get_scp_channel(task);
        break;
    case PSSH_TASK_STAT_CHANNEL_READY:
        /* Channel ready. Try send data. */
        pssh_printf("%s: %s: PSSH_TASK_STAT_CHANNEL_READY\n", task->task.cp.l_fn, task->hostname);
        ret = pssh_task_transfer_data(task);
        break;
    case PSSH_TASK_STAT_DATA_TRANSFER:
        /* Data transferred. Cleanup & shutdown channel */
        pssh_printf("%s: %s: PSSH_TASK_STAT_DATA_TRANSFER\n", task->task.cp.l_fn, task->hostname);
        ret = pssh_task_cleanup_copy(task);
        break;
    case PSSH_TASK_STAT_ERROR:
        /* Some error on copy occored. */
        pssh_printf("%s: %s: PSSH_TASK_STAT_ERROR\n", task->task.cp.l_fn, task->hostname);
        ret = 0;
        break;
    case PSSH_TASK_STAT_DONE:
        /* Hooooooooraaaay! */
        pssh_printf("%s: %s: PSSH_TASK_STAT_DONE\n", task->task.cp.l_fn, task->hostname);
        ret = 0;
        break;
    default:
        pssh_printf("%s: task->stat %d\n", task->hostname, task->stat);
        abort();
    }

    return ret;
}

/*
 * cmds on remote servers routines.
 */

static int pssh_task_get_exec_channel(struct pssh_task_t *task)
{
    int ret = 0;
    task->channel = libssh2_channel_open_session(task->sess_entry->ssh_sess);
    if (task->channel == NULL) {
        if (libssh2_session_last_errno(task->sess_entry->ssh_sess) != LIBSSH2_ERROR_EAGAIN) {
            pssh_printf("%s: lasrt_errno: %d\n", __func__, libssh2_session_last_errno(task->sess_entry->ssh_sess));
            task->stat = PSSH_TASK_STAT_ERROR;
        } else {
            ret = PSSH_EAGAIN;
        }
    } else {
        task->stat = PSSH_TASK_STAT_CHANNEL_READY;
    }
    return ret;
}

static int pssh_task_do_cmd(struct pssh_task_t *task)
{
    int ret = 0;
    pssh_exec_task_t *ex = &task->task.ex;

    ret = libssh2_channel_exec(task->channel, ex->cmd);
    if (ret < 0) {
        if (libssh2_session_last_errno(task->sess_entry->ssh_sess) != LIBSSH2_ERROR_EAGAIN) {
            pssh_printf("%s: lasrt_errno: %d\n", __func__, libssh2_session_last_errno(task->sess_entry->ssh_sess));
            task->stat = PSSH_TASK_STAT_ERROR;
            ret = 0;
        } else {
            ret = PSSH_EAGAIN;
        }
   } else {
        task->stat = PSSH_TASK_STAT_CMD_RUNNING;
        ret = 0;
    }
    return ret;
}

static int pssh_task_read_stream(struct pssh_task_t *task, pssh_exec_stream_t *s, int stream_id)
{
    int ret;
    if (s->data == NULL) {
        s->data = xmalloc(PSSH_EXEC_ALLOC_SIZE);
        s->alloc = PSSH_EXEC_ALLOC_SIZE;
        s->len = 0;
    }

    ret = libssh2_channel_read_ex(task->channel, stream_id, s->data + s->len, s->alloc - s->len);
    if (ret < 0) {
        if (libssh2_session_last_errno(task->sess_entry->ssh_sess) != LIBSSH2_ERROR_EAGAIN) {
            pssh_printf("%s: lasrt_errno: %d\n", __func__, libssh2_session_last_errno(task->sess_entry->ssh_sess));
            pssh_printf("ret %d\n", ret);
            task->stat = PSSH_TASK_STAT_ERROR;
        }
    } else if (ret == 0) {
            s->eof = 1;
    } else {
        s->len += ret;
/*         pssh_printf("%s: all %d len %d ret %d\n", task->hostname, s->alloc, s->len, ret); */
        if (s->len == s->alloc) {
            s->data = xrealloc(s->data, s->alloc + PSSH_EXEC_ALLOC_SIZE);
            s->alloc += PSSH_EXEC_ALLOC_SIZE;
        }
    }
    return ret;
}

static int pssh_task_read_stdout(struct pssh_task_t *task)
{
    pssh_exec_task_t *ex = &task->task.ex;
    int ret = pssh_task_read_stream(task, &ex->out_stream, 0);
    switch(ret) {
    case -1:
        if (libssh2_session_last_errno(task->sess_entry->ssh_sess) != LIBSSH2_ERROR_EAGAIN) {
            pssh_printf("%s: lasrt_errno: %d\n", __func__, libssh2_session_last_errno(task->sess_entry->ssh_sess));
            task->stat = PSSH_TASK_STAT_ERROR;
            ret = 0;
        } else {
            ret = PSSH_EAGAIN;
        }
        break;
    case 0:
        break;
    }
    return ret;
}

static int pssh_task_read_stderr(struct pssh_task_t *task)
{
    pssh_exec_task_t *ex = &task->task.ex;
    int ret = pssh_task_read_stream(task, &ex->err_stream, SSH_EXTENDED_DATA_STDERR);
    switch(ret) {
    case -1:
        if (libssh2_session_last_errno(task->sess_entry->ssh_sess) != LIBSSH2_ERROR_EAGAIN) {
            pssh_printf("%s: lasrt_errno: %d\n", __func__, libssh2_session_last_errno(task->sess_entry->ssh_sess));
            task->stat = PSSH_TASK_STAT_ERROR;
            ret = 0;
        } else {
            ret = PSSH_EAGAIN;
        }
        break;
    case 0:
        ret = 0;
        break;
    }
    return ret;
}

static int pssh_task_cleanup_exec(struct pssh_task_t *task)
{
/*     pssh_exec_task_t *ex = &task->task.ex; */

    task->task.ex.ret_code = libssh2_channel_get_exit_status(task->channel);
/*
    free(ex->out_stream.data);
    free(ex->err_stream.data);
    ex->out_stream.data = NULL;
    ex->err_stream.data = NULL;
*/
    libssh2_channel_free(task->channel);
    task->stat = PSSH_TASK_STAT_DONE;
    return 0;
}

static int pssh_task_run_exec_task(struct pssh_task_t *task)
{
    int ret = -1;
    switch (task->stat) {
    case PSSH_TASK_STAT_START:
        /* Init state. Try get channel. */
/*         pssh_printf("%s: PSSH_TASK_STAT_NONE\n", task->hostname); */
        ret = pssh_task_get_exec_channel(task);
        break;
    case PSSH_TASK_STAT_CHANNEL_READY:
        /* Channel ready. Try exec cmd. */
/*         pssh_printf("%s: PSSH_TASK_STAT_CHANNEL_READY\n", task->hostname); */
        ret = pssh_task_do_cmd(task);
        break;
    case PSSH_TASK_STAT_CMD_RUNNING:
    {
        /* Cmd running on remote host. Getting stdout & stderr. */
        int eof_err, eof_out;
        int ret_err, ret_out;

/*         pssh_printf("%s: PSSH_TASK_STAT_CMD_RUNNING\n", task->hostname); */

        for (;;) {
            ret_err = ret_out = 0;
            eof_err = task->task.ex.err_stream.eof;
            eof_out = task->task.ex.out_stream.eof;
            if (!eof_err)
                ret_err = pssh_task_read_stderr(task);
            if (!eof_out)
                ret_out = pssh_task_read_stdout(task);

/*             pssh_printf("%s: %s: eof_out %d, eof_err %d, ret_out %d, ret_err %d\n", */
/*                    __func__, task->hostname, eof_out, eof_err, ret_out, ret_err); */

            if ((ret_out == 0) && (ret_err == 0)) {
                task->stat = PSSH_TASK_STAT_STREAMS_READED;
                pssh_task_cleanup_exec(task);
                ret = 0;
                break;
            }
            if ((ret_err == PSSH_EAGAIN) || (ret_out == PSSH_EAGAIN)) {
                ret = PSSH_EAGAIN;
                break;
            }
        }
        break;
    }
    case PSSH_TASK_STAT_STREAMS_READED:
        /* stdout readed. Getting stderr. */
/*         pssh_printf("%s: PSSH_TASK_STAT_STDOUT_READED\n", task->hostname); */
        ret = pssh_task_cleanup_exec(task);
        break;
    case PSSH_TASK_STAT_ERROR:
/*         pssh_printf("%s: PSSH_TASK_STAT_ERROR\n", task->hostname); */
        ret = 0;
        break;
    case PSSH_TASK_STAT_DONE:
        /* Nothing more */
/*         pssh_printf("%s: PSSH_TASK_STAT_DONE\n", task->hostname); */
        ret = 0;
        break;
    default:
/*         pssh_printf("task->stat %d\n", task->stat); */
        abort();
    }
    return ret;
}

static void pssh_task_fsm(struct pssh_task_t *t)
{
    int ret = -1;
    int timeout = 100;

    switch (t->type) {
    case PSSH_TASK_TYPE_COPY:
        ret = pssh_task_run_copy_task(t);
        break;
    case PSSH_TASK_TYPE_EXEC:
        ret = pssh_task_run_exec_task(t);
        break;
    default:
        pssh_printf("%s: unknown task type(%d)\n", __func__, t->type);
        t->stat = PSSH_TASK_STAT_ERROR;
    }

    pssh_printf("%s: %s: ret %d, lastio: %d\n",
           __func__, t->hostname, ret, libssh2_session_last_io(t->sess_entry->ssh_sess));

    switch (ret) {
    case PSSH_EAGAIN:
        if (t->stat == PSSH_TASK_STAT_CMD_RUNNING)
            ret = pssh_task_event_update(t, EV_WRITE|EV_READ, timeout);
        else
            ret = pssh_task_eagain_handle(t, timeout);
        break;
    case 0:
        if (t->stat != PSSH_TASK_STAT_DONE &&
            t->stat != PSSH_TASK_STAT_ERROR) {
            ret = pssh_task_event_update(t, EV_WRITE, timeout);
            if (ret != 0) {
                pssh_printf("%s: can't update event for %s\n",
                            __func__, t->hostname);
                t->stat = PSSH_TASK_STAT_ERROR;
            }
        }
        break;
    default:
        pssh_printf("%s: unwaiting ret code %d. Setting error for %s\n",
                    __func__, ret, t->hostname);
        t->stat = PSSH_TASK_STAT_ERROR;
    }

}

static void pssh_task_event_handler(int fd, short type, void *arg)
{
    struct pssh_task_t *t = (struct pssh_task_t *)arg;
    assert(t != NULL);
    /* pssh_printf("%s\n", __func__); */
    t->sess_entry->ev_type = type;
    t->sess_entry->ev_up = 0;

    pssh_task_fsm(t);
    (void) fd;
    return;
}

int pssh_exec(pssh_task_list_t *tl, struct pssh_task_t **t, int timeout)
{
    int ret;
    struct pssh_task_t *task;
    int tot_cnt, ok_cnt, err_cnt, skip_cnt;
    int need_return = 0;

    for (;;) {
        tot_cnt = ok_cnt = err_cnt = skip_cnt = 0;
        TAILQ_FOREACH(task, tl->tasks, entries) {
            tot_cnt++;
            if ((task->sess_entry == NULL) ||
                (task->sess_entry->stat != PSSH_STAT_CONNECTED)) {
                skip_cnt++;
                continue;
            }

            if ((task->stat == PSSH_TASK_STAT_NONE) || (task->stat == PSSH_TASK_STAT_START)) {
                ret = pssh_task_event_update(task, EV_WRITE, timeout);
                assert(ret == 0);
                task->stat = PSSH_TASK_STAT_START;
                continue;
            }

            if (task->stat == PSSH_TASK_STAT_ERROR) {
                err_cnt++;
                continue;
            }

            if (task->stat == PSSH_TASK_STAT_DONE) {
                if (task->is_reported == 0) {
                    task->is_reported = 1;
                    need_return = 1;
                    *t = task;
                    ret = PSSH_RUNNING;
                    break;
                } else {
                    ok_cnt++;
                }
            }
        }

        if (need_return)
            break;

        pssh_printf("tot_cnt %d, err_cnt %d, ok_cnt %d, skip_cnt %d\n",
                    tot_cnt, err_cnt, ok_cnt, skip_cnt);

        if ((ok_cnt + err_cnt + skip_cnt) == tot_cnt) {
            need_return = 1;
            if (err_cnt)
                ret = PSSH_FAILED;
            else
                ret = PSSH_SUCCESS;
            *t = NULL;
            break;
        }

        event_loop(EVLOOP_ONCE);
    }

    return ret;
}

struct pssh_task_t *pssh_task_first(pssh_task_list_t *tl)
{
    if (!tl)
        return NULL;
    else {
        tl->curr_task = TAILQ_FIRST(tl->tasks);
        return tl->curr_task;
    }
}

struct pssh_task_t *pssh_task_next(pssh_task_list_t *tl)
{
    if (!tl)
        return NULL;
    else {
        tl->curr_task = TAILQ_NEXT(tl->curr_task, entries);
        return tl->curr_task;
    }
}

/* Interface for private members of pssh_task_t structure */
inline char *pssh_task_server_name(struct pssh_task_t *task)
{
    if (task)
        return task->hostname;
    else
        return NULL;
}

inline pssh_task_stat_t pssh_task_stat(struct pssh_task_t *task)
{
    pssh_task_stat_t st;

    switch (task->stat) {
    case PSSH_TASK_STAT_NONE:
        st = PSSH_TASK_NONE;
        break;
    case PSSH_TASK_STAT_ERROR:
        st = PSSH_TASK_ERROR;
        break;
    case PSSH_TASK_STAT_DONE:
        st = PSSH_TASK_DONE;
        break;
    default:
        st = PSSH_TASK_INPROGRESS;
        break;
    }
    return st;
}

inline static int pssh_is_cmd_task(struct pssh_task_t *task)
{
    if (task && (task->type == PSSH_TASK_TYPE_EXEC))
        return 1;
    else
        return 0;
}

inline char *pssh_task_get_cmd(struct pssh_task_t *task)
{
    if (task) {
        if (pssh_is_cmd_task(task)) {
            return task->task.ex.cmd;
        } else {
            return task->task.cp.l_fn;
        }
    }
    return NULL;
}

inline int pssh_task_stdout_len(struct pssh_task_t *task)
{
    if (pssh_is_cmd_task(task))
        return task->task.ex.out_stream.len;
    return -1;
}

inline char *pssh_task_stdout(struct pssh_task_t *task)
{
    if (pssh_is_cmd_task(task))
        return task->task.ex.out_stream.data;
    pssh_printf("%s: return NULL\n", __func__);
    return NULL;
}

inline int pssh_task_stderr_len(struct pssh_task_t *task)
{
    if (pssh_is_cmd_task(task))
        return task->task.ex.err_stream.len;
    return -1;
}

inline char *pssh_task_stderr(struct pssh_task_t *task)
{
    if (pssh_is_cmd_task(task))
        return task->task.ex.err_stream.data;
    return NULL;
}

inline int pssh_task_exit_status(struct pssh_task_t *task)
{
    if (pssh_is_cmd_task(task))
        return task->task.ex.ret_code;
    return -1;
}

inline pssh_task_type_t  pssh_task_type(struct pssh_task_t *task)
{
    return task->type;
}

/*
 * Cleanup
 */
void pssh_task_list_free(pssh_task_list_t *tl)
{
    if (tl) {
        while (tl->tasks->tqh_first != NULL) {
            struct pssh_task_t *task = tl->tasks->tqh_first;
            TAILQ_REMOVE(tl->tasks, tl->tasks->tqh_first, entries);
            /* free(task->ev); */
            free(task);
        }
        free(tl->tasks);
        free(tl);
    }
}
