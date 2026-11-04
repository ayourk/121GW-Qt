# SPDX-License-Identifier: MIT
"""Load the patched libunicorn (2.1.4 + upstream PR #2344's ARM fix) instead of Ubuntu's python3-unicorn library.

Unicorn 2.1.4 loses the Thumb IT state when a UC_HOOK_MEM_WRITE/READ hook covers a store inside an IT block, so the
first instruction(s) after the block run under a stale condition (upstream issue #2309, fixed on dev by #2344, not
released). Our peripheral models are such hooks. The fixed library is built by 121gw-libs/unicorn/build.zsh
beside the project (UNICORN_LIB overrides the build directory). Ubuntu's bindings load "libunicorn.so.2" by soname, and LD_LIBRARY_PATH is read only at process start, so
importing this module re-executes the process (same command line, from /proc/self/cmdline) with LD_LIBRARY_PATH set.
Import it before anything imports unicorn. Set UCFIX=0 to run on the system library (for comparisons). Once the fixed .deb is installed this does nothing."""
import os, sys
LIB = os.environ.get("UNICORN_LIB", os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "121gw-libs", "unicorn", "build"))
def _system_fixed():
    """True when the installed libunicorn2t64 carries the fix (our unicorn backport, 2.1.4-4+p1 or later)."""
    import subprocess
    try: v = subprocess.run(["dpkg-query", "-W", "-f", "${Version}", "libunicorn2t64"], capture_output=True, text=True).stdout
    except OSError: return False
    return v and subprocess.run(["dpkg", "--compare-versions", v, "ge", "2.1.4-4+p1"]).returncode == 0
if os.environ.get("UCFIX", "1") != "0" and os.path.exists(LIB + "/libunicorn.so.2") and not _system_fixed():
    paths = [p for p in os.environ.get("LD_LIBRARY_PATH", "").split(":") if p]
    if LIB not in paths:
        os.environ["LD_LIBRARY_PATH"] = ":".join([LIB] + paths)
        argv = [x.decode() for x in open("/proc/self/cmdline", "rb").read().split(b"\0")[:-1]]
        sys.stdout.flush(); sys.stderr.flush()
        os.execve(sys.executable, [sys.executable] + argv[1:], os.environ)
