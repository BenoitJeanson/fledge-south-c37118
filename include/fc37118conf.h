/*
 * Fledge south plugin.

 * Copyright (c) 2022, RTE (http://www.rte-france.com)*

 * Released under the Apache 2.0 Licence
 *
 * Author: Benoit Jeanson <benoit.jeanson at rte-france.com>
 */

#ifndef _F_C37118CONF_H
#define _F_C37118CONF_H

#include "rapidjson/document.h"
#include "logger.h"
#include "retrievejson.h"
#include "c37118configuration.h"

#include <vector>

#define IP_ADDR "IP_ADDR"
#define IP_PORT "IP_PORT"
#define RECONNECTION_DELAY "RECONNECTION_DELAY"
#define MY_IDCODE "MY_IDCODE"
#define STREAMSOURCE_IDCODE "STREAMSOURCE_IDCODE"
#define SPLIT_STATIONS "SPLIT_STATIONS"

#define REQUEST_CONFIG_TO_SENDER "REQUEST_CONFIG_TO_SENDER"
#define SENDER_HARD_CONFIG "SENDER_HARD_CONFIG"

#define TIME_BASE "TIME_BASE"

#define STATIONS "STATIONS"
#define STN "STN"
#define STN_IDCODE "STN_IDCODE"
#define STN_FORMAT "STN_FORMAT"

#define STN_PHNAM "PHNAM"
#define STN_ANNAM "ANNAM"
#define STN_DGNAM "DGNAM"

#define STN_PHUNIT "PHUNIT"
#define STN_ANUNIT "ANUNIT"
#define STN_DIGUNIT "DIGUNIT"
#define STN_FNOM "FNOM"
#define CFGCNT "CFGCNT"
#define DATA_RATE "DATA_RATE"

class FC37118StnConf
{
public:
    FC37118StnConf();
    ~FC37118StnConf();

    bool import(rapidjson::Value *value);
    bool get_is_complete() { return m_is_complete; }
    void to_PMU_station(PMU_Station *pmu_station);

private:
    bool m_is_complete;

    std::string m_name;
    uint m_idcode;
    uint m_format;
    std::vector<std::string> m_phnam;
    std::vector<std::string> m_annam;
    std::vector<std::string> m_dgnam;
    std::vector<uint> m_phunit;
    std::vector<uint> m_anunit;
    std::vector<uint> m_digunit;
    uint m_fnom;
    uint m_cfgcnt;
};

class FC37118Conf
{
public:
    FC37118Conf();
    FC37118Conf(const std::string &json_config);
    ~FC37118Conf();

    void import_json(const std::string &json_config);
    bool is_complete() { return m_is_complete; }
    bool is_split_stations() { return m_is_split_stations; }

    std::string get_pmu_IP_addr() { return m_pmu_IP_addr; }
    uint get_pmu_port() { return m_pmu_IP_port; }
    uint get_pmu_IDCODE() { return m_pmu_IDCODE; }
    uint get_my_IDCODE() { return m_my_IDCODE; }
    uint get_reconnection_delay() { return m_reconnection_delay; }

    /**
     * @brief if true, the plugin will request the configuration to the PMU
     * if false, the plugin will use the configuration set in PMU_HARD_CONFIG
     * Warning, the configuration shall be exactly the same as the one of the PMU
     *
     * @return true
     * @return false
     */
    bool is_request_config_to_pmu() { return m_request_config_to_pmu; }

    void to_conf_frame(CONFIG_Frame *conf_frame);

private:
    bool m_is_complete;
    bool m_is_split_stations;

    // connection parameters
    std::string m_pmu_IP_addr;
    uint m_pmu_IP_port;
    uint m_reconnection_delay;

    // c37.118 parameters
    uint m_my_IDCODE;
    uint m_pmu_IDCODE;

    bool m_request_config_to_pmu;

    uint m_time_base;
    int m_data_rate;
    std::vector<FC37118StnConf *> m_stns;
};

#endif
