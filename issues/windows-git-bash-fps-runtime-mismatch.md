# Known issues

## Windows: approximately 64 FPS when launched from Git Bash

### Symptoms

The demo targets 120 FPS. On the investigated Windows system, the same
`circle.exe` ran at approximately 119–120 FPS when launched directly, but at
approximately 64 FPS when launched from Git Bash.

### Confirmed cause

Git Bash changes `PATH`, placing its runtime directory ahead of the installed
compiler's directory. If the required runtime DLLs are not shipped alongside
`circle.exe`, Windows can load identically named DLLs from Git's installation
instead of the MinGW toolchain used to build the demo.

The observed runtime locations were:

- Direct launch: `libstdc++-6.dll` and `libgcc_s_seh-1.dll` from the installed
  MinGW toolchain, built with the Win32 thread model.
- Git Bash launch: `libstdc++-6.dll`, `libgcc_s_seh-1.dll`, and
  `libwinpthread-1.dll` from `C:\Program Files\Git\ucrt64\bin`.

These runtime builds implement short thread waits differently. Measurements in
a separate diagnostic copy of the demo showed:

| Runtime libraries | Frame work | Requested sleep | Actual sleep | FPS |
| --- | --- | --- | --- | --- |
| Installed MinGW toolchain | About 0.5–0.7 ms | About 7.7–7.9 ms | About 7.8–7.9 ms | 119–120 |
| Git for Windows | About 0.6 ms | About 7.7 ms | About 15 ms | 64 |

With Git's libraries, the resulting frame interval was approximately 15.6 ms,
which corresponds to 64 FPS. The rendering workload did not account for the
difference.

The result was reproduced without Bash by changing only the child process's
`PATH`: toolchain libraries produced 119–120 FPS, Git's libraries produced
64 FPS, and restoring the toolchain libraries restored 120 FPS. This confirms
runtime selection as the cause; the exact internal source of the longer wait
in the installed Git runtime was not determined.

### Distribution guidance

Ship the required MinGW runtime DLLs, including their dependencies, next to
`circle.exe`. Use a consistent set from the toolchain used to build the
application. Under the standard Windows DLL search order, the application
directory is searched before `PATH`, preventing this particular conflict.

For local investigation, placing the matching toolchain's `bin` directory
before Git's runtime directory in the launching process's `PATH` also avoids
the observed conflict. No system-wide environment change is required.

This issue does not justify changing the demo's FPS limiter. Bundling the
matching runtime addresses the observed DLL conflict, but does not guarantee
exact frame timing under all workloads or operating system scheduling
conditions. No FPS limiter or packaging changes were made as part of the
investigation.

### Related reports and documentation

- [MinGW runtime mismatch in GitHub Actions, issue #8399](https://github.com/actions/runner-images/issues/8399):
  another confirmed case of MinGW applications loading Git's C++ runtime
  through `PATH`; its reported symptom was a startup failure rather than
  reduced FPS.
- [MinGW-w64 discussion of timed-wait accuracy](https://sourceforge.net/p/mingw-w64/mailman/mingw-w64-public/thread/ddf057d8-a796-f0ae-a37f-92b815af21e4%40martin.st/):
  related discussion of excessive waits and coarse time measurements in
  winpthreads; not a confirmed identification of the exact installed-library
  behavior observed here.
- [Windows DLL search order](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-search-order).
