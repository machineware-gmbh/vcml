/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <arpa/inet.h>

#define DEFAULT_DEVICE  "tap0"
#define DEFAULT_IPADDR  "10.0.0.1"
#define DEFAULT_NETMASK "255.0.0.0"

int  tap_uid     (void);
int  tap_gid     (void);
int  tap_open    (const char *dev, int persistent);
void tap_setup   (const char *dev, int uid, int gid);
void tap_cleanup (const char *dev);
void ip_setup    (const char *dev, const char *addr, const char *mask);

void print_usage(const char *name) {
    fprintf(stderr, "Usage: %s {start|stop} [dev] [ip] [mask]\n", name);
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 5) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    const char *cmd  = argv[1];
    const char *dev  = argc > 2 ? argv[2] : DEFAULT_DEVICE;
    const char *ip   = argc > 3 ? argv[3] : DEFAULT_IPADDR;
    const char *mask = argc > 4 ? argv[4] : DEFAULT_NETMASK;

    int uid = tap_uid();
    int gid = tap_gid();

    if (strcmp(cmd, "start") == 0) {
        tap_setup(dev, uid, gid);
        ip_setup(dev, ip, mask);
        return EXIT_SUCCESS;
    } else if (strcmp(cmd, "stop") == 0) {
        tap_cleanup(dev);
        return EXIT_SUCCESS;
    } else {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
}

int tap_uid(void) {
    const char *sudo_uid = getenv("SUDO_UID");
    return sudo_uid ? atoi(sudo_uid) : getuid();
}

int tap_gid(void) {
    const char *sudo_gid = getenv("SUDO_GID");
    return sudo_gid ? atoi(sudo_gid) : getgid();
}

int tap_open(const char *dev, int persistent) {
    int tap_fd;
    struct ifreq ifr;

    tap_fd = open("/dev/net/tun", O_RDWR);
    if (tap_fd < 0) {
        perror("failed to open tap device driver");
        exit(EXIT_FAILURE);
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, dev, IFNAMSIZ - 1);
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;

    if (ioctl(tap_fd, TUNSETIFF, &ifr) < 0) {
        perror("TUNSETIFF");
        close(tap_fd);
        exit(EXIT_FAILURE);
    }

    if (ioctl(tap_fd, TUNSETPERSIST, persistent) < 0) {
        perror("TUNSETPERSIST");
        close(tap_fd);
        exit(EXIT_FAILURE);
    }

    return tap_fd;
}

void tap_setup(const char *dev, int uid, int gid) {
    /* Open the tap device. We set it to be persistent, so that we can be sure
     * that it does not get deleted by the kernel even after closing it (we
     * need to close it to allow other programs to use it.) */
    int tap_fd = tap_open(dev, 1);

    /* Most operations here can only be done by root, so the program needs to
     * be executed with root privileges. However, we want the user that
     * actually calls this program to also be able to use the device. If the
     * program is run with sudo or setuid, this will set the correct IDs. */
    if ((ioctl(tap_fd, TUNSETOWNER, uid) < 0) ||
        (ioctl(tap_fd, TUNSETGROUP, gid) < 0)) {
        perror("TUNSETOWNER");
        close(tap_fd);
        exit(EXIT_FAILURE);
    }

    /* Close the device to allow others to actually use it. */
    printf("started tap device %s for user %d (group %d)\n", dev, uid, gid);
    close(tap_fd);
}

void tap_cleanup(const char *dev) {
    /* Open the device again. This time we set it to be non-persistent. Closing
     * it will then allow the kernel to delete it (once all other references
     * have been closed as well.) */
    int tap_fd = tap_open(dev, 0);
    close(tap_fd);
    printf("stopped tap device %s\n", dev);
}

void ip_setup(const char *dev, const char *ipaddr, const char *netmask) {
    int net_fd;
    struct ifreq ifr;
    struct sockaddr_in* addr = (struct sockaddr_in *)&ifr.ifr_addr;

    net_fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (net_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, dev, IFNAMSIZ - 1);
    ifr.ifr_addr.sa_family = AF_INET;

    /* Set IP address */
    inet_pton(AF_INET, ipaddr, &addr->sin_addr);
    if (ioctl(net_fd, SIOCSIFADDR, &ifr) < 0) {
        perror("SIOCIFADDR");
        close(net_fd);
        exit(EXIT_FAILURE);
    }

    /* Set net mask */
    inet_pton(AF_INET, netmask, &addr->sin_addr);
    if (ioctl(net_fd, SIOCSIFNETMASK, &ifr) < 0) {
        perror("SIOCSIFNETMASK");
        close(net_fd);
        exit(EXIT_FAILURE);
    }

    /* Switch device on */
    if (ioctl(net_fd, SIOCGIFFLAGS, &ifr) < 0) {
        perror("SIOCSIFNETMASK");
        close(net_fd);
        exit(EXIT_FAILURE);
    }

    strncpy(ifr.ifr_name, dev, IFNAMSIZ - 1);
    ifr.ifr_flags |= (IFF_UP | IFF_RUNNING);

    if (ioctl(net_fd, SIOCSIFFLAGS, &ifr) < 0) {
        perror("SIOCSIFNETMASK");
        close(net_fd);
        exit(EXIT_FAILURE);
    }

    printf("tap device %s online using %s/%s\n", dev, ipaddr, netmask);
    close(net_fd);
}
