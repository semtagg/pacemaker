/*
 * Copyright 2022 the Pacemaker project contributors
 *
 * The version control history for this file may have further details.
 *
 * This source code is licensed under the GNU Lesser General Public License
 * version 2.1 or later (LGPLv2.1+) WITHOUT ANY WARRANTY.
 */

#include <crm_internal.h>
#include "mock_private.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <setjmp.h>
#include <cmocka.h>

#include <unistd.h>
#include <string.h>
#include <errno.h>

#if SUPPORT_PROCFS

static void
no_exe_file(void **state)
{
    char path[PATH_MAX];

    // Set readlink() errno and link contents
    pcmk__mock_readlink = true;
    will_return(__wrap_readlink, ENOENT);
    will_return(__wrap_readlink, NULL);

    assert_int_equal(pcmk__procfs_pid2path(1000, path, sizeof(path)), ENOENT);

    pcmk__mock_readlink = false;
}

static void
contents_too_long(void **state)
{
    char path[10];

    // Set readlink() errno and link contents
    pcmk__mock_readlink = true;
    will_return(__wrap_readlink, 0);
    will_return(__wrap_readlink, "/more/than/10/characters");

    assert_int_equal(pcmk__procfs_pid2path(1000, path, sizeof(path)),
                     ENAMETOOLONG);

    pcmk__mock_readlink = false;
}

static void
contents_ok(void **state)
{
    char path[PATH_MAX];

    // Set readlink() errno and link contents
    pcmk__mock_readlink = true;
    will_return(__wrap_readlink, 0);
    will_return(__wrap_readlink, "/ok");

    assert_int_equal(pcmk__procfs_pid2path((pid_t) 1000, path, sizeof(path)),
                     pcmk_rc_ok);
    assert_string_equal(path, "/ok");

    pcmk__mock_readlink = false;
}

#endif // SUPPORT_PROCFS

int main(int argc, char **argv)
{
    const struct CMUnitTest tests[] = {
#if SUPPORT_PROCFS
        cmocka_unit_test(no_exe_file),
        cmocka_unit_test(contents_too_long),
        cmocka_unit_test(contents_ok),
#endif
    };

    cmocka_set_message_output(CM_OUTPUT_TAP);
    return cmocka_run_group_tests(tests, NULL, NULL);
}
