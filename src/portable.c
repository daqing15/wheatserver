#include "wheatserver.h"

#ifdef __APPLE__
/* OS X */
#include <sys/socket.h>
#include <sys/types.h>
ssize_t portable_sendfile(int out_fd, int in_fd, off_t off, off_t len) {
    if (sendfile(in_fd, out_fd, off, &len, NULL, 0) == -1)
        if (errno != EAGAIN)
            return -1;
    return len;
}
#endif
#ifdef __linux
/* Linux */
#include <sys/sendfile.h>

ssize_t portable_sendfile(int out_fd, int in_fd, off_t off, off_t len) {
    return sendfile(out_fd, in_fd, &off, len);
}

#endif

void setProctitle(const char *title)
{
    setproctitle("wheatserver: %s %s:%d", title,
            Server.bind_addr ? Server.bind_addr : "*",
            Server.port);
}
