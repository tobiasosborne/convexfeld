/**
 * @file test_validate_vartypes.c
 * @brief Tests for cxf_validate_vartypes (V2 spec: data_validation.md)
 *
 * V2 signature: (CxfEnv *env, int count, const char *vartypes)
 * Pure validation only -- no side effects, no bound clamping.
 * Case-insensitive: lowercase letters normalized to uppercase.
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"

/* Forward declaration -- V2 spec signature */
int cxf_validate_vartypes(CxfEnv *env, int count, const char *vartypes);

void setUp(void) {}
void tearDown(void) {}

/*============================================================================
 * Edge cases: NULL and empty
 *===========================================================================*/

void test_null_vartypes_returns_ok(void) {
    int result = cxf_validate_vartypes(NULL, 5, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_zero_count_returns_ok(void) {
    const char types[] = "X";
    int result = cxf_validate_vartypes(NULL, 0, types);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_negative_count_returns_ok(void) {
    const char types[] = "X";
    int result = cxf_validate_vartypes(NULL, -3, types);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

/*============================================================================
 * Valid type codes (uppercase)
 *===========================================================================*/

void test_all_continuous(void) {
    const char types[] = {'C', 'C', 'C'};
    int result = cxf_validate_vartypes(NULL, 3, types);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_all_five_valid_types(void) {
    const char types[] = {'C', 'B', 'I', 'S', 'N'};
    int result = cxf_validate_vartypes(NULL, 5, types);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_single_binary(void) {
    const char types[] = {'B'};
    int result = cxf_validate_vartypes(NULL, 1, types);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_single_integer(void) {
    const char types[] = {'I'};
    int result = cxf_validate_vartypes(NULL, 1, types);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_single_semi_continuous(void) {
    const char types[] = {'S'};
    int result = cxf_validate_vartypes(NULL, 1, types);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_single_semi_integer(void) {
    const char types[] = {'N'};
    int result = cxf_validate_vartypes(NULL, 1, types);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

/*============================================================================
 * Case normalization: lowercase accepted per V2 spec
 *===========================================================================*/

void test_lowercase_c_accepted(void) {
    const char types[] = {'c'};
    int result = cxf_validate_vartypes(NULL, 1, types);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_lowercase_b_accepted(void) {
    const char types[] = {'b'};
    int result = cxf_validate_vartypes(NULL, 1, types);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_lowercase_all_five_accepted(void) {
    const char types[] = {'c', 'b', 'i', 's', 'n'};
    int result = cxf_validate_vartypes(NULL, 5, types);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_mixed_case_accepted(void) {
    const char types[] = {'C', 'b', 'I', 's', 'N'};
    int result = cxf_validate_vartypes(NULL, 5, types);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

/*============================================================================
 * Invalid type codes
 *===========================================================================*/

void test_invalid_x_rejected(void) {
    const char types[] = {'C', 'X', 'I'};
    int result = cxf_validate_vartypes(NULL, 3, types);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);
}

void test_invalid_digit_rejected(void) {
    const char types[] = {'C', '1', 'B'};
    int result = cxf_validate_vartypes(NULL, 3, types);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);
}

void test_invalid_space_rejected(void) {
    const char types[] = {' '};
    int result = cxf_validate_vartypes(NULL, 1, types);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);
}

void test_invalid_null_char_rejected(void) {
    const char types[] = {'\0'};
    int result = cxf_validate_vartypes(NULL, 1, types);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);
}

void test_early_exit_on_first_invalid(void) {
    /* Only first invalid matters; rest is not examined */
    const char types[] = {'C', 'B', 'Z', 'Q', 'W'};
    int result = cxf_validate_vartypes(NULL, 5, types);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);
}

void test_invalid_at_end(void) {
    const char types[] = {'C', 'B', 'I', 'S', '!'};
    int result = cxf_validate_vartypes(NULL, 5, types);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);
}

void test_invalid_at_start(void) {
    const char types[] = {'?', 'C', 'B'};
    int result = cxf_validate_vartypes(NULL, 3, types);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);
}

/*============================================================================
 * Pure validation: no side effects (regression guard)
 *===========================================================================*/

void test_binary_type_does_not_modify_input(void) {
    /* V2 spec: validate only, no bound clamping. The input array
     * must not be modified by the function. */
    char types[] = {'B', 'B', 'B'};
    int result = cxf_validate_vartypes(NULL, 3, types);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
    /* The array is const -- compiler enforces no modification */
    TEST_ASSERT_EQUAL_CHAR('B', types[0]);
    TEST_ASSERT_EQUAL_CHAR('B', types[1]);
    TEST_ASSERT_EQUAL_CHAR('B', types[2]);
}

/*============================================================================
 * Main
 *===========================================================================*/

int main(void) {
    UNITY_BEGIN();

    /* Edge cases */
    RUN_TEST(test_null_vartypes_returns_ok);
    RUN_TEST(test_zero_count_returns_ok);
    RUN_TEST(test_negative_count_returns_ok);

    /* Valid uppercase types */
    RUN_TEST(test_all_continuous);
    RUN_TEST(test_all_five_valid_types);
    RUN_TEST(test_single_binary);
    RUN_TEST(test_single_integer);
    RUN_TEST(test_single_semi_continuous);
    RUN_TEST(test_single_semi_integer);

    /* Case normalization (V2 spec) */
    RUN_TEST(test_lowercase_c_accepted);
    RUN_TEST(test_lowercase_b_accepted);
    RUN_TEST(test_lowercase_all_five_accepted);
    RUN_TEST(test_mixed_case_accepted);

    /* Invalid types */
    RUN_TEST(test_invalid_x_rejected);
    RUN_TEST(test_invalid_digit_rejected);
    RUN_TEST(test_invalid_space_rejected);
    RUN_TEST(test_invalid_null_char_rejected);
    RUN_TEST(test_early_exit_on_first_invalid);
    RUN_TEST(test_invalid_at_end);
    RUN_TEST(test_invalid_at_start);

    /* No side effects (regression guard) */
    RUN_TEST(test_binary_type_does_not_modify_input);

    return UNITY_END();
}
