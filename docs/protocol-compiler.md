# Protocol compiler

`efe_protocolc` turns `schemas/itch50.efe` into a C++ header during the CMake build:

```text
schema -> lexer -> tokens -> parser/AST -> semantic checks -> deterministic C++ emission
```

The DSL supports `u8`, big-endian `u16/u32/u48/u64`, `char`, `ascii[N]`, `price4`, and `price8`. Message type, message name, and field names must be unique; message identifiers are one byte; ASCII widths must be positive; and unknown types or malformed declarations are rejected.

Generated views hold a byte span, validate exact type and length, and expose typed fixed-offset accessors backed by the bounds-checked byte reader. `message_size` and `message_name` provide static dispatch metadata. There is no runtime schema lookup.

CTest runs valid and invalid schemas and compares SHA-256 hashes from two generations. The normal build also compiles all 23 generated ITCH views into `efe_core`.

## Schema example

```text
message AddOrder "A" {
    stock_locate:    u16_be;
    tracking_number: u16_be;
    timestamp:       u48_be;
    order_reference: u64_be;
    side:            char;
    shares:          u32_be;
    stock:           ascii[8];
    price:           price4;
}
```

Fields are laid out sequentially after the one-byte message type. The compiler calculates offsets from declared widths, making the schema the source of truth for production layouts.

## Generated API shape

```cpp
using efe::generated::nasdaqitch50::AddOrderView;

const AddOrderView message{bytes};
const std::uint64_t id = message.order_reference();
const std::uint32_t raw_price = message.price();
```

Construction rejects a span with the wrong type or exact size. Accessors return fixed-width values or borrowed ASCII views; prices remain integers. A generated object contains a span, not copied message fields.

## Validation and determinism

Compilation rejects duplicate message names or types, duplicate fields, invalid identifiers, unsupported types, invalid ASCII widths, malformed syntax, and invalid layouts. Deterministic emission is checked by generating the same schema twice and comparing hashes.

Generated code avoids a runtime reflection table and keeps byte offsets from spreading across the codebase. The handwritten decoder stays small and independent so the differential tests have something trustworthy to compare against. It is not a second source of truth for production parsing.

## Checking layouts

Layout changes must be checked against the [official Nasdaq TotalView-ITCH 5.0 specification](https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHSpecification.pdf), then expressed in `schemas/itch50.efe` and covered by compiler and decoder tests.
