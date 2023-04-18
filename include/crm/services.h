/*
 * Copyright 2010-2022 the Pacemaker project contributors
 *
 * The version control history for this file may have further details.
 *
 * This source code is licensed under the GNU Lesser General Public License
 * version 2.1 or later (LGPLv2.1+) WITHOUT ANY WARRANTY.
 */

#ifndef PCMK__CRM_SERVICES__H
#  define PCMK__CRM_SERVICES__H


#  include <glib.h>
#  include <stdio.h>
#  include <stdint.h>
#  include <string.h>
#  include <stdbool.h>
#  include <sys/types.h>

#  include <crm_config.h>       // OCF_ROOT_DIR
#  include "common/results.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \file
 * \brief Services API
 * \ingroup core
 */
/* TODO: Autodetect these two ?*/
#  ifndef SYSTEMCTL
#    define SYSTEMCTL "/bin/systemctl"
#  endif

/* Known resource classes */
#define PCMK_RESOURCE_CLASS_OCF     "ocf"
#define PCMK_RESOURCE_CLASS_SERVICE "service"
#define PCMK_RESOURCE_CLASS_LSB     "lsb"
#define PCMK_RESOURCE_CLASS_SYSTEMD "systemd"
#define PCMK_RESOURCE_CLASS_UPSTART "upstart"
#define PCMK_RESOURCE_CLASS_NAGIOS  "nagios"
#define PCMK_RESOURCE_CLASS_STONITH "stonith"
#define PCMK_RESOURCE_CLASS_ALERT   "alert"
#define PCMK_RESOURCE_CLASS_DLOPEN  "dlopen"

/* This is the string passed in the OCF_EXIT_REASON_PREFIX environment variable.
 * The stderr output that occurs after this prefix is encountered is considered
 * the exit reason for a completed operation.
 */
#define PCMK_OCF_REASON_PREFIX "ocf-exit-reason:"

// Agent version to use if agent doesn't specify one
#define PCMK_DEFAULT_AGENT_VERSION "0.1"

enum lsb_exitcode {
    PCMK_LSB_OK                  = 0,
    PCMK_LSB_UNKNOWN_ERROR       = 1,
    PCMK_LSB_INVALID_PARAM       = 2,
    PCMK_LSB_UNIMPLEMENT_FEATURE = 3,
    PCMK_LSB_INSUFFICIENT_PRIV   = 4,
    PCMK_LSB_NOT_INSTALLED       = 5,
    PCMK_LSB_NOT_CONFIGURED      = 6,
    PCMK_LSB_NOT_RUNNING         = 7,
};

// LSB uses different return codes for status actions
enum lsb_status_exitcode {
    PCMK_LSB_STATUS_OK             = 0,
    PCMK_LSB_STATUS_VAR_PID        = 1,
    PCMK_LSB_STATUS_VAR_LOCK       = 2,
    PCMK_LSB_STATUS_NOT_RUNNING    = 3,
    PCMK_LSB_STATUS_UNKNOWN        = 4,

    /* custom codes should be in the 150-199 range reserved for application use */
    PCMK_LSB_STATUS_NOT_INSTALLED      = 150,
    PCMK_LSB_STATUS_INSUFFICIENT_PRIV  = 151,
};

enum nagios_exitcode {
    NAGIOS_STATE_OK        = 0,
    NAGIOS_STATE_WARNING   = 1,
    NAGIOS_STATE_CRITICAL  = 2,
    NAGIOS_STATE_UNKNOWN   = 3,

    /* This is a custom Pacemaker value (not a nagios convention), used as an
     * intermediate value between the services library and the executor, so the
     * executor can map it to the corresponding OCF code.
     */
    NAGIOS_INSUFFICIENT_PRIV = 100,

#if !defined(PCMK_ALLOW_DEPRECATED) || (PCMK_ALLOW_DEPRECATED == 1)
    NAGIOS_STATE_DEPENDENT   = 4,   //! \deprecated Unused
    NAGIOS_NOT_INSTALLED     = 101, //! \deprecated Unused
#endif
};

enum svc_action_flags {
    /* On timeout, only kill pid, do not kill entire pid group */
    SVC_ACTION_LEAVE_GROUP = 0x01,
    SVC_ACTION_NON_BLOCKED = 0x02,
};

typedef struct svc_action_private_s svc_action_private_t;

/*!
 * \brief Object for executing external actions
 *
 * \note This object should never be instantiated directly, but instead created
 *       using one of the constructor functions (resources_action_create() for
 *       resource agents, services_alert_create() for alert agents, or
 *       services_action_create_generic() for generic executables). Similarly,
 *       do not use sizeof() on this struct.
 *
 * \internal Internally, services__create_resource_action() is preferable to
 *           resources_action_create().
 */
typedef struct svc_action_s {
    /*! Operation key (<resource>_<action>_<interval>) for resource actions,
     *  XML ID for alert actions, or NULL for generic actions
     */
    char *id;

    //! XML ID of resource being executed for resource actions, otherwise NULL
    char *rsc;

    //! Name of action being executed for resource actions, otherwise NULL
    char *action;

    //! Action interval for recurring resource actions, otherwise 0
    guint interval_ms;

    //! Resource standard for resource actions, otherwise NULL
    char *standard;

    //! Resource provider for resource actions that require it, otherwise NULL
    char *provider;

    //! Resource agent name for resource actions, otherwise NULL
    char *agent;

    int timeout;    //!< Action timeout (in milliseconds)

    /*! A hash table of name/value pairs to use as parameters for resource and
     *  alert actions, otherwise NULL. These will be used to set environment
     *  variables for non-fencing resource agents and alert agents, and to send
     *  stdin to fence agents.
     */
    GHashTable *params;

    int rc;         //!< Exit status of action (set by library upon completion)

    //!@{
    //! This field should be treated as internal to Pacemaker
    int pid;        // Process ID of child
    int cancel;     // Whether this is a cancellation of a recurring action
    //!@}

    int status;     //!< Execution status (enum pcmk_exec_status set by library)

    /*! Action counter (set by library for resource actions, or by caller
     * otherwise)
     */
    int sequence;

    //!@{
    //! This field should be treated as internal to Pacemaker
    int expected_rc;    // Unused
    int synchronous;    // Whether execution should be synchronous (blocking)
    //!@}

    enum svc_action_flags flags;    //!< Flag group of enum svc_action_flags
    char *stderr_data;              //!< Action stderr (set by library)
    char *stdout_data;              //!< Action stdout (set by library)
    void *cb_data;                  //!< For caller's use (not used by library)

    //! This field should be treated as internal to Pacemaker
    svc_action_private_t *opaque;
} svc_action_t;

/**
 * \brief Get a list of files or directories in a given path
 *
 * \param[in] root       full path to a directory to read
 * \param[in] files      return list of files if TRUE or directories if FALSE
 * \param[in] executable if TRUE and files is TRUE, only return executable files
 *
 * \return a list of what was found.  The list items are char *.
 * \note It is the caller's responsibility to free the result with g_list_free_full(list, free).
 */
    GList *get_directory_list(const char *root, gboolean files, gboolean executable);

/**
 * \brief Get a list of providers
 *
 * \param[in] standard  list providers of this standard (e.g. ocf, lsb, etc.)
 *
 * \return a list of providers as char * list items (or NULL if standard does not support providers)
 * \note The caller is responsible for freeing the result using g_list_free_full(list, free).
 */
    GList *resources_list_providers(const char *standard);

/**
 * \brief Get a list of resource agents
 *
 * \param[in] standard  list agents using this standard (e.g. ocf, lsb, etc.) (or NULL for all)
 * \param[in] provider  list agents from this provider (or NULL for all)
 *
 * \return a list of resource agents.  The list items are char *.
 * \note The caller is responsible for freeing the result using g_list_free_full(list, free).
 */
    GList *resources_list_agents(const char *standard, const char *provider);

/**
 * Get list of available standards
 *
 * \return a list of resource standards. The list items are char *. This list _must_
 *         be destroyed using g_list_free_full(list, free).
 */
    GList *resources_list_standards(void);

/**
 * Does the given standard, provider, and agent describe a resource that can exist?
 *
 * \param[in] standard  Which class of agent does the resource belong to?
 * \param[in] provider  What provides the agent (NULL for most standards)?
 * \param[in] agent     What is the name of the agent?
 *
 * \return A boolean
 */
    gboolean resources_agent_exists(const char *standard, const char *provider, const char *agent);

/**
 * \brief Create a new resource action
 *
 * \param[in] name        Name of resource
 * \param[in] standard    Resource agent standard (ocf, lsb, etc.)
 * \param[in] provider    Resource agent provider
 * \param[in] agent       Resource agent name
 * \param[in] action      action (start, stop, monitor, etc.)
 * \param[in] interval_ms How often to repeat this action (if 0, execute once)
 * \param[in] timeout     Consider action failed if it does not complete in this many milliseconds
 * \param[in] params      Action parameters
 *
 * \return newly allocated action instance
 *
 * \post After the call, 'params' is owned, and later free'd by the svc_action_t result
 * \note The caller is responsible for freeing the return value using
 *       services_action_free().
 */
svc_action_t *resources_action_create(const char *name, const char *standard,
                                      const char *provider, const char *agent,
                                      const char *action, guint interval_ms,
                                      int timeout /* ms */, GHashTable *params,
                                      enum svc_action_flags flags);

/**
 * Kick a recurring action so it is scheduled immediately for re-execution
 */
gboolean services_action_kick(const char *name, const char *action,
                              guint interval_ms);

    const char *resources_find_service_class(const char *agent);

/**
 * Utilize services API to execute an arbitrary command.
 *
 * This API has useful infrastructure in place to be able to run a command
 * in the background and get notified via a callback when the command finishes.
 *
 * \param[in] exec command to execute
 * \param[in] args arguments to the command, NULL terminated
 *
 * \return a svc_action_t object, used to pass to the execute function
 * (services_action_sync() or services_action_async()) and is
 * provided to the callback.
 */
    svc_action_t *services_action_create_generic(const char *exec, const char *args[]);

    void services_action_cleanup(svc_action_t * op);
    void services_action_free(svc_action_t * op);
    int services_action_user(svc_action_t *op, const char *user);

    gboolean services_action_sync(svc_action_t * op);

/**
 * \brief Run an action asynchronously, with callback after process is forked
 *
 * \param[in] op                    Action to run
 * \param[in] action_callback       Function to call when the action completes
 *                                  (if NULL, any previously set callback will
 *                                  continue to be used)
 * \param[in] action_fork_callback  Function to call after action process forks
 *                                  (if NULL, any previously set callback will
 *                                  continue to be used)
 *
 * \return Boolean value
 *
 * \retval TRUE if the caller should not free or otherwise use \p op again,
 *         because one of these conditions is true:
 *
 *         * \p op is NULL.
 *         * The action was successfully initiated, in which case
 *           \p action_fork_callback has been called, but \p action_callback has
 *           not (it will be called when the action completes).
 *         * The action's ID matched an existing recurring action. The existing
 *           action has taken over the callback and callback data from \p op
 *           and has been re-initiated asynchronously, and \p op has been freed.
 *         * Another action for the same resource is in flight, and \p op will
 *           be blocked until it completes.
 *         * The action could not be initiated, and is either non-recurring or
 *           being cancelled. \p action_fork_callback has not been called, but
 *           \p action_callback has, and \p op has been freed.
 *
 * \retval FALSE if \op is still valid, because the action cannot be initiated,
 *         and is a recurring action that is not being cancelled.
 *         \p action_fork_callback has not been called, but \p action_callback
 *         has, and a timer has been set for the next invocation of \p op.
 */
gboolean services_action_async_fork_notify(svc_action_t *op,
        void (*action_callback) (svc_action_t *),
        void (*action_fork_callback) (svc_action_t *));

/**
 * \brief Run an action asynchronously
 *
 * \param[in] op                    Action to run
 * \param[in] action_callback       Function to call when the action completes
 *                                  (if NULL, any previously set callback will
 *                                  continue to be used)
 *
 * \return Boolean value
 *
 * \retval TRUE if the caller should not free or otherwise use \p op again,
 *         because one of these conditions is true:
 *
 *         * \p op is NULL.
 *         * The action was successfully initiated, in which case
 *           \p action_callback has not been called (it will be called when the
 *           action completes).
 *         * The action's ID matched an existing recurring action. The existing
 *           action has taken over the callback and callback data from \p op
 *           and has been re-initiated asynchronously, and \p op has been freed.
 *         * Another action for the same resource is in flight, and \p op will
 *           be blocked until it completes.
 *         * The action could not be initiated, and is either non-recurring or
 *           being cancelled. \p action_callback has been called, and \p op has
 *           been freed.
 *
 * \retval FALSE if \op is still valid, because the action cannot be initiated,
 *         and is a recurring action that is not being cancelled.
 *         \p action_callback has been called, and a timer has been set for the
 *         next invocation of \p op.
 */
gboolean services_action_async(svc_action_t *op,
                               void (*action_callback) (svc_action_t *));

gboolean services_action_cancel(const char *name, const char *action,
                                guint interval_ms);

/* functions for alert agents */
svc_action_t *services_alert_create(const char *id, const char *exec,
                                   int timeout, GHashTable *params,
                                   int sequence, void *cb_data);
gboolean services_alert_async(svc_action_t *action,
                              void (*cb)(svc_action_t *op));

enum ocf_exitcode services_result2ocf(const char *standard, const char *action,
                                      int exit_status);

    static inline const char *services_ocf_exitcode_str(enum ocf_exitcode code) {
        switch (code) {
            case PCMK_OCF_OK:
                return "ok";
            case PCMK_OCF_UNKNOWN_ERROR:
                return "error";
            case PCMK_OCF_INVALID_PARAM:
                return "invalid parameter";
            case PCMK_OCF_UNIMPLEMENT_FEATURE:
                return "unimplemented feature";
            case PCMK_OCF_INSUFFICIENT_PRIV:
                return "insufficient privileges";
            case PCMK_OCF_NOT_INSTALLED:
                return "not installed";
            case PCMK_OCF_NOT_CONFIGURED:
                return "not configured";
            case PCMK_OCF_NOT_RUNNING:
                return "not running";
            case PCMK_OCF_RUNNING_PROMOTED:
                return "promoted";
            case PCMK_OCF_FAILED_PROMOTED:
                return "promoted (failed)";
            case PCMK_OCF_DEGRADED:
                return "OCF_DEGRADED";
            case PCMK_OCF_DEGRADED_PROMOTED:
                return "promoted (degraded)";

#if !defined(PCMK_ALLOW_DEPRECATED) || (PCMK_ALLOW_DEPRECATED == 1)
            case PCMK_OCF_NOT_SUPPORTED:
                return "not supported (DEPRECATED STATUS)";
            case PCMK_OCF_CANCELLED:
                return "cancelled (DEPRECATED STATUS)";
            case PCMK_OCF_OTHER_ERROR:
                return "other error (DEPRECATED STATUS)";
            case PCMK_OCF_SIGNAL:
                return "interrupted by signal (DEPRECATED STATUS)";
            case PCMK_OCF_PENDING:
                return "pending (DEPRECATED STATUS)";
            case PCMK_OCF_TIMEOUT:
                return "timeout (DEPRECATED STATUS)";
#endif
            default:
                return "unknown";
        }
    }

#if !defined(PCMK_ALLOW_DEPRECATED) || (PCMK_ALLOW_DEPRECATED == 1)
#include <crm/services_compat.h>
#endif

#  ifdef __cplusplus
}
#  endif

#endif                          /* __PCMK_SERVICES__ */
