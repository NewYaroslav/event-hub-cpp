# Build And Test

## Build System

Use CMake as the project build system. Preserve Windows MinGW compatibility when
changing build scripts, compiler options, or examples.

## Windows Workflow

On Windows, prefer the system MinGW toolchain and the `MinGW Makefiles`
generator unless a task explicitly asks for a different generator.

Typical flow:

```bash
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
```

If MinGW is unavailable, use another local CMake generator and state the
generator in the final task result.

## Verification Expectations

- For C++ changes, run at least configure and build.
- Run `ctest --test-dir build --output-on-failure` when tests exist.
- Build examples when the touched behavior affects documented usage.
- If a check cannot be run because a local tool is missing, say that explicitly
  instead of treating it as a pass.
