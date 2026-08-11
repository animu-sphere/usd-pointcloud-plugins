from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    version = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        print(f"VERSION is not a semantic version: {version!r}", file=sys.stderr)
        return 1

    declarations = {
        "openstrata.toml": rf'^version = "{re.escape(version)}"$',
        "plugins/pointcloud-las/openstrata.plugin.yaml":
            rf'^  version: {re.escape(version)}$',
        "plugins/pointcloud-las/CMakeLists.txt":
            rf'^    VERSION {re.escape(version)}$',
        "plugins/pointcloud-laz/openstrata.plugin.yaml":
            rf'^  version: {re.escape(version)}$',
        "plugins/pointcloud-laz/CMakeLists.txt":
            rf'^    VERSION {re.escape(version)}$',
        "plugins/pointcloud-copc/openstrata.plugin.yaml":
            rf'^  version: {re.escape(version)}$',
        "plugins/pointcloud-copc/CMakeLists.txt":
            rf'^    VERSION {re.escape(version)}$',
        "plugins/pointcloud-ply/openstrata.plugin.yaml":
            rf'^  version: {re.escape(version)}$',
        "plugins/pointcloud-ply/CMakeLists.txt":
            rf'^    VERSION {re.escape(version)}$',
    }

    failures = []
    for relative_path, pattern in declarations.items():
        path = ROOT / relative_path
        text = path.read_text(encoding="utf-8")
        if re.search(pattern, text, flags=re.MULTILINE) is None:
            failures.append(relative_path)

    if failures:
        print(f"Release metadata does not match VERSION {version}:", file=sys.stderr)
        for relative_path in failures:
            print(f"- {relative_path}", file=sys.stderr)
        return 1

    print(f"Release metadata is consistent: {version}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())