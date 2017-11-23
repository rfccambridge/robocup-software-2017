import single_robot_behavior
import behavior
import robocup
import time
## Behavior that moves a robot to a specified location
# wraps up OurRobot.move() into a Skill so we can use it in the play system more easily
class Move(single_robot_behavior.SingleRobotBehavior):
    x = 0
    def __init__(self, pos=None):

        super().__init__(continuous=True) # was originally false

        self.threshold = 0.05
        self.pos = pos
        self._start_time = time.time()
        self._x = 0
        self.add_transition(behavior.Behavior.State.start,
                            behavior.Behavior.State.running, lambda: True,
                            'immediately')

        self.add_transition(
            behavior.Behavior.State.running, behavior.Behavior.State.completed,
            lambda: self.pos != None and (self.robot.pos - self.pos).mag() < self.threshold,
            'target pos reached')
        self.add_transition(
            behavior.Behavior.State.completed, behavior.Behavior.State.running,
            lambda: self.pos != None and (self.robot.pos - self.pos).mag() > self.threshold,
            'away from target')

    ## the position to move to (a robocup.Point object)
    @property
    def pos(self):
        return self._pos

    @pos.setter
    def pos(self, value):
        self._pos = value

    ## how close (in meters) the robot has to be to the target position for it be complete
    @property
    def threshold(self):
        return self._threshold

    @threshold.setter
    def threshold(self, value):
        self._threshold = value

    def print_info(self):
        print(self.robot.pos)

    def execute_running(self):
        #print(self._start_time)
        self._time = time.time()
        speeds = [robocup.Point(1, 0), robocup.Point(0, 0),robocup.Point(-1, 0),robocup.Point(0, 0)]#,
                    # robocup.Point(0, 1), robocup.Point(0, -1)]
        #print(type(speeds[1]))
        # start = time.time()
        #print(speeds[self._x])
        if self.pos != None:
            if self._time  - self._start_time >1:
                self._start_time =time.time()
                self._x = self._x+1
                if(self._x == 4):
                    self._x = 0
            self.robot.move_to_158(speeds[self._x])
        #if self.pos != None:
        #    self.robot.move_to_158(self.pos)

    def role_requirements(self):
        reqs = super().role_requirements()
        reqs.destination_shape = self.pos
        return reqs
