"""Run reproducible rate-limiter benchmark matrices and create CSV/SVG reports."""

import argparse
import csv
import datetime as dt
import html
import subprocess
import sys
from collections import defaultdict
from pathlib import Path
from statistics import fmean


ALGORITHMS = ["token-bucket", "fixed-window", "sliding-counter", "sliding-log", "leaky-bucket"]
THREADS = [1, 2, 4, 8, 16, 32]
METRICS = [
    "requests_per_second", "average_latency_us", "p50_latency_us",
    "p95_latency_us", "p99_latency_us", "allowed", "rejected", "errors",
]


def default_benchmark(root: Path) -> Path:
    candidates = [root / "build" / "Release" / "benchmark.exe", root / "build" / "benchmark.exe",
                  root / "build" / "benchmark"]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[0]


def parse_args() -> argparse.Namespace:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--benchmark", type=Path, default=default_benchmark(root),
                        help="Path to the benchmark executable.")
    parser.add_argument("--requests", type=int, default=100_000, help="Total calls in each run.")
    parser.add_argument("--clients", type=int, default=100, help="Distinct keys in each run.")
    parser.add_argument("--repetitions", type=int, default=3, help="Repetitions per configuration (3-5).")
    parser.add_argument("--threads", type=int, nargs="+", default=THREADS)
    parser.add_argument("--algorithms", nargs="+", default=ALGORITHMS, choices=ALGORITHMS)
    parser.add_argument("--pool-size", type=int, default=0,
                        help="Fixed Redis pool size; 0 uses one connection per worker.")
    parser.add_argument("--redis-host", default="127.0.0.1")
    parser.add_argument("--redis-port", type=int, default=6379)
    parser.add_argument("--output-dir", type=Path, default=root / "results")
    args = parser.parse_args()
    if not 3 <= args.repetitions <= 5:
        parser.error("--repetitions must be between 3 and 5")
    if args.requests <= 0 or args.clients <= 0 or any(thread <= 0 for thread in args.threads):
        parser.error("requests, clients, and thread counts must be positive")
    if not args.benchmark.is_file():
        parser.error(f"benchmark executable was not found: {args.benchmark}")
    return args


def run_once(args: argparse.Namespace, algorithm: str, threads: int, repetition: int, run_id: str) -> dict:
    # Use a distinct key space for every sample. Capacity/limit make this a
    # performance baseline: every valid request is admitted rather than being
    # dominated by the cheap reject path.
    pool_size = args.pool_size or threads
    prefix = f"matrix-{run_id}-{algorithm}-t{threads}-r{repetition}"
    command = [
        str(args.benchmark), "--format", "csv", "--algorithm", algorithm,
        "--clients", str(args.clients), "--requests", str(args.requests),
        "--threads", str(threads), "--pool-size", str(pool_size),
        "--capacity", str(args.requests), "--refill-rate", "0",
        "--leak-rate", "0", "--limit", str(args.requests), "--window", "60",
        "--redis-host", args.redis_host, "--redis-port", str(args.redis_port),
        "--key-prefix", prefix,
    ]
    completed = subprocess.run(command, capture_output=True, text=True, timeout=300,
                               cwd=Path(__file__).resolve().parents[1])
    rows = list(csv.DictReader(line for line in completed.stdout.splitlines() if line.strip()))
    if len(rows) != 1:
        raise RuntimeError("benchmark did not return exactly one CSV row\n"
                           f"command: {' '.join(command)}\nstdout: {completed.stdout}\nstderr: {completed.stderr}")
    row = rows[0]
    row.update({"repetition": repetition, "key_prefix": prefix, "exit_code": completed.returncode,
                "stderr": completed.stderr.strip()})
    return row


def write_csv(path: Path, rows: list[dict], fields: list[str]) -> None:
    with path.open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def summarize(rows: list[dict]) -> list[dict]:
    grouped: dict[tuple[str, str, str], list[dict]] = defaultdict(list)
    for row in rows:
        grouped[(row["algorithm"], row["threads"], row["pool_size"])].append(row)
    summary = []
    for (algorithm, threads, pool_size), samples in grouped.items():
        result = {"algorithm": algorithm, "threads": threads, "pool_size": pool_size,
                  "runs": len(samples)}
        for metric in METRICS:
            result[metric] = f"{fmean(float(sample[metric]) for sample in samples):.6f}"
        summary.append(result)
    return sorted(summary, key=lambda row: (ALGORITHMS.index(row["algorithm"]), int(row["threads"])))


def svg_chart(summary: list[dict], metric: str, title: str, y_label: str, output: Path) -> None:
    width, height, left, right, top, bottom = 1100, 460, 90, 230, 45, 75
    plot_width, plot_height = width - left - right, height - top - bottom
    values = [float(row[metric]) for row in summary]
    maximum = max(values) if values else 1.0
    maximum = maximum * 1.1 if maximum else 1.0
    threads = sorted({int(row["threads"]) for row in summary})
    colors = ["#2563eb", "#dc2626", "#16a34a", "#9333ea", "#ea580c"]

    def x(value: int) -> float:
        return left + (threads.index(value) / max(1, len(threads) - 1)) * plot_width

    def y(value: float) -> float:
        return top + plot_height - (value / maximum) * plot_height

    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
             '<style>text{font-family:Arial,sans-serif;fill:#111827}.axis{font-size:12px}.title{font-size:18px;font-weight:bold}.legend{font-size:13px}</style>',
             f'<text class="title" x="{left}" y="26">{html.escape(title)}</text>',
             f'<rect x="{left}" y="{top}" width="{plot_width}" height="{plot_height}" fill="white" stroke="#9ca3af"/>']
    for tick in range(6):
        value = maximum * tick / 5
        y_pos = y(value)
        parts += [f'<line x1="{left}" y1="{y_pos:.1f}" x2="{left + plot_width}" y2="{y_pos:.1f}" stroke="#e5e7eb"/>',
                  f'<text class="axis" x="{left - 8}" y="{y_pos + 4:.1f}" text-anchor="end">{value:.0f}</text>']
    for thread in threads:
        parts += [f'<line x1="{x(thread):.1f}" y1="{top + plot_height}" x2="{x(thread):.1f}" y2="{top + plot_height + 5}" stroke="#111827"/>',
                  f'<text class="axis" x="{x(thread):.1f}" y="{top + plot_height + 22}" text-anchor="middle">{thread}</text>']
    parts += [f'<text class="axis" x="{left + plot_width / 2:.1f}" y="{height - 25}" text-anchor="middle">Worker threads</text>',
              f'<text class="axis" transform="translate(20 {top + plot_height / 2:.1f}) rotate(-90)" text-anchor="middle">{html.escape(y_label)}</text>']
    for index, algorithm in enumerate(ALGORITHMS):
        series = [row for row in summary if row["algorithm"] == algorithm]
        if not series:
            continue
        color = colors[index]
        points = " ".join(f"{x(int(row['threads'])):.1f},{y(float(row[metric])):.1f}" for row in series)
        parts.append(f'<polyline fill="none" stroke="{color}" stroke-width="2.5" points="{points}"/>')
        for row in series:
            parts.append(f'<circle cx="{x(int(row["threads"])):.1f}" cy="{y(float(row[metric])):.1f}" r="4" fill="{color}"/>')
        legend_y = top + 25 + index * 25
        parts += [f'<line x1="{left + plot_width + 24}" y1="{legend_y}" x2="{left + plot_width + 44}" y2="{legend_y}" stroke="{color}" stroke-width="3"/>',
                  f'<text class="legend" x="{left + plot_width + 52}" y="{legend_y + 5}">{algorithm}</text>']
    parts.append("</svg>")
    output.write_text("\n".join(parts), encoding="utf-8")


def main() -> int:
    args = parse_args()
    run_id = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    output_dir = args.output_dir / run_id
    output_dir.mkdir(parents=True)
    rows = []
    total = len(args.algorithms) * len(args.threads) * args.repetitions
    sample = 0
    for algorithm in args.algorithms:
        for threads in args.threads:
            for repetition in range(1, args.repetitions + 1):
                sample += 1
                print(f"[{sample}/{total}] {algorithm}, {threads} threads, run {repetition}", flush=True)
                row = run_once(args, algorithm, threads, repetition, run_id)
                rows.append(row)
                if int(row["exit_code"]) != 0:
                    print(f"  warning: benchmark exit code {row['exit_code']}: {row['stderr']}", file=sys.stderr)
    raw_fields = ["algorithm", "clients", "requests", "threads", "pool_size", "elapsed_seconds",
                  "requests_per_second", "average_latency_us", "p50_latency_us", "p95_latency_us",
                  "p99_latency_us", "allowed", "rejected", "errors", "repetition", "key_prefix",
                  "exit_code", "stderr"]
    write_csv(output_dir / "raw-results.csv", rows, raw_fields)
    summary = summarize(rows)
    summary_fields = ["algorithm", "threads", "pool_size", "runs", *METRICS]
    write_csv(output_dir / "summary.csv", summary, summary_fields)
    svg_chart(summary, "requests_per_second", "Throughput by algorithm and thread count",
              "Requests / second", output_dir / "throughput.svg")
    svg_chart(summary, "p99_latency_us", "P99 latency by algorithm and thread count",
              "P99 latency (microseconds)", output_dir / "p99-latency.svg")
    print(f"\nSaved raw results, summary, and charts to: {output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
