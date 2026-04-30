#include <unity.h>

#include "can_frame_types.h"
#include "drivers/mock_driver.h"

static unsigned long fakeMillis = 1000;
static unsigned long millis()
{
    return fakeMillis;
}

#include "plugin_engine.h"

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

void test_mux_mask_matches_full_byte_mux()
{
    installPlugin(R"JSON({
      "name":"full mux",
      "rules":[{
        "id":2047,
        "mux":3,
        "mux_mask":255,
        "ops":[
          {"type":"set_byte","byte":0,"val":3,"mask":255},
          {"type":"set_bit","bit":12,"val":1}
        ]
      }]
    })JSON");

    MockDriver driver;
    CanFrame wrongMux = {.id = 2047};
    wrongMux.data[0] = 0x83;
    TEST_ASSERT_FALSE(pluginProcessFrame(wrongMux, driver));
    TEST_ASSERT_EQUAL_size_t(0, driver.sent.size());

    CanFrame rightMux = {.id = 2047};
    rightMux.data[0] = 0x03;
    TEST_ASSERT_TRUE(pluginProcessFrame(rightMux, driver));
    TEST_ASSERT_EQUAL_size_t(1, driver.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x03, driver.sent[0].data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x10, driver.sent[0].data[1] & 0x10);
}

void test_bus_pin_matches_known_bus_and_allows_unknown_bus()
{
    installPlugin(R"JSON({
      "name":"bus pin",
      "rules":[{
        "id":1021,
        "bus":"VEH",
        "mux":0,
        "ops":[
          {"type":"set_byte","byte":0,"val":0,"mask":7},
          {"type":"set_bit","bit":38,"val":1}
        ]
      }]
    })JSON");

    MockDriver driver;

    CanFrame chFrame = {.id = 1021};
    chFrame.bus = CAN_BUS_CH;
    TEST_ASSERT_FALSE(pluginProcessFrame(chFrame, driver));
    TEST_ASSERT_EQUAL_size_t(0, driver.sent.size());

    CanFrame unknownFrame = {.id = 1021};
    unknownFrame.bus = CAN_BUS_ANY;
    TEST_ASSERT_TRUE(pluginProcessFrame(unknownFrame, driver));
    TEST_ASSERT_EQUAL_size_t(1, driver.sent.size());

    CanFrame vehFrame = {.id = 1021};
    vehFrame.bus = CAN_BUS_VEH;
    TEST_ASSERT_TRUE(pluginProcessFrame(vehFrame, driver));
    TEST_ASSERT_EQUAL_size_t(2, driver.sent.size());
}

void test_filter_ids_keep_sixteen_rule_ids_when_gtw_silent_is_disabled_without_key()
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
    TEST_ASSERT_EQUAL_UINT8(16, plugin.filterIdCount);
}

void test_gtw_silent_is_disabled_without_custom_key()
{
    PluginData plugin = {};
    TEST_ASSERT_TRUE(pluginParseJson(R"JSON({
      "name":"silent",
      "rules":[{
        "id":2047,
        "mux":3,
        "ops":[{"type":"emit_periodic","interval":100,"gtw_silent":true}]
      }]
    })JSON",
                                     plugin));

    TEST_ASSERT_EQUAL_UINT8(1, plugin.ruleCount);
    TEST_ASSERT_EQUAL_UINT8(1, plugin.rules[0].opCount);
    TEST_ASSERT_FALSE(plugin.rules[0].ops[0].gtwSilent);
    TEST_ASSERT_EQUAL_UINT8(1, plugin.filterIdCount);
    TEST_ASSERT_EQUAL_UINT32(2047, plugin.filterIds[0]);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_mux_mask_matches_full_byte_mux);
    RUN_TEST(test_bus_pin_matches_known_bus_and_allows_unknown_bus);
    RUN_TEST(test_filter_ids_keep_sixteen_rule_ids_when_gtw_silent_is_disabled_without_key);
    RUN_TEST(test_gtw_silent_is_disabled_without_custom_key);
    return UNITY_END();
}
