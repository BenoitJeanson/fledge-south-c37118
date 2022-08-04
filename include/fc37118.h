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
    // configuration
    FC37118Conf *m_conf;

    // when m_exit_promise is set when shutdown is requested and trigger stop to the threads through m_exit_future
    bool m_is_running;
    std::promise<void> m_terminate_promise;
    std::future<void> m_terminate_future;
    bool m_terminate();

    std::thread *m_configuration_thread;
    std::thread *m_receiving_thread;

    // Connection to PMU
    const char *m_inet_add;
    int m_portno;
    int m_sockfd;
    struct sockaddr_in m_serv_addr;
    bool m_connect();
    void m_init_Pmu_Dialog(std::promise<void> *configuration_promise);

    // C37.118
    CMD_Frame *m_cmd;
    CONFIG_Frame *m_config;
    HEADER_Frame *m_header;
    DATA_Frame *m_data_frame;
    PMU_Station *m_pmu_station;
    bool m_send_cmd(int cmd);

    // Fledge
    Reading m_dataframe_to_reading();
    std::string m_asset;
    INGEST_CB m_ingest; // Callback function used to send data to south service
    void *m_data;       // Ingest function data

    void m_receiveAndPushDatapoints(std::future<void> *configuration_future);
};
#endif