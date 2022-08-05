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

// PLUGIN DEFAULT TLS CONF
#define PMU_CONF_LABEL "PMU"
#define PMU_CONF QUOTE({        \
        IP_ADDR : "127.0.0.1",  \
        IP_PORT : 1410,         \
        RECONNECTION_DELAY : 1, \
        PMU_IDCODE : 1,         \
        MY_IDCODE : 7           \
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

        "asset" : {
                "description" : "Asset name",
                "type" : "string",
                "default" : "C37.118",
                "order" : "1",
                "displayName" : "C37.118",
                "mandatory" : "true"
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

                if (config->itemExists("asset"))
                {
                        fc37118->set_asset_name(config->getValue("asset"));
                }
                else
                {
                        fc37118->set_asset_name("C37118");
                }
                Logger::getLogger()->info("m_assetName set to %s", fc37118->get_asset_name());

                return (PLUGIN_HANDLE)fc37118;
        }

        /**
         * Start the Async handling for the plugin
         */
        void plugin_start(PLUGIN_HANDLE *handle)
        {
                if (!handle)
                        return;

                Logger::getLogger()->info("Starting the plugin");

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
                if (conf.itemExists("asset"))
                        fc37118->set_asset_name(conf.getValue("asset"));

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
                auto *fc37118 = (FC37118 *)handle;
                delete fc37118;
        }
};
