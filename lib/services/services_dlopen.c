#include <crm_internal.h>
#pragma GCC diagnostic ignored "-Waggregate-return"
#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <crm/crm.h>
#include <crm/services.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "services_private.h"
#include "services_dlopen.h"

#define PCMK_DLOPEN_DIR  "/usr/lib/dlopen" // or "/usr/lib/dlopen/"

typedef int go_int;
typedef struct{const char *p; go_int len;} go_str;

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

// TODO: 
enum ocf_exitcode
services__dlopen2ocf(int exit_status)
{
    return (enum ocf_exitcode) exit_status;
}

int
services__execute_dlopen(svc_action_t *op) {
    if (strcasecmp(op->action, "meta-data") == 0) {
        return services__execute_dlopen_metadata(op);
    }

    return services__execute_dlopen_action(op);
}

int
services__execute_dlopen_metadata(svc_action_t *op) {
    void *lib;
    char *lib_error;
    go_str (*metadata)();
    char dst[200] = "/usr/lib/dlopen/";
    strcat(dst, op->agent);
    lib = dlopen(dst, RTLD_NOW | RTLD_LOCAL | RTLD_NODELETE);

    if (!lib) {
        return pcmk_rc_error;
    }

    metadata = dlsym(lib, "metadata");

    if ((lib_error = dlerror()) != NULL){
        free(lib_error);

        return pcmk_rc_error;
    }
    
    op->rc = PCMK_OCF_OK;
    op->status = PCMK_EXEC_DONE;
    op->pid = 0;
    op->stdout_data = strdup(metadata().p);

    if (op->opaque->callback) {
        op->opaque->callback(op);
    }

    dlclose(lib);
    return pcmk_rc_ok;
}

int
services__execute_dlopen_action(svc_action_t *op) {
    void *lib;
    char *lib_error;
    char *error;
    go_str msg = {op->rsc, sizeof(op->rsc)};
    go_int (*exec)(go_str);
    char dst[200] = "/usr/lib/dlopen/";
    strcat(dst, op->agent);
    g_hash_table_replace(op->params, strdup("DLOPEN_RESOURCE_INSTANCE"), strdup(op->rsc));
    
    crm_info("Resource %s", op->rsc);
    
    lib = dlopen(dst, RTLD_NOW | RTLD_LOCAL | RTLD_NODELETE);

    if (!lib) {
        return pcmk_rc_error;
    }

    exec = dlsym(lib, op->action);

    if ((lib_error = dlerror()) != NULL){
        free(lib_error);

        return pcmk_rc_error;
    }

    op->rc = exec(msg);
    op->status = PCMK_EXEC_DONE;
    op->pid = 0;

    if (op->interval_ms != 0) {
        // Recurring operations must be either cancelled or rescheduled
        if (op->cancel) {
            services__set_cancelled(op);
            cancel_recurring_action(op);
        } else {
            op->opaque->repeat_timer = g_timeout_add(op->interval_ms,
                                                    recurring_action_timer,
                                                    (void *) op);
        }
    }

    if (op->opaque->callback) {
        op->opaque->callback(op);
    }

    crm_info("Exit code: %d, error: %s", op->rc, error);

    dlclose(lib);
    return pcmk_rc_ok;
}
