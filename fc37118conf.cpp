/*
 * Fledge south plugin.

 * Copyright (c) 2022, RTE (http://www.rte-france.com)*

 * Released under the Apache 2.0 Licence
 *
 * Author: Benoit Jeanson <benoit.jeanson at rte-france.com>
 */

#include "fc37118conf.h"

FC37118Conf::FC37118Conf() : m_is_complete(false)
{
}

FC37118Conf::FC37118Conf(const std::string &json_config) {
    import_json(json_config);
}

FC37118Conf::~FC37118Conf()
{
}

void FC37118Conf::import_json(const std::string &json_config)
{
    m_is_complete = false;
    rapidjson::Document document;
    document.Parse(const_cast<char *>(json_config.c_str()));
    if (!document.IsObject())
        return;
 
    if (!document.HasMember(IP_ADDR) || !document[IP_ADDR].IsString())
        return;
    m_pmu_IP_addr = document[IP_ADDR].GetString();

    if (!document.HasMember(IP_PORT) || !document[IP_PORT].IsUint())
    {
        return;
    }
    m_pmu_IP_port = document[IP_PORT].GetUint();

    if (!document.HasMember(PMU_IDCODE) || !document[PMU_IDCODE].IsUint())
    {
        return;
    }
    m_pmu_IDCODE = document[PMU_IDCODE].GetUint();

    if (!document.HasMember(MY_IDCODE) || !document[MY_IDCODE].IsUint())
    {
        return;
    }
    m_my_IDCODE = document[MY_IDCODE].GetUint();

    if (!document.HasMember(RECONNECTION_DELAY) || !document[RECONNECTION_DELAY].IsUint())
    {
        return;
    }
    m_reconnection_delay = document[RECONNECTION_DELAY].GetUint();

    m_is_complete = true;
}
