import argparse
import sys
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("buildtrees", type=Path)
    args = parser.parse_args()

    lists = sorted(args.buildtrees.glob("*/error-logs-*.txt"))
    if not lists:
        print(f"No vcpkg error log lists found in {args.buildtrees}")

    for log_list in lists:
        try:
            paths = log_list.read_text(encoding="utf-8").splitlines()
        except OSError as error:
            print(f"Cannot read {log_list}: {error}", file=sys.stderr)
            continue
        for path in paths:
            if not path or path.endswith(("-CMakeCache.txt.log", "-CMakeConfigureLog.yaml.log")):
                continue
            log = Path(path)
            try:
                contents = log.read_text(encoding="utf-8", errors="replace")
            except OSError as error:
                print(f"Cannot read {log}: {error}", file=sys.stderr)
                continue
            print(f"===== Contents of {log} =====")
            print(contents, end="" if contents.endswith("\n") else "\n")


if __name__ == "__main__":
    main()
