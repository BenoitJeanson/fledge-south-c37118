/*
 * Fledge south plugin.

 * Copyright (c) 2022, RTE (http://www.rte-france.com)*

 * Released under the Apache 2.0 Licence
 *
 * Author: Benoit Jeanson <benoit.jeanson at rte-france.com>
 */

#include "fc37118.h"

#define DEBUG_LEVEL "debug"

FC37118::FC37118() : m_conf(nullptr),
                     m_config_frame(nullptr),
                     m_data_frame(nullptr),
                     m_is_running(false),
                     m_sockfd(0)
{
    Logger::getLogger()->setMinLevel(DEBUG_LEVEL);
}

FC37118::~FC37118()
{
    Logger::getLogger()->info("shutting down");
    if (m_is_running)
    {
        stop();
    }
    delete m_config_frame;
    delete m_data_frame;
    delete m_conf;
}

void FC37118::start()
{
    Logger::getLogger()->setMinLevel(DEBUG_LEVEL);

    Logger::getLogger()->info("Start");
    m_is_running = true;
    m_receiving_thread = new std::thread(&FC37118::m_receiveAndPushDatapoints, this);
}

void FC37118::stop()
{
    m_is_running = false;
    close(m_sockfd);
    if (m_receiving_thread != nullptr)
    {
        Logger::getLogger()->info("waiting receiving thread to stop");
        m_receiving_thread->join();
        delete m_receiving_thread;
    }
    sleep(2);
    Logger::getLogger()->info("Stoped");
}

bool FC37118::m_terminate()
{
    return !m_is_running;
}

void FC37118::m_init_c37118()
{
    delete m_data_frame;
    delete m_config_frame;
    m_config_frame = new CONFIG_Frame();
    m_data_frame = new DATA_Frame(m_config_frame);
}

bool FC37118::set_conf(const std::string &conf)
{
    bool was_running = m_is_running;
    if (m_is_running)
    {
        Logger::getLogger()->info("Configuration change requested, stoping the plugin");
        stop();
    }
    delete m_conf;
    m_conf = new FC37118Conf();
    m_conf->import_json(conf);
    if (!m_conf->is_complete())
    {
        Logger::getLogger()->error("Unable to ingest Plugin configuration");
        return false;
    }
    Logger::getLogger()->debug("Plugin configuration successfully ingested");

    if (m_conf->is_request_config_to_pmu())
    {
        m_c37118_configuration_ready = false;
    }
    else
    {
        m_init_c37118();
        m_conf->to_conf_frame(m_config_frame);

        m_c37118_configuration_ready = true;

        m_log_configuration();
    }

    m_serv_addr.sin_family = AF_INET;
    m_serv_addr.sin_addr.s_addr = inet_addr(const_cast<char *>(m_conf->get_pmu_IP_addr().c_str()));
    m_serv_addr.sin_port = htons(m_conf->get_pmu_port());
    if (was_running)
    {
        Logger::getLogger()->info("Restarting");
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
bool FC37118::m_send_cmd(unsigned short cmd)
{
    unsigned char *buffer_tx;
    m_cmd.CMD_set(cmd);
    m_cmd.SOC_set((unsigned long)time(NULL));
    m_cmd.FRACSEC_set(0); // no need to be finer than the second for commands
    unsigned short size = m_cmd.pack(&buffer_tx);
    int n = write(m_sockfd, buffer_tx, size);
    return n > 0;
}

/**
 * @brief Initiate the dialog with the PMU, log the header and retrieve the C37.118 configuration.
 * Once the configuration is retrieved, set configuration_ready_promise signal
 */
void FC37118::m_init_Pmu_Dialog()
{
    unsigned char *buffer_tx, buffer_rx[BUFFER_SIZE];

    Logger::getLogger()->debug("Start PMU dialog");

    m_c37118_configuration_ready = false;

    m_cmd.IDCODE_set(m_conf->get_my_IDCODE());

    Logger::getLogger()->debug("Send HDR");

    // Request & receive Header Frame
    if (m_send_cmd(C37118_CMD_SEND_HDR))
    {
        Logger::getLogger()->debug("HDR sent");
        int size = read(m_sockfd, buffer_rx, BUFFER_SIZE);
        if (size > 0)
        {
            HEADER_Frame header("");
            header.unpack(buffer_rx);
            Logger::getLogger()->info("header from PMU: " + header.DATA_get());
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
            m_init_c37118();
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

bool FC37118::m_init_receiving()
{
    if (!m_connect())
    {
        return false;
    }

    if (m_conf->is_request_config_to_pmu())
    {
        m_init_Pmu_Dialog();
    }

    if (!m_c37118_configuration_ready || m_terminate())
    {
        return false;
    }

    m_log_configuration();

    return m_send_cmd(C37118_CMD_TURNON_TX);
}

/**
 * @brief Receive the real time data from the c37.118 equipment. Leaves once  Wait for the configuration to be ready before requesting data.
 */
void FC37118::m_receiveAndPushDatapoints()
{
    unsigned char buffer_rx[BUFFER_SIZE];
    int size;
    int k;
    bool init_ok = false;

    while (!m_terminate())
    {
        while (!init_ok && !m_terminate())
        {
            init_ok = m_init_receiving();
            if (init_ok)
                Logger::getLogger()->debug("Connection and configuration OK, ready to receive real time data");
        }
        if (m_terminate())
            break;

        size = read(m_sockfd, buffer_rx, BUFFER_SIZE);
        if (size > 0)
        {
            m_data_frame->unpack(buffer_rx);
            for (auto reading : m_dataframe_to_reading())
            {
                ingest(*reading);
                delete reading;
            }
        }
        else
        {
            Logger::getLogger()->info("Connection lost, reconnect");
            init_ok = false;
        }
    }
    Logger::getLogger()->debug("Terminate signal received: stop receiving");
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

    Logger::getLogger()->debug("  STREAMSOURCE_IDCODE: %u", m_config_frame->IDCODE_get());

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

template <typename T>
Datapoint *create_dp(const std::string &name, const T &value)
{
    auto dpv = DatapointValue(value);
    return new Datapoint(name, dpv);
}

/**
 * @brief Create a composed data point object
 *
 * @param name
 * @param dps a Datapoint vector
 * @param is_dict true: is a dict, false: is a list
 * @return Datapoint*
 */
Datapoint *create_dp_list(const std::string &name, std::vector<Datapoint *> *dps, bool is_dict)
{
    auto dpv = DatapointValue(dps, is_dict);

    return new Datapoint(name, dpv);
}

Datapoint *FC37118::m_pmu_station_to_datapoint(PMU_Station *pmu_station)
{
    auto dp_IDCODE = create_dp(DP_IDCODE, (double)(pmu_station->IDCODE_get()));
    auto dp_STN = create_dp(DP_STN, pmu_station->STN_get());
    auto dp_id = create_dp_list(DP_ID, new std::vector<Datapoint *>({dp_STN, dp_IDCODE}), true);

    auto dp_FREQ = create_dp(DP_FREQ, pmu_station->FREQ_get());
    auto dp_DFREQ = create_dp(DP_DFREQ, pmu_station->DFREQ_get());
    auto dp_frequency = create_dp_list(DP_FREQUENCY, new std::vector<Datapoint *>({dp_FREQ, dp_DFREQ}), true);

    auto phasor_dps = new std::vector<Datapoint *>;
    for (int k = 0; k < pmu_station->PHNMR_get(); k++)
    {
        auto dp_mag = create_dp(DP_MAGNITUDE, abs(pmu_station->PHASOR_VALUE_get(k)));
        auto dp_angle = create_dp(DP_ANGLE, arg(pmu_station->PHASOR_VALUE_get(k)));
        auto dp_val = create_dp_list(pmu_station->PH_NAME_get(k), new std::vector<Datapoint *>({dp_mag, dp_angle}), true);
        phasor_dps->push_back(dp_val);
    }
    auto dp_phasors = create_dp_list(DP_PHASORS, phasor_dps, true);

    auto analog_dps = new std::vector<Datapoint *>;
    for (int k = 0; k < pmu_station->ANNMR_get(); k++)
    {
        auto dp_an = create_dp(pmu_station->AN_NAME_get(k), pmu_station->ANALOG_VALUE_get(k));
        analog_dps->push_back(dp_an);
    }
    auto dp_analogs = create_dp_list(DP_ANALOGS, analog_dps, true);
    return create_dp_list(READING_PREFIX + to_string(pmu_station->IDCODE_get()), new std::vector<Datapoint *>({dp_id, dp_frequency, dp_phasors, dp_analogs}), true);
}

/**
 * @brief transform the c37118 dataframe to fledge Reading
 *
 * @return Reading
 */

vector<Reading *> FC37118::m_dataframe_to_reading()
{
    std::vector<Reading *> readings;
    auto dp_SOC = create_dp(DP_SOC, (long)(m_data_frame->SOC_get()));
    auto dp_FRACSEC = create_dp(DP_FRACSEC, (long)(m_data_frame->FRACSEC_get()));
    auto dp_TIME_BASE = create_dp(DP_TIME_BASE, (long)(m_config_frame->TIME_BASE_get()));
    auto dp_time = create_dp_list(DP_TIMESTAMP, new std::vector<Datapoint *>({dp_SOC, dp_FRACSEC, dp_TIME_BASE}), true);

    auto pmu_dps = new std::vector<Datapoint *>;
    for (auto pmu_station : m_config_frame->pmu_station_list)
    {
        // if (m_conf->get_stn_idcodes_filter()o)
        auto dp_pmu_station = m_pmu_station_to_datapoint(pmu_station);
        if (m_conf->is_split_stations())
        {
            auto dp_reading = create_dp_list("Single_PMU", new std::vector<Datapoint *>({new Datapoint(*dp_time), dp_pmu_station}), true);
            readings.push_back(
                new Reading(to_string(m_config_frame->IDCODE_get()) + "-" + to_string(pmu_station->IDCODE_get()),
                            dp_reading));
        }
        else
            pmu_dps->push_back(dp_pmu_station);
    }

    if (!m_conf->is_split_stations())
    {
        auto dp_pmu_stations = create_dp_list(DP_PMUSTATIONS, pmu_dps, true);
        auto dp_reading = create_dp_list("Multi_PMU", new std::vector<Datapoint *>({dp_time, dp_pmu_stations}), true);
        readings.push_back(new Reading(to_string(m_config_frame->IDCODE_get()), dp_reading));
    }
    else
    {
        delete dp_time;
        delete pmu_dps;
    }
    return readings;
}
