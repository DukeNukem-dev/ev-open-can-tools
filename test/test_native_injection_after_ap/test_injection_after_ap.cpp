#include <unity.h>
#include "can_frame_types.h"
#include "can_helpers.h"
#include "drivers/mock_driver.h"
#include "handlers.h"

static MockDriver mock;

void setUp()
{
    mock.reset();
    enhancedAutopilotRuntime = true;
}

void tearDown() {}

static CanFrame hw3Mux1Frame()
{
    CanFrame f = {.id = 1021};
    f.data[0] = 0x01;
    setBit(f, 19, true);
    return f;
}

static CanFrame hw4Mux1Frame()
{
    CanFrame f = {.id = 1021};
    f.data[0] = 0x01;
    setBit(f, 19, true);
    return f;
}

static void activateAp(CarManagerBase &handler)
{
    CanFrame f = {.id = 921};
    f.data[0] = 0x03; // ACTIVE_1
    handler.handleMessage(f, mock);
    TEST_ASSERT_TRUE(handler.APActive);
    mock.reset();
}

void test_hw3_enhanced_autopilot_waits_for_ap_before_mux1_injection()
{
    HW3Handler handler;
    handler.enablePrint = false;

    CanFrame beforeAp = hw3Mux1Frame();
    handler.handleMessage(beforeAp, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());

    CanFrame observedUiConfig = {.id = 1021};
    observedUiConfig.data[0] = 0x00;
    observedUiConfig.data[4] = 0x20;
    handler.handleMessage(observedUiConfig, mock);
    TEST_ASSERT_TRUE(handler.ADEnabled);
    TEST_ASSERT_FALSE(handler.APActive);
    mock.reset();

    CanFrame stillBeforeAp = hw3Mux1Frame();
    handler.handleMessage(stillBeforeAp, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());

    activateAp(handler);

    CanFrame afterAp = hw3Mux1Frame();
    handler.handleMessage(afterAp, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_FALSE((mock.sent[0].data[2] >> 3) & 0x01);
    TEST_ASSERT_EQUAL_HEX8(0x40, mock.sent[0].data[5] & 0x40);
}

void test_hw4_enhanced_autopilot_waits_for_ap_before_mux1_injection()
{
    HW4Handler handler;
    handler.enablePrint = false;

    CanFrame beforeAp = hw4Mux1Frame();
    handler.handleMessage(beforeAp, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());

    CanFrame observedUiConfig = {.id = 1021};
    observedUiConfig.data[0] = 0x00;
    observedUiConfig.data[4] = 0x20;
    handler.handleMessage(observedUiConfig, mock);
    TEST_ASSERT_TRUE(handler.ADEnabled);
    TEST_ASSERT_FALSE(handler.APActive);
    mock.reset();

    CanFrame stillBeforeAp = hw4Mux1Frame();
    handler.handleMessage(stillBeforeAp, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());

    activateAp(handler);

    CanFrame afterAp = hw4Mux1Frame();
    handler.handleMessage(afterAp, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_FALSE((mock.sent[0].data[2] >> 3) & 0x01);
    TEST_ASSERT_EQUAL_HEX8(0x80, mock.sent[0].data[5] & 0x80);
}

int main()
{
    UNITY_BEGIN();

    RUN_TEST(test_hw3_enhanced_autopilot_waits_for_ap_before_mux1_injection);
    RUN_TEST(test_hw4_enhanced_autopilot_waits_for_ap_before_mux1_injection);

    return UNITY_END();
}
