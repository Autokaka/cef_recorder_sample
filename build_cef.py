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
                    # CEF 分支号是 Chromium 版本的第三段，如 143.0.7499.193 -> 7499
                    cef_branch = chromium_version.split(".")[2]
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
    # 确保 depot_tools 已初始化
    run("./update_depot_tools", cwd=depot_tools_dir, check=False)
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
    sync_done_marker = chromium_git / ".sync_done"
    
    chromium_git.mkdir(parents=True, exist_ok=True)
    git_cache.mkdir(parents=True, exist_ok=True)
    
    need_sync = True
    if version_file.exists():
        cached_version = version_file.read_text().strip()
        if cached_version == branch:
            print(f"Chromium version unchanged: {branch}")
            # 检查是否已经成功 sync 过
            if sync_done_marker.exists() and chromium_src.exists() and (chromium_src / "BUILD.gn").exists():
                need_sync = False
                print("Source already synced, skipping gclient sync")
        else:
            print(f"Chromium version changed: {cached_version} -> {branch}")
            sync_done_marker.unlink(missing_ok=True)
    
    gclient_content = f'''solutions = [
  {{
    "name": "src",
    "url": "https://chromium.googlesource.com/chromium/src.git@{branch}",
    "managed": False,
    "custom_deps": {{}},
    "custom_vars": {{
      "checkout_pgo_profiles": True,
      "source_tarball": False,
    }},
  }},
]
cache_dir = "{git_cache}"
'''
    gclient_file.write_text(gclient_content)
    version_file.write_text(branch)
    
    if need_sync:
        max_retries = 10
        for attempt in range(1, max_retries + 1):
            print(f"\n=== Sync attempt {attempt}/{max_retries} ===")
            
            clean_bad_state(chromium_git)
            
            ret = run("gclient sync --nohooks --no-history -R -f",
                      cwd=chromium_git, env=env, check=False)
            
            if ret == 0 and chromium_src.exists():
                print("Chromium sync successful!")
                break
            
            print(f"Sync failed (attempt {attempt}), retrying...")
        else:
            raise RuntimeError("Failed to sync chromium after max retries")
        
        run("gclient runhooks", cwd=chromium_git, env=env)
        sync_done_marker.touch()
    
    return chromium_src

def download_cef(chromium_src, branch):
    cef_dir = chromium_src / "cef"
    cef_version_file = cef_dir / ".cef_branch"
    
    if not cef_dir.exists():
        run(f"git clone {CEF_URL} cef", cwd=chromium_src)
    
    # 检查 CEF 分支是否需要更新
    current_branch = ""
    if cef_version_file.exists():
        current_branch = cef_version_file.read_text().strip()
    
    if current_branch != branch:
        run("git fetch --all", cwd=cef_dir)
        run(f"git checkout {branch}", cwd=cef_dir)
        cef_version_file.write_text(branch)
    else:
        print(f"CEF branch unchanged: {branch}")
    
    return cef_dir

def build_cef(chromium_src, env, plat, build_type, clean=False, jobs=None):
    cef_dir = chromium_src / "cef"
    is_arm = "arm" in plat
    is_linux = "linux" in plat
    is_mac = "macos" in plat
    
    target_cpu = "arm64" if is_arm else "x64"
    
    # GN 构建参数（启用 PGO 以获得最佳性能）
    # CEF BUILD.gn 中的 assert 检查要求以下参数必须为 true：
    # - enable_print_preview
    # - enable_widevine (及相关 CDM 参数)
    # - toolkit_views
    # - enable_rlz (当 enable_widevine 时)
    gn_args_list = [
        "is_official_build=true",
        "proprietary_codecs=true",
        'ffmpeg_branding="Chrome"',
        f'target_cpu="{target_cpu}"',
        "use_sysroot=false",
        # CEF 必需参数
        "enable_print_preview=true",
        "toolkit_views=true",
        # Widevine DRM 相关参数
        "enable_widevine=true",
        "enable_cdm_host_verification=true",
        "enable_cdm_storage_id=true",
        'alternate_cdm_storage_id_key="pup_cef_cdm_storage"',
        "enable_rlz=true",
        # CEF 要求禁用 Chrome clang 插件
        "clang_use_chrome_plugins=false",
        # 减少链接时内存使用
        "use_thin_lto=false",  # ThinLTO 省时间但吃内存
    ]
    
    if build_type == "Debug":
        gn_args_list.extend(["is_debug=true", "symbol_level=2"])
    else:
        gn_args_list.extend(["is_debug=false", "symbol_level=0"])
    
    if is_linux:
        gn_args_list.append("use_allocator=none")
    
    gn_args_str = "\n".join(gn_args_list)
    
    # CEF 构建目录
    out_dir = f"out/{build_type}_GN_{plat}"
    out_path = chromium_src / out_dir
    
    # 清理构建目录（如果指定 --clean）
    if clean and out_path.exists():
        print(f"Cleaning {out_path}...")
        shutil.rmtree(out_path)
    
    # 写入 GN args（只在内容变化时写入）
    args_file = out_path / "args.gn"
    args_file.parent.mkdir(parents=True, exist_ok=True)
    
    current_args = args_file.read_text() if args_file.exists() else ""
    args_changed = current_args.strip() != gn_args_str.strip()
    
    if args_changed:
        args_file.write_text(gn_args_str)
        print("args.gn updated")
    
    # 检查是否需要运行 gclient_hook.py 和 gn gen
    # gclient_hook.py 会 patch chromium 源码，每次运行都会改变文件时间戳导致重新编译
    build_ninja = out_path / "build.ninja"
    need_gen = args_changed or not build_ninja.exists()
    
    if need_gen:
        # 只在首次或 args 变化时运行 gclient_hook.py
        run(f"python3 cef/tools/gclient_hook.py", cwd=chromium_src, env=env, check=False)
        run(f"gn gen {out_dir}", cwd=chromium_src, env=env)
    else:
        print("Build config unchanged, skipping gn gen (incremental build)")
    
    # 使用 CEF 的 sandbox target
    jobs_flag = f"-j {jobs}" if jobs else ""
    run(f"ninja {jobs_flag} -C {out_dir} cefsimple", cwd=chromium_src, env=env)
    
    # 创建分发包
    if is_mac:
        distrib_flag = "mac64" if not is_arm else "macarm64"
    elif is_linux:
        distrib_flag = "linux64" if not is_arm else "linuxarm64"
    else:
        distrib_flag = plat
    
    run(f"python3 tools/make_distrib.py --ninja-build --{distrib_flag} --output-dir ../../distrib/",
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
    parser.add_argument("--clean", action="store_true", help="Clean build output before building")
    parser.add_argument("-j", "--jobs", type=int, default=None,
                        help="Number of parallel jobs (default: auto, reduce if OOM)")
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
        build_cef(chromium_src, env, plat, args.build_type, clean=args.clean, jobs=args.jobs)
        copy_output(plat)
    
    print(f"\nBuild complete!")
    print(f"Source: {SRC_DIR}")
    for plat in platforms:
        print(f"Output: {VENDOR_CEF_DIR / 'out' / plat}")

if __name__ == "__main__":
    main()
