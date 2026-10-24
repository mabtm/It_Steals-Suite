"""Keep repository copy and GitHub release pages in sync. Standard library only."""

import argparse
import json
import re
import subprocess
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent
REPOSITORY = "mabtm/It_Steals-Suite"
ASSETS = {
    "ItStealsSuite-Setup.exe": "Setup — recommended",
    "ItStealsSuite-Portable.zip": "Portable package",
    "ItStealsTrainer.exe": "Trainer EXE",
    "PracticeSuiteEncoder.exe": "Encoder EXE",
    "ItStealsHelper.dll": "Mod DLL",
    "Encoder-Sources.zip": "Encoder sources and licenses",
    "SHA256SUMS.txt": "SHA-256 checksums",
}
VERSION = r"\d+\.\d+\.\d+"


def current_version():
    header = (ROOT / "Trainer/SuiteBranding.h").read_text(encoding="utf-8")
    match = re.search(r'#define SUITE_VERSION "(' + VERSION + r')"', header)
    if not match:
        raise ValueError("Missing SUITE_VERSION in Trainer/SuiteBranding.h")
    return match[1]


def history():
    text = (ROOT / "CHANGELOG.md").read_text(encoding="utf-8")
    headings = list(
        re.finditer(r"^### v(" + VERSION + r") Changelog$", text, re.MULTILINE)
    )
    entries = {}
    for index, heading in enumerate(headings):
        end = headings[index + 1].start() if index + 1 < len(headings) else len(text)
        version = heading[1]
        if version in entries:
            raise ValueError(f"Duplicate changelog: {version}")
        body = text[heading.end() : end].strip()
        lines = [line for line in body.splitlines() if line]
        if not lines or any(
            not re.match(r"^- (Removed |Improved |Fixed a bug caused by )", line)
            for line in lines
        ):
            raise ValueError(f"Invalid changelog format: {version}")
        entries[version] = body
    versions = [tuple(map(int, value.split("."))) for value in entries]
    if versions != sorted(versions, reverse=True):
        raise ValueError("Changelogs must run from newest to oldest")
    return entries


def spotlight():
    version = current_version()
    changes = history()
    if next(iter(changes), None) != version:
        raise ValueError("The first changelog must match SUITE_VERSION")
    bullets = "\n".join(changes[version].splitlines()[:3])
    return (
        "<!-- release:start -->\n"
        f"**[v{version}](https://github.com/{REPOSITORY}/releases/tag/v{version})**\n\n"
        f"{bullets}\n\n"
        "[Full changelog](CHANGELOG.md)\n"
        "<!-- release:end -->"
    )


def sync():
    path = ROOT / "README.md"
    text = path.read_text(encoding="utf-8")
    text, count = re.subn(
        r"<!-- release:start -->.*?<!-- release:end -->",
        spotlight(),
        text,
        flags=re.DOTALL,
    )
    if count != 1:
        raise ValueError("README needs exactly one release spotlight")
    path.write_text(text, encoding="utf-8")


def check():
    version = current_version()
    header = (ROOT / "Trainer/SuiteBranding.h").read_text(encoding="utf-8")
    components = re.search(r"#define SUITE_VERSION_COMPONENTS ([\d, ]+)", header)
    if not components or tuple(map(int, components[1].split(","))) != (
        *map(int, version.split(".")),
        0,
    ):
        raise ValueError("SUITE_VERSION_COMPONENTS does not match SUITE_VERSION")
    expected_mod = re.search(r'#define SUITE_MOD_VERSION "(' + VERSION + r')"', header)
    mod_source = (ROOT / "Mod/ItStealsMod.cs").read_text(encoding="utf-8")
    mod = re.search(r'public const string Version = "(' + VERSION + r')"', mod_source)
    if not expected_mod or not mod or mod[1] != expected_mod[1]:
        raise ValueError("The game mod source does not match SUITE_MOD_VERSION")
    package = (ROOT / "Trainer/ReplayPackage.h").read_text(encoding="utf-8")
    compatible = set(re.findall(r'\{"(' + VERSION + r')",\s*(?:true|false)\}', package))
    required = {
        previous
        for previous in history()
        if previous != version and tuple(map(int, previous.split("."))) >= (7, 2, 0)
    }
    if required - compatible:
        raise ValueError(
            "Missing replay package compatibility: "
            + ", ".join(sorted(required - compatible))
        )
    readme = (ROOT / "README.md").read_text(encoding="utf-8")
    if spotlight() not in readme:
        raise ValueError("Release spotlight is stale; run python repository.py sync")
    for document in ("README.md", "DEVELOPING.md", "CHANGELOG.md"):
        text = (ROOT / document).read_text(encoding="utf-8")
        targets = re.findall(r'\]\(([^)]+)\)|(?:src|srcset)="([^"]+)"', text)
        for markdown, image in targets:
            target = (markdown or image).split("#", 1)[0]
            if (
                target
                and not re.match(r"[a-z]+://", target)
                and not (ROOT / target).exists()
            ):
                raise ValueError(f"Broken local link in {document}: {target}")
    if set(ASSETS) - set(re.findall(r"download/([^\s)<\"]+)", readme)):
        raise ValueError("README must link to all seven release assets")
    for path in (ROOT / "assets").glob("*.svg"):
        ET.parse(path)
    for pattern in (
        "old trainer",
        "older trainer",
        "created by mab",
        "built by mab",
        "work in progress",
        "**WIP**",
    ):
        if pattern.lower() in readme.lower():
            raise ValueError(f"Outdated README wording: {pattern}")
    print("Repository copy, versions, changelogs, assets, and local links: PASS")


def release_notes(version):
    changes = history()
    if version not in changes:
        raise ValueError(f"No changelog for {version}")
    base = f"https://github.com/{REPOSITORY}"
    download = f"{base}/releases/download/v{version}"
    table = "\n".join(
        f"| [{label}]({download}/{name}) | `{name}` |" for name, label in ASSETS.items()
    )
    if tuple(map(int, version.split("."))) >= (7, 12, 1):
        updating = "For in-app updates, open **Updates → Check**. Private releases need a GitHub token with **Contents: read** access, saved in Windows Credential Manager."
    else:
        updating = f"To update, download Setup from the [latest release]({base}/releases/latest)."
    return f"""![PracticeSuite — Practice. Understand. Improve.]({base}/blob/main/assets/release.svg?raw=true)

**[Download Setup]({download}/ItStealsSuite-Setup.exe)** · [Portable ZIP]({download}/ItStealsSuite-Portable.zip) · [Install guide]({base}#install)

### v{version} Changelog

{changes[version]}

### Install

Close the game and trainer, run Setup, and confirm your game folder. Then open the Suite shortcut and press **F3**. Settings and replays stay in place.

{updating}

<details>
<summary><strong>All downloads</strong></summary>

| Download | File |
| --- | --- |
{table}

Keep the encoder beside the trainer and put the mod DLL in the game's **Mods** folder. The portable package includes the encoder sources and licenses.

</details>

<details>
<summary><strong>Compatibility & checks</strong></summary>

Windows 10/11 x64 · Windows Mono copy of It Steals · MelonLoader.

Build and test logs are kept in [Actions](https://github.com/{REPOSITORY}/actions). Automated checks use controlled fixtures; they do not replace testing in the game. Windows builds are unsigned.

</details>

[Vibe-coded project & development credits]({base}#about-the-project) · [Full version history]({base}/blob/main/CHANGELOG.md)
"""


def refresh_releases(repository):
    # Editing descriptions leaves published binaries, tags, and checksums intact.
    if repository != REPOSITORY:
        raise ValueError("Release refresh is restricted to the Suite repository")
    response = subprocess.run(
        ["gh", "api", f"repos/{repository}/releases?per_page=100"],
        check=True,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    versions = history()
    output = ROOT / "build/release-notes"
    output.mkdir(parents=True, exist_ok=True)
    for release in json.loads(response.stdout):
        version = release["tag_name"].removeprefix("v")
        if release["draft"] or release["prerelease"] or version not in versions:
            continue
        path = output / f"v{version}.md"
        path.write_text(release_notes(version), encoding="utf-8")
        subprocess.run(
            [
                "gh",
                "release",
                "edit",
                release["tag_name"],
                "--repo",
                repository,
                "--title",
                f"PracticeSuite v{version}",
                "--notes-file",
                str(path),
            ],
            check=True,
        )
        print(f"Updated release page: v{version}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser(
        "sync", help="Update the README spotlight from version and changelog"
    )
    commands.add_parser(
        "check", help="Check versions, links, assets, and changelog format"
    )
    notes = commands.add_parser(
        "notes", help="Write the complete notes for one version"
    )
    notes.add_argument("version")
    notes.add_argument("--output", type=Path, required=True)
    refresh = commands.add_parser(
        "refresh-releases", help="Update published release descriptions using gh"
    )
    refresh.add_argument("--repository", default=REPOSITORY)
    args = parser.parse_args()
    if args.command == "sync":
        sync()
    elif args.command == "check":
        check()
    elif args.command == "notes":
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(release_notes(args.version), encoding="utf-8")
    else:
        refresh_releases(args.repository)


if __name__ == "__main__":
    main()
