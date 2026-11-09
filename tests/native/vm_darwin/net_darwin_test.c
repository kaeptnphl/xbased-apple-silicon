#include <poll.h>
#include "native/net/net.h"

#include <stdio.h>
#include <sys/socket.h>

int main(void)
{
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, fds) != 0) {
        perror("socketpair");
        return 1;
    }
    if (!nc_net_socket_flags_ready(fds[0]) || !nc_net_socket_flags_ready(fds[1])) {
        perror("socket flags");
        return 2;
    }
    if (!(fcntl(fds[0], F_GETFL) & O_NONBLOCK) || !(fcntl(fds[0], F_GETFD) & FD_CLOEXEC)) {
        fputs("wake socket is not nonblocking and close-on-exec\n", stderr);
        return 3;
    }

    const char first = 1, second = 2;
    if (write(fds[1], &first, sizeof(first)) != 1 || write(fds[1], &second, sizeof(second)) != 1) {
        perror("wake socket write");
        return 4;
    }
    struct pollfd pfd = { .fd = fds[0], .events = POLLIN };
    char value;
    if (poll(&pfd, 1, 0) != 1 || !(pfd.revents & POLLIN) ||
        read(fds[0], &value, sizeof(value)) != 1 || value != first ||
        poll(&pfd, 1, 0) != 1 || read(fds[0], &value, sizeof(value)) != 1 || value != second) {
        fputs("wake socket did not preserve pollable datagrams\n", stderr);
        return 5;
    }

    close(fds[0]);
    close(fds[1]);
    puts("Darwin network wake sockets passed");
    return 0;
}
