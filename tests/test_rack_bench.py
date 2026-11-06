"""The software bench of the rack (tools/rack_bench.py): the scripts it writes, how it reads the answers, and the run against the virtual rack when a compiler is there."""
import contextlib
import io
import os
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import rack_bench as bench  # noqa: E402


class FramesTests(unittest.TestCase):
    def test_a_command_frame_has_the_layout_of_the_protocol(self):
        data = bench.set_preheat(5, 0, 200)
        self.assertEqual(data[:5], bytes([0xA5, 2, 5, 1, 3]))
        self.assertEqual(data[5:8], bytes([0, 0, 200]))
        self.assertEqual(data[8], bench.crc8(data[:8]))

    def test_a_can_command_carries_the_tool_the_temperature_and_the_sequence(self):
        self.assertEqual(bench.set_preheat_can(9, 19, 0x0102), bytes([19, 1, 2, 9, 0, 0, 0, 0]))

    def test_the_reason_of_a_reply_is_read_from_the_serial_and_the_can_form(self):
        self.assertEqual(bench.reason_of(("C", bytes([1, 0, 7]))), "held_rate")
        serial_reply = bench.frame(3, 0x82, bytes([1, 0, 4]))
        self.assertEqual(bench.reason_of(("S", serial_reply)), "temp_out_of_range")


class ScriptTests(unittest.TestCase):
    def test_every_event_is_a_clock_line_a_command_a_state_request_and_a_marker(self):
        events = [bench.slot(0, 1, 19, 2), bench.serial(10, b"\xa5\x02"), bench.can(20, bytes(8)), bench.tick(30)]
        lines = bench.script_of(events).splitlines()
        self.assertEqual(lines[:4], ["T 0", "I 1 19 2", "Q", "M 0"])
        self.assertIn("S a502", lines)
        self.assertIn("C 260 0000000000000000", lines)
        self.assertEqual(lines[-1], "M 3")

    def test_the_answers_are_split_at_the_markers(self):
        text = "S a50201\nQ 0 1 0 1 200 0\nQ link 0\nQ dropped 0\nM 0\nC 261 010003\nQ link 1\nQ dropped 2\nM 1\n"
        steps = bench.parse_output(text, 2)
        self.assertEqual(steps[0].replies, [("S", bytes.fromhex("a50201"))])
        self.assertEqual(steps[0].slots, [(1, 0, 1, 200, 0)])
        self.assertFalse(steps[0].link_lost)
        self.assertEqual(steps[1].replies, [("C", bytes([1, 0, 3]))])
        self.assertEqual((steps[1].link_lost, steps[1].dropped), (True, 2))

    def test_a_line_outside_the_protocol_or_a_missing_step_is_an_error(self):
        with self.assertRaises(RuntimeError):
            bench.parse_output("X what\nM 0\n", 1)
        with self.assertRaises(RuntimeError):
            bench.parse_output("M 0\n", 2)


class ScenarioTests(unittest.TestCase):
    def test_the_scenarios_have_distinct_names_and_the_clock_never_goes_back(self):
        scenarios = [*bench.fixed_scenarios(), bench.random_scenario(1, 200)]
        self.assertEqual(len({s.name for s in scenarios}), len(scenarios))
        for scenario in scenarios:
            times = [event.at for event in scenario.events]
            self.assertEqual(times, sorted(times), scenario.name)

    def test_the_random_scenarios_repeat_for_a_seed_and_differ_between_seeds(self):
        self.assertEqual(bench.random_scenario(3, 100).events, bench.random_scenario(3, 100).events)
        self.assertNotEqual(bench.random_scenario(3, 100).events, bench.random_scenario(4, 100).events)

    def test_the_invariants_see_a_target_above_the_ceiling_a_target_in_an_empty_slot_and_a_target_with_the_link_lost(self):
        quiet = [(0, 31, 0, 0, 0)] * bench.SLOTS
        too_hot = bench.Step(slots=[(1, 10, 3, 320, 0)] + quiet[1:], link_lost=False)
        empty = bench.Step(slots=[(0, 31, 0, 100, 0)] + quiet[1:], link_lost=False)
        lost = bench.Step(slots=[(1, 0, 1, 100, 0)] + quiet[1:], link_lost=True)
        padding = [bench.Step(slots=list(quiet))] * (len(bench.RACK) + 1)
        for step in (too_hot, empty, lost):
            self.assertTrue(bench.random_invariants([*padding, step]), step)
        self.assertEqual(bench.random_invariants([*padding, bench.Step(slots=list(quiet))]), [])

    def test_a_difference_between_the_firmware_and_the_model_is_reported(self):
        events = [bench.tick(0)]
        a = bench.Step(replies=[("C", bytes([1, 0, 0]))], slots=[(1, 0, 1, 200, 0)], link_lost=False)
        same = bench.Step(replies=[("C", bytes([1, 0, 0]))], slots=[(1, 0, 1, 200, 0)], link_lost=False)
        other = bench.Step(replies=[("C", bytes([1, 0, 1]))], slots=[(1, 0, 1, 190, 0)], link_lost=True)
        self.assertEqual(bench.compare("x", [a], [same], events), [])
        self.assertEqual(len(bench.compare("x", [a], [other], events)), 3)

    def test_an_expectation_that_is_not_met_is_reported(self):
        scenario = bench.Scenario("x", "", [], replies=["applied"], targets={0: 200}, link_lost=False)
        step = bench.Step(replies=[("C", bytes([1, 0, 4]))], slots=[(1, 0, 1, 100, 0)], link_lost=True)
        self.assertEqual(len(bench.check_expectations(scenario, [step])), 3)

    def test_the_hand_written_expectations_hold_for_the_python_model_too(self):
        model = bench.import_model(os.environ.get("URTC_COMMON_SRC"))
        if model is None:
            self.skipTest("urtc_common is not importable (set URTC_COMMON_SRC to URTC-COMMON/src)")
        for scenario in [*bench.fixed_scenarios(), bench.random_scenario(11, 300)]:
            self.assertEqual(bench.check_expectations(scenario, bench.model_steps(model, scenario.events)), [], scenario.name)


class CommandLineTests(unittest.TestCase):
    def test_list_names_every_scenario_and_builds_nothing(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(bench.main(["--list"]), 0)
        for scenario in bench.fixed_scenarios():
            self.assertIn(scenario.name + ":", out.getvalue())

    def test_dump_writes_one_script_per_scenario(self):
        with tempfile.TemporaryDirectory() as directory, contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(bench.main(["--dump", directory, "--events", "20"]), 0)
            self.assertTrue((Path(directory) / "happy_path.txt").read_text(encoding="utf-8").startswith("T 0\n"))

    def test_without_a_compiler_it_says_skipped(self):
        original = bench.build
        bench.build = lambda binary: None
        try:
            with contextlib.redirect_stdout(io.StringIO()) as out:
                self.assertEqual(bench.main([]), 2)
            self.assertIn("SKIPPED", out.getvalue())
        finally:
            bench.build = original


class RealRackTests(unittest.TestCase):
    @unittest.skipIf(os.name == "nt", "the virtual rack is built on Linux (the CI); on Windows run tools/rack_bench.py with WSL")
    def test_every_scenario_passes_against_the_real_rack_logic(self):
        with tempfile.TemporaryDirectory() as directory, contextlib.redirect_stdout(io.StringIO()) as out:
            status = bench.main(["--build-dir", directory, "--events", "400"])
        if status == 2:
            self.skipTest("no host C compiler, or local builds are off (ALLOW_LOCAL_BUILD=1 allows them)")
        self.assertEqual(status, 0, out.getvalue()[-3000:])


if __name__ == "__main__":
    unittest.main()
