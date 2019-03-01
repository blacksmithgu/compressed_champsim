# Cleanup

- Rename "COMPRESSED_CACHE_BLOCK" and co to "COMPRESSED_BLOCK".
- Think of how to eliminate the #ifdef and/or is_compressed.

- The code doesn't actually have buffer overflows, but evicted_cf is used very confusedly when it is used (it is passed by reference and mutated multiple times). This flow should be cleared up.
- There are several places where code duplicates `check_hit_cc` behavior to find the right CF index; this should be removed.
- `evict_compressed_line` is an ugly mess.