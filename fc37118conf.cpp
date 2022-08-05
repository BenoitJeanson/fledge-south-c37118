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
    bool is_complete = true;
    is_complete &= RETRIEVEJSON::retrieve(value, STN, &m_name);
    is_complete &= RETRIEVEJSON::retrieve(value, STN_IDCODE, &m_idcode);
    is_complete &= RETRIEVEJSON::retrieve(value, STN_FORMAT, &m_format);
    is_complete &= RETRIEVEJSON::retrieve(value, STN_PHNMR, &m_phnmr);
    is_complete &= RETRIEVEJSON::retrieve(value, STN_ANNMR, &m_anmr);
    is_complete &= RETRIEVEJSON::retrieve(value, STN_DGNMR, &m_dgnmr);
    is_complete &= RETRIEVEJSON::retrieve(value, STN_CHNAM, &m_chnam);
    is_complete &= RETRIEVEJSON::retrieve(value, STN_PHUNIT, &m_phunit);
    is_complete &= RETRIEVEJSON::retrieve(value, STN_ANUNIT, &m_anunit);
    is_complete &= RETRIEVEJSON::retrieve(value, STN_DIGUNIT, &m_digunit);
    is_complete &= RETRIEVEJSON::retrieve(value, STN_FNOM, &m_fnom);
    m_is_complete = is_complete;
    return m_is_complete;
}

void FC37118StnConf::to_PMU_station(PMU_Station *pmu_station)
{
    pmu_station->STN_set(m_name);
    pmu_station->IDCODE_set(m_idcode);
    pmu_station->FORMAT_set(m_format);
    pmu_station->PHNMR_set(m_phnmr);
    pmu_station->ANNMR_set(m_anmr);
    pmu_station->DGNMR_set(m_dgnmr);
    int count = 0;
    // TODO : clarify configuration for phasors and analogs
    for (int ph = 0; ph < m_phnmr; ph++)
    {
        pmu_station->PHASOR_add(m_chnam[count], STN_PHUNIT[ph]);
        count++;
    }
    for (int an = 0; an < m_anmr; an++)
    {
        pmu_station->ANALOG_add(m_chnam[count], STN_PHUNIT[an]);
        count++;
    }
    // TODO : handle digital
    // for (int dg = 0; dg<m_dgnmr; dg++){
    //     pmu_station->DIGITAL_add(m_chnam[count], STN_PHUNIT[dg]);
    //     count++;
    // }
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

    is_complete &= RETRIEVEJSON::retrieve(doc, IP_ADDR, &m_pmu_IP_addr);
    is_complete &= RETRIEVEJSON::retrieve(doc, IP_PORT, &m_pmu_IP_port);
    is_complete &= RETRIEVEJSON::retrieve(doc, RECONNECTION_DELAY, &m_reconnection_delay);
    is_complete &= RETRIEVEJSON::retrieve(doc, MY_IDCODE, &m_my_IDCODE);

    is_complete &= RETRIEVEJSON::retrieve(doc, PMU_IDCODE, &m_pmu_IDCODE);
    is_complete &= RETRIEVEJSON::retrieve(doc, REQUEST_CONFIG_TO_PMU, &m_request_config_to_pmu);
    if (m_request_config_to_pmu)
    {
        is_complete &= RETRIEVEJSON::retrieve(doc, TIME_BASE, &m_time_base);
        is_complete &= RETRIEVEJSON::retrieve(doc, NUM_PMU, &m_num_pmu);
        is_complete &= RETRIEVEJSON::retrieve(doc, CFGCNT, &m_cfgcnt);
        is_complete &= RETRIEVEJSON::retrieve(doc, DATA_RATE, &m_data_rate);

        if (!doc->HasMember(STNS) || !(*doc)[STNS].IsArray())
            return;
        else
        {
            for (auto &stn : (*doc)[STNS].GetArray())
            {
                if (!stn.IsObject())
                    return;
                auto stn_conf = new FC37118StnConf();
                if (!stn_conf->import(&stn))
                    return;
                m_stns.push_back(*stn_conf);
            }
        }
    }
    m_is_complete = is_complete;
}

void FC37118Conf::to_conf_frame(CONFIG_Frame *conf_frame)
{
    Logger::getLogger()->debug("enter to_conf_frame()");
    conf_frame->IDCODE_set(m_my_IDCODE);
    conf_frame->TIME_BASE_set(m_time_base);
    conf_frame->NUM_PMU_set(m_num_pmu);
    for (auto &stn : m_stns)
    {
        PMU_Station pmu_station;
        stn.to_PMU_station(&pmu_station);
    }
    Logger::getLogger()->debug("to_conf_frame() succeeded");
}
