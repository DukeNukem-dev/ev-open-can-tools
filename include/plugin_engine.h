#pragma once

#if defined(ESP32_DASHBOARD) && !defined(NATIVE_BUILD)

#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "can_frame_types.h"
#include "can_helpers.h"
#include "drivers/can_driver.h"

#define PLUGIN_MAX 8
#define PLUGIN_RULES_MAX 16
#define PLUGIN_OPS_MAX 8

enum PluginOpType : uint8_t
{
    OP_SET_BIT = 0,
    OP_SET_BYTE = 1,
    OP_OR_BYTE = 2,
    OP_AND_BYTE = 3,
    OP_CHECKSUM = 4,
};

struct PluginOp
{
    PluginOpType type;
    uint8_t index; // bit (0-63) or byte (0-7) index
    uint8_t value;
    uint8_t mask; // for SET_BYTE
};

struct PluginRule
{
    uint32_t canId;
    int8_t mux; // -1 = match any mux
    PluginOp ops[PLUGIN_OPS_MAX];
    uint8_t opCount;
    bool sendAfter;
};

struct PluginData
{
    char name[32];
    char version[16];
    char author[32];
    char filename[32];
    char sourceUrl[200];
    bool enabled;
    PluginRule rules[PLUGIN_RULES_MAX];
    uint8_t ruleCount;
    uint32_t filterIds[PLUGIN_RULES_MAX];
    uint8_t filterIdCount;
};

static PluginData pluginStore[PLUGIN_MAX];
static uint8_t pluginCount = 0;
static volatile bool pluginsLocked = false;

// ── JSON PARSING ────────────────────────────────────────────────

static bool pluginParseJson(const String &json, PluginData &out)
{
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err)
        return false;

    strlcpy(out.name, doc["name"] | "Unknown", sizeof(out.name));
    strlcpy(out.version, doc["version"] | "1.0", sizeof(out.version));
    strlcpy(out.author, doc["author"] | "", sizeof(out.author));
    out.enabled = true;
    out.ruleCount = 0;
    out.filterIdCount = 0;

    JsonArray rules = doc["rules"];
    if (!rules)
        return false;

    for (JsonObject rule : rules)
    {
        if (out.ruleCount >= PLUGIN_RULES_MAX)
            break;
        PluginRule &r = out.rules[out.ruleCount];
        r.canId = rule["id"] | (uint32_t)0;
        r.mux = rule["mux"] | (int)-1;
        r.sendAfter = rule["send"] | true;
        r.opCount = 0;

        JsonArray ops = rule["ops"];
        if (ops)
        {
            for (JsonObject op : ops)
            {
                if (r.opCount >= PLUGIN_OPS_MAX)
                    break;
                PluginOp &o = r.ops[r.opCount];
                const char *type = op["type"] | "";
                if (strcmp(type, "set_bit") == 0)
                {
                    o.type = OP_SET_BIT;
                    o.index = op["bit"] | (uint8_t)0;
                    o.value = op["val"] | (uint8_t)1;
                }
                else if (strcmp(type, "set_byte") == 0)
                {
                    o.type = OP_SET_BYTE;
                    o.index = op["byte"] | (uint8_t)0;
                    o.mask = op["mask"] | (uint8_t)0xFF;
                    o.value = op["val"] | (uint8_t)0;
                }
                else if (strcmp(type, "or_byte") == 0)
                {
                    o.type = OP_OR_BYTE;
                    o.index = op["byte"] | (uint8_t)0;
                    o.value = op["val"] | (uint8_t)0;
                }
                else if (strcmp(type, "and_byte") == 0)
                {
                    o.type = OP_AND_BYTE;
                    o.index = op["byte"] | (uint8_t)0;
                    o.value = op["val"] | (uint8_t)0xFF;
                }
                else if (strcmp(type, "checksum") == 0)
                {
                    o.type = OP_CHECKSUM;
                }
                else
                {
                    continue;
                }
                r.opCount++;
            }
        }

        // Deduplicate filter IDs
        bool found = false;
        for (uint8_t i = 0; i < out.filterIdCount; i++)
        {
            if (out.filterIds[i] == r.canId)
            {
                found = true;
                break;
            }
        }
        if (!found && out.filterIdCount < PLUGIN_RULES_MAX)
            out.filterIds[out.filterIdCount++] = r.canId;

        out.ruleCount++;
    }

    return out.ruleCount > 0;
}

// ── SPIFFS STORAGE ──────────────────────────────────────────────

static String pluginFilePath(const char *filename)
{
    return String("/p_") + filename;
}

static bool pluginSaveToSpiffs(const String &json, const char *filename)
{
    File f = SPIFFS.open(pluginFilePath(filename), "w");
    if (!f)
        return false;
    f.print(json);
    f.close();
    return true;
}

static void pluginLoadAll()
{
    pluginsLocked = true;
    pluginCount = 0;

    File root = SPIFFS.open("/");
    if (!root)
    {
        pluginsLocked = false;
        return;
    }

    File f = root.openNextFile();
    while (f && pluginCount < PLUGIN_MAX)
    {
        String name = f.name();
        // Normalize: some SPIFFS versions include leading /
        if (name.startsWith("/"))
            name = name.substring(1);

        if (name.startsWith("p_") && name.endsWith(".json"))
        {
            String json = f.readString();
            PluginData &p = pluginStore[pluginCount];
            if (pluginParseJson(json, p))
            {
                // Store just the user-facing filename (without /p_ prefix)
                String userFilename = name.substring(2); // remove "p_"
                strlcpy(p.filename, userFilename.c_str(), sizeof(p.filename));
                pluginCount++;
            }
        }
        f = root.openNextFile();
    }

    pluginsLocked = false;
}

static bool pluginRemove(uint8_t index)
{
    if (index >= pluginCount)
        return false;
    pluginsLocked = true;

    SPIFFS.remove(pluginFilePath(pluginStore[index].filename));

    for (uint8_t i = index; i < pluginCount - 1; i++)
        pluginStore[i] = pluginStore[i + 1];
    pluginCount--;

    pluginsLocked = false;
    return true;
}

static int pluginFindByName(const char *name)
{
    for (uint8_t i = 0; i < pluginCount; i++)
    {
        if (strcmp(pluginStore[i].name, name) == 0)
            return i;
    }
    return -1;
}

// ── RULE EXECUTION ──────────────────────────────────────────────

static void pluginApplyOp(CanFrame &frame, const PluginOp &op)
{
    switch (op.type)
    {
    case OP_SET_BIT:
        setBit(frame, op.index, op.value);
        break;
    case OP_SET_BYTE:
        if (op.index < 8)
            frame.data[op.index] = (frame.data[op.index] & ~op.mask) | (op.value & op.mask);
        break;
    case OP_OR_BYTE:
        if (op.index < 8)
            frame.data[op.index] |= op.value;
        break;
    case OP_AND_BYTE:
        if (op.index < 8)
            frame.data[op.index] &= op.value;
        break;
    case OP_CHECKSUM:
        frame.data[7] = computeVehicleChecksum(frame);
        break;
    }
}

static bool pluginProcessFrame(const CanFrame &original, CanDriver &driver)
{
    if (pluginsLocked || pluginCount == 0)
        return false;

    bool processed = false;
    for (uint8_t p = 0; p < pluginCount; p++)
    {
        if (!pluginStore[p].enabled)
            continue;
        for (uint8_t r = 0; r < pluginStore[p].ruleCount; r++)
        {
            const PluginRule &rule = pluginStore[p].rules[r];
            if (rule.canId != original.id)
                continue;

            if (rule.mux >= 0)
            {
                uint8_t frameMux = original.data[0] & 0x07;
                if (frameMux != (uint8_t)rule.mux)
                    continue;
            }

            CanFrame modified = original;
            for (uint8_t o = 0; o < rule.opCount; o++)
                pluginApplyOp(modified, rule.ops[o]);

            if (rule.sendAfter)
                driver.send(modified);

            processed = true;
        }
    }
    return processed;
}

// ── FILTER MERGING ──────────────────────────────────────────────

static uint8_t pluginGetFilterIds(uint32_t *ids, uint8_t maxIds)
{
    uint8_t count = 0;
    for (uint8_t p = 0; p < pluginCount; p++)
    {
        if (!pluginStore[p].enabled)
            continue;
        for (uint8_t i = 0; i < pluginStore[p].filterIdCount; i++)
        {
            if (count >= maxIds)
                return count;
            bool dup = false;
            for (uint8_t j = 0; j < count; j++)
            {
                if (ids[j] == pluginStore[p].filterIds[i])
                {
                    dup = true;
                    break;
                }
            }
            if (!dup)
                ids[count++] = pluginStore[p].filterIds[i];
        }
    }
    return count;
}

#endif
