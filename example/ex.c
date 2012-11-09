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

#define TIMEOUT 15
#define SRV_GROUP_CNT 10
#define SRV_IN_GROUP 10

pssh_session_t *pssh = NULL;
pssh_session_t *old_pssh = NULL;
pssh_task_list_t *task_list = NULL;

static const char *statstr[] = {
    "PSSH_TASK_STAT_NONE",
    "PSSH_TASK_STAT_ERROR",
    "PSSH_TASK_INPROGRESS"
    "PSSH_TASK_STAT_DONE",
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
#if 1
    for (j = 2; j < SRV_GROUP_CNT; j++) {
        printf("start group # %d\n", j);
        pssh = pssh_init("rushba", "/home/rushba/tmp/id_dsa.pub", NULL, "", PSSH_OPT_NO_SEARCH);
        if (old_pssh)
            pssh_free(old_pssh);
        old_pssh = pssh;
        assert(pssh != NULL);

        for (i = 1; i < SRV_IN_GROUP; i++) {
            /* if (i == 6 || i == 77) */
            /*     continue; */
            sprintf(name, "www%d.mlan", j*SRV_IN_GROUP + i);
            ret = pssh_server_add(pssh, name, 22);
        }
        /* pssh_server_add(pssh, "blah", 22); */
        /* pssh_server_add(pssh, "test", 22); */
        /* pssh_server_add(pssh, "main1.mlan", 22); */

        /* pssh_server_add(pssh, "www1", 22); */
        /* pssh_server_add(pssh, "www3", 22); */
        /* pssh_server_add(pssh, "www5", 22); */
        /* pssh_server_add(pssh, "joppa", 22); */
        /* pssh_server_add(pssh, "joppa2", 22); */
        /* pssh_server_add(pssh, "www6", 22); */

        i = 0;

        /* wrong call, need be still! */
        pssh_server_next(pssh);

        /* connect */
        do {
            ret = pssh_connect(pssh, &ent, TIMEOUT);
            printf("ret = %s for %s\n", conn_ret_str[ret], pssh_serv_name(ent));
            i++;
        } while (ret == PSSH_CONNECTED);
        printf("ret = %s\n", conn_ret_str[ret]);
        printf("i = %d\n", i);

        /* stat */
        ent = pssh_server_first(pssh);
        while (ent != NULL) {
            printf("%s: %s\n", pssh_serv_name(ent), pssh_stat_str(ent));
            ent = pssh_server_next(pssh);
        }

/*     if (ret == PSSH_TIMEOUT) */
/*         exit(0); */

        /* add cp & exec */
        task_list = pssh_task_list_init(pssh);
        assert(task_list != NULL);

        /* Err */
        ret = pssh_cp_to_server(task_list, "", "a", "b", TIMEOUT);
        assert(ret == -1);
        ret = pssh_cp_to_server(task_list, "s", "", "b", TIMEOUT);
        assert(ret == -1);
        ret = pssh_cp_to_server(task_list, "s", "a", "", TIMEOUT);
        assert(ret == -1);
        ret = pssh_add_cmd(task_list, "", "c", TIMEOUT);
        assert(ret == -1);
        ret = pssh_add_cmd(task_list, "s", "", TIMEOUT);
        assert(ret == -1);
        

        for (i = 1; i < SRV_IN_GROUP; i++) {
            sprintf(name, "www%d.mlan", j*SRV_IN_GROUP + i);
#if 0
            sprintf(l_fn, "/tmp/passwd.%s", name);
            pssh_cp_from_server(task_list, name, l_fn, "/etc/passwd");
            sprintf(l_fn, "/tmp/group.%s", name);
            pssh_cp_from_server(task_list, name, l_fn, "/etc/group");
            sprintf(l_fn, "/tmp/profile.%s", name);
            pssh_cp_from_server(task_list, name, l_fn, "/etc/profile");
#else
            
            if ((i == 2) || (i == 4)) {
                pssh_cp_to_server(task_list, name, "/etc", "/tmp/profile", TIMEOUT);
            } else {
                pssh_cp_to_server(task_list, name, "/etc", "/tmp/profile", TIMEOUT);
                pssh_cp_to_server(task_list, name, "/etc/profile", "/tmp/profile", TIMEOUT);
                pssh_cp_to_server(task_list, name, "/etc/passwd", "/tmp/passwd", TIMEOUT);
                pssh_cp_to_server(task_list, name, "/etc/hosts", "/tmp/hosts", TIMEOUT);
            }
/*         pssh_add_cmd(task_list, name, "cat /tmp/profile"); */
#endif
        }
        /* pssh_cp_from_server(task_list, "www1", "/tmp/group.www1", "/etc/group"); */
        /* pssh_cp_from_server(task_list, "www1", "/tmp/profile.www1", "/etc/profile"); */
        /* pssh_cp_from_server(task_list, "www1", "/tmp/passwd.www1", "/etc/passwd"); */

        /* pssh_cp_from_server(task_list, "www3", "/tmp/profile.www3", "/etc/profile"); */
        /* pssh_cp_from_server(task_list, "www3", "/tmp/group.www3", "/etc/group"); */
        /* pssh_cp_from_server(task_list, "www3", "/tmp/passwd.www3", "/etc/passwd"); */

        /* pssh_cp_from_server(task_list, "www5", "/tmp/profile.www5", "/etc/profile"); */
        /* pssh_cp_from_server(task_list, "www5", "/tmp/group.www5", "/etc/group"); */
        /* pssh_cp_from_server(task_list, "www5", "/tmp/passwd.www5", "/etc/passwd"); */

        /* pssh_cp_from_server(task_list, "joppa", "/tmp/passwd.j", "/etc/profile"); */
        /* pssh_cp_from_server(task_list, "joppa2", "/tmp/passwd.j2", "/etc/profile"); */
        /* pssh_cp_from_server(task_list, "www6", "/tmp/passwd.www6", "/etc/profile"); */

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
/*                 printf("host: %s exit_status: %d\n", */
/*                        pssh_task_server_name(task), pssh_task_exit_status(task)); */
/*                 printf("stdout <%.*s>\n", pssh_task_stdout_len(task), pssh_task_stdout(task)); */
/*                 printf("stderr <%.*s>\n", pssh_task_stderr_len(task), pssh_task_stderr(task)); */
                }
            } else if (pssh_task_type(task) == PSSH_TASK_TYPE_COPY) {
            } else {
                printf("shit happened. unknown task type %d\n", pssh_task_type(task));
                abort();
            }
            task = pssh_task_next(task_list);
        }

    }
#else

    pssh = pssh_init("rushba", "/home/rushba/tmp/id_dsa.pub", NULL, "");
    assert(pssh != NULL);
    pssh_server_add(pssh, "www110", 22);
    /* connect */
    do {
        ret = pssh_connect(pssh, &ent, TIMEOUT);
        printf("ret = %s for %s\n", conn_ret_str[ret], pssh_serv_name(ent));
        i++;
    } while (ret == PSSH_CONNECTED);

    /* add cp & exec */
    task_list = pssh_task_list_init(pssh);
    assert(task_list != NULL);

pssh_cp_to_server(task_list, "www110", "/etc/raw", "/tmp/raw");
pssh_cp_to_server(task_list, "www110", "/etc/rpc", "/tmp/rpc");
pssh_cp_to_server(task_list, "www110", "/etc/nsswitch.conf", "/tmp/nsswitch.conf");
pssh_cp_to_server(task_list, "www110", "/etc/motd", "/tmp/motd");
pssh_cp_to_server(task_list, "www110", "/etc/mtab", "/tmp/mtab");
pssh_cp_to_server(task_list, "www110", "/etc/defaultdomain", "/tmp/defaultdomain");
pssh_cp_to_server(task_list, "www110", "/etc/wvdial.conf", "/tmp/wvdial.conf");
pssh_cp_to_server(task_list, "www110", "/etc/ltrace.conf", "/tmp/ltrace.conf");
pssh_cp_to_server(task_list, "www110", "/etc/smpppd.conf", "/tmp/smpppd.conf");
pssh_cp_to_server(task_list, "www110", "/etc/issue.net", "/tmp/issue.net");
pssh_cp_to_server(task_list, "www110", "/etc/ftpusers", "/tmp/ftpusers");
pssh_cp_to_server(task_list, "www110", "/etc/securetty", "/tmp/securetty");
pssh_cp_to_server(task_list, "www110", "/etc/shadow.YaST2save", "/tmp/shadow.YaST2save");
pssh_cp_to_server(task_list, "www110", "/etc/crontab", "/tmp/crontab");
pssh_cp_to_server(task_list, "www110", "/etc/csh.cshrc", "/tmp/csh.cshrc");
pssh_cp_to_server(task_list, "www110", "/etc/auto.master", "/tmp/auto.master");
pssh_cp_to_server(task_list, "www110", "/etc/csh.login", "/tmp/csh.login");
pssh_cp_to_server(task_list, "www110", "/etc/generateCRL.conf", "/tmp/generateCRL.conf");
pssh_cp_to_server(task_list, "www110", "/etc/HOSTNAME", "/tmp/HOSTNAME");
pssh_cp_to_server(task_list, "www110", "/etc/defkeymap.map", "/tmp/defkeymap.map");
pssh_cp_to_server(task_list, "www110", "/etc/modprobe.conf", "/tmp/modprobe.conf");
pssh_cp_to_server(task_list, "www110", "/etc/fdprm", "/tmp/fdprm");
pssh_cp_to_server(task_list, "www110", "/etc/fstab", "/tmp/fstab");
pssh_cp_to_server(task_list, "www110", "/etc/group", "/tmp/group");
pssh_cp_to_server(task_list, "www110", "/etc/hosts", "/tmp/hosts");
pssh_cp_to_server(task_list, "www110", "/etc/issue", "/tmp/issue");
pssh_cp_to_server(task_list, "www110", "/etc/magic", "/tmp/magic");
pssh_cp_to_server(task_list, "www110", "/etc/vimrc", "/tmp/vimrc");
pssh_cp_to_server(task_list, "www110", "/etc/zshrc", "/tmp/zshrc");
pssh_cp_to_server(task_list, "www110", "/etc/Muttrc", "/tmp/Muttrc");
pssh_cp_to_server(task_list, "www110", "/etc/microcode.dat", "/tmp/microcode.dat"); 
pssh_cp_to_server(task_list, "www110", "/etc/yp.conf", "/tmp/yp.conf");
pssh_cp_to_server(task_list, "www110", "/etc/smartd.conf", "/tmp/smartd.conf");
pssh_cp_to_server(task_list, "www110", "/etc/krb5.conf", "/tmp/krb5.conf");
pssh_cp_to_server(task_list, "www110", "/etc/defkeymap.name", "/tmp/defkeymap.name");
pssh_cp_to_server(task_list, "www110", "/etc/scpm.users", "/tmp/scpm.users");
pssh_cp_to_server(task_list, "www110", "/etc/insserv.conf", "/tmp/insserv.conf");
pssh_cp_to_server(task_list, "www110", "/etc/lesskey", "/tmp/lesskey");
pssh_cp_to_server(task_list, "www110", "/etc/c-client.cf", "/tmp/c-client.cf");
pssh_cp_to_server(task_list, "www110", "/etc/mysqlaccess.conf", "/tmp/mysqlaccess.conf");
pssh_cp_to_server(task_list, "www110", "/etc/auto.net", "/tmp/auto.net");
pssh_cp_to_server(task_list, "www110", "/etc/auto.smb", "/tmp/auto.smb");
pssh_cp_to_server(task_list, "www110", "/etc/nscd.conf", "/tmp/nscd.conf");
pssh_cp_to_server(task_list, "www110", "/etc/grub.conf", "/tmp/grub.conf");
pssh_cp_to_server(task_list, "www110", "/etc/cron.deny", "/tmp/cron.deny");
pssh_cp_to_server(task_list, "www110", "/etc/mail.rc", "/tmp/mail.rc");
pssh_cp_to_server(task_list, "www110", "/etc/mailcap", "/tmp/mailcap");
pssh_cp_to_server(task_list, "www110", "/etc/permissions", "/tmp/permissions");
pssh_cp_to_server(task_list, "www110", "/etc/slp.reg", "/tmp/slp.reg");
pssh_cp_to_server(task_list, "www110", "/etc/slp.spi", "/tmp/slp.spi");
pssh_cp_to_server(task_list, "www110", "/etc/pythonstart", "/tmp/pythonstart");
pssh_cp_to_server(task_list, "www110", "/etc/slsh.rc", "/tmp/slsh.rc");
pssh_cp_to_server(task_list, "www110", "/etc/filesystems", "/tmp/filesystems");
pssh_cp_to_server(task_list, "www110", "/etc/DIR_COLORS", "/tmp/DIR_COLORS");
pssh_cp_to_server(task_list, "www110", "/etc/permissions.paranoid", "/tmp/permissions.paranoid");
pssh_cp_to_server(task_list, "www110", "/etc/idn.conf", "/tmp/idn.conf");
pssh_cp_to_server(task_list, "www110", "/etc/idnalias.conf", "/tmp/idnalias.conf");
pssh_cp_to_server(task_list, "www110", "/etc/xinetd.conf", "/tmp/xinetd.conf");
pssh_cp_to_server(task_list, "www110", "/etc/rpasswd.conf", "/tmp/rpasswd.conf");
pssh_cp_to_server(task_list, "www110", "/etc/scsi_id.config", "/tmp/scsi_id.config");
pssh_cp_to_server(task_list, "www110", "/etc/hosts.allow", "/tmp/hosts.allow");
pssh_cp_to_server(task_list, "www110", "/etc/uniconf.conf", "/tmp/uniconf.conf");
pssh_cp_to_server(task_list, "www110", "/etc/hosts.equiv", "/tmp/hosts.equiv");
pssh_cp_to_server(task_list, "www110", "/etc/screenrc", "/tmp/screenrc");
pssh_cp_to_server(task_list, "www110", "/etc/sudoers", "/tmp/sudoers");
pssh_cp_to_server(task_list, "www110", "/etc/modprobe.conf.local", "/tmp/modprobe.conf.local");
pssh_cp_to_server(task_list, "www110", "/etc/exports", "/tmp/exports");
pssh_cp_to_server(task_list, "www110", "/etc/ethers", "/tmp/ethers");
pssh_cp_to_server(task_list, "www110", "/etc/rc.splash", "/tmp/rc.splash");
pssh_cp_to_server(task_list, "www110", "/etc/rc.status", "/tmp/rc.status");
pssh_cp_to_server(task_list, "www110", "/etc/smpppd-c.conf", "/tmp/smpppd-c.conf");
pssh_cp_to_server(task_list, "www110", "/etc/permissions.local", "/tmp/permissions.local");
pssh_cp_to_server(task_list, "www110", "/etc/fb.modes", "/tmp/fb.modes");
pssh_cp_to_server(task_list, "www110", "/etc/shadow.old", "/tmp/shadow.old");
pssh_cp_to_server(task_list, "www110", "/etc/rsyncd.secrets", "/tmp/rsyncd.secrets");
pssh_cp_to_server(task_list, "www110", "/etc/evms.conf", "/tmp/evms.conf");
pssh_cp_to_server(task_list, "www110", "/etc/my.cnf", "/tmp/my.cnf");
pssh_cp_to_server(task_list, "www110", "/etc/hushlogins", "/tmp/hushlogins");
pssh_cp_to_server(task_list, "www110", "/etc/ttytype", "/tmp/ttytype");
pssh_cp_to_server(task_list, "www110", "/etc/adjtime", "/tmp/adjtime");
pssh_cp_to_server(task_list, "www110", "/etc/suseRegister.conf", "/tmp/suseRegister.conf");
pssh_cp_to_server(task_list, "www110", "/etc/passwd", "/tmp/passwd");
pssh_cp_to_server(task_list, "www110", "/etc/group.YaST2save", "/tmp/group.YaST2save");
pssh_cp_to_server(task_list, "www110", "/etc/sysctl.conf", "/tmp/sysctl.conf");
pssh_cp_to_server(task_list, "www110", "/etc/.pwd.lock", "/tmp/.pwd.lock");
pssh_cp_to_server(task_list, "www110", "/etc/aliases.db", "/tmp/aliases.db");
pssh_cp_to_server(task_list, "www110", "/etc/bash.bashrc.local", "/tmp/bash.bashrc.local");
pssh_cp_to_server(task_list, "www110", "/etc/papersize", "/tmp/papersize");
pssh_cp_to_server(task_list, "www110", "/etc/logindevperm", "/tmp/logindevperm");
pssh_cp_to_server(task_list, "www110", "/etc/slp.conf", "/tmp/slp.conf");
pssh_cp_to_server(task_list, "www110", "/etc/shadow", "/tmp/shadow");
pssh_cp_to_server(task_list, "www110", "/etc/shells", "/tmp/shells");
pssh_cp_to_server(task_list, "www110", "/etc/netgroup", "/tmp/netgroup");
pssh_cp_to_server(task_list, "www110", "/etc/localtime", "/tmp/localtime");
pssh_cp_to_server(task_list, "www110", "/etc/networks", "/tmp/networks");
pssh_cp_to_server(task_list, "www110", "/etc/auto.misc", "/tmp/auto.misc");
pssh_cp_to_server(task_list, "www110", "/etc/idmapd.conf", "/tmp/idmapd.conf");
pssh_cp_to_server(task_list, "www110", "/etc/ld.so.conf", "/tmp/ld.so.conf");
pssh_cp_to_server(task_list, "www110", "/etc/aliases", "/tmp/aliases");
pssh_cp_to_server(task_list, "www110", "/etc/wgetrc", "/tmp/wgetrc");
pssh_cp_to_server(task_list, "www110", "/etc/rc.d.README", "/tmp/rc.d.README");
pssh_cp_to_server(task_list, "www110", "/etc/openct.conf", "/tmp/openct.conf");
pssh_cp_to_server(task_list, "www110", "/etc/printcap_nodefault", "/tmp/printcap_nodefault");
pssh_cp_to_server(task_list, "www110", "/etc/printcap", "/tmp/printcap");
pssh_cp_to_server(task_list, "www110", "/etc/opiekeys", "/tmp/opiekeys");
pssh_cp_to_server(task_list, "www110", "/etc/group.old", "/tmp/group.old");
pssh_cp_to_server(task_list, "www110", "/etc/ntp.conf", "/tmp/ntp.conf");
pssh_cp_to_server(task_list, "www110", "/etc/environment", "/tmp/environment");
pssh_cp_to_server(task_list, "www110", "/etc/at.deny", "/tmp/at.deny");
pssh_cp_to_server(task_list, "www110", "/etc/hosts.YaST2save", "/tmp/hosts.YaST2save");
pssh_cp_to_server(task_list, "www110", "/etc/services", "/tmp/services");
pssh_cp_to_server(task_list, "www110", "/etc/SuSE-release", "/tmp/SuSE-release");
pssh_cp_to_server(task_list, "www110", "/etc/sensors.conf", "/tmp/sensors.conf");
pssh_cp_to_server(task_list, "www110", "/etc/zshenv", "/tmp/zshenv");
pssh_cp_to_server(task_list, "www110", "/etc/ld.so.cache", "/tmp/ld.so.cache");
pssh_cp_to_server(task_list, "www110", "/etc/permissions.easy", "/tmp/permissions.easy");
pssh_cp_to_server(task_list, "www110", "/etc/rsyncd.conf", "/tmp/rsyncd.conf");
pssh_cp_to_server(task_list, "www110", "/etc/gssapi_mech.conf", "/tmp/gssapi_mech.conf");
pssh_cp_to_server(task_list, "www110", "/etc/opensc.conf", "/tmp/opensc.conf");
pssh_cp_to_server(task_list, "www110", "/etc/aclocal_dirlist", "/tmp/aclocal_dirlist");
pssh_cp_to_server(task_list, "www110", "/etc/hosts.deny", "/tmp/hosts.deny");
pssh_cp_to_server(task_list, "www110", "/etc/nntpserver", "/tmp/nntpserver");
pssh_cp_to_server(task_list, "www110", "/etc/logrotate.conf", "/tmp/logrotate.conf");
pssh_cp_to_server(task_list, "www110", "/etc/lesskey.bin", "/tmp/lesskey.bin");
pssh_cp_to_server(task_list, "www110", "/etc/bindresvport.blacklist", "/tmp/bindresvport.blacklist");
pssh_cp_to_server(task_list, "www110", "/etc/bash.bashrc", "/tmp/bash.bashrc");
pssh_cp_to_server(task_list, "www110", "/etc/inittab", "/tmp/inittab");
pssh_cp_to_server(task_list, "www110", "/etc/inputrc", "/tmp/inputrc");
pssh_cp_to_server(task_list, "www110", "/etc/host.conf", "/tmp/host.conf");
pssh_cp_to_server(task_list, "www110", "/etc/powerd.conf", "/tmp/powerd.conf");
pssh_cp_to_server(task_list, "www110", "/etc/login.defs", "/tmp/login.defs");
pssh_cp_to_server(task_list, "www110", "/etc/protocols", "/tmp/protocols");
pssh_cp_to_server(task_list, "www110", "/etc/passwd.YaST2save", "/tmp/passwd.YaST2save");
pssh_cp_to_server(task_list, "www110", "/etc/ldap.conf", "/tmp/ldap.conf");
pssh_cp_to_server(task_list, "www110", "/etc/passwd.old", "/tmp/passwd.old");
pssh_cp_to_server(task_list, "www110", "/etc/resolv.conf", "/tmp/resolv.conf");
pssh_cp_to_server(task_list, "www110", "/etc/manpath.config", "/tmp/manpath.config");
pssh_cp_to_server(task_list, "www110", "/etc/profile", "/tmp/profile");
pssh_cp_to_server(task_list, "www110", "/etc/resmgr.conf", "/tmp/resmgr.conf");
pssh_cp_to_server(task_list, "www110", "/etc/hosts.lpd", "/tmp/hosts.lpd");
pssh_cp_to_server(task_list, "www110", "/etc/permissions.secure", "/tmp/permissions.secure");
pssh_cp_to_server(task_list, "www110", "/etc/mime.types", "/tmp/mime.types");
#endif
    /* run */
    i = 0;
    do {
        i++;
        ret = pssh_exec(task_list, &task);
        printf("time: %ld, ret = %s for %s <%s>\n", time(0), conn_ret_str[ret], pssh_task_server_name(task), pssh_task_get_cmd(task));
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
    utime = (ru.ru_utime.tv_usec + ru.ru_utime.tv_sec*1000000) / 1000;
    stime = (ru.ru_stime.tv_usec + ru.ru_stime.tv_sec*1000000) / 1000;
    printf("utime : %lu, stime: %lu\n", utime, stime);

    (void) argc;
    (void) argv;
    return 0;
}
