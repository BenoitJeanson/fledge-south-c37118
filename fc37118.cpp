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
                     m_pmu_station(nullptr),
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
        m_pmu_station = m_config_frame->PMUSTATION_GETbyIDCODE(m_conf->get_pmu_IDCODE());

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
            m_pmu_station = m_config_frame->PMUSTATION_GETbyIDCODE(m_conf->get_pmu_IDCODE());
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
            auto reading = m_dataframe_to_reading();
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

    for (auto pmuStation : m_config_frame->pmu_station_list)
    {
        Logger::getLogger()->debug("  pmuStation STN: " + pmuStation->STN_get());
        Logger::getLogger()->debug("    pmuStation IDCODE: %u", pmuStation->IDCODE_get());
        Logger::getLogger()->debug("    pmuStation FORMAT_COORD: %d", pmuStation->FORMAT_COORD_get());
        Logger::getLogger()->debug("    pmuStation FORMAT_PHASOR_TYPE: %d", pmuStation->FORMAT_PHASOR_TYPE_get());
        Logger::getLogger()->debug("    pmuStation FORMAT_ANALOG_TYPE: %d", pmuStation->FORMAT_ANALOG_TYPE_get());
        Logger::getLogger()->debug("    pmuStation FORMAT_FREQ_TYPE: %d", pmuStation->FORMAT_FREQ_TYPE_get());
        Logger::getLogger()->debug("    pmuStation FORMAT: %u", pmuStation->FORMAT_get());
        Logger::getLogger()->debug("    pmuStation PHNMR: %u", pmuStation->PHNMR_get());
        int count = 0;
        for (int i = 0; i < pmuStation->PHNMR_get(); i++)
        {
            Logger::getLogger()->debug("      pmuStation CHNAM PH %i: " + pmuStation->PH_NAME_get(i), count++);
            Logger::getLogger()->debug("      pmuStation PHUNIT: %u ", pmuStation->PHUNIT_get(i));
        }
        
        Logger::getLogger()->debug("    pmuStation ANNMR: %u", pmuStation->ANNMR_get());
        for (int i = 0; i < pmuStation->ANNMR_get(); i++)
        {
            Logger::getLogger()->debug("      pmuStation CHNAM AN %i: " + pmuStation->AN_NAME_get(i), count++);
            Logger::getLogger()->debug("      pmuStation ANUNIT: %u", pmuStation->ANUNIT_get(i));
        }

        Logger::getLogger()->debug("    pmuStation DGNMR: %u", pmuStation->DGNMR_get());
        for (int i = 0; i < pmuStation->DGNMR_get(); i++)
        {
            for (int j = 0; j < 16; j++)
            {
                Logger::getLogger()->debug("        pmuStation CHNAM DG %i: " + pmuStation->DG_NAME_get(j), count++);
            }
            Logger::getLogger()->debug("      pmuStation DGUNIT: %u ", pmuStation->DGUNIT_get(i));
        }
    }
}

/**
 * @brief transform the c37118 dataframe to fledge Reading
 * 
 * @return Reading 
 */

Reading FC37118::m_dataframe_to_reading()
{
    auto time_datapoints = new vector<Datapoint *>;
    time_datapoints->push_back(new Datapoint("SOC", *new DatapointValue((long)(m_data_frame->SOC_get()))));
    time_datapoints->push_back(new Datapoint("FRACSEC", *new DatapointValue((long)(m_data_frame->FRACSEC_get()))));
    time_datapoints->push_back(new Datapoint("TIME_BASE", *new DatapointValue((long)(m_config_frame->TIME_BASE_get()))));
    auto datapoint_time = new Datapoint("Time", *new DatapointValue(time_datapoints, true));

    auto ph_datapoints = new vector<Datapoint *>;
    for (int k = 0; k < m_pmu_station->PHNMR_get(); k++)
    {
        auto phase_datapoints = new vector<Datapoint *>;

        phase_datapoints->push_back(new Datapoint("magnitude", *new DatapointValue(abs(m_pmu_station->PHASOR_VALUE_get(k)))));
        phase_datapoints->push_back(new Datapoint("angle", *new DatapointValue(arg(m_pmu_station->PHASOR_VALUE_get(k)))));

        DatapointValue dpv(phase_datapoints, true);
        ph_datapoints->push_back(new Datapoint(m_pmu_station->PH_NAME_get(k), dpv));
    }
    auto datapoint_phasors = new Datapoint("Phasors", *new DatapointValue(ph_datapoints, true));

    auto analog_datapoints = new vector<Datapoint *>;
    for (int k = 0; k < m_pmu_station->ANNMR_get(); k++)
    {
        analog_datapoints->push_back(new Datapoint(m_pmu_station->AN_NAME_get(k),
                                                   *new DatapointValue(m_pmu_station->ANALOG_VALUE_get(k))));
    }
    auto datapoint_analogs = new Datapoint("Analogs", *new DatapointValue(analog_datapoints, true));

    auto datapoint_FREQ = new Datapoint("FREQ", *new DatapointValue(m_pmu_station->FREQ_get()));
    auto datapoint_DFREQ = new Datapoint("DFREQ", *new DatapointValue(m_pmu_station->DFREQ_get()));
    auto freq_datapoints = new vector<Datapoint *>;
    freq_datapoints->push_back(datapoint_FREQ);
    freq_datapoints->push_back(datapoint_DFREQ);
    auto datapoint_frequency = new Datapoint("Frequency", *new DatapointValue(freq_datapoints, true));

    Reading reading(m_pmu_station->STN_get(), {datapoint_time, datapoint_phasors, datapoint_analogs, datapoint_frequency});
    return reading;
}
