#pragma once

#define SWIT_WINDOW_CLASS L"SWiT.Agent.Window"
#define SWIT_TEST_MESSAGE_NAME L"SWiT.Agent.TestMessage"

enum SwitTestCommand : unsigned int {
    SWIT_TEST_PING = 1,
    SWIT_TEST_QUERY_SHUTDOWN = 2,
    SWIT_TEST_QUERY_RESTART = 3,
    SWIT_TEST_QUERY_LOGOFF = 4,
    SWIT_TEST_EXIT = 99,
};
