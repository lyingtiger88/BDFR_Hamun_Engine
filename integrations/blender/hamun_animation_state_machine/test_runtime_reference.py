"""Run: python -m unittest discover -s integrations/blender/hamun_animation_state_machine -p 'test_*.py'"""
import unittest
from runtime_reference import StateMachine


GRAPH = {
    "format": "hamun.animation.state_machine", "version": 1, "entry": "idle",
    "states": [{"id": "idle", "name": "Idle"}, {"id": "walk", "name": "Walk"}],
    "transitions": [
        {"from": "idle", "to": "walk",
         "condition": {"parameter": "Speed", "operator": "GT", "value": 0.1},
         "blend_seconds": 0.2, "priority": 0},
        {"from": "walk", "to": "idle",
         "condition": {"parameter": "Speed", "operator": "LE", "value": 0.1},
         "blend_seconds": 0.3, "priority": 0},
    ],
}


class StateMachineTests(unittest.TestCase):
    def test_idle_walk_idle(self):
        m = StateMachine(GRAPH)
        self.assertEqual(m.update({"Speed": 0})["name"], "Idle")
        self.assertEqual(m.update({"Speed": 1})["name"], "Walk")
        self.assertEqual(m.previous, "idle")
        self.assertEqual(m.blend_seconds, 0.2)
        self.assertEqual(m.update({"Speed": 0})["name"], "Idle")

    def test_missing_parameter_does_not_transition(self):
        m = StateMachine(GRAPH)
        self.assertEqual(m.update({})["name"], "Idle")

    def test_invalid_entry(self):
        graph = dict(GRAPH, entry="missing")
        with self.assertRaises(ValueError):
            StateMachine(graph)


if __name__ == "__main__":
    unittest.main()
