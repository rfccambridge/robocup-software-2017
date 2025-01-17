
// FIXME - Move a lot of stuff like blueTeam and worldToTeam to a globally
// accessible place

#pragma once

#include <vector>

#include <QThread>
#include <QMutex>
#include <QMutexLocker>

#include <protobuf/LogFrame.pb.h>
#include <Logger.hpp>
#include <Geometry2d/TransformMatrix.hpp>
#include <SystemState.hpp>
#include <modeling/RobotFilter.hpp>
#include <NewRefereeModule.hpp>
#include "VisionReceiver.hpp"

#include "firmware-common/common2015/utils/rtp.hpp"

class Configuration;
class RobotStatus;
class Joystick;
struct JoystickControlValues;
class Radio;
class BallTracker;

namespace Gameplay {
class GameplayModule;
}

namespace Planning {
class MultiRobotPathPlanner;
}

class DebugQMutex : public QMutex {
public:
    DebugQMutex(QMutex::RecursionMode mode = QMutex::NonRecursive)
        : QMutex(mode) {}

    void lock() {
        // printf("thread %ld tries to lock\n", QThread::currentThreadId());
        QMutex::lock();
    }

    bool tryLock() {
        // printf("tryLock\n");
        return QMutex::tryLock();
    }

    bool tryLock(int timeout) {
        // printf("tryLock\n");
        return QMutex::tryLock(timeout);
    }

    void unlock() {
        QMutex::unlock();
        // printf("thread %ld unlocked\n", QThread::currentThreadId());
    }
};

/**
 * @brief Brings all the pieces together
 *
 * @details The processor ties together all the moving parts for controlling
 * a team of soccer robots.  Its responsibities include:
 * - receiving and handling vision packets (see VisionReceiver)
 * - receiving and handling referee packets (see RefereeModule)
 * - radio IO (see Radio)
 * - running the BallTracker
 * - running the Gameplay::GameplayModule
 * - running the Logger
 * - handling the Configuration
 * - handling the Joystick
 * - running motion control for each robot (see OurRobot#motionControl)
 */
class Processor : public QThread {
public:
    struct Status {
        Status() {}

        RJ::Time lastLoopTime;
        RJ::Time lastVisionTime;
        RJ::Time lastRefereeTime;
        RJ::Time lastRadioRxTime;
    };

    enum VisionChannel { primary, secondary, full };

    static void createConfiguration(Configuration* cfg);

    Processor(bool sim, bool defendPlus, VisionChannel visionChannel);
    Processor(bool sim, bool defendPlus, VisionChannel visionChannel, bool xbee);
    virtual ~Processor();

    void stop();

    bool autonomous();
    bool joystickValid() const;
    JoystickControlValues getJoystickControlValues();

    void externalReferee(bool value) {
        _refereeModule->useExternalReferee(value);
    }

    bool externalReferee() const {
        return _refereeModule->useExternalReferee();
    }

    void manualID(int value);
    int manualID() const { return _manualID; }

    bool useFieldOrientedManualDrive() const {
        return _useFieldOrientedManualDrive;
    }
    void setUseFieldOrientedManualDrive(bool foc) {
        _useFieldOrientedManualDrive = foc;
    }

    /**
     * @brief Set the shell ID of the goalie
     * @details The rules require us to specify at the start of a match/period
     * which
     * robot will be the goalie.  A value of -1 indicates that there is no one
     * assigned.
     */
    void goalieID(int value);
    /**
     * @brief Shell ID of the goalie robot
     */
    int goalieID();

    void dampedRotation(bool value);
    void dampedTranslation(bool value);

    void joystickKickOnBreakBeam(bool value);

    void blueTeam(bool value);
    bool blueTeam() const { return _blueTeam; }

    std::shared_ptr<Gameplay::GameplayModule> gameplayModule() const {
        return _gameplayModule;
    }

    std::shared_ptr<NewRefereeModule> refereeModule() const {
        return _refereeModule;
    }

    SystemState* state() { return &_state; }

    bool simulation() const { return _simulation; }

    void defendPlusX(bool value);
    bool defendPlusX() { return _defendPlusX; }

    Status status() {
        QMutexLocker lock(&_statusMutex);
        return _status;
    }

    float framerate() { return _framerate; }

    const Logger& logger() const { return _logger; }

    bool openLog(const QString& filename) { return _logger.open(filename); }

    void closeLog() { _logger.close(); }

    // Use all/part of the field
    void useOurHalf(bool value) { _useOurHalf = value; }

    void useOpponentHalf(bool value) { _useOpponentHalf = value; }

    QMutex& loopMutex() { return _loopMutex; }

    Radio* radio() { return _radio; }

    void changeVisionChannel(int port);

    void setRobotConfigs(std::vector<
        std::pair<DebugCommunication::ConfigCommunication, float>> configs) {
        _robotConfigs = std::move(configs);
    }

    void setRobotDebugResponses(
        std::vector<DebugCommunication::DebugResponse> debugResponses) {
        _robotDebugResponses = std::move(debugResponses);
    }

    VisionChannel visionChannel() { return _visionChannel; }

    void recalculateWorldToTeamTransform();

    void setFieldDimensions(const Field_Dimensions& dims);

    bool isRadioOpen() const;

    bool isInitialized() const;

    ////////

    // Time of the first LogFrame
    boost::optional<RJ::Time> firstLogTime;

protected:
    void run() override;

    void applyJoystickControls(const JoystickControlValues& controlVals,
                               Packet::Control* txRobot, OurRobot* robot);

private:
    // Configuration for different models of robots
    static RobotConfig* robotConfig2008;
    static RobotConfig* robotConfig2011;
    static RobotConfig* robotConfig2015;

    // per-robot status configs
    static std::vector<RobotStatus*> robotStatuses;

    /** send out the radio data for the radio program */
    void sendRadioData();

    void updateGeometryPacket(const SSL_GeometryFieldSize& fieldSize);

    void runModels(
        const std::vector<const SSL_DetectionFrame*>& detectionFrames);

    /** Used to start and stop the thread **/
    volatile bool _running;

    Logger _logger;

    Radio* _radio;


    // True if we are using XBEE radio communcation
    // Will override all simulator communications
    bool _xbee;

    bool _useOurHalf, _useOpponentHalf;

    // True if we are running with a simulator.
    // This changes network communications.
    bool _simulation;

    // True if we are blue.
    // False if we are yellow.
    bool _blueTeam;

    // Locked when processing loop stuff is happening (not when blocked for
    // timing or I/O). This is public so the GUI thread can lock it to access
    // SystemState, etc.
    QMutex _loopMutex;

    /** global system state */
    SystemState _state;

    // Transformation from world space to team space.
    // This depends on which goal we're defending.
    //
    // _teamTrans is used for positions, not angles.
    // _teamAngle is used for angles.
    Geometry2d::TransformMatrix _worldToTeam;
    float _teamAngle;

    // Board ID of the robot to manually control or -1 if none
    int _manualID;

    bool _defendPlusX;

    // Processing period in microseconds
    RJ::Seconds _framePeriod = RJ::Seconds(1) / 60;

    /// Measured framerate
    float _framerate;

    // This is used by the GUI to indicate status of the processing loop and
    // network
    QMutex _statusMutex;
    Status _status;

    // ConfigCommunication Storage
    std::vector<std::pair<DebugCommunication::ConfigCommunication, float>>
        _robotConfigs{};
    std::vector<DebugCommunication::DebugResponse> _robotDebugResponses{};

    // modules
    std::shared_ptr<NewRefereeModule> _refereeModule;
    std::shared_ptr<Gameplay::GameplayModule> _gameplayModule;
    std::unique_ptr<Planning::MultiRobotPathPlanner> _pathPlanner;
    std::shared_ptr<BallTracker> _ballTracker;

    // mixes values from all joysticks to control the single manual robot
    std::vector<Joystick*> _joysticks;

    // joystick damping
    bool _dampedRotation;
    bool _dampedTranslation;

    bool _kickOnBreakBeam;

    // If true, rotates robot commands from the joystick based on its
    // orientation on the field
    bool _useFieldOrientedManualDrive = false;

    VisionReceiver vision;

    VisionChannel _visionChannel;

    bool _initialized;
};
