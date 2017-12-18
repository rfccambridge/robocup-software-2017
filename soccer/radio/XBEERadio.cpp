#include <stdio.h>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>

#include "XBEERadio.hpp"

static const int Control_Timeout = 1000;

static const char* XBEE_MODE = "xbee1";
static const char* XBEE_DATATYPE = "64-bit Data";


XBEERadio::XBEERadio() {

}

XBEERadio::XBEERadio(int id) {

}

XBEERadio::XBEERadio(std::string usbport) {
    // Default on Ubuntu is "/dev/ttyUSB0"
    if ((ret = xbee_setup(&xbee, XBEE_MODE, usbport, 57600)) != XBEE_ENONE) {
        printf("ret: %d (%s)\n", ret, xbee_errorToStr(ret));
        std::cout<< "[Error]: XBEE not found" << std::endl;
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

    if ((ret = xbee_conNew(xbee, &con, XBEE_DATATYPE, &address)) != XBEE_ENONE) {
        xbee_log(xbee, -1, "xbee_conNew() returned: %d (%s)", ret, xbee_errorToStr(ret));
        std::cout<<"Something went wrong with setting up the connection" << std::endl;
    }
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



void XBEERadio::send(Packet::RadioTx& packet) {
    send_broadcast(packet);
}

void XBEERadio::send_broadcast(Packet::RadioTx& packet) {
    for (int i = 0; i < 1000; i++) {
        xbee_conTx(con, NULL, "Hello World\r\n", i);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
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




