"""MkDocs hook that builds the Doxygen C++ API reference and publishes it under api/.

Doxygen runs once per MkDocs process into build/doxygen/api (outside docs/, so `mkdocs serve`
does not rebuild in a loop), and every generated file is added to the site. Without Doxygen
the site gets a placeholder page instead, unless NMEASIM_REQUIRE_DOXYGEN=1, which CI sets.
Set DOXYGEN to use a Doxygen binary that is not in PATH.
"""

import logging
import os
import pathlib
import re
import shutil
import subprocess

from mkdocs.exceptions import PluginError
from mkdocs.structure.files import File

log = logging.getLogger("mkdocs.hooks.doxygen")

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUTPUT_ROOT = ROOT / "build" / "doxygen"
API_DIR = OUTPUT_ROOT / "api"

PLACEHOLDER = """<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8"><title>C++ API reference</title></head>
<body><h1>C++ API reference</h1>
<p>This build of the documentation was made without Doxygen. Install Doxygen and run
<code>mkdocs build</code> again, or read the published reference at
<a href="https://dimitrios-kafetzis.github.io/NMEA_Simulator_X/api/">
dimitrios-kafetzis.github.io/NMEA_Simulator_X/api/</a>.</p></body></html>
"""

_built = False


def project_version() -> str:
    text = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(r'set\(NMEASIM_VERSION "([^"]+)"\)', text)
    return match.group(1) if match else ""


def on_pre_build(config) -> None:
    global _built
    if _built:
        return
    shutil.rmtree(OUTPUT_ROOT, ignore_errors=True)
    doxygen = shutil.which(os.environ.get("DOXYGEN", "doxygen"))
    if doxygen is None:
        if os.environ.get("NMEASIM_REQUIRE_DOXYGEN") == "1":
            raise PluginError("Doxygen is required (NMEASIM_REQUIRE_DOXYGEN=1) but not found")
        log.info("Doxygen not found; publishing a placeholder API page")
        API_DIR.mkdir(parents=True)
        (API_DIR / "index.html").write_text(PLACEHOLDER, encoding="utf-8")
    else:
        OUTPUT_ROOT.mkdir(parents=True)
        environment = dict(os.environ, NMEASIM_VERSION=project_version())
        result = subprocess.run(
            [doxygen, "tools/doxygen/Doxyfile"], cwd=ROOT, env=environment,
            capture_output=True, text=True, check=False)
        if result.returncode != 0:
            raise PluginError("Doxygen failed:\n" + result.stderr.strip())
    _built = True


def on_files(files, config):
    for path in sorted(API_DIR.rglob("*")):
        if path.is_file():
            files.append(File(path.relative_to(OUTPUT_ROOT).as_posix(), src_dir=str(OUTPUT_ROOT),
                              dest_dir=config["site_dir"],
                              use_directory_urls=config["use_directory_urls"]))
    return files
