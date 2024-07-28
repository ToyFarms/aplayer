from pathlib import Path
import os
import subprocess
import argparse
from concurrent.futures import ThreadPoolExecutor
from typing import Optional
from pygments import highlight, lexers, formatters


class Unit:
    DEFINED_FLAGS = ["DEBUG", "EXPECT_FAIL"]
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
        self.extra_command = []

    def _check_flags(self) -> None:
        for flag in self.flags:
            if flag not in Unit.DEFINED_FLAGS:
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
            for cflag in map(Path, self.cflags.splitlines())
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

    def build(self, force: bool = False) -> None:
        self._check_flags()
        if not force and not self.need_rebuild():
            print(f"Using \x1b[1m{self.test_name()}\x1b[0m cached build")
            return

        self.prepare()
        print(
            f"\x1b[1mbuilding\x1b[0m {self.test_name()!r}: {' '.join(self.command)!r}"
        )
        res = subprocess.run(self.command, capture_output=True)
        if res.returncode != 0:
            print(res.stderr.decode())
            print(f"\x1b[31mFailed to build {self.name}\x1b[0m")
            self.build_status = -1

            return

        # self.update_cache()

    def run(self, is_multithreaded: bool = False) -> None:
        if not self.out.exists():
            print(f"Could not find test '{self.test_name()}'")
            return
        if self.build_status != 0:
            print(f"Skip test {self.test_name()} because build failed")
            return

        expect_fail = "EXPECT_FAIL" in self.flags
        debug = "DEBUG" in self.flags

        if debug:
            print(f"    \x1b[34mDEBUG\x1b[0m {self.test_name()}", flush=True)
            subprocess.run(["gdb", "--tui", self.out])
            return

        mp_prefix = ""
        if not is_multithreaded:
            print(f"{self.test_name()}", end="", flush=True)
            mp_prefix = "\r "

        res = subprocess.run(f"./{self.out}", capture_output=True)
        if res.returncode != 0 and not expect_fail:
            print(
                f"{mp_prefix} [ \x1b[31mFAIL\x1b[0m ] {self.test_name()} ({res.returncode})",
                flush=True,
            )
            print(f"@@@ stdout=\x1b[1m{res.stdout}\x1b[0m", flush=True)
            print(f"@@@ stderr=\x1b[1m{res.stderr}\x1b[0m", flush=True)
            content = find_path_and_read(res.stderr.decode())
            if not content:
                return

            print(content, flush=True)
            self.run_status = -1
        elif res.returncode == 0 and expect_fail:
            print(f"{mp_prefix} [ \x1b[31mFAIL\x1b[0m ] {self.test_name()}", flush=True)
            print(f"@@@ Expect to fail", flush=True)
            self.run_status = -1
        else:
            print(f"{mp_prefix} [  \x1b[32mOK\x1b[0m  ] {self.test_name()}", flush=True)
            self.run_status = 1

    def __repr__(self) -> str:
        return f"{self.name}({self.cflags}):{len(self.source)}"


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

        margin = 10
        start = max(0, row - margin)
        end = min(len(lines), row + margin + 1)
        result = []

        lexer = lexers.get_lexer_by_name("C")
        formatter = formatters.TerminalTrueColorFormatter(style="monokai")

        max_length = len(max(lines[start:end], key=lambda x: len(x)))
        for i, line in enumerate(lines[start:end], start + 1):
            pad = max_length - len(line)
            bg = (30, 30, 30)
            nb = (224, 198, 29)
            swap = ""
            if i - 1 == row:
                bg = (150, 17, 13)
                swap = ";7"

            highlighted = highlight(line, lexer, formatter).removesuffix("\n")
            result.append(
                f"\x1b[38;2;{nb[0]};{nb[1]};{nb[2]}{swap}m{i:>4} \x1b[0m"
                f"\x1b[48;2;{bg[0]};{bg[1]};{bg[2]}m{highlighted}"
                f"{' ' * pad}\x1b[0m"
            )

        return (
            f"@@@ \x1b[38;2;138;183;255;48;2;30;30;30m{path}:{row + 1}\x1b[0m"
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


def parse_testcases(source: list[str]) -> dict[str, tuple[str, list[str]]]:
    testcases_str: list[list[str]] = []
    mark_start, mark_end = "TEST_BEGIN", "TEST_END"

    start_index = 0
    for i, line in enumerate(source):
        if mark_start in line:
            start_index = i
        elif mark_end in line:
            testcases_str.append(source[start_index:i])

    testcases: dict[str, tuple[str, list[str]]] = {}
    for testcase in testcases_str:
        name = testcase[0].removeprefix(f"{mark_start}(")
        flag = []
        if "," not in name:
            name = name[:-1]
        else:
            name, *flag = name.split(",")
            flag = list(map(lambda x: x.strip().removesuffix(")"), flag))
        testcases[name] = ("\n".join(testcase[1:]), flag)

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
) -> str:
    _ = flags

    base = Path("tests/base_test.h").read_text()
    return f"""{base}
{includes}
void _{group}_{test}(){{{source}}}
int main()
{{
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


def print_chart(units: list[Unit]) -> None:
    total = len(units)
    success = [unit for unit in units if unit.run_status > 0]
    fails = [unit for unit in units if unit.run_status < 0]
    build_failed = [unit for unit in units if unit.build_status < 0]
    width = os.get_terminal_size().columns - 1

    percentage = [
        (
            "SUCCESS",
            (len(success) / total) if success else 0,
            len(success),
            (0, 255, 0),
        ),
        (
            "FAIL",
            (len(fails) / total) if fails else 0,
            len(fails),
            (255, 0, 0),
        ),
        (
            "BUILD_FAILED",
            (len(build_failed) / total) if build_failed else 0,
            len(build_failed),
            (150, 150, 150),
        ),
    ]

    to_color = lambda x: f"{x[0]};{x[1]};{x[2]}"
    correction = 0
    for i, (name, n, length, col) in enumerate(percentage):
        if length == 0:
            continue
        joined_color = col
        j = 1
        while i + j < len(percentage):
            if percentage[i + j][2] != 0:
                joined_color = percentage[i + j][3]
                break
            j += 1

        w = (n * width) - correction
        int_part = int(w)
        float_part = w - int_part

        if int_part > 0:
            print(f"\x1b[48;2;{to_color(col)}m{' ' * int_part}", end="")
        if float_part > 1 / len(blocks):
            print(
                f"\x1b[38;2;{to_color(joined_color)};7m{get_block(float_part)}", end=""
            )
        print("\x1b[0m", end="")

        correction += float_part

    print()
    for i, (name, n, length, col) in enumerate(percentage):
        print(
            f"\x1b[38;2;{to_color(col)}m■\x1b[0m {name} ({length}/{total} {length/total*100:.1f}%)",
            end="      ",
        )
    print()


def print_summary(units: list[Unit]) -> None:
    print_chart(units)


def main():
    if not Path("tests").exists:
        print("No tests/ directory found")
        return

    parser = argparse.ArgumentParser()
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

    units: list[Unit] = []
    for file in Path("tests").glob("*.c"):
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
                        source[0],
                        source[1],
                    ),
                    cflags,
                    source[1],
                )
            )

    with ThreadPoolExecutor(max_workers=args.jobs) as executor:
        list(executor.map(lambda x: x.build(args.rebuild), units))

    for unit in units:
        unit.update_cache()

    with ThreadPoolExecutor(max_workers=args.jobs) as executor:
        list(executor.map(lambda x: x.run(True), units))

    print_summary(units)


if __name__ == "__main__":
    main()
