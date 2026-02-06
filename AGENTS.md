# AGENTS.md

## Purpose
This repository is an experimental playground for:
- Hand-authoring WebAssembly binaries (Python/C++),
- Running and validating them in Node.js/browser,
- Parsing/translating WASM (`wasm_to_wat.cpp`),
- Exploring Mach-O parsing/writing on macOS (`parse_macho.cpp`, `write_macho.cpp`).

Keep changes minimal, local, and workflow-driven. Prefer targeted edits over broad refactors.

## Tech Stack
- C++ (single-file tools, no build system)
- Python scripts (WASM byte generation and codegen)
- Node.js (`test.js`) for runtime validation
- macOS toolchain utilities (`as`, `ld`, `clang++`) for Mach-O flow

## Repository Map
- `README.md`: primary manual workflows.
- `t1.py`, `t2.py`, `t3.py`, `t4.py`, `t4.cpp`: WASM generation experiments.
- `test.js`: loads `test.wasm`, calls exports, prints output.
- `wasm_to_wat.cpp`, `wasm_to_wat.h`: decode WASM binary and print WAT.
- `wasm_instructions.txt`: instruction definitions source.
- `wasm_instructions_visitor.py`: generator for visitor header.
- `wasm_visitor.h`: generated visitor code (do not hand-edit unless necessary).
- `parse_macho.cpp`, `write_macho.cpp`, `macho_utils.h`: Mach-O experiments.
- `build.sh`: scripted Mach-O build/run pipeline.

## Environment Assumptions
- macOS (Mach-O flow and linker paths are Apple-specific).
- `clang++`, `g++`, `python`, `node`, `as`, `ld` available in `PATH`.
- Python package `leb128` installed for Python WASM scripts.

## Verified Workflows

### 1) Quick WASM sanity path (works)
```bash
g++ t4.cpp && ./a.out && node test.js
```
Expected output:
- `-10`
- `-1`
- `Success!`

### 2) Python WASM generation
```bash
pip install leb128
python t1.py
# or: python t4.py && cp test4.wasm test.wasm
```
Then run:
```bash
node test.js
```

### 3) Browser check
```bash
python -m http.server
```
Open `http://localhost:8000/a.html`, then inspect browser console.

### 4) WASM -> WAT tool
Strict warnings-as-errors currently fail due unused warnings in shared/generated code.

Fails:
```bash
clang++ -std=c++17 -Wall -Wextra -Werror wasm_to_wat.cpp -o wasm_to_wat
```

Works (relaxed warnings):
```bash
clang++ -std=c++17 -Wall -Wextra wasm_to_wat.cpp -o wasm_to_wat
./wasm_to_wat
```

### 5) Mach-O flow (`build.sh`)
`build.sh` is expected to pass under `-Werror` on modern macOS SDKs.

## Editing Rules
- Treat this as an experimentation repo: keep behavior-preserving unless explicitly asked to redesign.
- If touching generated visitor behavior:
  - Prefer editing `wasm_instructions.txt` and/or `wasm_instructions_visitor.py`, then regenerate `wasm_visitor.h`.
- Do not silently switch toolchains or introduce heavy build systems unless requested.
- Keep commands reproducible from repo root.

## Regeneration Notes
To regenerate visitor header from local definitions:
```bash
python wasm_instructions_visitor.py wasm_instructions.txt wasm_visitor.h
```

## Validation Guidance
When changing WASM generation/parsing logic, run at least:
1. `g++ t4.cpp && ./a.out && node test.js`
2. `clang++ -std=c++17 -Wall -Wextra wasm_to_wat.cpp -o wasm_to_wat && ./wasm_to_wat`

When changing Mach-O logic, run:
1. `./build.sh` (or equivalent individual compile commands)
2. If `-Werror` fails, report exact diagnostics instead of masking them.
