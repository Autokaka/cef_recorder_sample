#!/usr/bin/env python3
# Usage: enable_proxy && python build_cef.py

import os
import json
import shutil
import platform
import subprocess
import argparse
import urllib.request
from pathlib import Path

DEPOT_TOOLS_URL = "https://chromium.googlesource.com/chromium/tools/depot_tools.git"
CEF_URL = "https://bitbucket.org/chromiumembedded/cef.git"
CEF_INDEX_URL = "https://cef-builds.spotifycdn.com/index.json"

SCRIPT_DIR = Path(__file__).parent.resolve()
VENDOR_CEF_DIR = SCRIPT_DIR / "vendor" / "cef"
SRC_DIR = VENDOR_CEF_DIR / "src"

SUPPORTED_PLATFORMS = ["linux64", "linuxarm64", "macosx64", "macosarm64"]

def run(cmd, cwd=None, env=None, check=True):
    print(f">>> {cmd}")
    result = subprocess.run(cmd, shell=True, cwd=cwd, env=env)
    if check and result.returncode != 0:
        raise subprocess.CalledProcessError(result.returncode, cmd)
    return result.returncode

def fetch_json(url):
    print(f"Fetching {url}")
    with urllib.request.urlopen(url, timeout=30) as resp:
        return json.loads(resp.read().decode())

def get_latest_stable_versions():
    data = fetch_json(CEF_INDEX_URL)
    for plat_key in ["linux64", "macosx64"]:
        if plat_key in data:
            versions = data[plat_key]["versions"]
            for v in versions:
                if v["channel"] == "stable":
                    cef_version = v["cef_version"]
                    chromium_version = v["chromium_version"]
                    cef_branch = cef_version.split(".")[0]
                    print(f"Latest stable: CEF {cef_version} (branch {cef_branch}), Chromium {chromium_version}")
                    return cef_branch, chromium_version
    raise RuntimeError("No stable version found")

def get_host_platform():
    system = platform.system().lower()
    machine = platform.machine().lower()
    if system == "linux":
        return "linuxarm64" if machine == "aarch64" else "linux64"
    elif system == "darwin":
        return "macosarm64" if machine == "arm64" else "macosx64"
    raise RuntimeError(f"Unsupported platform: {system}")

def setup_depot_tools():
    depot_tools_dir = SRC_DIR / "depot_tools"
    if not depot_tools_dir.exists():
        SRC_DIR.mkdir(parents=True, exist_ok=True)
        run(f"git clone {DEPOT_TOOLS_URL}", cwd=SRC_DIR)
    return depot_tools_dir

def setup_env(depot_tools_dir):
    env = os.environ.copy()
    env["PATH"] = f"{depot_tools_dir}:{env['PATH']}"
    env["DEPOT_TOOLS_UPDATE"] = "0"
    env["GIT_CACHE_PATH"] = str(SRC_DIR / "git_cache")
    return env

def clean_bad_state(chromium_git):
    bad_scm = chromium_git / "_bad_scm"
    if bad_scm.exists():
        print(f"Cleaning up {bad_scm}")
        shutil.rmtree(bad_scm)
    for item in chromium_git.iterdir():
        if item.name.startswith("_gclient_"):
            print(f"Cleaning up {item}")
            shutil.rmtree(item)

def download_chromium(env, branch):
    chromium_git = SRC_DIR / "chromium_git"
    chromium_src = chromium_git / "src"
    gclient_file = chromium_git / ".gclient"
    git_cache = SRC_DIR / "git_cache"
    version_file = chromium_git / ".chromium_version"
    
    chromium_git.mkdir(parents=True, exist_ok=True)
    git_cache.mkdir(parents=True, exist_ok=True)
    
    need_update = True
    if version_file.exists():
        cached_version = version_file.read_text().strip()
        if cached_version == branch:
            need_update = False
            print(f"Chromium version unchanged: {branch}")
        else:
            print(f"Chromium version changed: {cached_version} -> {branch}")
    
    if need_update:
        gclient_content = f'''solutions = [
  {{
    "name": "src",
    "url": "https://chromium.googlesource.com/chromium/src.git@{branch}",
    "managed": False,
    "custom_deps": {{}},
    "custom_vars": {{}},
  }},
]
cache_dir = "{git_cache}"
'''
        gclient_file.write_text(gclient_content)
        version_file.write_text(branch)
        print(f"Updated .gclient for Chromium {branch}")
    
    max_retries = 10
    for attempt in range(1, max_retries + 1):
        print(f"\n=== Sync attempt {attempt}/{max_retries} ===")
        
        clean_bad_state(chromium_git)
        
        ret = run("gclient sync --nohooks --no-history -D -R -f",
                  cwd=chromium_git, env=env, check=False)
        
        if ret == 0 and chromium_src.exists():
            print("Chromium sync successful!")
            break
        
        print(f"Sync failed (attempt {attempt}), retrying...")
    else:
        raise RuntimeError("Failed to sync chromium after max retries")
    
    run("gclient runhooks", cwd=chromium_git, env=env)
    return chromium_src

def download_cef(chromium_src, branch):
    cef_dir = chromium_src / "cef"
    if not cef_dir.exists():
        run(f"git clone {CEF_URL} cef", cwd=chromium_src)
    run("git fetch --all", cwd=cef_dir)
    run(f"git checkout {branch}", cwd=cef_dir)
    return cef_dir

def build_cef(chromium_src, env, plat, build_type):
    cef_dir = chromium_src / "cef"
    is_arm = "arm" in plat
    is_linux = "linux" in plat
    
    gn_args = [
        "is_official_build=true",
        "proprietary_codecs=true",
        "ffmpeg_branding=Chrome",
        f'target_cpu="{"arm64" if is_arm else "x64"}"',
        f'is_debug={"true" if build_type == "Debug" else "false"}',
        f'symbol_level={"2" if build_type == "Debug" else "0"}',
        "use_sysroot=false",
    ]
    if is_linux:
        gn_args.append("use_allocator=none")
    
    out_dir = f"out/{build_type}_GN_{plat}"
    run(f'gn gen {out_dir} --args="{" ".join(gn_args)}"', cwd=chromium_src, env=env)
    run(f"ninja -C {out_dir} cef", cwd=chromium_src, env=env)
    
    distrib_flag = plat.replace("64", "").replace("macos", "macos")
    run(f"python3 tools/make_distrib.py --ninja-build --{distrib_flag} --output-dir ../distrib/",
        cwd=cef_dir, env=env)

def copy_output(plat):
    distrib_dir = SRC_DIR / "chromium_git" / "distrib"
    output_dir = VENDOR_CEF_DIR / "out" / plat
    
    if output_dir.exists():
        shutil.rmtree(output_dir)
    
    if not distrib_dir.exists():
        print(f"Warning: distrib dir not found at {distrib_dir}")
        return
    
    for item in distrib_dir.iterdir():
        if item.is_dir() and plat.replace("macos", "macos") in item.name.lower():
            shutil.copytree(item, output_dir)
            print(f"Output copied to {output_dir}")
            return
    
    print(f"Warning: No distrib found for {plat}")

def main():
    parser = argparse.ArgumentParser(description="Build CEF from source")
    parser.add_argument("--branch", default=None, help="CEF branch (auto-detect if not set)")
    parser.add_argument("--chromium-branch", default=None, help="Chromium version (auto-detect if not set)")
    parser.add_argument("--build-type", default="Release", choices=["Release", "Debug"])
    parser.add_argument("--platform", default=None, choices=SUPPORTED_PLATFORMS + ["all"],
                        help="Target platform (default: host platform)")
    args = parser.parse_args()
    
    if args.branch and args.chromium_branch:
        cef_branch, chromium_branch = args.branch, args.chromium_branch
    else:
        cef_branch, chromium_branch = get_latest_stable_versions()
        if args.branch:
            cef_branch = args.branch
        if args.chromium_branch:
            chromium_branch = args.chromium_branch
    
    platforms = SUPPORTED_PLATFORMS if args.platform == "all" else [args.platform or get_host_platform()]
    
    print(f"CEF branch: {cef_branch}")
    print(f"Chromium version: {chromium_branch}")
    print(f"Building for: {', '.join(platforms)}")
    print(f"Source dir: {SRC_DIR}")
    
    depot_tools = setup_depot_tools()
    env = setup_env(depot_tools)
    chromium_src = download_chromium(env, chromium_branch)
    download_cef(chromium_src, cef_branch)
    
    for plat in platforms:
        print(f"\n{'='*50}")
        print(f"Building for {plat}")
        print(f"{'='*50}\n")
        build_cef(chromium_src, env, plat, args.build_type)
        copy_output(plat)
    
    print(f"\nBuild complete!")
    print(f"Source: {SRC_DIR}")
    for plat in platforms:
        print(f"Output: {VENDOR_CEF_DIR / 'out' / plat}")

if __name__ == "__main__":
    main()
