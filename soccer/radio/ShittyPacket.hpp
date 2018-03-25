#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <sstream>


/* ShittyPacket
 * A packet for use with XBEEs, since apparently our team
 * is too cheap to get legitimate radio controllers. Apparently
 * we get funded by HURC
 */
class ShittyPacket {
    public:
        int16_t robot_id; // zero indexed
        int16_t robot_x; // velocity in x direction
        int16_t robot_y; // velocity in y direction
        int16_t robot_w; // omega, angular velocity
        /*
        std::vector<uint8_t> robot_ids;
        std::vector<int16_t> robot_x;
        std::vector<int16_t> robot_y;
        std::vector<int16_t> robot_w;
        */

        ShittyPacket()
            : robot_x(0),
            robot_y(0),
            robot_w(0),
            robot_id(0)
            {};

        std::string serialize() {
            std::ostringstream ss;
            ss << robot_id << ","
            << robot_x << ","
            << robot_y << ","
            << robot_w;
            std::string message = ss.str();
            return message;
        };
        std::string fuckyou() {
            return std::string("abcabcabcabc");
        };
};