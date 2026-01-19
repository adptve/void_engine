Port bottom-up, crate by crate

I would take this order (rough sketch):

void_math, void_core, void_structures, void_memory

void_ecs, void_event

void_asset + file formats, void_audio

void_render stack (+ shader pipeline & graph)

void_physics, void_ai, gameplay systems (void_combat, void_inventory, etc.)

void_engine, void_runtime, editor/tools.

For each crate:

Mirror the public API in C++.

Replace struct and enum with C++ equivalents.

Map Rust generics to templates or concrete specialisations where appropriate.

Map Result<T,E> / thiserror to either:

expected<T,E> (if you adopt that pattern), or

C++ exceptions at the API boundary (if you are comfortable with them).

Translate tests first.

For each Rust crate, translate unit tests to C++ (GoogleTest, Catch2, etc.).

Run Rust tests and C++ tests against the same inputs (e.g. math vectors, ECS ops, serialised asset blobs) and verify same outputs.

Port implementation essentially “mechanically”, but with RAII.

For code that doesn’t depend heavily on the borrow checker (math, small algorithms), it is fine to go fairly line-by-line.

For parts that do depend heavily on lifetimes (resource management, async), take a step back and redesign using RAII and ownership semantics that are natural in C++.

Use Rust only as an oracle.

For a function f, generate a big set of random or recorded inputs, log outputs on the Rust side, and compare to the C++ results.

You avoid most FFI complexity while still leveraging the existing code.