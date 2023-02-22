#ifndef SERVICES_DLOPEN__H
#  define SERVICES_DLOPEN__H

#include <glib.h>

G_GNUC_INTERNAL int services__get_dlopen_metadata(const char *type, char **output); // ???
G_GNUC_INTERNAL GList *services__list_dlopen_agents(void);  // +
G_GNUC_INTERNAL bool services__dlopen_agent_exists(const char *agent); // + +
G_GNUC_INTERNAL int services__dlopen_prepare(svc_action_t *op); // +

G_GNUC_INTERNAL
enum ocf_exitcode services__dlopen2ocf(const char *action, int exit_status); // +

#endif