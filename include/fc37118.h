/*
 * Fledge south plugin.

 * Copyright (c) 2022, RTE (http://www.rte-france.com)*

 * Released under the Apache 2.0 Licence
 *
 * Author: Benoit Jeanson <benoit.jeanson at rte-france.com>
 */

#ifndef _F_C37118_H
#define _F_C37118_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <algorithm>
#include <bitset>


#include "reading.h"
#include "logger.h"
#include "c37118.h"
#include "c37118configuration.h"
#include "c37118pmustation.h"
#include "c37118data.h"
#include "c37118header.h"
#include "c37118command.h"

#include "fc37118conf.h"

#define BUFFER_SIZE 80000

#define C37118_CMD_TURNOFF_TX 0x01
#define C37118_CMD_TURNON_TX 0x02
#define C37118_CMD_SEND_HDR 0x03
#define C37118_CMD_SEND_CONFIGURATION_1 0x04
#define C37118_CMD_SEND_CONFIGURATION_2 0x05

#define PMU_DATA "PMU_data"

#define DP_TIMESTAMP "TimeStamp"
#define DP_SOC "SOC"
#define DP_FRACSEC "FRACSEC"
#define DP_TIME_BASE "TIME_BASE"
#define DP_LS "LeapSecond"
#define DP_LS_DIR_ADD "DirectionAdd"
#define DP_LS_OCCURS "Occurs"
#define DP_LS_PENDING "Pending"
#define DP_TIME_QUALITY "QualityIndicator"

#define DP_PMUSTATIONS "PMUStations"
#define DP_SINGLE_PMU "Single_PMU"
#define DP_MULTI_PMU "Multi_PMU"

#define DP_ID "Id"
#define DP_IDCODE "IDCODE"
#define DP_STN "STN"
#define DP_QUAL "MeasurementQuality"
#define DP_TIME_SYNC "PMUSync"

#define DP_FREQUENCY "Frequency"
#define DP_FREQ "FREQ"
#define DP_DFREQ "DFREQ"

#define DP_LABEL "Label"
#define DP_VALUE "Value"

#define DP_PHASORS "Phasors"
#define DP_MAGNITUDE "Mag"
#define DP_ANGLE "Ang"

#define DP_ANALOGS "Analogs"

typedef void (*INGEST_CB)(void *, Reading);

class FC37118
{

public:
    FC37118(const char *inet_add, int portno);
    FC37118();
    ~FC37118();

    bool set_conf(const std::string &conf);
    void start();
    void stop();
    void register_ingest(void *data, INGEST_CB cb);
    void ingest(Reading &reading);

private:
    // Configuration
    FC37118Conf *m_conf;
    bool m_c37118_configuration_ready;
    void m_log_configuration();

    // Running
    bool m_is_running;
    bool m_terminate();
    std::thread *m_receiving_thread;

    // Connection to PMU
    int m_sockfd;
    struct sockaddr_in m_serv_addr;
    bool m_connect();

    // C37.118 objects handling
    CMD_Frame m_cmd;
    CONFIG_Frame *m_config_frame;
    DATA_Frame *m_data_frame;
    void m_init_c37118();
    bool m_send_cmd(unsigned short cmd);
    void m_init_Pmu_Dialog();

    // Fledge
    std::vector<Reading *> m_dataframe_to_reading();

    Datapoint *m_pmu_station_to_datapoint(PMU_Station *pmu_station);

    INGEST_CB m_ingest; // Callback function used to send data to south service
    void *m_data;       // Ingest function data
    bool m_init_receiving();
    void m_receiveAndPushDatapoints();
};
#endif