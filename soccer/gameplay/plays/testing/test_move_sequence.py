import single_robot_sequence
import skills.move_158
import behavior
import play
import robocup


class TestMoveSequence(play.Play):
    def __init__(self):
        super().__init__(continuous=True)

        self.add_transition(behavior.Behavior.State.start,
                            behavior.Behavior.State.running, lambda: True,
                            'immediately')

        #arbitrary points to move to
        positions = [robocup.Point(0, 1.5), robocup.Point(2, 0),
                     robocup.Point(0, 3), robocup.Point(-3, 0)]
        sequence = single_robot_sequence.SingleRobotSequence(
            repeat=True,
            behaviors=list(map(skills.move_158.Move, positions)))
        self.add_subbehavior(sequence, 'sequence', required=False)

    #keep running the sequence while testing
    def execute_running(self):
        sequence = self.subbehavior_with_name('sequence')
        #print(sequence.behavior_state)
        #print(skills.move.Move.print_info)
        if sequence.is_done_running():
            print("Restarting sequence manually")
            sequence.restart()
