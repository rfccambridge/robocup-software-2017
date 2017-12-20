#include <gtest/gtest.h>
#include "XBEERadio.hpp"

#include <iostream>

TEST(XBEERadio, Initialization) {
    XBEERadio radio;
    EXPECT_NE(&radio, nullptr);
}

TEST(XBEERadio, EstablishXBEEHostConnection) {
    XBEERadio radio("/dev/ttyUSB2");
    radio.send_debug_message("aaaaaa");
    EXPECT_NE(&radio, nullptr);
}

TEST(XBEERadio, SendForceStop) {
    XBEERadio radio("/dev/ttyUSB0");
    Packet::RadioTx tx;
    tx.add_robots();
    radio.send(tx);
    EXPECT_NE(&radio, nullptr);
}

