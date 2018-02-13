#pragma once

#include <QUdpSocket>
#include <SystemState.hpp>

#include "Radio.hpp"
#include "XBEERadio.hpp"

/**
 * @brief Radio IO with robots in the simulator
 */
class SimRadio : public Radio {
public:
    static std::size_t instance_count;
    SimRadio(SystemState& system_state, bool blueTeam = false);

    virtual bool isOpen() const override;
    virtual void send(Packet::RadioTx& packet) override;
    virtual void receive() override;
    virtual void switchTeam(bool blueTeam) override;
    virtual void send(std::string packet) override;
    void stopRobots();

private:
    SystemState& _state;
    XBEERadio *xbee;

    QUdpSocket _tx_socket;
    QUdpSocket _rx_socket;
    int _channel;
    bool _blueTeam;
};
