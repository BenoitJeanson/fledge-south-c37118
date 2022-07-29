/**(C) Copyright 2014 Rafael Marsolla - rafamarsolla@gmail.com
 
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>

#include "reading.h"
#include "logger.h"
#include "c37118.h"
#include "c37118configuration.h"
#include "c37118pmustation.h"
#include "c37118data.h"
#include "c37118header.h"
#include "c37118command.h"
#include "fc37118.h"
#include <unistd.h>
#include <iostream>

#define TCP_PORT 1410
#define INET_ADDR "127.0.0.1"

/**
* Simulate a PMU server/dispatcher C37.118-2011
*/

int main( int argc, char *argv[] ){
    unsigned char *buffer_tx, buffer_rx[SIZE_BUFFER];
    unsigned short size;

    FC37118* fc37118 = new FC37118(INET_ADDR, TCP_PORT);

    fc37118->kickOff();
    fc37118->readForTest();
    
    // Enable DATA OFF 
    fc37118->sendCmd(0x01, buffer_tx);
    return EXIT_SUCCESS;
} 
