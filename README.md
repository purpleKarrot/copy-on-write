# `copy_on_write`: A Vocabulary Type for Lazily-Copied Values

This repository contains a C++20 reference implementation of `copy_on_write<T>`,
an allocator-aware vocabulary type for values that are expensive to copy.
Copies can share the underlying storage, with copying deferred until a shared
value is modified. Read access is const-qualified; mutation goes through the
`modify` interface, which detaches shared storage as needed.

- [Latest released specification (P4210)](https://wg21.link/p4210)
- [Paper source repository](https://github.com/purpleKarrot/wg21-papers)
