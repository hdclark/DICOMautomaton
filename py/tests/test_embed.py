#!/usr/bin/env python3

import os
from pathlib import Path
import subprocess
import tempfile
import textwrap
import unittest


class EmbeddedPythonTest(unittest.TestCase):
    def setUp(self):
        self.dispatcher = Path(os.environ["DCMA_BIN"])

    def test_mutation_native_continuation_reuse_and_lifetime(self):
        with tempfile.TemporaryDirectory() as directory_name:
            directory = Path(directory_name)
            first = directory / "first.py"
            second = directory / "second.py"
            marker = directory / "success.txt"
            first.write_text(textwrap.dedent("""
                import builtins
                import os
                import sys

                if os.environ.get("DCMA_EXPECT_PACKAGE_IMPORT"):
                    import dicomautomaton

                def process(session):
                    table = session.data.get_table(0)
                    session.data.set_table(0, [(0, 0, "changed")], table.metadata)
                    builtins._dcma_expired_session = session
                    sys.path = list(sys.path)
            """))
            second.write_text(textwrap.dedent(f"""
                import builtins
                import sys

                def process(session):
                    assert {str(directory)!r} not in sys.path
                    try:
                        builtins._dcma_expired_session.data.table_count
                    except RuntimeError as error:
                        assert "no longer active" in str(error)
                    else:
                        raise AssertionError("expired operation session remained usable")
                    assert session.data.table_count == 2
                    assert session.data.get_table(0).cells == [(0, 0, "changed")]
                    with open({str(marker)!r}, "w", encoding="utf-8") as stream:
                        stream.write("ok")
            """))

            command = [
                str(self.dispatcher),
                "-o", "GenerateTable",
                "-o", "Python", "-p", f"Filename={first}",
            ]
            environment = os.environ.copy()
            module_search_paths = [str(directory)]
            python_path = os.environ.get("DCMA_PYTHONPATH")
            if python_path:
                module_search_paths.append(python_path)
                environment["DCMA_EXPECT_PACKAGE_IMPORT"] = "1"
            command.extend(["-p", f"ModuleSearchPaths={';'.join(module_search_paths)}"])
            command.extend([
                "-o", "CopyTables",
                "-o", "PythonScript", "-p", f"Filename={second}",
            ])
            result = subprocess.run(
                command,
                capture_output=True,
                text=True,
                env=environment,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertTrue(marker.exists(), result.stdout + result.stderr)
            self.assertEqual(marker.read_text(), "ok", result.stderr)

    def test_traceback_is_reported(self):
        with tempfile.TemporaryDirectory() as directory_name:
            script = Path(directory_name) / "failure.py"
            script.write_text(textwrap.dedent("""
                def process(session):
                    raise RuntimeError("embedded traceback sentinel")
            """))
            result = subprocess.run(
                [str(self.dispatcher), "-o", "Python", "-p", f"Filename={script}"],
                capture_output=True,
                text=True,
            )
            diagnostics = result.stdout + result.stderr
            self.assertNotEqual(result.returncode, 0, diagnostics)
            self.assertIn("embedded traceback sentinel", diagnostics)
            self.assertIn("failure.py", diagnostics)
            self.assertIn("process", diagnostics)


if __name__ == "__main__":
    unittest.main()
