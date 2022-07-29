#ifndef _F_C37118_H
#define _F_C37118_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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

class FC37118 {

    public:
        FC37118(const char* inet_add, int portno);
        ~FC37118();

        void setAssetName(const std::string& asset) { m_asset = asset; }
        std::string getAssetName() { return m_asset;}
        void initSocket();
        void sendCmd(int cmd, unsigned char *buffer_tx);
        void initPmuDialog();
        void kickOff();
        void start();
        void registerIngest(void *data, INGEST_CB cb);
        void ingest(Reading& reading);
        void receiveAndPushDatapoints();
        void readForTest();

    
    private:
        Reading dataFrameToReading();
        int m_sockfd;
        CMD_Frame* m_cmd;
        CONFIG_Frame* m_config;
        HEADER_Frame* m_header;
        DATA_Frame* m_data_frame;
        PMU_Station* m_pmu_station;
        std::string	m_asset;
        int m_portno;
        const char* m_inet_add;
        INGEST_CB m_ingest;     // Callback function used to send data to south service
        void* m_data;       // Ingest function data

};
#endif