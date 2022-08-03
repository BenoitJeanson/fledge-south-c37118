#include "fc37118.h"

#define DEBUG_LEVEL "debug"
#define TIMEOUT 3
#define MY_IDCODE 7
#define PMU_STATION_IDCODE 1410

FC37118::FC37118(const char *inet_add, int portno) : m_cmd(new CMD_Frame()),
                                                     m_config(new CONFIG_Frame()),
                                                     m_header(new HEADER_Frame("")),
                                                     m_data_frame(new DATA_Frame(m_config)),
                                                     m_pmu_station(nullptr),
                                                     m_inet_add(inet_add),
                                                     m_portno(portno),
                                                     m_sockfd(0),
                                                     m_receiving_thread(nullptr),
                                                     m_terminate_future(m_terminate_promise.get_future())
{
    m_serv_addr.sin_family = AF_INET;
    m_serv_addr.sin_addr.s_addr = inet_addr(m_inet_add);
    m_serv_addr.sin_port = htons(m_portno);
}

bool FC37118::m_terminate()
{
    return m_terminate_future.wait_for(std::chrono::microseconds(1)) == std::future_status::ready;
}

void FC37118::start()
{
    Logger::getLogger()->setMinLevel(DEBUG_LEVEL);

    Logger::getLogger()->info("C37118 start");

    std::promise<void> configuration_ready_promise;
    std::future<void> configuration_ready_future = configuration_ready_promise.get_future();
    m_configuration_thread = new std::thread(&FC37118::m_init_Pmu_Dialog, this, &configuration_ready_promise);
    m_receiving_thread = new std::thread(&FC37118::m_receiveAndPushDatapoints, this, &configuration_ready_future);
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
        Logger::getLogger()->info("Connection attempt in %i seconds", TIMEOUT);
        if (m_terminate_future.wait_for(std::chrono::seconds(TIMEOUT)) == std::future_status::ready)
        {
            Logger::getLogger()->debug("terminate signal received during connection. Abort");
            return false;
        }
    }
    Logger::getLogger()->info("connected");
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
 *
 * @param configuration_ready_promise the promise to send configuration_ready signal
 */
void FC37118::m_init_Pmu_Dialog(std::promise<void> *configuration_ready_promise)
{
    unsigned char *buffer_tx, buffer_rx[BUFFER_SIZE];

    Logger::getLogger()->debug("Start connection process");

    if (!m_connect())
    {
        return;
    }

    m_cmd->IDCODE_set(MY_IDCODE);

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
            m_config->unpack(buffer_rx);
            m_pmu_station = m_config->PMUSTATION_GETbyIDCODE(PMU_STATION_IDCODE);
            Logger::getLogger()->info("c37.118 configuration retrieved");
        }
        Logger::getLogger()->info("could not retrieve c37.118 configuration");
        return;
    }

    configuration_ready_promise->set_value(); // the configuration is properly set, configuration_ready signal is sent
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
 * @brief Receive the real time data from the c37.118 equipment. Wait for the configuration to be ready before requesting data.
 *
 * @param configuration_ready_future the future to receice configuration_ready signal
 */
void FC37118::m_receiveAndPushDatapoints(std::future<void> *configuration_ready_future)
{
    unsigned char buffer_rx[BUFFER_SIZE];
    int size;
    int k;

    while (configuration_ready_future->wait_for(chrono::seconds(1)) == std::future_status::timeout)
    {
        if (m_terminate())
        {
            Logger::getLogger()->debug("terminate signal received: stop waiting for configuration");
            return;
        }
        Logger::getLogger()->debug("waiting for configuration");
    }
    Logger::getLogger()->debug("receiving real time data, waiting conf availability");
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
            Logger::getLogger()->info("connexion lost with PMU. Reconnecting...");
            if (m_connect())
            {
                m_send_cmd(C37118_CMD_TURNON_TX); // after a deconnection, shall request data
            };
        }
    } while (!m_terminate());
    Logger::getLogger()->debug("terminate signal received: stop receiving");
}

Reading FC37118::m_dataframe_to_reading()
{
    auto ph_datapoints = new vector<Datapoint *>;
    for (int k = 0; k < m_pmu_station->PHNMR_get(); k++)
    {
        auto phase_datapoints = new vector<Datapoint *>;

        phase_datapoints->push_back(new Datapoint("magnitude", *new DatapointValue(abs(m_pmu_station->PHASOR_VALUE_get(k)))));
        phase_datapoints->push_back(new Datapoint("angle", *new DatapointValue(arg(m_pmu_station->PHASOR_VALUE_get(k)) * 180 / M_PI)));

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

    Reading reading(m_pmu_station->STN_get(), {datapoint_phasors, datapoint_analogs, datapoint_frequency});
    return reading;
}

FC37118::~FC37118()
{
    Logger::getLogger()->info("shutting down ********************************************");
    m_send_cmd(C37118_CMD_TURNOFF_TX);
    m_terminate_promise.set_value();
    close(m_sockfd);
    if (m_receiving_thread != nullptr)
    {
        m_receiving_thread->join();
    }
    if (m_configuration_thread != nullptr)
    {
        m_configuration_thread->join();
    }
}