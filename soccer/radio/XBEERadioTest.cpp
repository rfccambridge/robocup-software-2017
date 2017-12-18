#include <gtest/gtest.h>
#include "XBEERadio.hpp"

#include <iostream>

TEST(XBEERadio, Initialization) {
    XBEERadio radio;
    EXPECT_NE(&radio, nullptr);
}

TEST(XBEERadio, EstablishXBEEHostConnection) {
    XBEERadio radio("/dev/ttyUSB0");
    EXPECT_NE(&radio, nullptr);
}

