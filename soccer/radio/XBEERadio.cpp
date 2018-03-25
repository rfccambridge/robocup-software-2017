#include <stdio.h>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <sstream>

#include "XBEERadio.hpp"

static const int Control_Timeout = 1000;

static const char* XBEE_MODE = "xbee1";
static const char* XBEE_DATATYPE = "64-bit Data";
static const char* XBEE_USBPORT = "/dev/ttyUSB1";
static const int NUM_ROBOT_SLOTS = 6;
static const int BAUD_RATE = 9600;


XBEERadio::XBEERadio(int id) {

}

XBEERadio::XBEERadio() {
    // Default on Ubuntu is "/dev/ttyUSB0"
    printf("fuckwfu\n");
    if ((ret = xbee_setup(&xbee, XBEE_MODE, XBEE_USBPORT, BAUD_RATE)) != XBEE_ENONE) {
        printf("ret: %d (%s)\n", ret, xbee_errorToStr(ret));
        exit(-1);
    }
    printf("hello\n");
    memset(&address, 0, sizeof(address));
    address.addr64_enabled = 1;
    address.addr64[0] = 0x00;
    address.addr64[1] = 0x00;
    address.addr64[2] = 0x00;
    address.addr64[3] = 0x00;
    address.addr64[4] = 0x00;
    address.addr64[5] = 0x00;
    address.addr64[6] = 0xFF;
    address.addr64[7] = 0xFF;
    printf("hello\n");
    if ((ret = xbee_conNew(xbee, &con, "64-bit Data", &address)) != XBEE_ENONE) {
        xbee_log(xbee, -1, "xbee_conNew() returned: %d (%s)", ret, xbee_errorToStr(ret));
        exit(-1);
    }

    xbee_conSettings(con, NULL, &settings);
    settings.disableAck = 1;
    xbee_conSettings(con, &settings, NULL);
}

XBEERadio::XBEERadio(std::string usbport) {
    // Default on Ubuntu is "/dev/ttyUSB0"
    if ((ret = xbee_setup(&xbee, XBEE_MODE, XBEE_USBPORT, BAUD_RATE)) != XBEE_ENONE) {
        printf("ret: %d (%s)\n", ret, xbee_errorToStr(ret));
        std::cout<< "[Error]: XBEE not found" << std::endl;
        exit(1);
    }

    memset(&address, 0, sizeof(address));
    address.addr64_enabled = 1;
    address.addr64[0] = 0x00;
    address.addr64[1] = 0x00;
    address.addr64[2] = 0x00;
    address.addr64[3] = 0x00;
    address.addr64[4] = 0x00;
    address.addr64[5] = 0x00;
    address.addr64[6] = 0xFF;
    address.addr64[7] = 0xFF;
    /* 0x000000000000FFFF is the default broadcast address */

    xbee_logLevelSet(xbee, 100); 

    if ((ret = xbee_conNew(xbee, &con, XBEE_DATATYPE, &address)) != XBEE_ENONE) {
        xbee_log(xbee, -1, "xbee_conNew() returned: %d (%s)", ret, xbee_errorToStr(ret));
        std::cout<<"Something went wrong with setting up the connection" << std::endl;
    }

    xbee_conSettings(con, NULL, &settings);
    settings.disableAck = 1;
    xbee_conSettings(con, &settings, NULL);
}


XBEERadio::~XBEERadio() {
    if ((ret = xbee_conEnd(con)) != XBEE_ENONE) {
        xbee_log(xbee, -1, "xbee_conEnd() returned %d", ret);
    }    
    xbee_shutdown(xbee);
}

bool XBEERadio::isOpen() const {
    //TODO: WFU
    return true;
}


void XBEERadio::send(std::string packet) {
    xbee_conSettings(con, NULL, &settings);
    settings.disableAck = 1;
    xbee_conSettings(con, &settings, NULL);
    if (con != NULL) 
        xbee_conTx(con, NULL, "%s\r\n", packet.c_str());
    return;
}



void XBEERadio::send(Packet::RadioTx& packet) {
    /*
    std::stringstream ss;
    ss << ">>>STARTMESSAGE<<<" << std::endl;
    for (int slot = 0; slot < NUM_ROBOT_SLOTS; slot++) {
        if (slot < packet.robots_size()) {
            ss << ">>>STARTROBOT<<<" << std::endl;
            const Packet::Control &robot = packet.robots(slot).control();
            ss << packet.robots(slot).uid() << std::endl;
            ss << static_cast<int16_t>(robot.xvelocity()) << std::endl;
            ss << static_cast<int16_t>(robot.yvelocity()) << std::endl;
            ss << static_cast<int16_t>(robot.avelocity()) << std::endl;
            ss << static_cast<uint16_t>(robot.dvelocity()) << std::endl;
            ss << robot.kcstrength() << std::endl;
            ss << robot.shootmode() << std::endl;
            ss << robot.triggermode() << std::endl;
            ss << robot.song() << std::endl;
            ss << ">>>ENDROBOT<<<" << std::endl;
        }
    }
    ss << ">>>ENDMESSAGE<<<" << std::endl;
    std::string message = ss.str();
    std::cout << message << std::endl;
    xbee_conTx(con, NULL, "%s\r\n", message.c_str());*/
}


void XBEERadio::send_broadcast(Packet::RadioTx& packet) {
    /*
    for (int i = 0; i < 10; i++) {
        xbee_conTx(con, NULL, "%d\r\n", i);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }*/
}

void XBEERadio::send_debug_message(std::string msg) {
    /*
    char buf[4096];
    snprintf(buf, 4096, "%s\r\n", msg.c_str());
    for (int i = 0; i < 5; i++) {
        xbee_conTx(con, NULL, "%s\r\n", msg.c_str());
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }*/
}

void xbee_callback(struct xbee *xbee, struct xbee_con *con, struct xbee_pkt **pkt, void **data) {
    if ((*pkt)->dataLen == 0) {
        std::cout << "Packet too short" << std::endl;
        return;
    }
    printf("rx: [%s]\n", (*pkt)->data);
}


void XBEERadio::receive() {
    if ((ret = xbee_conCallbackSet(con, xbee_callback, NULL)) != XBEE_ENONE) {
        xbee_log(xbee, -1, "xbee_callback_setting returned %d:", ret);
    }
}

void XBEERadio::channel(int n) {

}



bool XBEERadio::open() {
    //TODO: WFU
    return true;
}


void XBEERadio::command(uint8_t cmd) {

}

void XBEERadio::write(uint8_t reg, uint8_t value) {

}

uint8_t XBEERadio::read(uint8_t reg) {
    //TODO: wfu
    return 0;
}


void XBEERadio::testonly() {

}




