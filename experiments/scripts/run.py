"""Compare the project CLI with SciPy's independent HiGHS IPM solver."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
import math
from pathlib import Path
import platform
import statistics
import subprocess
import time

import numpy as np
import scipy
from scipy.optimize import linprog


EXPERIMENTS = Path(__file__).resolve().parents[1]
SEED = 20261108
SIZES = (8, 16, 32, 64)
TRIALS_PER_SIZE = 5
FEASIBILITY_TOLERANCE = 1e-7
OBJECTIVE_TOLERANCE = 1e-8
REFERENCE_STATUS = {0: "optimal", 1: "iteration_limit", 2: "infeasible", 3: "unbounded"}
FIXTURES = (
    ("worked-example", "optimal", 10),
    ("negative-b", "optimal", 4),
    ("infeasible", "infeasible", None),
    ("unbounded", "unbounded", None),
    ("degenerate", "optimal", 1),
    ("multiple-optima", "optimal", 1),
    ("scaled", "optimal", 10),
    ("no-constraints", "optimal", 0),
)


@dataclass(frozen=True)
class Problem:
    name: str
    direction: str
    a: np.ndarray
    b: np.ndarray
    c: np.ndarray
    expected_status: str
    expected_objective: float | None = None

    def serialize(self) -> str:
        m, n = self.a.shape
        rows = [self.c, *np.column_stack((self.a, self.b))]
        body = "\n".join(" ".join(format(value, ".17g") for value in row) for row in rows)
        return f"LP1 {self.direction} {m} {n}\n{body}\n"


def fixtures():
    for name, status, objective in FIXTURES:
        source = EXPERIMENTS / "data" / "input" / f"{name}.lp"
        version, direction, rows, columns, *tokens = source.read_text(encoding="utf-8").split()
        if version != "LP1" or direction not in {"min", "max"}:
            raise ValueError(f"Invalid header in {source}")
        m, n = int(rows), int(columns)
        c = np.array(tokens[:n], dtype=float)
        constraints = np.array(tokens[n:], dtype=float).reshape(m, n + 1)
        yield Problem(name, direction, constraints[:, :-1], constraints[:, -1], c, status, objective)


def dense_problems():
    rng = np.random.default_rng(SEED)
    for n in SIZES:
        for trial in range(1, TRIALS_PER_SIZE + 1):
            witness = rng.integers(2, 10, size=n)
            dense_a = rng.integers(-6, 7, size=(n, n))
            dense_b = dense_a @ witness + rng.integers(1, 9, size=n)
            identity = np.eye(n)
            a = np.vstack((dense_a, identity, -identity[0]))
            b = np.concatenate((dense_b, witness + rng.integers(1, 9, size=n), [-1]))
            c = rng.integers(-9, 10, size=n)
            yield Problem(f"dense-{n}-{trial}", "max", a, b, c, "optimal")


def invoke(binary: Path, source: str) -> dict:
    process = subprocess.run(
        [str(binary)],
        input=source,
        encoding="utf-8",
        capture_output=True,
        check=True,
        timeout=15,
    )
    return json.loads(process.stdout)


def compare(binary: Path, problem: Problem, repeats: int) -> dict:
    m, n = problem.a.shape
    source = problem.serialize()
    result = invoke(binary, source)
    samples = []
    for _ in range(repeats):
        start = time.perf_counter_ns()
        repeated = invoke(binary, source)
        samples.append((time.perf_counter_ns() - start) / 1e6)
        if repeated != result:
            raise RuntimeError(f"{problem.name}: different results from identical CLI calls")

    reference = linprog(
        -problem.c if problem.direction == "max" else problem.c,
        A_ub=problem.a if m else None,
        b_ub=problem.b if m else None,
        bounds=(0, None),
        method="highs-ipm",
    )
    reference_status = REFERENCE_STATUS.get(reference.status, "numerical_failure")
    cli_status = result["status"]
    passed = cli_status == reference_status == problem.expected_status
    violation = None
    objective_gap = None
    reference_objective = None
    if reference_status == "optimal":
        reference_objective = float(problem.c @ reference.x)
    if cli_status == "optimal":
        x = np.array(result["variables"], dtype=float)
        value = float(result["objective"])
        if x.shape != (n,) or not np.isfinite(x).all() or not math.isfinite(value):
            raise ValueError(f"{problem.name}: malformed optimal solution")
        violation = float(max(0.0, np.max(problem.a @ x - problem.b, initial=0.0), -np.min(x)))
        passed &= violation <= FEASIBILITY_TOLERANCE and objectives_match(value, float(problem.c @ x))
        if reference_objective is not None:
            objective_gap = abs(value - reference_objective)
            passed &= objectives_match(value, reference_objective)
        if problem.expected_objective is not None:
            passed &= objectives_match(value, problem.expected_objective)
    else:
        passed &= result["objective"] is None and result["variables"] == []

    return {
        "name": problem.name,
        "m": m,
        "n": n,
        "input_sha256": hashlib.sha256(source.encode("utf-8")).hexdigest(),
        "expected_status": problem.expected_status,
        "cli": result,
        "reference_status": reference_status,
        "reference_objective": reference_objective,
        "objective_abs_gap": objective_gap,
        "max_violation": violation,
        "cli_median_ms": statistics.median(samples),
        "cli_ms_samples": samples,
        "passed": passed,
    }


def objectives_match(actual: float, expected: float) -> bool:
    return abs(actual - expected) <= OBJECTIVE_TOLERANCE * max(1.0, abs(expected))


def source_hashes() -> dict[str, str]:
    root = EXPERIMENTS.parent
    paths = [root / "CMakeLists.txt"]
    for directory in ("app", "include", "src"):
        paths.extend(path for path in (root / directory).rglob("*") if path.is_file())
    return {
        path.relative_to(root).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in sorted(paths)
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path, help="path to the built simplex_cli executable")
    parser.add_argument("--output", type=Path, required=True, help="output JSON file")
    parser.add_argument("--repeats", type=int, default=7, help="timed CLI calls per instance")
    args = parser.parse_args()
    if args.repeats < 1:
        parser.error("--repeats must be positive")
    binary = args.binary.resolve()
    binary_hash = hashlib.sha256(binary.read_bytes()).hexdigest()
    sources = source_hashes()

    cases = []
    for problem in (*fixtures(), *dense_problems()):
        case = compare(binary, problem, args.repeats)
        cases.append(case)
        print(
            f"{case['name']}  CLI={case['cli']['status']}  HiGHS={case['reference_status']}"
            f"  iterations={case['cli']['iterations']}",
            flush=True,
        )
    if binary_hash != hashlib.sha256(binary.read_bytes()).hexdigest() or sources != source_hashes():
        raise RuntimeError("The executable or solver sources changed during the experiment")
    data = {
        "environment": {
            "platform": platform.platform(),
            "python": platform.python_version(),
            "numpy": np.__version__,
            "scipy": scipy.__version__,
            "solver_binary_sha256": binary_hash,
            "solver_source_sha256": sources,
        },
        "seed": SEED,
        "repeats": args.repeats,
        "reference_method": "highs-ipm",
        "feasibility_tolerance": FEASIBILITY_TOLERANCE,
        "objective_tolerance": OBJECTIVE_TOLERANCE,
        "cases": cases,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(data, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    passed = sum(case["passed"] for case in cases)
    print(f"{passed}/{len(cases)} cases passed; results: {args.output}")
    return 0 if passed == len(cases) else 1


if __name__ == "__main__":
    raise SystemExit(main())
