/*
 * $Id: ex.c,v 1.30 2008/05/05 15:01:27 rushba Exp $
 */
#include <pssh.h>

#include <sys/time.h>
#include <sys/resource.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

#define TIMEOUT 15
#define SRV_GROUP_CNT 1
#define SRV_IN_GROUP 1

pssh_session_t *pssh = NULL;
pssh_session_t *old_pssh = NULL;
pssh_task_list_t *task_list = NULL;

static const char *statstr[] = {
	"PSSH_TASK_STAT_NONE",
	"PSSH_TASK_STAT_ERROR",
	"PSSH_TASK_INPROGRESS" "PSSH_TASK_STAT_DONE",
};

static const char *conn_ret_str[] = {
	"PSSH_CONNECTED",
	"PSSH_RUNNING",
	"PSSH_TIMEOUT",
	"PSSH_FAILED",
	"PSSH_SUCCESS"
};

int main(int argc, char *argv[])
{
	int ret;
	int i, j;
	char name[20];
	char l_fn[50];
	struct pssh_sess_entry *ent;
	struct pssh_task_t *task;
	struct rusage ru;
	unsigned long utime, stime;

	/* add */
	j = 1;
	printf("start group # %d\n", j);
	pssh = pssh_init("vagner", "/home/vagner/.ssh/bsdway/bsdway.ru.pub", "/home/vagner/.ssh/bsdway/bsdway.ru", "", PSSH_OPT_NO_SEARCH);
	if (old_pssh)
		pssh_free(old_pssh);
	old_pssh = pssh;
	assert(pssh != NULL);

	strcpy(name, "bsdway.ru");
	pssh_server_add(pssh, "bsdway.ru", 22); 

	i = 0;

	do {
		ret = pssh_connect(pssh, &ent, TIMEOUT);
		printf("ret = %s for %s\n", conn_ret_str[ret], pssh_serv_name(ent));
		i++;
	} while (ret == PSSH_CONNECTED);
	printf("ret = %s\n", conn_ret_str[ret]);
	printf("i = %d\n", i);

	/* stat */
	ent = pssh_server_first(pssh);

	if (ret == PSSH_TIMEOUT) 
		exit(0); 

	/* add cp & exec */
	task_list = pssh_task_list_init(pssh);
	assert(task_list != NULL);

	pssh_cp_to_server(task_list, name, "/tmp/test", "/tmp/profile", TIMEOUT);
	pssh_add_cmd(task_list, name, "cat /etc/profile", TIMEOUT); 

	/* run */
	i = 0;
	do {
		i++;
		ret = pssh_exec(task_list, &task);
		printf("time: %ld, ret = %s for %s\n", time(0), conn_ret_str[ret], pssh_task_server_name(task));
	} while (ret == PSSH_RUNNING);

	printf("ret = %s\n", conn_ret_str[ret]);
	printf("i = %d\n", i);

	task = pssh_task_first(task_list);
	while (task != NULL) {
		printf("%s: get_cmd: <%s>\n", pssh_task_server_name(task), pssh_task_get_cmd(task));
		if (pssh_task_type(task) == PSSH_TASK_TYPE_EXEC) {
			if (pssh_task_stat(task) == PSSH_TASK_DONE) {
				printf("host: %s exit_status: %d\n", 
						pssh_task_server_name(task), pssh_task_exit_status(task)); 
				printf("stdout <%.*s>\n", pssh_task_stdout_len(task), pssh_task_stdout(task)); 
				printf("stderr <%.*s>\n", pssh_task_stderr_len(task), pssh_task_stderr(task)); 
			}
		} else if (pssh_task_type(task) == PSSH_TASK_TYPE_COPY) {
		} else {
			printf("shit happened. unknown task type %d\n", pssh_task_type(task));
			abort();
		}
		task = pssh_task_next(task_list);
	}

	//	}
/* run */
i = 0;
do {
	i++;
	ret = pssh_exec(task_list, &task);
	printf("time: %ld, ret = %s for %s <%s>\n", time(0), conn_ret_str[ret], pssh_task_server_name(task),
			pssh_task_get_cmd(task));
} while (ret == PSSH_RUNNING);

task = pssh_task_first(task_list);
while (task != NULL) {
	printf("%s: get_cmd: <%s> stat %s\n", pssh_task_server_name(task), pssh_task_get_cmd(task),
			statstr[pssh_task_stat(task)]);
	task = pssh_task_next(task_list);
}

pssh_task_list_free(task_list);
pssh_free(pssh);
printf("ret = %s\n", conn_ret_str[ret]);

/* Cleanup */
getrusage(RUSAGE_SELF, &ru);
utime = (ru.ru_utime.tv_usec + ru.ru_utime.tv_sec * 1000000) / 1000;
stime = (ru.ru_stime.tv_usec + ru.ru_stime.tv_sec * 1000000) / 1000;
printf("utime : %lu, stime: %lu\n", utime, stime);

(void)argc;
(void)argv;
return 0;
}
