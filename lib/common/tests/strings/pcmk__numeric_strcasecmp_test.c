/*
 * Copyright 2022 the Pacemaker project contributors
 *
 * The version control history for this file may have further details.
 *
 * This source code is licensed under the GNU Lesser General Public License
 * version 2.1 or later (LGPLv2.1+) WITHOUT ANY WARRANTY.
 */

#include <crm_internal.h>

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>

static void
no_numbers(void **state)
{
    /* All comparisons are done case-insensitively. */
    assert_int_equal(pcmk__numeric_strcasecmp("abcd", "efgh"), -1);
    assert_int_equal(pcmk__numeric_strcasecmp("abcd", "abcd"), 0);
    assert_int_equal(pcmk__numeric_strcasecmp("efgh", "abcd"), 1);

    assert_int_equal(pcmk__numeric_strcasecmp("AbCd", "eFgH"), -1);
    assert_int_equal(pcmk__numeric_strcasecmp("ABCD", "abcd"), 0);
    assert_int_equal(pcmk__numeric_strcasecmp("EFgh", "ABcd"), 1);
}

static void
trailing_numbers(void **state)
{
    assert_int_equal(pcmk__numeric_strcasecmp("node1", "node2"), -1);
    assert_int_equal(pcmk__numeric_strcasecmp("node1", "node1"), 0);
    assert_int_equal(pcmk__numeric_strcasecmp("node2", "node1"), 1);

    assert_int_equal(pcmk__numeric_strcasecmp("node1", "node10"), -1);
    assert_int_equal(pcmk__numeric_strcasecmp("node10", "node10"), 0);
    assert_int_equal(pcmk__numeric_strcasecmp("node10", "node1"), 1);

    assert_int_equal(pcmk__numeric_strcasecmp("node10", "remotenode9"), -1);
    assert_int_equal(pcmk__numeric_strcasecmp("remotenode9", "node10"), 1);

    /* Longer numbers sort higher than shorter numbers. */
    assert_int_equal(pcmk__numeric_strcasecmp("node001", "node1"), 1);
    assert_int_equal(pcmk__numeric_strcasecmp("node1", "node001"), -1);
}

static void
middle_numbers(void **state)
{
    assert_int_equal(pcmk__numeric_strcasecmp("node1abc", "node1def"), -1);
    assert_int_equal(pcmk__numeric_strcasecmp("node1def", "node1abc"), 1);

    assert_int_equal(pcmk__numeric_strcasecmp("node1abc", "node2abc"), -1);
    assert_int_equal(pcmk__numeric_strcasecmp("node2abc", "node1abc"), 1);
}

static void
unequal_lengths(void **state)
{
    assert_int_equal(pcmk__numeric_strcasecmp("node-ab", "node-abc"), -1);
    assert_int_equal(pcmk__numeric_strcasecmp("node-abc", "node-ab"), 1);

    assert_int_equal(pcmk__numeric_strcasecmp("node1ab", "node1abc"), -1);
    assert_int_equal(pcmk__numeric_strcasecmp("node1abc", "node1ab"), 1);
}

int
main(int argc, char **argv)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(no_numbers),
        cmocka_unit_test(trailing_numbers),
        cmocka_unit_test(middle_numbers),
        cmocka_unit_test(unequal_lengths),
    };

    cmocka_set_message_output(CM_OUTPUT_TAP);
    return cmocka_run_group_tests(tests, NULL, NULL);
}
