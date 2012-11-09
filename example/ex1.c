#include <pssh.h>

#include <sys/time.h>
#include <sys/resource.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static const char *conn_ret_str[] = {
    "PSSH_CONNECTED",
    "PSSH_RUNNING",
    "PSSH_TIMEOUT",
    "PSSH_FAILED",
    "PSSH_SUCCESS"
};

int main(int argc, char *argv[])
{
	pssh_session_t *pssh = NULL;
	struct pssh_sess_entry *ent;
	pssh_task_list_t *task_list = NULL;
	struct pssh_task_t *task;
	int ret, i;
	int timeout;
	char *cmd;
	char *hostname = NULL;

	if (argc < 4) {
		printf("usage: ex1 <hostname> <timeout> <cmd>\n");
		return -1;
	}

	hostname = argv[1];
	timeout = atoll(argv[2]);
	cmd = argv[3];

	pssh = pssh_init("rushba", "/home/rushba/.ssh/id_dsa.pub", NULL, "", PSSH_OPT_NO_SEARCH);

	assert(pssh != NULL);

	ret = pssh_server_add(pssh, hostname, 22);

	do {
		ret = pssh_connect(pssh, &ent, timeout);
	} while (ret == PSSH_CONNECTED);

        task_list = pssh_task_list_init(pssh);
        assert(task_list != NULL);

	pssh_add_cmd(task_list, hostname, cmd, timeout);
	
        i = 0;
        do {
            i++;
            ret = pssh_exec(task_list, &task);
            printf("time: %ld, ret = %s for %s\n", time(0), conn_ret_str[ret], pssh_task_server_name(task));
        } while (ret == PSSH_RUNNING);

        printf("ret = %s\n", conn_ret_str[ret]);
        printf("i = %d\n", i);

	printf("stdout: [%s]\n", pssh_task_stdout(pssh_task_first(task_list)));
	printf("stderr: [%s]\n", pssh_task_stderr(pssh_task_first(task_list)));

	return 0;
}
