#include <crm_internal.h>

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>

#include <crm/crm.h>
#include <crm/services.h>
#include "services_private.h"
#include "services_dlopen.h"

#define PCMK_DLOPEN_DIR  "/usr/lib/dlopen" // or "/usr/lib/dlopen/"

GList *
services__list_dlopen_agents(void)
{
    return services_os_get_directory_list(PCMK_DLOPEN_DIR, TRUE, FALSE);
}

bool
services__dlopen_agent_exists(const char *agent)
{
    bool rc = FALSE;
    struct stat st;
    char *path = pcmk__full_path(agent, PCMK_DLOPEN_DIR);

    rc = (stat(path, &st) == 0);
    free(path);
    return rc;
}

int
services__dlopen_prepare(svc_action_t *op)
{
    op->opaque->exec = pcmk__full_path(op->agent, PCMK_DLOPEN_DIR);
    op->opaque->args[0] = strdup(op->opaque->exec);
    op->opaque->args[1] = strdup(op->action);
    if ((op->opaque->args[0] == NULL) || (op->opaque->args[1] == NULL)) {
        return ENOMEM;
    }
    return pcmk_rc_ok;
}

enum ocf_exitcode
services__dlopen2ocf(int exit_status)
{
    return (enum ocf_exitcode) exit_status;
}
