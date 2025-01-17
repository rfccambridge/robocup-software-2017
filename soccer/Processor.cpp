#include <gameplay/GameplayModule.hpp>

#include <poll.h>
#include <QMutexLocker>

#include <protobuf/RadioRx.pb.h>
#include <protobuf/RadioTx.pb.h>
#include <protobuf/messages_robocup_ssl_detection.pb.h>
#include <protobuf/messages_robocup_ssl_geometry.pb.h>
#include <protobuf/messages_robocup_ssl_wrapper.pb.h>
#include <Constants.hpp>
#include <Geometry2d/Util.hpp>
#include <LogUtils.hpp>
#include <Robot.hpp>
#include <RobotConfig.hpp>
#include <Utils.hpp>
#include <git_version.hpp>
#include <joystick/GamepadController.hpp>
#include <joystick/GamepadJoystick.hpp>
#include <joystick/Joystick.hpp>
#include <joystick/SpaceNavJoystick.hpp>
#include <motion/MotionControl.hpp>
#include <multicast.hpp>
#include <planning/IndependentMultiRobotPathPlanner.hpp>
#include "Processor.hpp"
#include "modeling/BallTracker.hpp"
#include "radio/SimRadio.hpp"
#include "radio/USBRadio.hpp"
#include "radio/XBEERadio.hpp"
#include "radio/ShittyPacket.hpp"

#include "firmware-common/common2015/utils/DebugCommunicationStrings.hpp"
REGISTER_CONFIGURABLE(Processor)

using namespace std;
using namespace boost;
using namespace Geometry2d;
using namespace google::protobuf;

static const auto Command_Latency = 0ms;

RobotConfig* Processor::robotConfig2008;
RobotConfig* Processor::robotConfig2011;
RobotConfig* Processor::robotConfig2015;
std::vector<RobotStatus*>
    Processor::robotStatuses;  ///< FIXME: verify that this is correct

Field_Dimensions* currentDimensions = &Field_Dimensions::Current_Dimensions;

int xbeeControlTicks = 0;
auto xbeePacketSentTime = std::chrono::system_clock::now();
const double XBEE_PACKET_DELAY = 0.05; // xbee packet delay in seconds
const bool VERBOSE = false;

void Processor::createConfiguration(Configuration* cfg) {
    robotConfig2008 = new RobotConfig(cfg, "Rev2008");
    robotConfig2011 = new RobotConfig(cfg, "Rev2011");
    robotConfig2015 = new RobotConfig(cfg, "Rev2015");

    for (size_t s = 0; s < Num_Shells; ++s) {
        robotStatuses.push_back(
            new RobotStatus(cfg, QString("Robot Statuses/Robot %1").arg(s)));
    }
}

Processor::Processor(bool sim, bool defendPlus, VisionChannel visionChannel)
    : _loopMutex() {
    _running = true;
    _manualID = -1;
    _framerate = 0;
    _useOurHalf = true;
    _useOpponentHalf = true;
    _initialized = false;
    _simulation = sim;
    _radio = nullptr;

    // joysticks
    _joysticks.push_back(new GamepadController());
    _joysticks.push_back(new SpaceNavJoystick());
    // Enable this if you have issues with the new controller.
    // _joysticks.push_back(new GamepadJoystick());

    _dampedTranslation = true;
    _dampedRotation = true;

    _kickOnBreakBeam = false;

    // Initialize team-space transformation
    defendPlusX(defendPlus);

    QMetaObject::connectSlotsByName(this);

    _ballTracker = std::make_shared<BallTracker>();
    _refereeModule = std::make_shared<NewRefereeModule>(_state);
    _refereeModule->start();
    _gameplayModule = std::make_shared<Gameplay::GameplayModule>(&_state);
    _pathPlanner = std::unique_ptr<Planning::MultiRobotPathPlanner>(
        new Planning::IndependentMultiRobotPathPlanner());
    vision.simulation = _simulation;
    if (sim) {
        vision.port = SimVisionPort;
    }
    vision.start();

    _visionChannel = visionChannel;

    // Create radio socket
    //_radio = _simulation ? static_cast<Radio*>(new SimRadio(_state, _blueTeam))
    //                     : static_cast<Radio*>(new USBRadio());

    _radio = _simulation ? static_cast<Radio*>(new SimRadio(_state, _blueTeam))
                         : static_cast<Radio*>(new XBEERadio());
}

Processor::Processor(bool sim, bool defendPlus, VisionChannel visionChannel, bool xbee)
    : _loopMutex() {
    _running = true;
    _manualID = -1;
    _framerate = 0;
    _useOurHalf = true;
    _useOpponentHalf = true;
    _initialized = false;
    _simulation = sim;
    _radio = nullptr;
    _xbee = xbee;

    // joysticks
    _joysticks.push_back(new GamepadController());
    _joysticks.push_back(new SpaceNavJoystick());
    // Enable this if you have issues with the new controller.
    // _joysticks.push_back(new GamepadJoystick());

    _dampedTranslation = true;
    _dampedRotation = true;

    _kickOnBreakBeam = false;

    // Initialize team-space transformation
    defendPlusX(defendPlus);

    QMetaObject::connectSlotsByName(this);

    _ballTracker = std::make_shared<BallTracker>();
    _refereeModule = std::make_shared<NewRefereeModule>(_state);
    _refereeModule->start();
    _gameplayModule = std::make_shared<Gameplay::GameplayModule>(&_state);
    _pathPlanner = std::unique_ptr<Planning::MultiRobotPathPlanner>(
        new Planning::IndependentMultiRobotPathPlanner());
    vision.simulation = _simulation;
    if (sim) {
        vision.port = SimVisionPort;
    }
    vision.start();

    _visionChannel = visionChannel;
    
    // Create radio socket
    // WFUEDIT
    /*
    if (_xbee) {
        _radio = new XBEERadio();
    } else if (_simulation) {
        _radio = new SimRadio(_state, _blueTeam);
    } else {
        _radio = static_cast<Radio*>(new USBRadio());
    }    */
    
    _radio = static_cast<Radio*>(new XBEERadio());
}

Processor::~Processor() {
    stop();

    for (Joystick* joy : _joysticks) {
        delete joy;
    }

    // DEBUG - This is unnecessary, but lets us determine which one breaks.
    //_refereeModule.reset();
    _gameplayModule.reset();
}

void Processor::stop() {
    if (_running) {
        _running = false;
        wait();
    }
}

void Processor::manualID(int value) {
    QMutexLocker locker(&_loopMutex);
    _manualID = value;

    for (Joystick* joy : _joysticks) {
        joy->reset();
    }
}

void Processor::goalieID(int value) {
    QMutexLocker locker(&_loopMutex);
    _gameplayModule->goalieID(value);
}

int Processor::goalieID() {
    QMutexLocker locker(&_loopMutex);
    return _gameplayModule->goalieID();
}

void Processor::dampedRotation(bool value) {
    QMutexLocker locker(&_loopMutex);
    _dampedRotation = value;
}

void Processor::dampedTranslation(bool value) {
    QMutexLocker locker(&_loopMutex);
    _dampedTranslation = value;
}

void Processor::joystickKickOnBreakBeam(bool value) {
    QMutexLocker locker(&_loopMutex);
    _kickOnBreakBeam = value;
}

/**
 * sets the team
 * @param value the value indicates whether or not the current team is blue or
 * yellow
 */
void Processor::blueTeam(bool value) {
    // This is called from the GUI thread
    QMutexLocker locker(&_loopMutex);

    if (_blueTeam != value) {
        _blueTeam = value;
        if (_radio) _radio->switchTeam(_blueTeam);
        if (!externalReferee()) _refereeModule->blueTeam(value);
    }
}

bool Processor::joystickValid() const {
    for (Joystick* joy : _joysticks) {
        if (joy->valid()) return true;
    }
    return false;
}

void Processor::runModels(
    const vector<const SSL_DetectionFrame*>& detectionFrames) {
    vector<BallObservation> ballObservations;

    for (const SSL_DetectionFrame* frame : detectionFrames) {
        RJ::Time time = RJ::Time(chrono::duration_cast<chrono::microseconds>(
            RJ::Seconds(frame->t_capture())));

        // Add ball observations
        ballObservations.reserve(ballObservations.size() +
                                 frame->balls().size());
        for (const SSL_DetectionBall& ball : frame->balls()) {
            ballObservations.push_back(BallObservation(
                _worldToTeam * Point(ball.x() / 1000, ball.y() / 1000), time));
        }

        // Add robot observations
        const RepeatedPtrField<SSL_DetectionRobot>& selfRobots =
            _blueTeam ? frame->robots_blue() : frame->robots_yellow();

        std::vector<std::array<RobotObservation, RobotFilter::Num_Cameras>>
            robotObservations{_state.self.size()};

        // Collect camera data from all robots
        for (const SSL_DetectionRobot& robot : selfRobots) {
            unsigned int id = robot.robot_id();

            if (id < _state.self.size()) {
                const float angleRad =
                    fixAngleRadians(robot.orientation() + _teamAngle);
                const auto camera_id = frame->camera_id();
                robotObservations[id][camera_id] = RobotObservation(
                    _worldToTeam * Point(robot.x() / 1000, robot.y() / 1000),
                    angleRad, time, frame->frame_number(), true, camera_id);
            }
        }

        // Run robots through filter
        for (int i = 0; i < robotObservations.size(); i++) {
            _state.self[i]->filter()->update(robotObservations[i],
                                             _state.self[i], time,
                                             frame->frame_number());
        }

        const RepeatedPtrField<SSL_DetectionRobot>& oppRobots =
            _blueTeam ? frame->robots_yellow() : frame->robots_blue();

        std::vector<std::array<RobotObservation, RobotFilter::Num_Cameras>>
            oppRobotObservations{_state.self.size()};

        for (const SSL_DetectionRobot& robot : oppRobots) {
            unsigned int id = robot.robot_id();

            if (id < _state.self.size()) {
                const float angleRad =
                    fixAngleRadians(robot.orientation() + _teamAngle);
                const auto camera_id = frame->camera_id();
                oppRobotObservations[id][camera_id] = RobotObservation(
                    _worldToTeam * Point(robot.x() / 1000, robot.y() / 1000),
                    angleRad, time, frame->frame_number(), true, camera_id);
            }
        }

        for (int i = 0; i < oppRobotObservations.size(); i++) {
            _state.opp[i]->filter()->update(oppRobotObservations[i],
                                            _state.opp[i], time,
                                            frame->frame_number());
        }
    }

    _ballTracker->run(ballObservations, &_state);
}

/**
 * program loop
 */
void Processor::run() {
    Status curStatus;

    bool first = true;
    // main loop
    while (_running) {
        RJ::Time startTime = RJ::now();
        auto deltaTime = startTime - curStatus.lastLoopTime;
        _framerate = RJ::Seconds(1) / deltaTime;
        curStatus.lastLoopTime = startTime;
        _state.time = startTime;

        if (!firstLogTime) {
            firstLogTime = startTime;
        }

        ////////////////
        // Reset

        // Make a new log frame
        _state.logFrame = std::make_shared<Packet::LogFrame>();
        _state.logFrame->set_timestamp(RJ::timestamp());
        _state.logFrame->set_command_time(
            RJ::timestamp(startTime + Command_Latency));
        _state.logFrame->set_use_our_half(_useOurHalf);
        _state.logFrame->set_use_opponent_half(_useOpponentHalf);
        _state.logFrame->set_manual_id(_manualID);
        _state.logFrame->set_blue_team(_blueTeam);
        _state.logFrame->set_defend_plus_x(_defendPlusX);

        if (first) {
            first = false;

            Packet::LogConfig* logConfig =
                _state.logFrame->mutable_log_config();
            logConfig->set_generator("soccer");
            logConfig->set_git_version_hash(git_version_hash);
            logConfig->set_git_version_dirty(git_version_dirty);
            logConfig->set_simulation(_simulation);
        }

        for (OurRobot* robot : _state.self) {
            // overall robot config
            switch (robot->hardwareVersion()) {
                case Packet::RJ2008:
                    robot->config = robotConfig2008;
                    break;
                case Packet::RJ2011:
                    robot->config = robotConfig2011;
                    break;
                case Packet::RJ2015:
                    robot->config = robotConfig2015;
                    break;
                case Packet::Unknown:
                    robot->config =
                        robotConfig2011;  // FIXME: defaults to 2011 robots
                    break;
            }

            // per-robot configs
            robot->status = robotStatuses.at(robot->shell());
        }

        ////////////////
        // Inputs

        // Read vision packets
        vector<const SSL_DetectionFrame*> detectionFrames;
        vector<VisionPacket*> visionPackets;
        vision.getPackets(visionPackets);
        for (VisionPacket* packet : visionPackets) {
            SSL_WrapperPacket* log = _state.logFrame->add_raw_vision();
            log->CopyFrom(packet->wrapper);

            curStatus.lastVisionTime = packet->receivedTime;

            // If packet has geometry data, attempt to read information and
            // update if changed.
            if (packet->wrapper.has_geometry()) {
                updateGeometryPacket(packet->wrapper.geometry().field());
            }

            if (packet->wrapper.has_detection()) {
                SSL_DetectionFrame* det = packet->wrapper.mutable_detection();

                double rt =
                    RJ::numSeconds(packet->receivedTime.time_since_epoch());
                det->set_t_capture(rt - det->t_sent() + det->t_capture());
                det->set_t_sent(rt);

                // Remove balls on the excluded half of the field
                google::protobuf::RepeatedPtrField<SSL_DetectionBall>* balls =
                    det->mutable_balls();
                for (int i = 0; i < balls->size(); ++i) {
                    float x = balls->Get(i).x();
                    // FIXME - OMG too many terms
                    if ((!_state.logFrame->use_opponent_half() &&
                         ((_defendPlusX && x < 0) ||
                          (!_defendPlusX && x > 0))) ||
                        (!_state.logFrame->use_our_half() &&
                         ((_defendPlusX && x > 0) ||
                          (!_defendPlusX && x < 0)))) {
                        balls->SwapElements(i, balls->size() - 1);
                        balls->RemoveLast();
                        --i;
                    }
                }

                // Remove robots on the excluded half of the field
                google::protobuf::RepeatedPtrField<SSL_DetectionRobot>*
                    robots[2] = {det->mutable_robots_yellow(),
                                 det->mutable_robots_blue()};

                for (int team = 0; team < 2; ++team) {
                    for (int i = 0; i < robots[team]->size(); ++i) {
                        float x = robots[team]->Get(i).x();
                        if ((!_state.logFrame->use_opponent_half() &&
                             ((_defendPlusX && x < 0) ||
                              (!_defendPlusX && x > 0))) ||
                            (!_state.logFrame->use_our_half() &&
                             ((_defendPlusX && x > 0) ||
                              (!_defendPlusX && x < 0)))) {
                            robots[team]->SwapElements(
                                i, robots[team]->size() - 1);
                            robots[team]->RemoveLast();
                            --i;
                        }
                    }
                }

                detectionFrames.push_back(det);
            }
        }

        // Read radio reverse packets
        _radio->receive();

        for (Packet::RadioRx& rx : _radio->reversePackets()) {
            _state.logFrame->add_radio_rx()->CopyFrom(rx);

            curStatus.lastRadioRxTime =
                RJ::Time(chrono::microseconds(rx.timestamp()));

            // Store this packet in the appropriate robot
            unsigned int board = rx.robot_id();
            if (board < Num_Shells) {
                // We have to copy because the RX packet will survive past this
                // frame but LogFrame will not (the RadioRx in LogFrame will be
                // reused).
                _state.self[board]->setRadioRx(rx);
                _state.self[board]->radioRxUpdated();
            }
        }
        _radio->clear();

        for (Joystick* joystick : _joysticks) {
            joystick->update();
            if (joystick->valid()) break;
        }

        runModels(detectionFrames);
        for (VisionPacket* packet : visionPackets) {
            delete packet;
        }

        // Log referee data
        vector<NewRefereePacket*> refereePackets;
        _refereeModule.get()->getPackets(refereePackets);
        for (NewRefereePacket* packet : refereePackets) {
            SSL_Referee* log = _state.logFrame->add_raw_refbox();
            log->CopyFrom(packet->wrapper);
            curStatus.lastRefereeTime =
                std::max(curStatus.lastRefereeTime, packet->receivedTime);
            delete packet;
        }

        // Update gamestate w/ referee data
        _refereeModule->updateGameState(blueTeam());
        _refereeModule->spinKickWatcher();

        string yellowname, bluename;

        if (blueTeam()) {
            bluename = _state.gameState.OurInfo.name;
            yellowname = _state.gameState.TheirInfo.name;
        } else {
            yellowname = _state.gameState.OurInfo.name;
            bluename = _state.gameState.TheirInfo.name;
        }

        _state.logFrame->set_team_name_blue(bluename);
        _state.logFrame->set_team_name_yellow(yellowname);

        // Run high-level soccer logic
        _gameplayModule->run();

        // recalculates Field obstacles on every run through to account for
        // changing inset
        if (_gameplayModule->hasFieldEdgeInsetChanged()) {
            _gameplayModule->calculateFieldObstacles();
        }
        /// Collect global obstacles
        Geometry2d::ShapeSet globalObstacles =
            _gameplayModule->globalObstacles();
        Geometry2d::ShapeSet globalObstaclesWithGoalZones = globalObstacles;
        Geometry2d::ShapeSet goalZoneObstacles =
            _gameplayModule->goalZoneObstacles();
        globalObstaclesWithGoalZones.add(goalZoneObstacles);

        // Build a plan request for each robot.
        std::map<int, Planning::PlanRequest> requests;
        for (OurRobot* r : _state.self) {
            if (r && r->visible) {
                if (_state.gameState.state == GameState::Halt) {
                    r->setPath(nullptr);
                    continue;
                }

                // Visualize local obstacles
                for (auto& shape : r->localObstacles().shapes()) {
                    _state.drawShape(shape, Qt::black, "LocalObstacles");
                }

                auto& globalObstaclesForBot =
                    (r->shell() == _gameplayModule->goalieID() ||
                     r->isPenaltyKicker || r->isBallPlacer)
                        ? globalObstacles
                        : globalObstaclesWithGoalZones;

                // create and visualize obstacles
                Geometry2d::ShapeSet staticObstacles =
                    r->collectStaticObstacles(
                        globalObstaclesForBot,
                        !(r->shell() == _gameplayModule->goalieID() ||
                          r->isPenaltyKicker || r->isBallPlacer));

                std::vector<Planning::DynamicObstacle> dynamicObstacles =
                    r->collectDynamicObstacles();

                requests.emplace(
                    r->shell(),
                    Planning::PlanRequest(
                        _state, Planning::MotionInstant(r->pos, r->vel),
                        r->motionCommand()->clone(), r->robotConstraints(),
                        std::move(r->angleFunctionPath.path),
                        std::move(staticObstacles), std::move(dynamicObstacles),
                        r->shell(), r->getPlanningPriority()));
            }
        }

        // Run path planner and set the path for each robot that was planned for
        auto pathsById = _pathPlanner->run(std::move(requests));
        for (auto& entry : pathsById) {
            OurRobot* r = _state.self[entry.first];
            auto& path = entry.second;
            path->draw(&_state, Qt::magenta, "Planning");
            path->drawDebugText(&_state);
            r->setPath(std::move(path));

            r->angleFunctionPath.angleFunction =
                angleFunctionForCommandType(r->rotationCommand());
        }

        // Visualize obstacles
        for (auto& shape : globalObstacles.shapes()) {
            _state.drawShape(shape, Qt::black, "Global Obstacles");
        }

        // Run velocity controllers
        for (OurRobot* robot : _state.self) {
            if (robot->visible) {
                if ((_manualID >= 0 && (int)robot->shell() == _manualID) ||
                    _state.gameState.halt()) {
                    robot->motionControl()->stopped();
                } else {
                    robot->motionControl()->run();
                }
            }
        }

        ////////////////
        // Store logging information

        // Debug layers
        const QStringList& layers = _state.debugLayers();
        for (const QString& str : layers) {
            _state.logFrame->add_debug_layers(str.toStdString());
        }

        // Add our robots data to the LogFram
        for (OurRobot* r : _state.self) {
            if (r->visible) {
                r->addStatusText();

                Packet::LogFrame::Robot* log = _state.logFrame->add_self();
                *log->mutable_pos() = r->pos;
                *log->mutable_world_vel() = r->vel;
                *log->mutable_body_vel() = r->vel.rotated(M_PI_2 - r->angle);
                //*log->mutable_cmd_body_vel() = r->
                // *log->mutable_cmd_vel() = r->cmd_vel;
                // log->set_cmd_w(r->cmd_w);
                log->set_shell(r->shell());
                log->set_angle(r->angle);
                auto radioRx = r->radioRx();
                if (radioRx.has_kicker_voltage()) {
                    log->set_kicker_voltage(radioRx.kicker_voltage());
                }

                if (radioRx.has_kicker_status()) {
                    log->set_charged(radioRx.kicker_status() & 0x01);
                    log->set_kicker_works(!(radioRx.kicker_status() & 0x90));
                }

                if (radioRx.has_ball_sense_status()) {
                    log->set_ball_sense_status(radioRx.ball_sense_status());
                }

                if (radioRx.has_battery()) {
                    log->set_battery_voltage(radioRx.battery());
                }

                log->mutable_motor_status()->Clear();
                log->mutable_motor_status()->MergeFrom(radioRx.motor_status());

                if (radioRx.has_quaternion()) {
                    log->mutable_quaternion()->Clear();
                    log->mutable_quaternion()->MergeFrom(radioRx.quaternion());
                } else {
                    log->clear_quaternion();
                }

                for (const Packet::DebugText& t : r->robotText) {
                    log->add_text()->CopyFrom(t);
                }
            }
        }

        // Opponent robots
        for (OpponentRobot* r : _state.opp) {
            if (r->visible) {
                Packet::LogFrame::Robot* log = _state.logFrame->add_opp();
                *log->mutable_pos() = r->pos;
                log->set_shell(r->shell());
                log->set_angle(r->angle);
                *log->mutable_world_vel() = r->vel;
                *log->mutable_body_vel() = r->vel.rotated(2 * M_PI - r->angle);
            }
        }

        // Ball
        if (_state.ball.valid) {
            Packet::LogFrame::Ball* log = _state.logFrame->mutable_ball();
            *log->mutable_pos() = _state.ball.pos;
            *log->mutable_vel() = _state.ball.vel;
        }

        ////////////////
        // Outputs

        // Send motion commands to the robots
        sendRadioData();

        // Write to the log
        _logger.addFrame(_state.logFrame);

        // Store processing loop status
        _statusMutex.lock();
        _status = curStatus;
        _statusMutex.unlock();

        // Processor Initialization Completed
        _initialized = true;

        ////////////////
        // Timing

        auto endTime = RJ::now();
        auto timeLapse = endTime - startTime;
        if (timeLapse < _framePeriod) {
            // Use system usleep, not QThread::usleep.
            //
            // QThread::usleep uses pthread_cond_wait which sometimes fails to
            // unblock.
            // This seems to depend on how many threads are blocked.
            ::usleep(RJ::numMicroseconds(_framePeriod - timeLapse));
        } else {
            //   printf("Processor took too long: %d us\n", lastFrameTime);
        }
    }
    vision.stop();
}

/*
 * Updates the geometry packet if different from the existing one,
 * Based on the geometry vision data.
 */
void Processor::updateGeometryPacket(const SSL_GeometryFieldSize& fieldSize) {
    if (fieldSize.field_lines_size() == 0) {
        return;
    }

    const SSL_FieldCicularArc* penalty = nullptr;
    const SSL_FieldCicularArc* center = nullptr;
    float displacement =
        Field_Dimensions::Default_Dimensions.GoalFlat();  // default displacment

    // Loop through field arcs looking for needed fields
    for (const SSL_FieldCicularArc& arc : fieldSize.field_arcs()) {
        if (arc.name() == "CenterCircle") {
            // Assume center circle
            center = &arc;
        } else if (arc.name() == "LeftFieldLeftPenaltyArc") {
            penalty = &arc;
        }
    }

    for (const SSL_FieldLineSegment& line : fieldSize.field_lines()) {
        if (line.name() == "RightPenaltyStretch") {
            displacement = abs(line.p2().y() - line.p1().y());
        }
    }

    float thickness = fieldSize.field_lines().Get(0).thickness() / 1000.0f;

    // The values we get are the center of the lines, we want to use the
    // outside, so we can add this as an offset.
    float adj = fieldSize.field_lines().Get(0).thickness() / 1000.0f / 2.0f;

    float fieldBorder = currentDimensions->Border();

    if (penalty != nullptr && center != nullptr && thickness != 0) {
        // Force a resize
        Field_Dimensions newDim = Field_Dimensions(
            fieldSize.field_length() / 1000.0f,
            fieldSize.field_width() / 1000.0f, fieldBorder, thickness,
            fieldSize.goal_width() / 1000.0f, fieldSize.goal_depth() / 1000.0f,
            Field_Dimensions::Default_Dimensions.GoalHeight(),
            penalty->radius() / 1000.0f + adj,  // PenaltyDist
            Field_Dimensions::Default_Dimensions.PenaltyDiam(),
            penalty->radius() / 1000.0f + adj,       // ArcRadius
            center->radius() / 1000.0f + adj,        // CenterRadius
            (center->radius()) * 2 / 1000.0f + adj,  // CenterDiameter
            displacement / 1000.0f,                  // GoalFlat
            (fieldSize.field_length() / 1000.0f + (fieldBorder)*2),
            (fieldSize.field_width() / 1000.0f + (fieldBorder)*2));

        if (newDim != *currentDimensions) {
            // Set the changed field dimensions to the current ones
            cout << "Updating field geometry based off of vision packet."
                 << endl;
            setFieldDimensions(newDim);
        }

    } else {
        cerr << "Error: failed to decode SSL geometry packet. Not resizing "
                "field." << endl;
        std::cerr << fieldSize.field_length() << std::endl;
        std::cerr << fieldSize.field_width() << std::endl;
        std::cerr << fieldSize.goal_width() << std::endl;

    }
}

void Processor::sendRadioData() {
    auto currentTime = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = currentTime - xbeePacketSentTime;
    if (elapsed_seconds.count() >= XBEE_PACKET_DELAY*5) {
        printf("packet sent time %u\n", std::chrono::system_clock::now());
        // Kill switch for robots, by pressing HALT
        if (_state.gameState.halt()) {
            ShittyPacket johnPacket;
                johnPacket.robot_id = -1;  //this is broadcast to all in firmware
                johnPacket.robot_x = 0;
                johnPacket.robot_y = 0;
                johnPacket.robot_w = 0;
                if (_radio) {
                    _radio->send(johnPacket.serialize());
                }  
        }

        // Add RadioTx commands for visible robots and apply joystick input
        
        /*for (OurRobot* r : _state.self) {
            if (r->visible || _manualID == r->shell()) {
                std::cout << "VISIBLE BOYS" <<std::endl;
                Packet::Robot* txRobot = tx->add_robots();

                // Copy motor commands.
                // Even if we are using the joystick, this sets robot_id and the
                // number of motors.
                //txRobot->CopyFrom(r->robotPacket);
                //std::cout << r->xvelocity() << "\n";

                // if (r->shell() == _manualID) {
                //     const JoystickControlValues controlVals =
                //         getJoystickControlValues();
                //     applyJoystickControls(controlVals, txRobot->mutable_control(),
                //                           r);
                // }
            }
        }*/

        const float JOHN_SCALE = 70.0;
        for (OurRobot* r : _state.self) {
            if (r->visible || _manualID == r->shell()) {

                if (VERBOSE) {
                    std::cout << "EHH GOOD MORNING FROM ROBOT" << " " << r->shell() << std::endl;
                    std::cout << r->motionControl()->xvel * JOHN_SCALE << " " 
                              << r->motionControl()->yvel * JOHN_SCALE << " " 
                              << r->motionControl()->wvel * JOHN_SCALE<< std::endl;
                }

                // Joystick should override the commands given by everyone else
                ShittyPacket packet2;
                if (_manualID != r->shell()) {
                    int16_t robot_id = static_cast<int16_t>(r->shell());  
                    auto xvel = r->motionControl()->xvel * JOHN_SCALE;
                    auto yvel = r->motionControl()->yvel * JOHN_SCALE;
                    auto avel = r->motionControl()->wvel * JOHN_SCALE * 0.25;
                    packet2.robot_id = robot_id;
                    packet2.robot_x = (int16_t)(xvel);
                    packet2.robot_y = (int16_t)(yvel);
                    packet2.robot_w = (int16_t)(avel);
                } else {
                    const JoystickControlValues controlVals = getJoystickControlValues();
                    Geometry2d::Point translation(controlVals.translation);
                    float scaleMovement = 1.0 + (float) controlVals.kickPower/100.0;
                    float scaleRotation = 1.0 + (float) controlVals.dribblerPower/50.0;
                    packet2.robot_id = _manualID;
                    packet2.robot_x = static_cast<int16_t>(translation.x() * 100.0 * scaleMovement);
                    packet2.robot_y = static_cast<int16_t>(translation.y() * 100.0 * scaleMovement);
                    packet2.robot_w = static_cast<int16_t>(controlVals.rotation * 5.0 * scaleRotation);
                }
                if (_radio) _radio->send(packet2.serialize());
            }
            else {
                if (r->shell() == 8) {
                    ShittyPacket packet2;
                    int16_t robot_id = static_cast<int16_t>(r->shell());  
                    packet2.robot_id = robot_id;
                    packet2.robot_x = 0;
                    packet2.robot_y = 0;
                    packet2.robot_w = 0;
                    if (_radio) {
                        _radio->send(packet2.serialize());
                    } 
                }
            }  
        }
        xbeePacketSentTime = std::chrono::system_clock::now();
    } else {
        return;
    }


    // //WFUEDIT
    // const JoystickControlValues controlVals = getJoystickControlValues();
    // Geometry2d::Point translation(controlVals.translation);

    // ShittyPacket packet;
    // if (_state.gameState.halt()) {
    //     packet.robot_x = 0;
    //     packet.robot_y = 0;
    //     packet.robot_w = 0;  
    //     packet.robot_id = 0; 
    // } else {
    //     // translation has a max of 0.7 on each scale
    //     // max kickpower 255
    //     // max dribblerpower 128
    //     float scaleMovement = 1.0 + (float) controlVals.kickPower/100.0;
    //     float scaleRotation = 1.0 + (float) controlVals.dribblerPower/50.0;
        
    //     packet.robot_id = 9;
    //     packet.robot_x = static_cast<int16_t>(translation.x() * 100.0 * scaleMovement);
    //     packet.robot_y = static_cast<int16_t>(translation.y() * 100.0 * scaleMovement);
    //     packet.robot_w = static_cast<int16_t>(controlVals.rotation * 5.0 * scaleRotation);
    // }

    /*
    auto currentTime = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = currentTime - xbeePacketSentTime;
    if (elapsed_seconds.count() >= XBEE_PACKET_DELAY) {
        if (_radio) {
            _radio->send(packet.serialize());
        }
        // std::cout << "Called the XBEE" << elapsed_seconds << std::endl;
        xbeePacketSentTime = std::chrono::system_clock::now();
    }  
    */
}

void Processor::applyJoystickControls(const JoystickControlValues& controlVals,
                                      Packet::Control* tx, OurRobot* robot) {
    Geometry2d::Point translation(controlVals.translation);

    // use world coordinates if we can see the robot
    // otherwise default to body coordinates
    if (robot && robot->visible && _useFieldOrientedManualDrive) {
        translation.rotate(-M_PI / 2 - robot->angle);
    }

    // translation
    tx->set_xvelocity(translation.x());
    tx->set_yvelocity(translation.y());

    // rotation
    tx->set_avelocity(controlVals.rotation);

    // kick/chip
    bool kick = controlVals.kick || controlVals.chip;
    tx->set_triggermode(kick
                            ? (_kickOnBreakBeam ? Packet::Control::ON_BREAK_BEAM
                                                : Packet::Control::IMMEDIATE)
                            : Packet::Control::STAND_DOWN);
    tx->set_kcstrength(controlVals.kickPower);
    tx->set_shootmode(controlVals.kick ? Packet::Control::KICK
                                       : Packet::Control::CHIP);

    // dribbler
    tx->set_dvelocity(controlVals.dribble ? controlVals.dribblerPower : 0);
}

JoystickControlValues Processor::getJoystickControlValues() {
    // if there's more than one joystick, we add their values
    JoystickControlValues vals;
    for (Joystick* joy : _joysticks) {
        if (joy->valid()) {
            JoystickControlValues newVals = joy->getJoystickControlValues();

            vals.dribble |= newVals.dribble;
            vals.kick |= newVals.kick;
            vals.chip |= newVals.chip;

            vals.rotation += newVals.rotation;
            vals.translation += newVals.translation;

            vals.dribblerPower =
                max<double>(vals.dribblerPower, newVals.dribblerPower);
            vals.kickPower = max<double>(vals.kickPower, newVals.kickPower);
            break;
        }
    }

    // keep it in range
    vals.translation.clamp(sqrt(2.0));
    if (vals.rotation > 1) vals.rotation = 1;
    if (vals.rotation < -1) vals.rotation = -1;

    // Gets values from the configured joystick control values,respecting damped
    // state
    if (_dampedTranslation) {
        vals.translation *=
            Joystick::JoystickTranslationMaxDampedSpeed->value();
    } else {
        vals.translation *= Joystick::JoystickTranslationMaxSpeed->value();
    }
    if (_dampedRotation) {
        vals.rotation *= Joystick::JoystickRotationMaxDampedSpeed->value();
    } else {
        vals.rotation *= Joystick::JoystickRotationMaxSpeed->value();
    }

    // scale up kicker and dribbler speeds
    vals.dribblerPower *= 128;
    vals.kickPower *= 255;

    return vals;
}

void Processor::defendPlusX(bool value) {
    _defendPlusX = value;

    if (_defendPlusX) {
        _teamAngle = -M_PI_2;
    } else {
        _teamAngle = M_PI_2;
    }

    recalculateWorldToTeamTransform();
}

void Processor::changeVisionChannel(int port) {
    _loopMutex.lock();

    vision.stop();

    vision.simulation = _simulation;
    vision.port = port;
    vision.start();

    _loopMutex.unlock();
}

void Processor::recalculateWorldToTeamTransform() {
    _worldToTeam = Geometry2d::TransformMatrix::translate(
        0, Field_Dimensions::Current_Dimensions.Length() / 2.0f);
    _worldToTeam *= Geometry2d::TransformMatrix::rotate(_teamAngle);
}

void Processor::setFieldDimensions(const Field_Dimensions& dims) {
    Field_Dimensions::Current_Dimensions = dims;
    recalculateWorldToTeamTransform();
    _gameplayModule->calculateFieldObstacles();
    _gameplayModule->updateFieldDimensions();
}

bool Processor::isRadioOpen() const { return _radio->isOpen(); }
bool Processor::isInitialized() const { return _initialized; }
