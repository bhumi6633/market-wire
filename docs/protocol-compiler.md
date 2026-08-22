# Protocol compiler

`efe_protocolc` turns `schemas/itch50.efe` into a C++ header during the CMake build:

```text
schema -> lexer -> tokens -> parser/AST -> semantic checks -> deterministic C++ emission
```

The DSL supports `u8`, big-endian `u16/u32/u48/u64`, `char`, `ascii[N]`, `price4`, and `price8`. Message type, message name, and field names must be unique; message identifiers are one byte; ASCII widths must be positive; and unknown types or malformed declarations are rejected.

Generated views hold a byte span, validate exact type and length, and expose typed fixed-offset accessors backed by the bounds-checked byte reader. `message_size` and `message_name` provide static dispatch metadata. There is no runtime schema lookup.

CTest runs valid and invalid schemas and compares SHA-256 hashes from two generations. The normal build also compiles all 23 generated ITCH views into `efe_core`.
