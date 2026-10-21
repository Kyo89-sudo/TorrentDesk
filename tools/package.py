"""Create clean portable/source archives; excludes user data and build output."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED

ROOT = Path(__file__).resolve().parent.parent
DIST = ROOT / "dist"
VERSION = "0.4.0"


def main():
    DIST.mkdir(exist_ok=True)
    portable = DIST / f"TorrentDesk-{VERSION}-portable-x64.zip"
    with ZipFile(portable, "w", ZIP_DEFLATED) as z:
        for name in ("TorrentDesk.exe", "README.md", "LICENSE", "NOTICE"):
            z.write(ROOT / name, "TorrentDesk/" + name)
        z.write(ROOT / "vendor/LICENSE.MIT", "TorrentDesk/LICENSE-json.txt")
        for path in sorted((ROOT / "profiles").glob("*.client")):
            z.write(path, "TorrentDesk/profiles/" + path.name)
        for directory in ("languages","docs"):
            for path in sorted((ROOT/directory).glob("*")):
                if path.is_file():z.write(path,"TorrentDesk/"+path.relative_to(ROOT).as_posix())
        z.write(ROOT/"assets/flow-logo.png","TorrentDesk/assets/flow-logo.png")
        z.writestr("TorrentDesk/portable.flag", "")
    source = DIST / f"TorrentDesk-{VERSION}-source.zip"
    with ZipFile(source, "w", ZIP_DEFLATED) as z:
        for name in ("CMakeLists.txt", "build.cmd", "sign.cmd", "installer.iss", "app.rc", "README.md",
                     "CONTRIBUTING.md", "CHANGELOG.md", "AUDIT.md", "README.it.md", "LANGUAGES.md", "LICENSE", "NOTICE", ".gitignore", ".clang-format"):
            z.write(ROOT / name, "TorrentDesk/" + name)
        for directory in ("src", "assets", "profiles", "tools", "vendor", ".github", "languages", "docs"):
            for path in sorted((ROOT / directory).rglob("*")):
                if path.is_file() and "__pycache__" not in path.parts:
                    z.write(path, "TorrentDesk/" + path.relative_to(ROOT).as_posix())
    print(portable)
    print(source)


if __name__ == "__main__":
    main()
