# Seed corpus

These small checked-in seeds give libFuzzer valid or near-valid structural prefixes immediately. The ITCH Add seed is exactly 36 bytes (its final price byte is a newline) and reaches generated/reference decoding; the other seeds exercise truncation and MoldUDP64 header/count parsing. Any future crashing input should be retained here as a regression seed after minimization.
