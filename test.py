#!/usr/bin/python

from pathlib import Path
import os
import re
import subprocess
import argparse
import shutil
import time
import attr
import math
from concurrent.futures import ThreadPoolExecutor
from typing import Any, Callable, Iterable, Literal, Optional, Generator

try:
    from pygments import highlight, lexers, formatters

    lexer = lexers.get_lexer_by_name("C")
    formatter = formatters.TerminalTrueColorFormatter(style="monokai")
except ModuleNotFoundError:
    lexer = None
    formatter = None

    def highlight(code, lexer, formatter, outfile=None):
        _ = lexer, formatter, outfile
        return code


class UnitStat:
    def __init__(self) -> None:
        self.build_time = 0
        self.run_time = 0

    class TimedContext:
        def __init__(self, cb: Callable[[float], None]) -> None:
            self.elapsed = 0
            self.cb = cb

        def __enter__(self):
            self.start = time.perf_counter()
            return self

        def __exit__(self, exc_type, exc_val, exc_tb):
            _ = exc_type, exc_val, exc_tb

            self.elapsed = (time.perf_counter() - self.start) * 1000
            self.cb(self.elapsed)

    def timed(self, ctx: Literal["build", "run"]) -> TimedContext:
        attr = {
            "build": "build_time",
            "run": "run_time",
        }[ctx]
        return self.TimedContext(lambda t: setattr(self, attr, t))


class Unit:
    VALID_FLAGS = ["DEBUG", "EXPECT_FAIL", "INIT"]

    def __init__(
        self,
        filename: str,
        name: str,
        source: str,
        cflags: str = "",
        flags: list[str] = [],
        compiler: str = "cc",
    ) -> None:
        self.filename = Path(filename)
        self.name = name
        self.source = source
        self.cflags = cflags
        self.flags = flags
        self.compiler = compiler
        self.out = Path(f"tests/build/{self.filename.stem}_{self.name}")
        self.src = Path(f"tests/build/src/{self.filename.stem}_{self.name}.c")
        self.build_status = 0
        self.run_status = 0
        self.retcode = 0
        self.extra_command = []
        self.stat = UnitStat()

    def _check_flags(self) -> None:
        for flag in self.flags:
            if not any(flag.startswith(valid) for valid in Unit.VALID_FLAGS):
                print(f"\x1b[33mWarning: Unknown flag {flag!r}\x1b[0m")

    def test_name(self) -> str:
        return f"{self.filename.stem}:{self.name}"

    def prepare(self) -> None:
        self.out.parent.mkdir(parents=True, exist_ok=True)
        self.src.parent.mkdir(parents=True, exist_ok=True)
        self.src.write_text(self.source, "utf-8")

    @property
    def command(self) -> list[str]:
        command: list[str] = [
            self.compiler,
            str(self.src),
            "-o",
            str(self.out),
            *self.cflags.splitlines(),
        ]
        command = list(map(lambda x: x.strip(), command))
        debug_flags: list[str] = []
        if "DEBUG" in self.flags:
            debug_flags.extend(["-O0", "-g3"])

        return command + debug_flags + self.extra_command

    def _hash(self, s: str) -> int:
        hash = 5831
        for ch in s:
            hash = (((hash << 5) + hash) + ord(ch)) & 0xFFFFFFFF
        return hash

    def __hash__(self) -> int:
        files: list[Path] = [
            cflag
            for cflag in map(Path, map(str.strip, self.cflags.splitlines()))
            if cflag.exists() and cflag.is_file()
        ]
        return self._hash(
            f"{self.source}{self.command}"
            f"{''.join(file.read_text() for file in files)}"
        )

    def need_rebuild(self) -> bool:
        cache_file = Path("tests/build.cache")
        if not cache_file.exists():
            cache_file.touch()
            return True

        content = cache_file.read_text().splitlines()
        for line in content:
            if self.test_name() in line:
                name, _hash = line.split("@@@")
                return (
                    int(_hash) != hash(self)
                    and Path("tests/build/" + name.replace(":", "_")).exists()
                )

        return True

    def update_cache(self) -> None:
        cache_file = Path("tests/build.cache")
        cache_file.touch()
        content = cache_file.read_text().splitlines()
        updated = False
        for i, line in enumerate(content):
            if self.test_name() in line:
                content[i] = f"{self.test_name()}@@@{hash(self)}"
                updated = True
                break

        if not updated:
            content.append(f"{self.test_name()}@@@{hash(self)}")

        cache_file.write_text("\n".join(content))

    def build(self) -> None:
        with self.stat.timed("build"):
            self._check_flags()
            if not self.need_rebuild():
                print(f"Using \x1b[1m{self.test_name()}\x1b[0m cached build")
                return

            self.prepare()

            res = subprocess.run(self.command, capture_output=True)

        msg = (
            f"\x1b[1mBuilding\x1b[0m {self.test_name()!r}: "
            f"{' '.join(self.command)!r}"
        )

        width = os.get_terminal_size().columns
        align_on_col = width - 18
        padding = max(align_on_col - len(msg), 0)
        msg = f"{msg} {'.'*padding} build@{self.stat.build_time:.2f} ms"
        print(msg, flush=True)

        if res.returncode != 0:
            print(res.stderr.decode())
            print(f"\x1b[31mFailed to build {self.test_name()}\x1b[0m")
            self.build_status = 1

            return

        self.build_status = 0

        # NOTE: Race condition when multithreaded, manually update in main
        # self.update_cache()

    def run(self, is_multithreaded: bool = False) -> None:
        STATUS_OK = 0
        STATUS_FAIL = 1

        def print_status(status: int, retcode: int, is_mp: bool) -> None:
            width = os.get_terminal_size().columns

            msg = "" if is_mp else "\r"
            if status == STATUS_OK:
                msg = f"{msg} [  \x1b[32mOK\x1b[0m  ] {self.test_name()}"
            elif status == STATUS_FAIL:
                msg = f"{msg} [ \x1b[31mFAIL\x1b[0m ] {self.test_name()} ({retcode})"

            align_on_col = min(80, width)
            padding = max(align_on_col - len(msg), 0)
            msg = f"{msg} {'.'*padding} exec@{self.stat.run_time:.5f} ms"

            print(msg, flush=True)

        with self.stat.timed("run"):
            if not self.build_success():
                print(f"Skip test {self.test_name()} because build failed")
                return
            if not self.out.exists():
                print(f"Could not find test '{self.test_name()}'")
                self.run_status = 1
                return

            expect_fail = "EXPECT_FAIL" in self.flags
            debug = "DEBUG" in self.flags

            if debug:
                print(f"    \x1b[34mDEBUG\x1b[0m {self.test_name()}", flush=True)
                subprocess.run(["gdb", "--tui", self.out])
                return

            if not is_multithreaded:
                print(f"{self.test_name()}", end="", flush=True)

            res = subprocess.run(f"./{self.out}", capture_output=True)
            self.retcode = res.returncode

        if res.returncode != 0 and not expect_fail:
            print_status(STATUS_FAIL, res.returncode, is_multithreaded)
            stderr = res.stderr.decode()

            print(f"@@@ stdout=\x1b[1m{res.stdout}\x1b[0m", flush=True)
            if stderr.startswith("MEM_EQ("):
                print(
                    f"@@@ stderr=\x1b[1m"
                    f"{res.stderr[:min(100, len(res.stderr))]} "
                    f"... ({max(len(res.stderr) - 100, 0):,} more)\x1b[0m",
                    flush=True,
                )
                print("\n@@@ Memory Diff View", flush=True)
                print(diff_memory(stderr), flush=True)
            else:
                print(f"@@@ stderr=\x1b[1m{res.stderr}\x1b[0m", flush=True)

            content = find_path_and_read(stderr)
            if content:
                print(content, flush=True)

            self.run_status = 1
        elif res.returncode == 0 and expect_fail:
            print_status(STATUS_FAIL, res.returncode, is_multithreaded)
            print(f"@@@ Expect to fail", flush=True)
            self.run_status = 1
        else:
            print_status(STATUS_OK, res.returncode, is_multithreaded)
            self.run_status = 0

    def success(self) -> bool:
        return self.run_status == 0

    def build_success(self) -> bool:
        return self.build_status == 0

    def __repr__(self) -> str:
        return (
            f"{self.name}({", ".join(self.cflags.splitlines())}):len={len(self.source)}"
        )


def find_maxlen[T](lst: list[T], key: Callable[[T], int] = len) -> int:
    return max((key(item) for item in lst))


def diff_memory(dump: str) -> str:
    if not dump.startswith("MEM_EQ("):
        return ""

    part1, part2 = dump.split("]> != ", 1)

    name1, part1 = part1.split("<[", 1)
    name1 = name1.removeprefix("MEM_EQ(")
    part1 = part1.split(" ")

    name2, part2 = part2.split("<[", 1)
    part2 = part2.split("]>) ")[0].split(" ")

    chunk = os.get_terminal_size().columns // 5
    name_max = find_maxlen([name1, name2])

    diff = []
    for i in range(0, min(len(part1), len(part2)), chunk):
        diff1 = part1[i : i + chunk]
        diff2 = part2[i : i + chunk]

        if diff1 == diff2:
            continue

        str1 = []
        str2 = []

        for a, b in zip(diff1, diff2):
            if a != b:
                str1.append(f"\x1b[31m{a:<2}\x1b[0m ")
                str2.append(f"\x1b[31m{b:<2}\x1b[0m ")
            else:
                str1.append(f"{a:<2} ")
                str2.append(f"{b:<2} ")

        diff.append(f"{i:>5} - {i+chunk:<5}  {name1:>{name_max}}: " + "".join(str1))
        diff.append(f"{i:>5} - {i+chunk:<5}  {name2:>{name_max}}: " + "".join(str2))

    return "\n".join(diff)


def find_path_and_read(string: str) -> Optional[str]:
    if string.endswith(('"', "'")):
        path = string[:-1].rsplit(string[-1])[-1]
    else:
        path = string.split(" ")[-1]

    colon_count = path.count(":")
    if colon_count >= 1:
        path, row, *_ = path.split(":")
        if not row.isnumeric():
            return None
        row = int(row) - 1
        file = Path(path)
        if not file.exists() or file.is_dir():
            return None

        lines = file.read_text("utf-8").splitlines()
        if row > len(lines):
            return None

        margin = 5
        start = max(0, row - margin)
        end = min(len(lines), row + margin + 1)
        result = []

        original_lookup: dict[str, list[str]] = {}
        lines_original: list[tuple[str, int]] = []
        original_file = file
        original_row = row
        for i, line in enumerate(lines[start:end], start):
            mapping: list[str] = re.findall(r"// SM: \^ .*\.c:\d+ \$$", line)
            if mapping:
                _, file, lineno = mapping[-1].split(":")
                file = file.strip().removeprefix("^ ")
                lineno = lineno.removesuffix(" $")
                lineno = int(lineno) - 1

                if not Path(file).exists():
                    continue

                original_file = file
                if i == row:
                    original_row = lineno

                original = (
                    Path(file).read_text().splitlines()
                    if file not in original_lookup
                    else original_lookup[file]
                )

                lines_original.append((original[lineno], lineno))

        lines = lines_original

        max_length = find_maxlen(lines, key=lambda x: len(x[0]))
        for line, lineno in lines:
            pad = max_length - len(line)
            bg = (30, 30, 30)
            nb = (224, 198, 29)
            swap = ""
            if lineno == original_row:
                bg = (150, 17, 13)
                swap = ";7"

            highlighted = highlight(line, lexer, formatter).removesuffix("\n")
            result.append(
                f"\x1b[38;2;{to_color(*nb)}{swap}m{lineno + 1:>4} \x1b[0m"
                f"\x1b[48;2;{to_color(*bg)}m{highlighted}"
                f"{' ' * pad}\x1b[0m"
            )

        return (
            f"@@@ \x1b[38;2;138;183;255;48;2;30;30;30m{original_file}:{original_row + 1}\x1b[0m"
            f"\n{'\n'.join(result)}"
        )
    else:
        return None


def get_line_index(source: list[str], partial: str) -> int:
    for i, line in enumerate(source):
        if partial in line:
            return i
    return -1


def get_line_inbetween(
    source: list[str],
    start: str,
    end: str,
) -> list[str]:
    start_index = get_line_index(source, start)
    end_index = get_line_index(source, end)
    if start_index < 0 or end_index < 0:
        print(f"Could not find mark {start} and {end}")
        return []

    return source[start_index + 1 : end_index]


@attr.define
class Testcase:
    source: str
    flags: list[str]
    start: int


def parse_testcases(source: list[str]) -> dict[str, Testcase]:
    testcases_str: list[tuple[list[str], int]] = []
    mark_start, mark_end = "TEST_BEGIN", "TEST_END"

    start_index = -1
    for i, line in enumerate(source):
        if line.startswith(mark_start):
            if start_index > 0 and i != 0:
                print(
                    f"\x1b[31m    From line {source[start_index]!r} "
                    "have no corresponding TEST_END()\x1b[0m"
                )
            start_index = i
        elif line.startswith(mark_end):
            testcases_str.append((source[start_index:i], start_index + 1))
            start_index = -1

    if start_index > 0:
        print(
            f"\x1b[31m    From line {source[start_index]!r} have "
            "no corresponding TEST_END()\x1b[0m"
        )

    testcases: dict[str, Testcase] = {}
    for testcase, start in testcases_str:
        name = testcase[0].removeprefix(f"{mark_start}(")
        flag = []
        if "," not in name:
            name = name[:-1]
        else:
            name, *flag = name.split(",")
            flag = list(map(lambda x: x.strip().removesuffix(")"), flag))
        testcases[name] = Testcase(
            source="\n".join(testcase[1:]),
            flags=flag,
            start=start,
        )

    return testcases


def parse_cflags(source: list[str]) -> str:
    return "\n".join(get_line_inbetween(source, "CFLAGS_BEGIN", "CFLAGS_END"))


def parse_includes(source: list[str]) -> list[str]:
    return get_line_inbetween(source, "INCLUDE_BEGIN", "INCLUDE_END")


def generate_source(
    includes: str,
    group: str,
    test: str,
    source: str,
    flags: list[str],
    cflags: str,
    start: tuple[str, int],
) -> str:
    _ = flags

    base = Path("tests/base_test.h").read_text()

    source_map = []
    for i, line in enumerate(source.split("\n"), 1):
        align_on_col = 80
        padding = max(align_on_col - len(line), 0)

        source_map.append(f"{line}{' ' * padding} // SM: ^ {start[0]}:{start[1] + i} $")
    source = "\n".join(source_map)

    init = ""
    for flag in flags:
        if not flag.startswith("INIT"):
            continue

        flag = flag.removeprefix("INIT : ").split(" | ")
        flag = list(map(lambda x: f"test_init_{x};\n", flag))
        init = "".join(flag)

        break

    logger = """#ifdef test_init_initlogger
#  include "logger.h"
    test_init_initlogger;
#endif /* test_init_initlogger */
"""

    return f"""{base}
{includes}
void _{group}_{test}()
{source}
int main()
{{
{logger if "logger.c" in cflags else ""}
    {init}
    _{group}_{test}();
    return 0;
}}
""".strip()


blocks = " ▏▎▍▌▋▊▉█"


def get_block(n: float) -> str:
    index = int(n * (len(blocks) - 1))
    if index > len(blocks) - 1:
        return ""
    return blocks[index]


def get_units_status(units: list[Unit]) -> tuple[list[Unit], list[Unit], list[Unit]]:
    fails = []
    build_failed = []
    success = []
    for unit in units:
        if not unit.build_success():
            build_failed.append(unit)
        elif not unit.success():
            fails.append(unit)
        else:
            success.append(unit)

    return (success, fails, build_failed)


def print_center(text: str) -> None:
    width = os.get_terminal_size().columns
    print(f"{text:^{width}}")


def to_color(r: int, g: int, b: int) -> str:
    return f"{r};{g};{b}"


type RGBColor = tuple[int, int, int]


def generate_gradient(colors: list[RGBColor]) -> Callable[[float], RGBColor]:
    def interpolate(value: float) -> RGBColor:
        value = max(0, min(1, value))
        n = len(colors) - 1

        index = int(value * n)
        t = (value * n) - index

        color1 = colors[index]
        color2 = colors[min(index + 1, n)]

        interpolated_color = [
            int(color1[i] + (color2[i] - color1[i]) * t) for i in range(3)
        ]
        return (interpolated_color[0], interpolated_color[1], interpolated_color[2])

    return interpolate


def pairwise[T](iterable: Iterable[T]) -> Generator[tuple[T, T], None, None]:
    it = iter(iterable)
    while True:
        try:
            yield next(it), next(it)
        except StopIteration:
            break


def print_chart(ratios: list[tuple[str, float, RGBColor]], diameter: int) -> None:
    ratios.sort(key=lambda x: x[1])

    cumulative_ratios = []
    cumulative = 0
    for _, ratio, color in ratios:
        if ratio == 0:
            continue
        cumulative += ratio
        cumulative_ratios.append((cumulative, color))

    diameter *= 2  # account each cell is actually 2 pixel

    pixels: list[list[RGBColor | None]] = []
    for y in range(diameter):
        row: list[RGBColor | None] = []
        for x in range(diameter):
            dx = x - diameter / 2
            dy = y - diameter / 2
            r = diameter / 2
            if dx * dx + dy * dy >= r * r:
                row.append(None)
                continue

            angle = ((90 + math.degrees(math.atan2(dy, dx))) % 360) / 360
            for cumulative_ratio, col in cumulative_ratios:
                if angle <= cumulative_ratio:
                    row.append(col)
                    break

        pixels.append(row)

    buf = []
    term_width = os.get_terminal_size().columns
    for row, next_row in pairwise(pixels):
        buf.append(" " * int(term_width * 0.1))
        for top_px, bot_px in zip(row, next_row):
            if top_px and bot_px:
                buf.append(
                    f"\x1b[38;2;{to_color(*top_px)};48;2;{to_color(*bot_px)}m▀\x1b[0m"
                )
            elif top_px:
                buf.append(f"\x1b[38;2;{to_color(*top_px)}m▀\x1b[0m")
            elif bot_px:
                buf.append(f"\x1b[38;2;{to_color(*bot_px)}m▄\x1b[0m")
            else:
                buf.append(" ")
        buf.append("\n")

    print("".join(buf))


def print_unit_chart(
    units: list[Unit], diameter=os.get_terminal_size().columns / 2
) -> None:
    total = len(units)
    success, fails, build_failed = get_units_status(units)

    template = [
        ("SUCCESS", success, (0, 255, 0)),
        ("FAIL", fails, (255, 0, 0)),
        ("BUILD_FAILED", build_failed, (150, 150, 150)),
    ]

    ratios = [
        (name, (len(lst) / total) if lst else 0.0, col) for name, lst, col in template
    ]
    print_chart(ratios, int(diameter))

    for name, ratio, color in ratios:
        print(
            f"    \x1b[38;2;{to_color(*color)}m■ {name} ({ratio * 100:.1f} %)\x1b[0m",
            end="",
        )
    print("\n")


def print_table(units: list[Unit]) -> None:
    groups: dict[str, list[Unit]] = {}
    for unit in units:
        group_name = unit.filename.stem
        if group_name in groups:
            groups[group_name].append(unit)
        else:
            groups[group_name] = []
            groups[group_name].append(unit)
    groups["all"] = units

    max_length = find_maxlen(list(groups.keys()))
    g2r_gradient = generate_gradient([(0, 255, 0), (255, 0, 0)])
    w2b_gradient = generate_gradient([(255, 255, 255), (0, 0, 0)])

    max_total_time = find_maxlen(
        list(groups.values()),
        key=lambda x: sum(unit.stat.run_time + unit.stat.build_time for unit in x),
    )

    for i, (group_name, group_units) in enumerate(groups.items(), 1):
        success, fails, build_failed = get_units_status(group_units)
        success_ratio = 0 if not group_units else 1 - (len(success) / len(group_units))
        influence = 0 if not units else 1 - (len(group_units) / len(units))
        total_time = sum(
            unit.stat.run_time + unit.stat.build_time for unit in group_units
        )
        print(
            f"{f'({i})':<6}  "
            f"{group_name:<{max_length}}  "
            f"{len(group_units):>5} tests  "
            f"\x1b[48;2;{to_color(*w2b_gradient(influence))}m  \x1b[0m  "
            f"{len(success):>5} success  "
            f"\x1b[48;2;{to_color(*g2r_gradient(success_ratio))}m  \x1b[0m  "
            f"{len(fails):>5} failed  "
            f"{len(build_failed):>5} build_failed"
            f"{total_time:>15.2f} ms  "
            f"\x1b[48;2;{to_color(*g2r_gradient(total_time / max_total_time))}m  \x1b[0m"
        )

        if group_name == "all":
            continue

        failed = fails + build_failed
        if failed:
            # failed = list({id(obj): obj for obj in failed}.values())
            for fail in failed:
                padding = max(35 - len(fail.name), 0)
                is_last = fail is failed[-1]
                fail_reason = (
                    "failed_both "
                    if not fail.success() and not fail.build_success()
                    else (
                        "failed_exec "
                        if not fail.success()
                        else (
                            "failed_build" if not fail.build_success() else "not_failed"
                        )
                    )
                )

                print(
                    f"{' ' * 10}{'└' if is_last else '├'}─ "
                    f"{fail.name} {'.'*padding} {fail_reason} ({fail.retcode})"
                )


def print_stats(units: list[Unit]) -> None:
    global global_stat

    print(f" * Using {nb_threads} thread")
    print(
        f" * Rebuilt {len(list(filter(lambda x: x.build_status != 0, units)))} "
        f"tests out of {len(units)} tests"
    )
    total = global_stat.run_time + global_stat.build_time
    print(
        f" * {len(units)} tests done in {total:.0f} ms\n"
        f"   ├─ {global_stat.run_time:.2f} ms in execution\n"
        f"   ├─ {global_stat.build_time:.2f} ms in building\n"
        f"   └─ {(total / len(units)):.2f} ms / test average"
    )


def print_summary(units: list[Unit]) -> None:
    print_center("-- Summary --")
    print()

    w = os.get_terminal_size().columns
    h = os.get_terminal_size().lines
    print_unit_chart(units, min(h / 2, w / 4))
    print_stats(units)

    print_center("-- Unit Status --")
    print_table(units)


global_stat = UnitStat()
nb_threads = 1


def main() -> None:
    global global_stat, nb_threads

    if not Path("tests").exists:
        print("No tests/ directory found")
        return

    Path("tests/logs").mkdir(parents=True, exist_ok=True)

    parser = argparse.ArgumentParser()
    parser.add_argument(
        "target",
        default=[],
        nargs="*",
        help="target unittest file, default all",
    )
    parser.add_argument(
        "-r",
        "--rebuild",
        help="Force rebuild all test",
        action="store_true",
    )
    parser.add_argument(
        "-j",
        "--jobs",
        help="Number of thread to build",
        type=int,
        default=4,
    )

    args = parser.parse_args()
    nb_threads = args.jobs

    # TODO: Rebuild only selected target (if any)
    if args.rebuild:
        print("Rebuilding all test...")
        print("Removing all cache and binary...")
        shutil.rmtree("tests/build")
        os.remove("tests/build.cache")

    files: list[Path] = []
    if not args.target:
        files = list(Path("tests").glob("*.c"))
    else:
        targets = args.target
        target: str
        for target in targets:
            if not target.endswith(".c"):
                target = f"{target}.c"
            p = "tests" / Path(target)
            if not p.exists():
                raise FileNotFoundError(f"Path {str(p)!r} doesn't exists!")
            files.append(p)

    print()
    units: list[Unit] = []

    for file in files:
        print(f"Transpiling \x1b[4m{file}\x1b[0m")
        source = file.read_text().splitlines()
        includes = parse_includes(source)
        cflags = parse_cflags(source)
        testcases = parse_testcases(source)
        print(
            f"Found \x1b[1;36m{len(includes)} \x1b[0;1mincludes\x1b[0m, "
            f"cflags={" ".join(cflags.splitlines())!r}, \x1b[1;36m{len(testcases)} "
            f"\x1b[0;1mtestcases\x1b[0m ({list(testcases.keys())})"
        )

        for name, source in testcases.items():
            units.append(
                Unit(
                    str(file),
                    name,
                    generate_source(
                        "\n".join(includes),
                        Path(file).stem,
                        name,
                        source.source,
                        source.flags,
                        cflags,
                        (str(file), source.start),
                    ),
                    cflags,
                    source.flags,
                )
            )

    def do_work[T](lst: list[T], fn: Callable[[T], Any]) -> None:
        if nb_threads == 0:
            list(map(fn, lst))
        else:
            with ThreadPoolExecutor(max_workers=nb_threads) as executor:
                list(executor.map(fn, lst))

    with global_stat.timed("build"):
        do_work(units, lambda x: x.build())
    print(f"Build done in {global_stat.build_time} ms")

    for unit in units:
        unit.update_cache()

    with global_stat.timed("run"):
        do_work(units, lambda x: x.run(nb_threads > 1))
    print(f"Exec done in {global_stat.run_time} ms")

    print_summary(units)


if __name__ == "__main__":
    main()
