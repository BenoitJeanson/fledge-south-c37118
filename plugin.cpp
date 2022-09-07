/*
 * Fledge south plugin.

 * Copyright (c) 2022, RTE (http://www.rte-france.com)*

 * Released under the Apache 2.0 Licence
 *
 * Author: Benoit Jeanson <benoit.jeanson at rte-france.com>
 */
#include <plugin_api.h>
#include <logger.h>
#include <plugin_exception.h>
#include <config_category.h>
#include <rapidjson/document.h>
#include <version.h>
#include "fc37118.h"
#include "fc37118conf.h"
#include "reading.h"
#include "logger.h"

using namespace std;
#define PLUGIN_NAME "c37118"

#define PMU_CONF_LABEL "PMU"

#define PMU_CONF QUOTE({                                \
    IP_ADDR : "127.0.0.1",                              \
    IP_PORT : 1410,                                     \
    RECONNECTION_DELAY : 1,                             \
    MY_IDCODE : 7,                                      \
    STREAMSOURCE_IDCODE : 2,                            \
    SPLIT_STATIONS : true,                              \
    STN_IDCODES_FILTER : [],                           \
    REQUEST_CONFIG_TO_SENDER : true,                    \
    SENDER_HARD_CONFIG : {                              \
        TIME_BASE : 1000000,                            \
        STATIONS : [                                    \
            {                                           \
                STN : "RANDOM STATION 1",               \
                STN_IDCODE : 5,                         \
                STN_FORMAT : 15,                        \
                STN_PHNAM : [ "VA", "VB", "VC" ],       \
                STN_ANNAM : ["ANALOG"],                 \
                STN_DGNAM :                             \
                    [ "DG1", "DG2", "DG3", "DG4",       \
                      "DG5", "DG6", "DG7", "DG8",       \
                      "DG9", "DG10", "DG11", "DG12",    \
                      "DG13", "DG14", "DG15", "DG16" ], \
                STN_PHUNIT : [ 0, 0, 0 ],               \
                STN_ANUNIT : [0],                       \
                STN_DIGUNIT : [65535],                  \
                STN_FNOM : 1,                           \
                CFGCNT : 1                              \
            },                                          \
            {                                           \
                STN : "random station 2",               \
                STN_IDCODE : 7,                         \
                STN_FORMAT : 15,                        \
                STN_PHNAM : [ "va", "vb", "vc" ],       \
                STN_ANNAM : ["analog1"],                \
                STN_DGNAM :                             \
                    [ "dg1", "dg2", "dg3", "dg4",       \
                      "dg5", "dg6", "dg7", "dg8",       \
                      "dg9", "dg10", "dg11", "dg12",    \
                      "dg13", "dg14", "dg15", "dg16" ], \
                STN_PHUNIT : [ 0, 0, 0 ],               \
                STN_ANUNIT : [0],                       \
                STN_DIGUNIT : [65535],                  \
                STN_FNOM : 1,                           \
                CFGCNT : 1                              \
            }                                           \
        ],                                              \
        DATA_RATE : 30                                  \
    }                                                   \
})

/**
 * Default configuration
 */

const static char *default_config = QUOTE({
    "plugin" : {
        "description" : "IEEE C37.118 south plugin",
        "type" : "string",
        "default" : PLUGIN_NAME,
        "readonly" : "true"
    },
    PMU_CONF_LABEL : {
        "description" : "PMU configuration",
        "type" : "JSON",
        "default" : PMU_CONF,
        "order" : "2",
        "displayName" : PMU_CONF_LABEL,
        "mandatory" : "true"
    }
});

/**
 * The c37118 plugin interface
 */
extern "C"
{

    /**
     * The plugin information structure
     */
    static PLUGIN_INFORMATION info = {
        PLUGIN_NAME,       // Name
        VERSION,           // Version
        SP_ASYNC,          // Flags
        PLUGIN_TYPE_SOUTH, // Type
        "1.0.0",           // Interface version
        default_config     // Default configuration
    };

    /**
     * Return the information about this plugin
     */
    PLUGIN_INFORMATION *plugin_info()
    {
        return &info;
    }

    /**
     * Initialise the plugin, called to get the plugin handle
     */
    PLUGIN_HANDLE plugin_init(ConfigCategory *config)
    {
        if (!config->itemExists(PMU_CONF_LABEL))
            return nullptr;
        FC37118 *fc37118 = new FC37118();
        if (!fc37118->set_conf(config->getValue(PMU_CONF_LABEL)))
            return nullptr;
        return (PLUGIN_HANDLE)fc37118;
    }

    /**
     * Start the Async handling for the plugin
     */
    void plugin_start(PLUGIN_HANDLE *handle)
    {
        if (!handle)
            return;

        auto *fc37118 = (FC37118 *)handle;
        fc37118->start();
    }

    /**
     * Register ingest callback
     */
    void plugin_register_ingest(PLUGIN_HANDLE *handle, INGEST_CB cb, void *data)
    {
        if (!handle)
            throw new exception();

        auto *fc37118 = (FC37118 *)handle;
        fc37118->register_ingest(data, cb);
    }
    /**
     * Poll for a plugin reading
     */
    Reading plugin_poll(PLUGIN_HANDLE *handle)
    {
        throw runtime_error("C37-118 is an async plugin, poll should not be called");
    }

    /**
     * Reconfigure the plugin
     */
    void plugin_reconfigure(PLUGIN_HANDLE *handle, string &newConfig)
    {
        ConfigCategory conf("dht", newConfig);
        auto *fc37118 = (FC37118 *)*handle;

        if (conf.itemExists(PMU_CONF_LABEL))
        {
            fc37118->set_conf(conf.getValue(PMU_CONF_LABEL));
        }
    }

    /**
     * plugin plugin_write entry point
     * NOT USED
     */
    bool plugin_write(PLUGIN_HANDLE *handle, string &name, string &value)
    {
        return false;
    }

    /**
     * Shutdown the plugin
     */
    void plugin_shutdown(PLUGIN_HANDLE *handle)
    {
        if (!handle)
            return;

        auto *fc37118 = (FC37118 *)handle;
        fc37118->stop();
        delete fc37118;
    }
};
