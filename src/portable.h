#ifndef WHEATSERVER_PORTABLE_H
#define WHEATSERVER_PORTABLE_H

ssize_t portable_sendfile(int out_fd, int in_fd, off_t, off_t len);

/* Check if we can use setproctitle().
 * BSD systems have support for it, we provide an implementation for
 * Linux and osx. */
#if (defined __NetBSD__ || defined __FreeBSD__ || defined __OpenBSD__)
#define USE_SETPROCTITLE
#endif

#if (defined __linux || defined __APPLE__)
#define USE_SETPROCTITLE
void setproctitle(const char *fmt, ...);
#endif

#endif
