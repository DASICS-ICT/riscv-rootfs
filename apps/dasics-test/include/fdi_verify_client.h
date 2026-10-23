#ifndef FDI_VERIFY_CLIENT_H
#define FDI_VERIFY_CLIENT_H

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "linux/fdi_verify.h"

struct fdi_verify_client {
    int fd;
    struct fdi_verify_result observed;
};

static inline int fdi_verify_begin(struct fdi_verify_client *client, unsigned long flags)
{
    struct fdi_verify_result request = {.version = FDI_VERIFY_VERSION, .flags = flags};
    client->fd = open("/dev/fdi-verify", O_RDWR | O_CLOEXEC);
    if (client->fd < 0)
        return -1;
    if (ioctl(client->fd, FDI_VERIFY_BEGIN, &request) == 0) {
        if (request.version == FDI_VERIFY_VERSION && request.flags == flags &&
            request.active == 1 && request.cookie && request.pid == (unsigned long)getpid()) {
            client->observed = request;
            return 0;
        }
        errno = EPROTO;
    }
    int saved = errno;
    close(client->fd);
    client->fd = -1;
    errno = saved;
    return -1;
}

static inline int fdi_verify_request(struct fdi_verify_client *client, unsigned long command,
                                      struct fdi_verify_result *request, unsigned long active)
{
    request->version = FDI_VERIFY_VERSION;
    if (ioctl(client->fd, command, request))
        return -1;
    if (request->version != FDI_VERIFY_VERSION || request->cookie != client->observed.cookie ||
        request->pid != (unsigned long)getpid() || request->active != active ||
        request->flags != client->observed.flags) {
        errno = EPROTO;
        return -1;
    }
    client->observed = *request;
    return 0;
}

static inline int fdi_verify_arm(struct fdi_verify_client *client, unsigned long kind, const void *site)
{
    struct fdi_verify_result request = {.kind = kind, .site = (unsigned long)site};
    return fdi_verify_request(client, FDI_VERIFY_ARM, &request, 1);
}

static inline int fdi_verify_query(struct fdi_verify_client *client)
{
    struct fdi_verify_result request = {0};
    return fdi_verify_request(client, FDI_VERIFY_QUERY, &request, 1);
}

static inline int fdi_verify_end(struct fdi_verify_client *client)
{
    struct fdi_verify_result request = {0};
    int result = fdi_verify_request(client, FDI_VERIFY_END, &request, 0);
    if (!result && ((request.senvcfg & FDI_VERIFY_ENVCFG_MASK) != request.saved_envcfg ||
                   (request.shadow_envcfg & FDI_VERIFY_ENVCFG_MASK) != request.saved_envcfg)) {
        errno = EPROTO;
        result = -1;
    }
    int saved = errno;
    if (close(client->fd) && !result) {
        saved = errno;
        result = -1;
    }
    client->fd = -1;
    errno = saved;
    return result;
}

static inline void fdi_verify_print(const char *name, const struct fdi_verify_result *record)
{
    printf("DASICS_VERIFY_CAUSE2 case=%s version=%llu pid=%llu cookie=0x%llx active=%llu "
           "kind=%llu site=0x%llx instruction=0x%llx count=%llu cause=0x%llx epc=0x%llx "
           "tval=0x%llx freason=0x%llx returnpc=0x%llx\n",
           name, record->version, record->pid, record->cookie, record->active,
           record->kind, record->site, record->instruction, record->illegal_count,
           record->cause, record->epc, record->tval, record->freason, record->return_pc);
    for (unsigned int group = 0; group < 4; group++) {
        printf("DASICS_VERIFY_GPR case=%s pid=%llu cookie=0x%llx group=%u",
               name, record->pid, record->cookie, group);
        for (unsigned int index = group * 8; index < group * 8 + 8; index++)
            printf(" x%u=0x%llx", index, record->gpr[index]);
        printf("\n");
    }
}

#endif
