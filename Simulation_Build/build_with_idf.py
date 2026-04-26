from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path


DEFAULT_ESP_IDF_ROOT = Path(r"D:\DevTools\Espressif")


def resolve_script_dir() -> Path:
    return Path(__file__).resolve().parent


def find_project_root(start: Path) -> Path:
    """Return the project root.

    If the current working directory looks like a project root, use it.
    Otherwise, fall back to the script directory so the script still works
    when launched from elsewhere.
    """
    cwd = Path.cwd().resolve()
    if (cwd / "CMakeLists.txt").exists() or (cwd / "build").exists():
        return cwd
    return start.resolve().parent


def remove_build_dir(project_root: Path) -> None:
    build_dir = project_root / "build"
    if build_dir.exists():
        shutil.rmtree(build_dir)
        print(f"Removed: {build_dir}")
    else:
        print(f"No build directory found: {build_dir}")


def build_log_path(script_dir: Path) -> Path:
    return script_dir / "build_log.txt"


def clear_log_file(log_file: Path) -> None:
    log_file.write_text("", encoding="utf-8")


def append_log(log_file: Path, text: str) -> None:
    with log_file.open("a", encoding="utf-8", newline="") as f:
        f.write(text)


def locate_script(esp_idf_root: Path, names: tuple[str, ...]) -> Path:
    direct_candidates = []
    for name in names:
        direct_candidates.extend(
            [
                esp_idf_root / name,
                esp_idf_root / "tools" / name,
            ]
        )

    for candidate in direct_candidates:
        if candidate.exists():
            return candidate

    for name in names:
        for match in esp_idf_root.glob(f"**/{name}"):
            if match.is_file():
                return match

    raise FileNotFoundError(
        f"Could not find any of: {', '.join(names)} under {esp_idf_root}."
    )


def locate_export_script(esp_idf_root: Path) -> Path:
    return locate_script(esp_idf_root, ("export.bat", "export.ps1"))


def locate_install_script(esp_idf_root: Path) -> Path:
    return locate_script(esp_idf_root, ("install.bat", "install.ps1"))


def python_env_is_ready(esp_idf_root: Path) -> bool:
    python_env_dir = esp_idf_root / "python_env"
    return python_env_dir.exists()


def run_command(command: list[str] | str, project_root: Path, use_shell: bool) -> int:
    proc = subprocess.run(
        command,
        cwd=project_root,
        check=False,
        shell=use_shell,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    return proc.returncode, proc.stdout or ""


def run_build(project_root: Path, esp_idf_root: Path, log_file: Path) -> int:
    if not python_env_is_ready(esp_idf_root):
        install_script = locate_install_script(esp_idf_root)
        print(f"ESP-IDF Python env missing, running installer: {install_script}")
        if install_script.suffix.lower() == ".bat":
            install_rc, install_output = run_command(
                f'cmd.exe /d /s /c "call \"{install_script}\""',
                project_root,
                use_shell=True,
            )
        else:
            install_rc, install_output = run_command(
                [
                    "powershell.exe",
                    "-NoProfile",
                    "-ExecutionPolicy",
                    "Bypass",
                    "-Command",
                    f'. "{install_script}"',
                ],
                project_root,
                use_shell=False,
            )
        if install_output:
            append_log(log_file, install_output)
            print(install_output, end="" if install_output.endswith("\n") else "\n")
        if install_rc != 0:
            return install_rc

    export_script = locate_export_script(esp_idf_root)
    print(f"Using ESP-IDF export script: {export_script}")

    append_log(log_file, "building")

    if export_script.suffix.lower() == ".bat":
        rc, output = run_command(
            f'cmd.exe /d /s /c "call \"{export_script}\" && idf.py build"',
            project_root,
            use_shell=True,
        )
    else:
        rc, output = run_command(
            [
                "powershell.exe",
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-Command",
                f'. "{export_script}"; if ($LASTEXITCODE -eq 0) {{ idf.py build }} else {{ exit $LASTEXITCODE }}',
            ],
            project_root,
            use_shell=False,
        )

    clear_log_file(log_file)
    failed_marker = "FAILED: "
    failed_index = output.find(failed_marker)
    if failed_index != -1:
        append_log(log_file, output[failed_index:])
    else:
        append_log(log_file, "successful")

    if output:
        print(output, end="" if output.endswith("\n") else "\n")

    return rc


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Delete build/ and run idf.py build in an ESP-IDF environment."
    )
    parser.add_argument(
        "--project-root",
        type=Path,
        default=None,
        help="Project root directory. Defaults to the current working directory if it looks like an ESP-IDF project, otherwise the script directory.",
    )
    parser.add_argument(
        "--esp-idf-root",
        type=Path,
        default=DEFAULT_ESP_IDF_ROOT,
        help=r"ESP-IDF installation root, e.g. D:\DevTools\Espressif",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    script_dir = resolve_script_dir()
    project_root = args.project_root.resolve() if args.project_root else find_project_root(script_dir)
    log_file = build_log_path(script_dir)

    if not args.esp_idf_root.exists():
        print(f"ESP-IDF root does not exist: {args.esp_idf_root}", file=sys.stderr)
        return 2

    clear_log_file(log_file)
    remove_build_dir(project_root)
    return run_build(project_root, args.esp_idf_root, log_file)


if __name__ == "__main__":
    raise SystemExit(main())
