/*
 * Fledge south plugin.

 * Copyright (c) 2022, RTE (http://www.rte-france.com)*

 * Released under the Apache 2.0 Licence
 *
 * Author: Benoit Jeanson <benoit.jeanson at rte-france.com>
 */

#include "fc37118conf.h"

FC37118StnConf::FC37118StnConf() {}
FC37118StnConf::~FC37118StnConf() {}

bool FC37118StnConf::import(rapidjson::Value *value)
{
    Logger::getLogger()->setMinLevel("debug");

    bool is_complete = true;
    is_complete &= retrieve(value, STN, &m_name);
    is_complete &= retrieve(value, STN_IDCODE, &m_idcode);
    is_complete &= retrieve(value, STN_FORMAT, &m_format);
    is_complete &= retrieve(value, STN_PHNAM, &m_phnam);
    is_complete &= retrieve(value, STN_PHUNIT, &m_phunit);
    is_complete &= retrieve(value, STN_ANNAM, &m_annam);
    is_complete &= retrieve(value, STN_ANUNIT, &m_anunit);
    is_complete &= retrieve(value, STN_DGNAM, &m_dgnam);
    is_complete &= retrieve(value, STN_DIGUNIT, &m_digunit);
    is_complete &= retrieve(value, STN_FNOM, &m_fnom);
    is_complete &= retrieve(value, CFGCNT, &m_cfgcnt);
    m_is_complete = is_complete;
    return m_is_complete;
}

void FC37118StnConf::to_PMU_station(PMU_Station *pmu_station)
{
    pmu_station->STN_set(m_name);
    pmu_station->IDCODE_set(m_idcode);
    pmu_station->FORMAT_set(m_format);
    for (int ph = 0; ph < m_phnam.size(); ph++)
    {
        pmu_station->PHASOR_add(m_phnam[ph], m_phunit[ph]);
    }

    for (int an = 0; an < m_annam.size(); an++)
    {
        pmu_station->ANALOG_add(m_annam[an], m_anunit[an]);
    }

    pmu_station->DIGITAL_add(m_dgnam, 0, 65535);
}

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

void FC37118Conf::import_json(const std::string &json_config)
{
    m_is_complete = false;

    bool is_complete = true;
    rapidjson::Document *doc = new rapidjson::Document;

    doc->Parse(const_cast<char *>(json_config.c_str()));
    if (!doc->IsObject())
        return;

    is_complete &= retrieve(doc, IP_ADDR, &m_pmu_IP_addr);
    is_complete &= retrieve(doc, IP_PORT, &m_pmu_IP_port);
    is_complete &= retrieve(doc, RECONNECTION_DELAY, &m_reconnection_delay);
    is_complete &= retrieve(doc, MY_IDCODE, &m_my_IDCODE);

    is_complete &= retrieve(doc, STREAMSOURCE_IDCODE, &m_pmu_IDCODE);
    is_complete &= retrieve(doc, SPLIT_STATIONS, &m_is_split_stations);
    is_complete &= retrieve(doc, REQUEST_CONFIG_TO_SENDER, &m_request_config_to_pmu);

    if (!m_request_config_to_pmu)
    {
        rapidjson::Value *pmu_hard_conf;
        if (!retrieve(doc, SENDER_HARD_CONFIG, pmu_hard_conf))
            return;
        if (!pmu_hard_conf->IsObject())
            return;

        is_complete &= retrieve(pmu_hard_conf, TIME_BASE, &m_time_base);
        is_complete &= retrieve(pmu_hard_conf, DATA_RATE, &m_data_rate);

        if (!pmu_hard_conf->HasMember(STATIONS) || !(*pmu_hard_conf)[STATIONS].IsArray())
            return;
        else
        {
            for (auto &stn : (*pmu_hard_conf)[STATIONS].GetArray())
            {
                if (!stn.IsObject())
                    return;
                auto stn_conf = new FC37118StnConf();
                if (!stn_conf->import(&stn))
                    return;
                m_stns.push_back(stn_conf);
            }
        }
    }
    Logger::getLogger()->debug("import_json() succeeded");
    m_is_complete = is_complete;
}

void FC37118Conf::to_conf_frame(CONFIG_Frame *conf_frame)
{
    conf_frame->IDCODE_set(m_my_IDCODE);
    conf_frame->TIME_BASE_set(m_time_base);
    //    conf_frame->NUM_PMU_set(m_num_pmu);
    conf_frame->DATA_RATE_set(m_data_rate);
    for (auto stn : m_stns)
    {
        auto pmu_station = new PMU_Station();
        stn->to_PMU_station(pmu_station);
        conf_frame->PMUSTATION_ADD(pmu_station);
    }
    Logger::getLogger()->debug("to_conf_frame() succeeded");
}
