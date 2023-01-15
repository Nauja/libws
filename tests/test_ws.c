#include "test_config.h"

static void test_client_server(void **state)
{
    (void)(state);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_client_server)};
    return cmocka_run_group_tests(tests, NULL, NULL);
}