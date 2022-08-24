/*
 * Fledge south plugin.

 * Copyright (c) 2022, RTE (http://www.rte-france.com)*

 * Released under the Apache 2.0 Licence
 *
 * Author: Benoit Jeanson <benoit.jeanson at rte-france.com>
 */

#include "fc37118.h"

#define DEBUG_LEVEL "debug"

FC37118::FC37118() : m_conf(new FC37118Conf),
                     m_cmd(new CMD_Frame()),
                     m_config_frame(new CONFIG_Frame()),
                     m_header(new HEADER_Frame("")),
                     m_data_frame(new DATA_Frame(m_config_frame)),
                     m_is_running(false),
                     m_sockfd(0)
{
    Logger::getLogger()->setMinLevel(DEBUG_LEVEL);
}

FC37118::~FC37118()
{
    Logger::getLogger()->info("shutting down ********************************************");
    if (m_is_running)
    {
        stop();
    }
}

void FC37118::start()
{
    Logger::getLogger()->setMinLevel(DEBUG_LEVEL);

    Logger::getLogger()->info("C37118 start");
    m_receiving_thread = new std::thread(&FC37118::m_receiveAndPushDatapoints, this);
    m_is_running = true;
}

void FC37118::stop()
{
    m_is_running = false;
    close(m_sockfd);
    if (m_receiving_thread != nullptr)
    {
        Logger::getLogger()->info("waiting receiving thread to stop");
        m_receiving_thread->join();
    }
    Logger::getLogger()->info("plugin stoped");
}

bool FC37118::m_terminate()
{
    return !m_is_running;
}

bool FC37118::set_conf(const std::string &conf)
{
    bool was_running = m_is_running;
    if (m_is_running)
    {
        Logger::getLogger()->info("Configuration change requested, stoping the plugin");
        stop();
    }
    m_conf->import_json(conf);
    if (!m_conf->is_complete())
    {
        Logger::getLogger()->error("Unable to ingest Plugin configuration");
        return false;
    }
    Logger::getLogger()->info("Plugin configuration successfully ingested");

    if (m_conf->is_request_config_to_pmu())
    {
        m_c37118_configuration_ready = false;
    }
    else
    {
        m_conf->to_conf_frame(m_config_frame);

        m_c37118_configuration_ready = true;

        m_log_configuration();
    }

    m_serv_addr.sin_family = AF_INET;
    m_serv_addr.sin_addr.s_addr = inet_addr(const_cast<char *>(m_conf->get_pmu_IP_addr().c_str()));
    m_serv_addr.sin_port = htons(m_conf->get_pmu_port());
    if (was_running)
    {
        Logger::getLogger()->info("restarting");
        start();
    }
    return true;
}

/**
 * @brief Establish or reestablish the connection with the c37.118 equipment. Keep trying to reconnect after timeout duration.
 * Throw run_time_error if unable to create a new socket
 *
 * @return true - the connection is established
 * @return false - the terminate signal was received
 */

bool FC37118::m_connect()
{
    Logger::getLogger()->info("Connecting to PMU");

    m_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_sockfd < 0)
    {
        Logger::getLogger()->fatal("FATAL error opening socket");
        throw std::runtime_error("could not initiate socket");
    }

    while (connect(m_sockfd, (struct sockaddr *)&m_serv_addr, sizeof(m_serv_addr)) != 0)
    {
        Logger::getLogger()->debug("Connection attempt in %i seconds", m_conf->get_reconnection_delay());
        sleep(m_conf->get_reconnection_delay());
        if (!m_is_running)
        {
            Logger::getLogger()->debug("terminate signal received during connection. Abort");
            return false;
        }
    }
    Logger::getLogger()->info("connected to PMU");
    return true;
}

/**
 * @brief send a C37.118 command
 *
 * @param cmd the command to be sent
 * @return true - the command was successfully sent
 * @return false - the terminate signal was received
 */
bool FC37118::m_send_cmd(int cmd)
{
    unsigned char *buffer_tx;
    m_cmd->CMD_set(cmd);
    unsigned short size = m_cmd->pack(&buffer_tx);
    int n;
    do
    {
        n = write(m_sockfd, buffer_tx, size);
        if (n < 0)
        {
            if (!m_connect())
            {
                return false;
            };
        }
    } while (n < 0 && !m_terminate());
    return n > 0;
}

/**
 * @brief Initiate the dialog with the PMU, log the header and retrieve the C37.118 configuration.
 * Once the configuration is retrieved, set configuration_ready_promise signal
 */
void FC37118::m_init_Pmu_Dialog()
{
    unsigned char *buffer_tx, buffer_rx[BUFFER_SIZE];

    Logger::getLogger()->debug("Start connection process");

    if (!m_connect())
    {
        return;
    }

    m_cmd->IDCODE_set(m_conf->get_my_IDCODE());

    // Request & receive Header Frame
    if (m_send_cmd(C37118_CMD_SEND_DATA))
    {
        int size = read(m_sockfd, buffer_rx, BUFFER_SIZE);
        if (size > 0)
        {
            m_header->unpack(buffer_rx);
            Logger::getLogger()->info("header from PMU: " + m_header->DATA_get());
        }
        else
        {
            Logger::getLogger()->warn("Unable to retrieve c37.118 header info");
            return;
        }
    }

    // Request & receive Config frame
    if (m_send_cmd(C37118_CMD_SEND_CONFIGURATION_2))
    {
        int size = read(m_sockfd, buffer_rx, BUFFER_SIZE);
        if (size > 0)
        {
            m_config_frame->unpack(buffer_rx);
            m_c37118_configuration_ready = true;
            Logger::getLogger()->info("c37.118 configuration retrieved");
        }
        else
        {
            Logger::getLogger()->error("could not retrieve c37.118 configuration");
            return;
        }
    }
}

/**
 * @brief Receive the real time data from the c37.118 equipment. Leaves once  Wait for the configuration to be ready before requesting data.
 */
void FC37118::m_receiveAndPushDatapoints()
{
    unsigned char buffer_rx[BUFFER_SIZE];
    int size;
    int k;

    if (!m_c37118_configuration_ready)
    {
        m_init_Pmu_Dialog();
    }

    m_log_configuration();

    if (!m_c37118_configuration_ready || m_terminate())
    {
        return;
    }

    Logger::getLogger()->debug("configuration OK, ready to receive real time data");
    m_send_cmd(C37118_CMD_TURNON_TX);

    do
    {
        size = read(m_sockfd, buffer_rx, BUFFER_SIZE);
        if (size > 0)
        {
            m_data_frame->unpack(buffer_rx);
            auto readings = m_dataframe_to_reading();
            for (auto reading : *readings)
                ingest(reading);
            if (m_terminate())
            {
                break;
            }
        }
        else
        {
            Logger::getLogger()->info("No connexion with PMU");
            if (m_connect())
            {
                m_send_cmd(C37118_CMD_TURNON_TX); // after a deconnection, shall request data
            };
        }
    } while (!m_terminate());
    Logger::getLogger()->debug("terminate signal received: stop receiving");
}

/**
 * Save the callback function and its data
 * @param data   The Ingest function data
 * @param cb     The callback function to call
 */
void FC37118::register_ingest(void *data, INGEST_CB cb)
{
    m_ingest = cb;
    m_data = data;
}

/**
 * Called when a data changed event is received. This calls back to the south service
 * and adds the points to the readings queue to send.
 *
 * @param points    The points in the reading we must create
 */
void FC37118::ingest(Reading &reading)
{
    (*m_ingest)(m_data, reading);
}
/**
 * @brief extended log of the c37.118 configuration
 *
 */
void FC37118::m_log_configuration()
{
    Logger::getLogger()->debug("Configuration Log:");

    Logger::getLogger()->debug("  TIME_BASE: %u", m_config_frame->TIME_BASE_get());
    Logger::getLogger()->debug("  NUM_PMU: %u", m_config_frame->NUM_PMU_get());
    Logger::getLogger()->debug("  DATA_RATE: %i", m_config_frame->DATA_RATE_get());

    for (auto pmu_station : m_config_frame->pmu_station_list)
    {
        Logger::getLogger()->debug("  pmuStation STN: " + pmu_station->STN_get());
        Logger::getLogger()->debug("    pmuStation IDCODE: %u", pmu_station->IDCODE_get());
        Logger::getLogger()->debug("    pmuStation FORMAT_COORD: %d", pmu_station->FORMAT_COORD_get());
        Logger::getLogger()->debug("    pmuStation FORMAT_PHASOR_TYPE: %d", pmu_station->FORMAT_PHASOR_TYPE_get());
        Logger::getLogger()->debug("    pmuStation FORMAT_ANALOG_TYPE: %d", pmu_station->FORMAT_ANALOG_TYPE_get());
        Logger::getLogger()->debug("    pmuStation FORMAT_FREQ_TYPE: %d", pmu_station->FORMAT_FREQ_TYPE_get());
        Logger::getLogger()->debug("    pmuStation FORMAT: %u", pmu_station->FORMAT_get());
        Logger::getLogger()->debug("    pmuStation PHNMR: %u", pmu_station->PHNMR_get());
        int count = 0;
        for (int i = 0; i < pmu_station->PHNMR_get(); i++)
        {
            Logger::getLogger()->debug("      pmuStation CHNAM PH %i: " + pmu_station->PH_NAME_get(i), count++);
            Logger::getLogger()->debug("      pmuStation PHUNIT: %u ", pmu_station->PHUNIT_get(i));
        }

        Logger::getLogger()->debug("    pmuStation ANNMR: %u", pmu_station->ANNMR_get());
        for (int i = 0; i < pmu_station->ANNMR_get(); i++)
        {
            Logger::getLogger()->debug("      pmuStation CHNAM AN %i: " + pmu_station->AN_NAME_get(i), count++);
            Logger::getLogger()->debug("      pmuStation ANUNIT: %u", pmu_station->ANUNIT_get(i));
        }

        Logger::getLogger()->debug("    pmuStation DGNMR: %u", pmu_station->DGNMR_get());
        for (int i = 0; i < pmu_station->DGNMR_get(); i++)
        {
            for (int j = 0; j < 16; j++)
            {
                Logger::getLogger()->debug("        pmuStation CHNAM DG %i: " + pmu_station->DG_NAME_get(j), count++);
            }
            Logger::getLogger()->debug("      pmuStation DGUNIT: %u ", pmu_station->DGUNIT_get(i));
        }
    }
}

/**
 * @brief Create a composed data point object
 *
 * @param name
 * @param dps
 * @param is_dict
 * @return Datapoint*
 */
Datapoint *create_composed_data_point(const std::string &name, vector<Datapoint *> dps, bool is_dict)
{
    // auto my_dps = new vector<Datapoint *>;
    auto my_dps = &dps;
    return new Datapoint(name, *new DatapointValue(my_dps, is_dict));
}

Datapoint *pmu_station_to_datapoint(PMU_Station *pmu_station)
{
    auto dp_IDCODE = new Datapoint(DP_IDCODE, *new DatapointValue((double)(pmu_station->IDCODE_get())));
    auto dp_STN = new Datapoint(DP_STN, *new DatapointValue(pmu_station->STN_get()));
    auto dp_id = create_composed_data_point(DP_ID, {dp_STN, dp_IDCODE}, true);

    auto dp_FREQ = new Datapoint(DP_FREQ, *new DatapointValue(pmu_station->FREQ_get()));
    auto dp_DFREQ = new Datapoint(DP_DFREQ, *new DatapointValue(pmu_station->DFREQ_get()));
    auto dp_frequency = create_composed_data_point(DP_FREQUENCY, {dp_FREQ, dp_DFREQ}, true);

    auto phasor_dps = new vector<Datapoint *>;
    for (int k = 0; k < pmu_station->PHNMR_get(); k++)
    {
        auto dp_mag = new Datapoint(DP_MAGNITUDE, *new DatapointValue(abs(pmu_station->PHASOR_VALUE_get(k))));
        auto dp_angle = new Datapoint(DP_ANGLE, *new DatapointValue(arg(pmu_station->PHASOR_VALUE_get(k))));
        phasor_dps->push_back(create_composed_data_point(pmu_station->PH_NAME_get(k), {dp_mag, dp_angle}, true));
    }
    auto dp_phasors = new Datapoint(DP_PHASORS, *new DatapointValue(phasor_dps, true));

    auto analog_dps = new vector<Datapoint *>;
    for (int k = 0; k < pmu_station->ANNMR_get(); k++)
    {
        analog_dps->push_back(new Datapoint(pmu_station->AN_NAME_get(k),
                                            *new DatapointValue(pmu_station->ANALOG_VALUE_get(k))));
    }
    auto dp_analogs = new Datapoint(DP_ANALOGS, *new DatapointValue(analog_dps, true));

    return create_composed_data_point(READING_PREFIX + to_string(pmu_station->IDCODE_get()), {dp_id, dp_frequency, dp_phasors, dp_analogs}, true);
}

/**
 * @brief transform the c37118 dataframe to fledge Reading
 *
 * @return Reading
 */

vector<Reading> *FC37118::m_dataframe_to_reading()
{
    auto readings = new vector<Reading>;
    auto dp_SOC = new Datapoint(DP_SOC, *new DatapointValue((long)(m_data_frame->SOC_get())));
    auto dp_FRACSEC = new Datapoint(DP_FRACSEC, *new DatapointValue((long)(m_data_frame->FRACSEC_get())));
    auto dp_TIME_BASE = new Datapoint(DP_TIME_BASE, *new DatapointValue((long)(m_config_frame->TIME_BASE_get())));
    auto dp_time = create_composed_data_point(DP_TIMESTAMP, {dp_SOC, dp_FRACSEC, dp_TIME_BASE}, true);

    auto pmu_dps = new vector<Datapoint *>;
    for (auto pmu_station : m_config_frame->pmu_station_list)
    {
        auto dp_pmu_station = pmu_station_to_datapoint(pmu_station);
        if (m_conf->is_split_stations())
        {
            auto dp_reading = create_composed_data_point("Single_PMU", {dp_time, dp_pmu_station}, true);
            readings->push_back(
                Reading(to_string(m_config_frame->IDCODE_get()) + "-" + to_string(pmu_station->IDCODE_get()),
                        dp_reading));
        }
        else
            pmu_dps->push_back(dp_pmu_station);
    }

    auto dp_pmu_stations = new Datapoint(DP_PMUSTATIONS, *new DatapointValue(pmu_dps, true));

    if (!m_conf->is_split_stations())
    {
        auto dp_reading = create_composed_data_point("Multi_PMU", {dp_time, dp_pmu_stations}, true);
        readings->push_back(Reading(to_string(m_config_frame->IDCODE_get()), dp_reading));
    }
    return readings;
}
