import ast
from pathlib import Path
import unittest


ADDON = Path(__file__).parents[1] / "hamun_authoring_tools" / "__init__.py"


class AddonSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = ADDON.read_text(encoding="utf-8")
        cls.tree = ast.parse(cls.source)

    def test_source_parses(self):
        self.assertIsNotNone(self.tree)

    def test_register_and_unregister_exist(self):
        functions = {
            node.name for node in self.tree.body if isinstance(node, ast.FunctionDef)
        }
        self.assertIn("register", functions)
        self.assertIn("unregister", functions)

    def test_native_transform_actions_are_present(self):
        for operator in (
            "transform.translate",
            "transform.rotate",
            "transform.resize",
        ):
            self.assertIn(operator, self.source)

    def test_scene_metadata_is_versioned(self):
        self.assertIn('scene["hamun_schema_version"] = 1', self.source)
        self.assertIn('scene["hamun_project"] = True', self.source)


if __name__ == "__main__":
    unittest.main()
