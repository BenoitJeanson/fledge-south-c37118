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
                                                     m_exit_future(m_exit_promise.get_future())
{
    m_serv_addr.sin_family = AF_INET;
    m_serv_addr.sin_addr.s_addr = inet_addr(m_inet_add);
    m_serv_addr.sin_port = htons(m_portno);
}

bool FC37118::m_shall_exit()
{
    return m_exit_future.wait_for(std::chrono::microseconds(1)) == std::future_status::ready;
}

void FC37118::m_connect()
{
    Logger::getLogger()->info("Connecting to PMU");

    m_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_sockfd < 0)
    {
        Logger::getLogger()->fatal("FATAL error opening socket");
        exit(EXIT_FAILURE);
    }

    while (connect(m_sockfd, (struct sockaddr *)&m_serv_addr, sizeof(m_serv_addr)) != 0)
    {
        Logger::getLogger()->info("Reconnection attempt in %i seconds", TIMEOUT);
        if (m_exit_future.wait_for(std::chrono::seconds(TIMEOUT)) == std::future_status::ready)
        {
            Logger::getLogger()->debug("Exit signal received during connection. Abort");
            break;
        }
    }
}

void FC37118::sendCmd(int cmd, unsigned char *buffer_tx)
{
    m_cmd->CMD_set(cmd);
    unsigned short size = m_cmd->pack(&buffer_tx);
    int n;
    do
    {
        n = write(m_sockfd, buffer_tx, size);
        if (n < 0)
        {
            m_connect();
        }
    } while (n < 0 && !m_shall_exit());
}

void FC37118::m_initPmuDialog()
{
    unsigned char *buffer_tx, buffer_rx[SIZE_BUFFER];

    Logger::getLogger()->debug("Enter init Pmu Dialog");

    m_cmd->SOC_set(1392733934);
    m_cmd->IDCODE_set(MY_IDCODE);

    // Request & receive Header Frame
    sendCmd(0x03, buffer_tx);
    int size = read(m_sockfd, buffer_rx, SIZE_BUFFER);
    m_header->unpack(buffer_rx);
    Logger::getLogger()->info("header from PMU: " + m_header->DATA_get());

    // Request & receive Config frame
    sendCmd(0x05, buffer_tx);
    size = read(m_sockfd, buffer_rx, SIZE_BUFFER);
    m_config->unpack(buffer_rx);
    m_pmu_station = m_config->PMUSTATION_GETbyIDCODE(PMU_STATION_IDCODE);
}

void FC37118::kickOff()
{
    unsigned char *buffer_tx;

    Logger::getLogger()->setMinLevel(DEBUG_LEVEL);

    Logger::getLogger()->info("C37118 start");
    m_connect();
    m_initPmuDialog();
}

void FC37118::start()
{
    kickOff();
    m_receiving_thread = new std::thread(&FC37118::receiveAndPushDatapoints, this);
}

/**
 * Save the callback function and its data
 * @param data   The Ingest function data
 * @param cb     The callback function to call
 */
void FC37118::registerIngest(void *data, INGEST_CB cb)
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

void FC37118::receiveAndPushDatapoints()
{
    unsigned char *buffer_tx, buffer_rx[SIZE_BUFFER];
    int size;
    int k;

    Logger::getLogger()->debug("receiving real time data");
    sendCmd(0x02, buffer_tx);

    do
    {
        size = read(m_sockfd, buffer_rx, SIZE_BUFFER);
        if (size > 0)
        {
            m_data_frame->unpack(buffer_rx);
            auto reading = dataFrameToReading();
            ingest(reading);
            if (m_shall_exit())
            {
                break;
            }
        }
        else
        {
            Logger::getLogger()->info("connexion lost with PMU. Reconnecting...");
            m_connect();
            sendCmd(0x02, buffer_tx); // after a deconnection, shall request data
        }
    } while (!m_shall_exit());
    Logger::getLogger()->debug("exit signal received: stop receiving");
}

Reading FC37118::dataFrameToReading()
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
    Logger::getLogger()->info("shutting down");
    m_exit_promise.set_value();
    close(m_sockfd);
    if (m_receiving_thread != nullptr)
    {
        m_receiving_thread->join();
    }
}