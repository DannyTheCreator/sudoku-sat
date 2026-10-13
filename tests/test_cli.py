"""Black-box CLI tests and a brute-force oracle; uses only Python's standard library."""
import itertools
import pathlib
import random
import subprocess
import sys
import tempfile
import unittest

EXECUTABLE = pathlib.Path(sys.argv.pop(1)).resolve()
PROJECT = pathlib.Path(sys.argv.pop(1)).resolve()


class CliTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="sudoku-sat-")
        self.work = pathlib.Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def run_cli(self, *args, expected=0, stdin=None):
        result = subprocess.run(
            [str(EXECUTABLE), *map(str, args)], cwd=self.work, input=stdin,
            text=True, capture_output=True, timeout=15,
        )
        self.assertEqual(result.returncode, expected, result.stdout + result.stderr)
        return result

    def test_help_and_invalid_inputs(self):
        self.assertIn("Sudoku SAT", self.run_cli("--help").stdout)
        for args in [("unknown",), ("sat",), ("solve", "missing.txt"),
                     ("generate", "--holes", "81"), ("generate", "--seed", "-1"),
                     ("generate", "--seed", "4294967296"),
                     ("generate", "--diagonal", "--unknown"),
                     ("generate", "--holes"), ("sat", "x.cnf", "--diagonal")]:
            self.run_cli(*args, expected=1)
        bad = self.work / "bad.txt"
        for text in ["123", "." * 82, "." * 80 + "x", "5" * 81]:
            bad.write_text(text, encoding="ascii")
            self.run_cli("solve", bad, expected=1)
        self.run_cli(stdin="bad\n0\n")
        self.run_cli(stdin="")

    def test_sudoku_pipeline_and_game(self):
        puzzle = PROJECT / "examples" / "standard.txt"
        solved = self.work / "solved board.txt"
        encoded = self.work / "puzzle.cnf"
        result_file = self.work / "puzzle.res"
        result = self.run_cli("solve", puzzle, "--check-unique", "-o", solved)
        self.assertIn("Solutions: unique", result.stdout)
        self.assertEqual(solved.read_text().splitlines()[-1], "345286179")
        self.run_cli("encode", puzzle, encoded)
        self.run_cli("sat", encoded, "-o", result_file)
        model = next(line for line in result_file.read_text().splitlines() if line.startswith("v "))
        self.assertEqual(len(model.split()[1:-1]), 729)
        game = self.run_cli("play", puzzle, stdin=(
            "set 1 1 9\nset 1 3 5\nset 0 2 3\n"
            "set 999999999999999999999999999 2 3\nclear 1 3\n"
            "clear 1 3\nhint 1 3\ncheck\nanswer\nquit\n"
        ))
        self.assertIn("original clue", game.stdout)
        self.assertIn("Move conflicts", game.stdout)
        self.assertIn("can be completed", game.stdout)
        unsat = self.work / "unsat.txt"
        text = "".join(puzzle.read_text().split())
        unsat.write_text(text[:2] + "1" + text[3:], encoding="ascii")
        self.run_cli("solve", unsat, expected=2)
        self.run_cli("solve", puzzle, "-o", self.work / "missing" / "out.txt", expected=1)
        self.run_cli("play", solved)

    def test_interactive_generation_and_sat(self):
        self.run_cli(stdin="3\n40\ny\nmenu-x.txt\nn\n0\n")
        puzzle = self.work / "menu-x.txt"
        self.assertEqual(puzzle.read_text().count("."), 40)
        result = self.run_cli("solve", puzzle, "--diagonal", "--check-unique")
        self.assertIn("Solutions: unique", result.stdout)
        self.run_cli(stdin=f"1\n{PROJECT / 'examples' / 'sat.cnf'}\nmenu.res\n0\n")
        self.assertTrue((self.work / "menu.res").read_text().startswith("s 1\n"))

    def test_generation_reproducibility_and_diagonal(self):
        for diagonal in [False, True]:
            flags = ["--diagonal"] if diagonal else []
            outputs = []
            for i in range(2):
                puzzle = self.work / f"generated-{diagonal}-{i}.txt"
                solution = self.work / f"answer-{diagonal}-{i}.txt"
                self.run_cli("generate", "--holes", "50", "--seed", "42",
                             *flags, "-o", puzzle, "--solution", solution)
                outputs.append((puzzle.read_text(), solution.read_text()))
                self.assertEqual(puzzle.read_text().count("."), 50)
                solved = self.work / "solved.txt"
                result = self.run_cli("solve", puzzle, *flags, "--check-unique", "-o", solved)
                self.assertIn("Solutions: unique", result.stdout)
                self.assertEqual(solved.read_text(), solution.read_text())
                if diagonal:
                    grid = "".join(solution.read_text().split())
                    self.assertEqual(set(grid[0::10]), set("123456789"))
                    self.assertEqual(set(grid[8:73:8]), set("123456789"))
            self.assertEqual(outputs[0], outputs[1])

    def test_unsat_and_parser_errors(self):
        result_file = self.work / "unsat.res"
        self.run_cli("sat", PROJECT / "examples" / "unsat.cnf", "-o", result_file, expected=2)
        self.assertTrue(result_file.read_text().startswith("s 0\n"))
        for fixture in (PROJECT / "tests" / "data").glob("bad-*.cnf"):
            self.run_cli("sat", fixture, expected=1)

    def test_original_cnf_compatibility(self):
        for diagonal in [False, True]:
            mode = "diagonal" if diagonal else "standard"
            puzzle = PROJECT / "examples" / f"{mode}.txt"
            grid = "".join(puzzle.read_text().split())
            givens = [i * 9 + int(digit) for i, digit in enumerate(grid) if digit not in ".0"]
            original = PROJECT / "examples" / "legacy" / f"{mode}-base.cnf"
            lines = original.read_text().splitlines()
            declared = int(lines[0].split()[3])
            self.assertEqual(declared, sum(line.strip().endswith(" 0") for line in lines[1:]))
            cnf = self.work / f"legacy-{mode}.cnf"
            cnf.write_text(f"p cnf 729 {declared + len(givens)}\n" + "\n".join(lines[1:]) + "\n" +
                           "".join(f"{literal} 0\n" for literal in givens), encoding="ascii")
            model_path = self.work / f"legacy-{mode}.res"
            self.run_cli("sat", cnf, "-o", model_path)
            model = next(line for line in model_path.read_text().splitlines() if line.startswith("v "))
            decoded = ["0"] * 81
            for literal in map(int, model.split()[1:-1]):
                if literal > 0:
                    decoded[(literal - 1) // 9] = str((literal - 1) % 9 + 1)
            solution = self.work / f"modern-{mode}.txt"
            self.run_cli("solve", puzzle, *(["--diagonal"] if diagonal else []), "-o", solution)
            self.assertEqual("".join(decoded), "".join(solution.read_text().split()))

    def test_random_cnf_against_exhaustive_oracle(self):
        rng = random.Random(20261013)
        cnf = self.work / "oracle.cnf"
        output = self.work / "oracle.res"
        for case in range(250):
            variables = rng.randint(0, 7)
            clauses = []
            for _ in range(rng.randint(0, 18)):
                length = rng.randint(0, 6) if variables else 0
                clauses.append([rng.choice((-1, 1)) * rng.randint(1, variables) for _ in range(length)])
            expected = any(
                all(any(values[abs(lit) - 1] == (lit > 0) for lit in clause) for clause in clauses)
                for values in itertools.product((False, True), repeat=variables)
            )
            cnf.write_text(f"c Oracle case {case}\np cnf {variables} {len(clauses)}\n" +
                           "".join(" ".join(map(str, clause)) + " 0\n" for clause in clauses), encoding="ascii")
            self.run_cli("sat", cnf, "-o", output, expected=0 if expected else 2)
            lines = output.read_text().splitlines()
            self.assertEqual(lines[0], "s 1" if expected else "s 0", f"case {case}")
            if expected:
                literals = list(map(int, lines[1].split()[1:]))
                self.assertEqual(literals[-1], 0)
                model = {abs(lit): lit > 0 for lit in literals[:-1]}
                self.assertEqual(set(model), set(range(1, variables + 1)))
                self.assertTrue(all(any(model[abs(lit)] == (lit > 0) for lit in clause) for clause in clauses))


if __name__ == "__main__":
    unittest.main()
