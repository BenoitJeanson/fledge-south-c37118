/*
 * Fledge south plugin.

 * Copyright (c) 2022, RTE (http://www.rte-france.com)*

 * Released under the Apache 2.0 Licence
 *
 * Author: Benoit Jeanson <benoit.jeanson at rte-france.com>
 */

#include "fc37118conf.h"

FC37118Conf::FC37118Conf() : m_is_complete(false),
                             m_request_config_to_pmu(false)
{
}

FC37118Conf::FC37118Conf(const std::string &json_config)
{
    import_json(json_config);
}

FC37118Conf::~FC37118Conf()
{
}

bool FC37118Conf::m_retrieve(rapidjson::Document *doc, const char *key, bool *target)
{
    if (!doc->HasMember(key) || !(*doc)[key].IsBool())
    {
        return false;
    }
    *target = (*doc)[key].GetBool();
    return true;
}

bool FC37118Conf::m_retrieve(rapidjson::Document *doc, const char *key, uint *target)
{
    if (!doc->HasMember(key) || !(*doc)[key].IsUint())
    {
        return false;
    }
    *target = (*doc)[key].GetUint();
    return true;
}

bool FC37118Conf::m_retrieve(rapidjson::Document *doc, const char *key, int *target)
{
    if (!doc->HasMember(key) || !(*doc)[key].IsInt())
    {
        return false;
    }
    *target = (*doc)[key].GetInt();
    return true;
}

bool FC37118Conf::m_retrieve(rapidjson::Document *doc, const char *key, std::string *target)
{
    if (!doc->HasMember(key) || !(*doc)[key].IsString())
    {
        return false;
    }
    *target = (*doc)[key].GetString();
    return true;
}

void FC37118Conf::import_json(const std::string &json_config)
{
    m_is_complete = false;

    bool is_complete = true;
    rapidjson::Document *doc = new rapidjson::Document;

    doc->Parse(const_cast<char *>(json_config.c_str()));
    if (!doc->IsObject())
        return;

    is_complete &= m_retrieve(doc, IP_ADDR, &m_pmu_IP_addr);
    is_complete &= m_retrieve(doc, IP_PORT, &m_pmu_IP_port);
    is_complete &= m_retrieve(doc, RECONNECTION_DELAY, &m_reconnection_delay);
    is_complete &= m_retrieve(doc, MY_IDCODE, &m_my_IDCODE);

    is_complete &= m_retrieve(doc, PMU_IDCODE, &m_pmu_IDCODE);
    is_complete &= m_retrieve(doc, REQUEST_CONFIG_TO_PMU, &m_request_config_to_pmu);
    if (m_request_config_to_pmu)
    {
        is_complete &= m_retrieve(doc, TIME_BASE, &m_time_base);
        is_complete &= m_retrieve(doc, NUM_PMU, &m_num_pmu);
        is_complete &= m_retrieve(doc, CFGCNT, &m_cfgcnt);
        is_complete &= m_retrieve(doc, DATA_RATE, &m_data_rate);
        
        for (int st = 0; st < m_num_pmu; ++st)
        {

        }
    }
    m_is_complete = is_complete;
}
