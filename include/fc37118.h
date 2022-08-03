/*
 * Fledge south plugin.

 * Copyright (c) 2021, RTE (http://www.rte-france.com)*

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

#include "reading.h"
#include "logger.h"
#include "c37118.h"
#include "c37118configuration.h"
#include "c37118pmustation.h"
#include "c37118data.h"
#include "c37118header.h"
#include "c37118command.h"

#define SIZE_BUFFER 80000

typedef void (*INGEST_CB)(void *, Reading);

class FC37118
{

public:
    FC37118(const char *inet_add, int portno);
    ~FC37118();

    void setAssetName(const std::string &asset) { m_asset = asset; }
    std::string getAssetName() { return m_asset; }
    void sendCmd(int cmd, unsigned char *buffer_tx);
    void kickOff();
    void start();
    void registerIngest(void *data, INGEST_CB cb);
    void ingest(Reading &reading);
    void receiveAndPushDatapoints();

private:
    // when m_exit_promise is set when shutdown is requested and trigger stop to the threads through m_exit_future
    std::promise<void> m_exit_promise;
    std::future<void> m_exit_future;
    bool m_shall_exit();

    std::thread *m_receiving_thread;

    // Connection to PMU
    const char *m_inet_add;
    int m_portno;
    int m_sockfd;
    struct sockaddr_in m_serv_addr;
    void m_connect();
    void m_initPmuDialog();

    // C37.118
    CMD_Frame *m_cmd;
    CONFIG_Frame *m_config;
    HEADER_Frame *m_header;
    DATA_Frame *m_data_frame;
    PMU_Station *m_pmu_station;

    // Fledge
    Reading dataFrameToReading();
    std::string m_asset;
    INGEST_CB m_ingest; // Callback function used to send data to south service
    void *m_data;       // Ingest function data
};
#endif