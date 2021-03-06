#include <fcntl.h>
#include <sys/stat.h>

#include "wheatserver.h"

struct dictType wstrDictType;

void nonBlockCloseOnExecPipe(int *fd0, int *fd1)
{
    int full_pipe[2];
    if (pipe(full_pipe) != 0) {
        wheatLog(WHEAT_WARNING, "Create pipe failed: %s", strerror(errno));
        halt(1);
    }
    if (wheatNonBlock(Server.neterr, full_pipe[0]) == NET_WRONG) {
        wheatLog(WHEAT_WARNING, "Set pipe %d nonblock failed: %s", full_pipe[0], Server.neterr);
        halt(1);
    }
    if (wheatNonBlock(Server.neterr, full_pipe[1]) == NET_WRONG) {
        wheatLog(WHEAT_WARNING, "Set pipe %d nonblock failed: %s", full_pipe[1], Server.neterr);
        halt(1);
    }
    *fd0 = full_pipe[0];
    *fd1 = full_pipe[1];
}


unsigned int dictWstrHash(const void *key) {
    return dictGenHashFunction((unsigned char*)key, wstrlen((char*)key));
}

unsigned int dictWstrCaseHash(const void *key) {
    return dictGenCaseHashFunction((unsigned char*)key, wstrlen((char*)key));
}

int dictWstrKeyCompare(const void *key1, const void *key2)
{
    int l1,l2;

    l1 = wstrlen((wstr)key1);
    l2 = wstrlen((wstr)key2);
    if (l1 != l2) return 0;
    return memcmp(key1, key2, l1) == 0;
}

void dictWstrDestructor(void *val)
{
    wstrFree(val);
}

struct dictType wstrDictType = {
    dictWstrHash,               /* hash function */
    NULL,                       /* key dup */
    NULL,                       /* val dup */
    dictWstrKeyCompare,         /* key compare */
    dictWstrDestructor,         /* key destructor */
    dictWstrDestructor,         /* val destructor */
};

unsigned int dictSliceHash(const void *key) {
    const struct slice *s = key;
    return dictGenHashFunction(s->data, s->len);
}

unsigned int dictSliceCaseHash(const void *key) {
    const struct slice *s = key;
    return dictGenCaseHashFunction(s->data, s->len);
}

int dictSliceKeyCompare(const void *key1, const void *key2)
{
    const struct slice *s1 = key1, *s2 = key2;
    int l1,l2;

    l1 = s1->len;
    l2 = s2->len;
    if (l1 != l2) return 0;
    return memcmp(s1->data, s2->data, l1) == 0;
}

void dictSliceDestructor(void *val)
{
    sliceFree(val);
}

struct dictType sliceDictType = {
    dictSliceHash,               /* hash function */
    NULL,                       /* key dup */
    NULL,                       /* val dup */
    dictSliceKeyCompare,         /* key compare */
    dictSliceDestructor,         /* key destructor */
    dictSliceDestructor,         /* val destructor */
};

/* ========== Configuration Validator/Print Area ========== */

int stringValidator(struct configuration *conf, const char *key, const char *val)
{
    ASSERT(val);

    if (conf->target.ptr && !conf->helper) {
        wfree(conf->target.ptr);
        conf->helper = NULL;
    }
    conf->target.ptr = strdup(val);
    return VALIDATE_OK;
}

int unsignedIntValidator(struct configuration *conf, const char *key, const char *val)
{
    ASSERT(val);
    int i, digit;
    intptr_t max_limit = 0;

    if (conf->helper)
        max_limit = (intptr_t)conf->helper;

    for (i = 0; val[i] != '\0'; i++) {
        digit = val[i] - 48;
        if (digit < 0 || digit > 9)
            return VALIDATE_WRONG;
        if (max_limit != 0 && digit > max_limit)
            return VALIDATE_WRONG;
    }
    conf->target.val = atoi(val);
    return VALIDATE_OK;
}

int enumValidator(struct configuration *conf, const char *key, const char *val)
{
    ASSERT(val);
    ASSERT(conf->helper);

    struct enumIdName *sentinel = (struct enumIdName *)conf->helper;
    while (sentinel->name) {
        if (strncasecmp(val, sentinel->name, strlen(sentinel->name)) == 0) {
            conf->target.enum_ptr = sentinel;
            return VALIDATE_OK;
        }
        else if (sentinel == NULL)
            break ;
        sentinel++;
    }

    return VALIDATE_WRONG;
}

int boolValidator(struct configuration *conf, const char *key, const char *val)
{
    size_t len = strlen(val) + 1;
    if (memcmp("on", val, len) == 0) {
        conf->target.val = 1;
    } else if (memcmp("off", val, len) == 0) {
        conf->target.val = 0;
    } else
        return VALIDATE_WRONG;
    return VALIDATE_OK;
}

int daemonize(int dump_core)
{
    int status;
    pid_t pid, sid;
    int fd;

    pid = fork();
    switch (pid) {
        case -1:
            wheatLog(WHEAT_WARNING, "fork() failed: %s", strerror(errno));
            return WHEAT_WRONG;

        case 0:
            break;

        default:
            /* parent terminates */
            _exit(0);
    }

    /* 1st child continues and becomes the session leader */

    sid = setsid();
    if (sid < 0) {
        wheatLog(WHEAT_WARNING, "setsid() failed: %s", strerror(errno));
        return WHEAT_WRONG;
    }

    pid = fork();
    switch (pid) {
        case -1:
            wheatLog(WHEAT_WARNING, "fork() failed: %s", strerror(errno));
            return WHEAT_WRONG;

        case 0:
            break;

        default:
            /* 1st child terminates */
            _exit(0);
    }

    /* 2nd child continues */

    /* change working directory */
    if (dump_core == 0) {
        status = chdir("/");
        if (status < 0) {
            wheatLog(WHEAT_WARNING, "chdir(\"/\") failed: %s", strerror(errno));
            return WHEAT_WRONG;
        }
    }

    /* clear file mode creation mask */
    umask(0);

    /* redirect stdin, stdout and stderr to "/dev/null" */

    fd = open("/dev/null", O_RDWR);
    if (fd < 0) {
        wheatLog(WHEAT_WARNING, "open(\"/dev/null\") failed: %s", strerror(errno));
        return WHEAT_WRONG;
    }

    status = dup2(fd, STDIN_FILENO);
    if (status < 0) {
        wheatLog(WHEAT_WARNING, "dup2(%d, STDIN) failed: %s", fd, strerror(errno));
        close(fd);
        return WHEAT_WRONG;
    }

    status = dup2(fd, STDOUT_FILENO);
    if (status < 0) {
        wheatLog(WHEAT_WARNING, "dup2(%d, STDOUT) failed: %s", fd, strerror(errno));
        close(fd);
        return WHEAT_WRONG;
    }

    status = dup2(fd, STDERR_FILENO);
    if (status < 0) {
        wheatLog(WHEAT_WARNING, "dup2(%d, STDERR) failed: %s", fd, strerror(errno));
        close(fd);
        return WHEAT_WRONG;
    }

    if (fd > STDERR_FILENO) {
        status = close(fd);
        if (status < 0) {
            wheatLog(WHEAT_WARNING, "close(%d) failed: %s", fd, strerror(errno));
            return WHEAT_WRONG;
        }
    }

    return WHEAT_OK;
}

void createPidFile()
{
    /* Try to write the pid file in a best-effort way. */
    FILE *fp = fopen(Server.pidfile, "w");
    if (fp) {
        fprintf(fp, "%d\n", (int)getpid());
        fclose(fp);
    }
}

void setTimer(int milliseconds)
{
    struct itimerval it;

    /* Will stop the timer if period is 0. */
    it.it_value.tv_sec = milliseconds / 1000;
    it.it_value.tv_usec = (milliseconds % 1000) * 1000;
    it.it_interval.tv_sec = it.it_value.tv_sec;
    it.it_interval.tv_usec = it.it_value.tv_usec;
    setitimer(ITIMER_REAL, &it, NULL);
}

int getFileSizeAndMtime(int fd, off_t *len, time_t *m_time)
{
    ASSERT(fd > 0);
    struct stat stat;
    int ret = fstat(fd, &stat);
    if (ret == -1)
        return WHEAT_WRONG;
    if (len) *len = stat.st_size;
    if (m_time) *m_time = stat.st_mtime;
    return WHEAT_OK;
}

int isRegFile(const char *path)
{
    ASSERT(path);
    struct stat stat;
    int ret = lstat(path, &stat);
    if (ret == -1)
        return WHEAT_WRONG;
    return S_ISREG(stat.st_mode) == 0 ? WHEAT_WRONG : WHEAT_OK;
}

int fromSameParentDir(wstr parent, wstr child)
{
    if (wstrlen(parent) > wstrlen(child))
        return 0;
    return memcmp(parent, child, wstrlen(parent)) == 0;
}
