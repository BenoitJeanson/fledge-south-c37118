/*
 * Fledge south plugin.

 * Copyright (c) 2022, RTE (http://www.rte-france.com)*

 * Released under the Apache 2.0 Licence
 *
 * Author: Benoit Jeanson <benoit.jeanson at rte-france.com>
 */

#ifndef _F_C37118conf_H
#define _F_C37118conf_H

#include "rapidjson/document.h"
#include "logger.h"

#define IP_ADDR "IP_ADDR"
#define IP_PORT "IP_PORT"
#define RECONNECTION_DELAY "RECONNECTION_DELAY"
#define MY_IDCODE "MY_IDCODE"
#define PMU_IDCODE "PMU_IDCODE"

#define REQUEST_CONFIG_TO_PMU "REQUEST_CONFIG_TO_PMU"

#define TIME_BASE "TIME_BASE"
#define NUM_PMU "NUM_PMU"

#define STNS "STNS"
#define STN "STN"
#define STN_IDCODE "STN_IDCODE"
#define STN_FORMAT "STN_FORMAT"
#define STN_PHNMR "PHNMR"
#define STN_ANNMR "ANNMR"
#define STN_DGNMR "DGNMR"

#define STN_CHNAMS "CHNAMS"
#define STN_CHNAM "CHNAM"
#define STN_PHUNIT "PHUNIT"
#define STN_ANUNIT "ANUNIT"
#define STN_DIGUNIT "DIGUNIT"
#define STN_FNOM "FNOM"
#define CFGCNT "CFGCNT"
#define DATA_RATE "DATA_RATE"

class FC37118Conf
{
public:
    FC37118Conf();
    FC37118Conf(const std::string &json_config);
    ~FC37118Conf();
    void import_json(const std::string &json_config);
    bool is_complete() { return m_is_complete; }
    std::string get_pmu_IP_addr() { return m_pmu_IP_addr; }
    uint get_pmu_port() { return m_pmu_IP_port; }
    uint get_pmu_IDCODE() { return m_pmu_IDCODE; }
    uint get_my_IDCODE() { return m_my_IDCODE; }
    uint get_reconnection_delay() { return m_reconnection_delay; }

private:
    std::string m_pmu_IP_addr;
    uint m_pmu_IP_port;
    uint m_my_IDCODE;
    uint m_pmu_IDCODE;
    bool m_is_complete;
    uint m_reconnection_delay;

    // bool m_retrieve_uint(rapidjson::Document doc, const char * key, int* target);
    bool m_retrieve_uint(rapidjson::Document *doc, const char * key, uint* target);
    bool m_retrieve_string(rapidjson::Document *doc, const char * key, std::string* target);
};

#endif
