#include "fc37118.h"

FC37118::FC37118(const char *inet_add, int portno) : m_cmd(new CMD_Frame()),
                                                     m_config(new CONFIG_Frame()),
                                                     m_header(new HEADER_Frame("")),
                                                     m_data_frame(new DATA_Frame(m_config)),
                                                     m_pmu_station(nullptr),
                                                     m_inet_add(inet_add),
                                                     m_portno(portno)
{
    Logger::getLogger()->warn("FPC37118 constructor");
}

void FC37118::initSocket()
{
    struct sockaddr_in serv_addr;
    m_sockfd = socket(AF_INET, SOCK_STREAM, 0);

    // int on = 1;
    // int status = setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on));

    if (m_sockfd < 0)
    {
        Logger::getLogger()->error("ERROR opening socket");
        exit(EXIT_FAILURE);
    }

    /* Initialize socket structure */
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(m_inet_add);
    serv_addr.sin_port = htons(m_portno);

    /* Now bind the host address using bind() call.*/
    if (connect(m_sockfd, (struct sockaddr *)&serv_addr,
                sizeof(serv_addr)) < 0)
    {
        Logger::getLogger()->error("Connect Failed !\n");
        exit(EXIT_FAILURE);
    }
}

void FC37118::sendCmd(int cmd, unsigned char *buffer_tx)
{
    m_cmd->CMD_set(cmd);
    unsigned short size = m_cmd->pack(&buffer_tx);
    int n = write(m_sockfd, buffer_tx, size);
    if (n < 0)
    {
        perror("ERROR writing to socket");
        exit(EXIT_FAILURE);
    }
}

void FC37118::initPmuDialog()
{
    unsigned char *buffer_tx, buffer_rx[SIZE_BUFFER];

    m_cmd->SOC_set(1392733934);
    m_cmd->IDCODE_set(7);

    // Request & receive Config frame
    sendCmd(0x05, buffer_tx);
    int size = read(m_sockfd, buffer_rx, SIZE_BUFFER);
    m_config->unpack(buffer_rx);

    // Request & receive Header Frame
    sendCmd(0x03, buffer_tx);
    size = read(m_sockfd, buffer_rx, SIZE_BUFFER);
    m_header->unpack(buffer_rx);
    cout << "INFO: " << m_header->DATA_get() << endl;

    m_pmu_station = m_config->PMUSTATION_GETbyIDCODE(1410);
    cout << "PMU: " << m_pmu_station->STN_get() << endl;
}

void FC37118::kickOff()
{
    unsigned char *buffer_tx;
    Logger::getLogger()->warn("C37118 start");
    initSocket();
    initPmuDialog();

    // Enable DATA ON
    sendCmd(0x02, buffer_tx);
}

void FC37118::start()
{
    kickOff();
    receiveAndPushDatapoints();
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
    do
    {
        size = read(m_sockfd, buffer_rx, SIZE_BUFFER);
        if (size > 0)
        {
            m_data_frame->unpack(buffer_rx);
            Logger::getLogger()->debug("Data received for: %s", m_pmu_station->STN_get());
            auto reading = dataFrameToReading();
            ingest(reading);
        }
    } while (size > 0);
};

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
    auto datapoint_phasors = new Datapoint("Phasors", * new DatapointValue(ph_datapoints, true));

    auto analog_datapoints = new vector<Datapoint *>;
    for (int k = 0; k < m_pmu_station->ANNMR_get(); k++)
    {
        analog_datapoints->push_back(new Datapoint(m_pmu_station->AN_NAME_get(k),
                                            *new DatapointValue(m_pmu_station->ANALOG_VALUE_get(k))));
    }
    auto datapoint_analogs = new Datapoint("Analogs", * new DatapointValue(analog_datapoints, true));

    auto datapoint_FREQ = new Datapoint("FREQ", * new DatapointValue(m_pmu_station->FREQ_get()));
    auto datapoint_DFREQ = new Datapoint("DFREQ", * new DatapointValue(m_pmu_station->DFREQ_get()));
    
    auto freq_datapoints = new vector<Datapoint *>;
    freq_datapoints->push_back(datapoint_FREQ);
    freq_datapoints->push_back(datapoint_DFREQ);
    
    auto datapoint_frequency = new Datapoint("Frequency", * new DatapointValue(freq_datapoints, true));
    
    Reading reading(m_pmu_station->STN_get(), {datapoint_phasors, datapoint_analogs, datapoint_frequency});
    return reading;
}

void FC37118::readForTest()
{
    unsigned char *buffer_tx, buffer_rx[SIZE_BUFFER];
    int size;
    int i, k;
    do
    {
        size = read(m_sockfd, buffer_rx, SIZE_BUFFER);
        if (size > 0)
        {
            m_data_frame->unpack(buffer_rx);
            cout << "MY PMU---------------------------------------- " << i++ << endl;
            for (k = 0; k < m_pmu_station->PHNMR_get(); k++)
            {
                cout << m_pmu_station->PH_NAME_get(k) << ":"
                     << abs(m_pmu_station->PHASOR_VALUE_get(k)) << "|_ "
                     << arg(m_pmu_station->PHASOR_VALUE_get(k)) * 180 / M_PI << endl;
            }
            for (k = 0; k < m_pmu_station->ANNMR_get(); k++)
            {
                cout << m_pmu_station->AN_NAME_get(k) << ":"
                     << m_pmu_station->ANALOG_VALUE_get(k) << endl;
            }
        }
    } while (size > 0);
};

FC37118::~FC37118()
{
    Logger::getLogger()->warn("shutting down");
    close(m_sockfd);
}