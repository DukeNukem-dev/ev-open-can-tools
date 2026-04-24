#include <unity.h>

#include "can_frame_types.h"
#include "drivers/mock_driver.h"

static unsigned long fakeMillis = 1000;
static unsigned long millis()
{
    return fakeMillis;
}

#include "plugin_engine.h"

bool pluginGtwUdsComputeKey(const uint8_t *seed, uint8_t seedLen, uint8_t *outKey, uint8_t &outLen)
{
    if (seedLen == 0 || seedLen > PLUGIN_GTW_UDS_SEED_MAX)
        return false;
    for (uint8_t i = 0; i < seedLen; i++)
        outKey[i] = seed[i];
    outLen = seedLen;
    return true;
}

void setUp()
{
    pluginCount = 0;
    pluginsLocked = false;
    pluginResetPeriodicEmit();
    fakeMillis = 1000;
}

void tearDown() {}

static void installPlugin(const char *json)
{
    PluginData plugin = {};
    TEST_ASSERT_TRUE(pluginParseJson(json, plugin));
    TEST_ASSERT_TRUE(pluginInsert(pluginCount, plugin));
}

void test_filter_ids_keep_sixteen_rule_ids_plus_uds_ids_when_custom_key_available()
{
    PluginData plugin = {};
    const char *json = R"JSON({
      "name":"filters",
      "rules":[
        {"id":100,"ops":[{"type":"set_bit","bit":0,"val":1}]},
        {"id":101,"ops":[{"type":"set_bit","bit":0,"val":1}]},
        {"id":102,"ops":[{"type":"set_bit","bit":0,"val":1}]},
        {"id":103,"ops":[{"type":"set_bit","bit":0,"val":1}]},
        {"id":104,"ops":[{"type":"set_bit","bit":0,"val":1}]},
        {"id":105,"ops":[{"type":"set_bit","bit":0,"val":1}]},
        {"id":106,"ops":[{"type":"set_bit","bit":0,"val":1}]},
        {"id":107,"ops":[{"type":"set_bit","bit":0,"val":1}]},
        {"id":108,"ops":[{"type":"set_bit","bit":0,"val":1}]},
        {"id":109,"ops":[{"type":"set_bit","bit":0,"val":1}]},
        {"id":110,"ops":[{"type":"set_bit","bit":0,"val":1}]},
        {"id":111,"ops":[{"type":"set_bit","bit":0,"val":1}]},
        {"id":112,"ops":[{"type":"set_bit","bit":0,"val":1}]},
        {"id":113,"ops":[{"type":"set_bit","bit":0,"val":1}]},
        {"id":114,"ops":[{"type":"set_bit","bit":0,"val":1}]},
        {"id":2047,"mux":3,"ops":[{"type":"emit_periodic","interval":100,"gtw_silent":true}]}
      ]
    })JSON";

    TEST_ASSERT_TRUE(pluginParseJson(json, plugin));
    TEST_ASSERT_EQUAL_UINT8(16, plugin.ruleCount);
    TEST_ASSERT_EQUAL_UINT8(18, plugin.filterIdCount);

    bool sawUdsRequest = false;
    bool sawUdsResponse = false;
    bool sawGtw = false;
    for (uint8_t i = 0; i < plugin.filterIdCount; i++)
    {
        sawUdsRequest |= plugin.filterIds[i] == PLUGIN_GTW_UDS_REQUEST_ID;
        sawUdsResponse |= plugin.filterIds[i] == PLUGIN_GTW_UDS_RESPONSE_ID;
        sawGtw |= plugin.filterIds[i] == 2047;
    }
    TEST_ASSERT_TRUE(sawUdsRequest);
    TEST_ASSERT_TRUE(sawUdsResponse);
    TEST_ASSERT_TRUE(sawGtw);
}

void test_gtw_silent_uds_requests_use_cached_frame_bus()
{
    installPlugin(R"JSON({
      "name":"silent bus",
      "rules":[{
        "id":2047,
        "bus":"VEH",
        "mux":3,
        "ops":[{"type":"emit_periodic","interval":100,"gtw_silent":true}]
      }]
    })JSON");

    MockDriver driver;
    CanFrame gtw = {.id = 2047};
    gtw.bus = CAN_BUS_VEH;
    gtw.data[0] = 3;
    TEST_ASSERT_TRUE(pluginProcessFrame(gtw, driver));
    TEST_ASSERT_EQUAL_size_t(0, driver.sent.size());

    pluginEmitPeriodicTick(driver, fakeMillis);
    TEST_ASSERT_EQUAL_size_t(1, driver.sent.size());
    TEST_ASSERT_EQUAL_UINT32(PLUGIN_GTW_UDS_REQUEST_ID, driver.sent[0].id);
    TEST_ASSERT_EQUAL_UINT8(CAN_BUS_VEH, driver.sent[0].bus);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_filter_ids_keep_sixteen_rule_ids_plus_uds_ids_when_custom_key_available);
    RUN_TEST(test_gtw_silent_uds_requests_use_cached_frame_bus);
    return UNITY_END();
}
