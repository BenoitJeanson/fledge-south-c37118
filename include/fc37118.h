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
#include <future>
#include <string>

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
#define C37118_CMD_SEND_DATA 0x03
#define C37118_CMD_SEND_CONFIGURATION_1 0x04
#define C37118_CMD_SEND_CONFIGURATION_2 0x05

#define READING_PREFIX "c37118-IDCODE "

#define DP_TIMESTAMP "TimeStamp"
#define DP_SOC "SOC"
#define DP_FRACSEC "FRACSEC"
#define DP_TIME_BASE "TIME_BASE"

#define DP_PMUSTATIONS "PMUStations"

#define DP_ID "id"
#define DP_IDCODE "IDCODE"
#define DP_STN "STN"

#define DP_FREQUENCY "Frequency"
#define DP_FREQ "FREQ"
#define DP_DFREQ "DFREQ"

#define DP_PHASORS "phasors"
#define DP_MAGNITUDE "mag"
#define DP_ANGLE "angle"

#define DP_ANALOGS "analogs"



typedef void (*INGEST_CB)(void *, Reading);

class FC37118
{

public:
    FC37118(const char *inet_add, int portno);
    FC37118();
    ~FC37118();

    void set_asset_name(const std::string &asset) { m_asset = asset; }
    bool set_conf(const std::string &conf);
    std::string get_asset_name() { return m_asset; }
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
    bool m_send_cmd(int cmd);
    void m_init_Pmu_Dialog();

    // Fledge
    vector<Reading>* m_dataframe_to_reading();
    std::string m_asset;
    INGEST_CB m_ingest; // Callback function used to send data to south service
    void *m_data;       // Ingest function data
    void m_receiveAndPushDatapoints();
};
#endif