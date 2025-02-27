/*
 * Copyright 2004-2022 the Pacemaker project contributors
 *
 * The version control history for this file may have further details.
 *
 * This source code is licensed under the GNU General Public License version 2
 * or later (GPLv2+) WITHOUT ANY WARRANTY.
 */

/* put these first so that uuid_t is defined without conflicts */
#include <crm_internal.h>

#include <string.h>

#include <crm/crm.h>
#include <crm/msg_xml.h>
#include <crm/common/xml.h>
#include <crm/common/xml_internal.h>
#include <crm/cluster/internal.h>

#include <pacemaker-controld.h>

gboolean membership_flux_hack = FALSE;
void post_cache_update(int instance);

int last_peer_update = 0;
guint highest_born_on = -1;

extern gboolean check_join_state(enum crmd_fsa_state cur_state, const char *source);

static void
reap_dead_nodes(gpointer key, gpointer value, gpointer user_data)
{
    crm_node_t *node = value;

    if (crm_is_peer_active(node) == FALSE) {
        crm_update_peer_join(__func__, node, crm_join_none);

        if(node && node->uname) {
            if (pcmk__str_eq(fsa_our_uname, node->uname, pcmk__str_casei)) {
                crm_err("We're not part of the cluster anymore");
                register_fsa_input(C_FSA_INTERNAL, I_ERROR, NULL);

            } else if (AM_I_DC == FALSE && pcmk__str_eq(node->uname, fsa_our_dc, pcmk__str_casei)) {
                crm_warn("Our DC node (%s) left the cluster", node->uname);
                register_fsa_input(C_FSA_INTERNAL, I_ELECTION, NULL);
            }
        }

        if (fsa_state == S_INTEGRATION || fsa_state == S_FINALIZE_JOIN) {
            check_join_state(fsa_state, __func__);
        }
        if(node && node->uuid) {
            fail_incompletable_actions(transition_graph, node->uuid);
        }
    }
}

gboolean ever_had_quorum = FALSE;

void
post_cache_update(int instance)
{
    xmlNode *no_op = NULL;

    crm_peer_seq = instance;
    crm_debug("Updated cache after membership event %d.", instance);

    g_hash_table_foreach(crm_peer_cache, reap_dead_nodes, NULL);
    controld_set_fsa_input_flags(R_MEMBERSHIP);

    if (AM_I_DC) {
        populate_cib_nodes(node_update_quick | node_update_cluster | node_update_peer |
                           node_update_expected, __func__);
    }

    /*
     * If we lost nodes, we should re-check the election status
     * Safe to call outside of an election
     */
    controld_set_fsa_action_flags(A_ELECTION_CHECK);
    trigger_fsa();

    /* Membership changed, remind everyone we're here.
     * This will aid detection of duplicate DCs
     */
    no_op = create_request(CRM_OP_NOOP, NULL, NULL, CRM_SYSTEM_CRMD,
                           AM_I_DC ? CRM_SYSTEM_DC : CRM_SYSTEM_CRMD, NULL);
    send_cluster_message(NULL, crm_msg_crmd, no_op, FALSE);
    free_xml(no_op);
}

static void
crmd_node_update_complete(xmlNode * msg, int call_id, int rc, xmlNode * output, void *user_data)
{
    fsa_data_t *msg_data = NULL;

    last_peer_update = 0;

    if (rc == pcmk_ok) {
        crm_trace("Node update %d complete", call_id);

    } else if(call_id < pcmk_ok) {
        crm_err("Node update failed: %s (%d)", pcmk_strerror(call_id), call_id);
        crm_log_xml_debug(msg, "failed");
        register_fsa_error(C_FSA_INTERNAL, I_ERROR, NULL);

    } else {
        crm_err("Node update %d failed: %s (%d)", call_id, pcmk_strerror(rc), rc);
        crm_log_xml_debug(msg, "failed");
        register_fsa_error(C_FSA_INTERNAL, I_ERROR, NULL);
    }
}

/*!
 * \internal
 * \brief Create an XML node state tag with updates
 *
 * \param[in,out] node    Node whose state will be used for update
 * \param[in]     flags   Bitmask of node_update_flags indicating what to update
 * \param[in,out] parent  XML node to contain update (or NULL)
 * \param[in]     source  Who requested the update (only used for logging)
 *
 * \return Pointer to created node state tag
 */
xmlNode *
create_node_state_update(crm_node_t *node, int flags, xmlNode *parent,
                         const char *source)
{
    const char *value = NULL;
    xmlNode *node_state;

    if (!node->state) {
        crm_info("Node update for %s cancelled: no state, not seen yet", node->uname);
       return NULL;
    }

    node_state = create_xml_node(parent, XML_CIB_TAG_STATE);

    if (pcmk_is_set(node->flags, crm_remote_node)) {
        pcmk__xe_set_bool_attr(node_state, XML_NODE_IS_REMOTE, true);
    }

    set_uuid(node_state, XML_ATTR_UUID, node);

    if (crm_element_value(node_state, XML_ATTR_UUID) == NULL) {
        crm_info("Node update for %s cancelled: no id", node->uname);
        free_xml(node_state);
        return NULL;
    }

    crm_xml_add(node_state, XML_ATTR_UNAME, node->uname);

    if ((flags & node_update_cluster) && node->state) {
        pcmk__xe_set_bool_attr(node_state, XML_NODE_IN_CLUSTER,
                               pcmk__str_eq(node->state, CRM_NODE_MEMBER, pcmk__str_casei));
    }

    if (!pcmk_is_set(node->flags, crm_remote_node)) {
        if (flags & node_update_peer) {
            value = OFFLINESTATUS;
            if (pcmk_is_set(node->processes, crm_get_cluster_proc())) {
                value = ONLINESTATUS;
            }
            crm_xml_add(node_state, XML_NODE_IS_PEER, value);
        }

        if (flags & node_update_join) {
            if (node->join <= crm_join_none) {
                value = CRMD_JOINSTATE_DOWN;
            } else {
                value = CRMD_JOINSTATE_MEMBER;
            }
            crm_xml_add(node_state, XML_NODE_JOIN_STATE, value);
        }

        if (flags & node_update_expected) {
            crm_xml_add(node_state, XML_NODE_EXPECTED, node->expected);
        }
    }

    crm_xml_add(node_state, XML_ATTR_ORIGIN, source);

    return node_state;
}

static void
remove_conflicting_node_callback(xmlNode * msg, int call_id, int rc,
                                 xmlNode * output, void *user_data)
{
    char *node_uuid = user_data;

    do_crm_log_unlikely(rc == 0 ? LOG_DEBUG : LOG_NOTICE,
                        "Deletion of the unknown conflicting node \"%s\": %s (rc=%d)",
                        node_uuid, pcmk_strerror(rc), rc);
}

static void
search_conflicting_node_callback(xmlNode * msg, int call_id, int rc,
                                 xmlNode * output, void *user_data)
{
    char *new_node_uuid = user_data;
    xmlNode *node_xml = NULL;

    if (rc != pcmk_ok) {
        if (rc != -ENXIO) {
            crm_notice("Searching conflicting nodes for %s failed: %s (%d)",
                       new_node_uuid, pcmk_strerror(rc), rc);
        }
        return;

    } else if (output == NULL) {
        return;
    }

    if (pcmk__str_eq(crm_element_name(output), XML_CIB_TAG_NODE, pcmk__str_casei)) {
        node_xml = output;

    } else {
        node_xml = pcmk__xml_first_child(output);
    }

    for (; node_xml != NULL; node_xml = pcmk__xml_next(node_xml)) {
        const char *node_uuid = NULL;
        const char *node_uname = NULL;
        GHashTableIter iter;
        crm_node_t *node = NULL;
        gboolean known = FALSE;

        if (!pcmk__str_eq(crm_element_name(node_xml), XML_CIB_TAG_NODE, pcmk__str_casei)) {
            continue;
        }

        node_uuid = crm_element_value(node_xml, XML_ATTR_ID);
        node_uname = crm_element_value(node_xml, XML_ATTR_UNAME);

        if (node_uuid == NULL || node_uname == NULL) {
            continue;
        }

        g_hash_table_iter_init(&iter, crm_peer_cache);
        while (g_hash_table_iter_next(&iter, NULL, (gpointer *) &node)) {
            if (node->uuid
                && pcmk__str_eq(node->uuid, node_uuid, pcmk__str_casei)
                && node->uname
                && pcmk__str_eq(node->uname, node_uname, pcmk__str_casei)) {

                known = TRUE;
                break;
            }
        }

        if (known == FALSE) {
            int delete_call_id = 0;
            xmlNode *node_state_xml = NULL;

            crm_notice("Deleting unknown node %s/%s which has conflicting uname with %s",
                       node_uuid, node_uname, new_node_uuid);

            delete_call_id = fsa_cib_conn->cmds->remove(fsa_cib_conn, XML_CIB_TAG_NODES, node_xml,
                                                        cib_scope_local | cib_quorum_override);
            fsa_register_cib_callback(delete_call_id, FALSE, strdup(node_uuid),
                                      remove_conflicting_node_callback);

            node_state_xml = create_xml_node(NULL, XML_CIB_TAG_STATE);
            crm_xml_add(node_state_xml, XML_ATTR_ID, node_uuid);
            crm_xml_add(node_state_xml, XML_ATTR_UNAME, node_uname);

            delete_call_id = fsa_cib_conn->cmds->remove(fsa_cib_conn, XML_CIB_TAG_STATUS, node_state_xml,
                                                        cib_scope_local | cib_quorum_override);
            fsa_register_cib_callback(delete_call_id, FALSE, strdup(node_uuid),
                                      remove_conflicting_node_callback);
            free_xml(node_state_xml);
        }
    }
}

static void
node_list_update_callback(xmlNode * msg, int call_id, int rc, xmlNode * output, void *user_data)
{
    fsa_data_t *msg_data = NULL;

    if(call_id < pcmk_ok) {
        crm_err("Node list update failed: %s (%d)", pcmk_strerror(call_id), call_id);
        crm_log_xml_debug(msg, "update:failed");
        register_fsa_error(C_FSA_INTERNAL, I_ERROR, NULL);

    } else if(rc < pcmk_ok) {
        crm_err("Node update %d failed: %s (%d)", call_id, pcmk_strerror(rc), rc);
        crm_log_xml_debug(msg, "update:failed");
        register_fsa_error(C_FSA_INTERNAL, I_ERROR, NULL);
    }
}

#define NODE_PATH_MAX 512

void
populate_cib_nodes(enum node_update_flags flags, const char *source)
{
    int call_id = 0;
    gboolean from_hashtable = TRUE;
    int call_options = cib_scope_local | cib_quorum_override;
    xmlNode *node_list = create_xml_node(NULL, XML_CIB_TAG_NODES);

#if SUPPORT_COROSYNC
    if (!pcmk_is_set(flags, node_update_quick) && is_corosync_cluster()) {
        from_hashtable = pcmk__corosync_add_nodes(node_list);
    }
#endif

    if (from_hashtable) {
        GHashTableIter iter;
        crm_node_t *node = NULL;

        g_hash_table_iter_init(&iter, crm_peer_cache);
        while (g_hash_table_iter_next(&iter, NULL, (gpointer *) &node)) {
            xmlNode *new_node = NULL;

            crm_trace("Creating node entry for %s/%s", node->uname, node->uuid);
            if(node->uuid && node->uname) {
                char xpath[NODE_PATH_MAX];

                /* We need both to be valid */
                new_node = create_xml_node(node_list, XML_CIB_TAG_NODE);
                crm_xml_add(new_node, XML_ATTR_ID, node->uuid);
                crm_xml_add(new_node, XML_ATTR_UNAME, node->uname);

                /* Search and remove unknown nodes with the conflicting uname from CIB */
                snprintf(xpath, NODE_PATH_MAX,
                         "/" XML_TAG_CIB "/" XML_CIB_TAG_CONFIGURATION "/" XML_CIB_TAG_NODES
                         "/" XML_CIB_TAG_NODE "[@uname='%s'][@id!='%s']",
                         node->uname, node->uuid);

                call_id = fsa_cib_conn->cmds->query(fsa_cib_conn, xpath, NULL,
                                                    cib_scope_local | cib_xpath);
                fsa_register_cib_callback(call_id, FALSE, strdup(node->uuid),
                                          search_conflicting_node_callback);
            }
        }
    }

    crm_trace("Populating <nodes> section from %s", from_hashtable ? "hashtable" : "cluster");

    fsa_cib_update(XML_CIB_TAG_NODES, node_list, call_options, call_id, NULL);
    fsa_register_cib_callback(call_id, FALSE, NULL, node_list_update_callback);

    free_xml(node_list);

    if (call_id >= pcmk_ok && crm_peer_cache != NULL && AM_I_DC) {
        /*
         * There is no need to update the local CIB with our values if
         * we've not seen valid membership data
         */
        GHashTableIter iter;
        crm_node_t *node = NULL;

        node_list = create_xml_node(NULL, XML_CIB_TAG_STATUS);

        g_hash_table_iter_init(&iter, crm_peer_cache);
        while (g_hash_table_iter_next(&iter, NULL, (gpointer *) &node)) {
            create_node_state_update(node, flags, node_list, source);
        }

        if (crm_remote_peer_cache) {
            g_hash_table_iter_init(&iter, crm_remote_peer_cache);
            while (g_hash_table_iter_next(&iter, NULL, (gpointer *) &node)) {
                create_node_state_update(node, flags, node_list, source);
            }
        }

        fsa_cib_update(XML_CIB_TAG_STATUS, node_list, call_options, call_id, NULL);
        fsa_register_cib_callback(call_id, FALSE, NULL, crmd_node_update_complete);
        last_peer_update = call_id;

        free_xml(node_list);
    }
}

static void
cib_quorum_update_complete(xmlNode * msg, int call_id, int rc, xmlNode * output, void *user_data)
{
    fsa_data_t *msg_data = NULL;

    if (rc == pcmk_ok) {
        crm_trace("Quorum update %d complete", call_id);

    } else {
        crm_err("Quorum update %d failed: %s (%d)", call_id, pcmk_strerror(rc), rc);
        crm_log_xml_debug(msg, "failed");
        register_fsa_error(C_FSA_INTERNAL, I_ERROR, NULL);
    }
}

void
crm_update_quorum(gboolean quorum, gboolean force_update)
{
    ever_had_quorum |= quorum;

    if(ever_had_quorum && quorum == FALSE && no_quorum_suicide_escalation) {
        pcmk__panic(__func__);
    }

    if (AM_I_DC && (force_update || fsa_has_quorum != quorum)) {
        int call_id = 0;
        xmlNode *update = NULL;
        int call_options = cib_scope_local | cib_quorum_override;

        update = create_xml_node(NULL, XML_TAG_CIB);
        crm_xml_add_int(update, XML_ATTR_HAVE_QUORUM, quorum);
        crm_xml_add(update, XML_ATTR_DC_UUID, fsa_our_uuid);

        fsa_cib_update(XML_TAG_CIB, update, call_options, call_id, NULL);
        crm_debug("Updating quorum status to %s (call=%d)",
                  pcmk__btoa(quorum), call_id);
        fsa_register_cib_callback(call_id, FALSE, NULL, cib_quorum_update_complete);
        free_xml(update);

        /* Quorum changes usually cause a new transition via other activity:
         * quorum gained via a node joining will abort via the node join,
         * and quorum lost via a node leaving will usually abort via resource
         * activity and/or fencing.
         *
         * However, it is possible that nothing else causes a transition (e.g.
         * someone forces quorum via corosync-cmaptcl, or quorum is lost due to
         * a node in standby shutting down cleanly), so here ensure a new
         * transition is triggered.
         */
        if (quorum) {
            /* If quorum was gained, abort after a short delay, in case multiple
             * nodes are joining around the same time, so the one that brings us
             * to quorum doesn't cause all the remaining ones to be fenced.
             */
            abort_after_delay(INFINITY, pcmk__graph_restart, "Quorum gained",
                              5000);
        } else {
            abort_transition(INFINITY, pcmk__graph_restart, "Quorum lost",
                             NULL);
        }
    }
    fsa_has_quorum = quorum;
}
