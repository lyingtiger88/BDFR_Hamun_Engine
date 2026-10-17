"""Pure Python reference evaluator for Hamun animation graph JSON (no Blender dependency)."""

COMPARISONS = {
    "GT": lambda a, b: a > b,
    "GE": lambda a, b: a >= b,
    "LT": lambda a, b: a < b,
    "LE": lambda a, b: a <= b,
    "EQ": lambda a, b: a == b,
    "NE": lambda a, b: a != b,
}


class StateMachine:
    def __init__(self, graph):
        if graph.get("format") != "hamun.animation.state_machine" or graph.get("version") != 1:
            raise ValueError("Unsupported Hamun state machine format/version")
        self.states = {state["id"]: state for state in graph["states"]}
        self.transitions = graph["transitions"]
        self.current = graph["entry"]
        if self.current not in self.states:
            raise ValueError("Entry state does not exist")
        for t in self.transitions:
            if t["from"] not in self.states or t["to"] not in self.states:
                raise ValueError("Transition references a missing state")
            if t["condition"]["operator"] not in COMPARISONS:
                raise ValueError("Invalid transition comparison")
        self.previous = None
        self.blend_seconds = 0.0

    def update(self, parameters):
        """Evaluate one transition per update; return the current state's authored data."""
        candidates = sorted(
            (t for t in self.transitions if t["from"] == self.current),
            key=lambda t: -t.get("priority", 0),
        )
        for t in candidates:
            condition = t["condition"]
            key = condition["parameter"]
            if key not in parameters:
                continue
            if COMPARISONS[condition["operator"]](parameters[key], condition["value"]):
                self.previous = self.current
                self.current = t["to"]
                self.blend_seconds = t["blend_seconds"]
                break
        return self.states[self.current]
