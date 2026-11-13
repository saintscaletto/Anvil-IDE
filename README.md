# Anvil IDE

## What is Anvil?

Anvil is a modern integrated development environment focused on C and C++, built for developers who want the analytical power of a heavyweight IDE without the memory cost, startup time, and background churn.

Most modern editors bolt C++ support on top of a generic text editor. Anvil takes the opposite approach: C++ is the first-class citizen from day one. Its architecture combines a battle-tested editor frontend (Monaco) with a purpose-built native core written in C++, giving you:

- Real C++ intelligence — powered by clangd, the same engine that drives world-class C++ tooling. Accurate completion, go-to-definition, find references, and diagnostics that understand your code, not just your text.

- A native core — indexing, build system integration (CMake, Make, Ninja), process management, and the debug adapter are all written in C++, so they're fast, predictable, and live in the same process as the language tooling, not behind a JavaScript bridge.

- Multi-language by design — Rust, Python, and more are supported through the Language Server Protocol and Debug Adapter Protocol. Add a language server, and Anvil speaks its language.

- A tiny footprint — no bundled Chromium, no 800 MB install. Anvil launches in under a second and uses a fraction of the memory of Electron-based editors.

- Full debugging — breakpoints, call stack inspection, variable watch, and step-through execution via GDB and LLDB, with a UI that treats debugging as a first-class workflow, not an afterthought.

## Why "Anvil"?

Because good code isn't written — it's forged. Anvil is the surface you strike against: hard, precise, and honest. Every tool in your workflow swings down onto it, and what comes out is something you can ship.
