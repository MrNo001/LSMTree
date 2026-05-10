# LSM and B+ tree.

Simple implementation of a Log-Streuctured Merge storage engine

---

## Overview

An LSM-Tree is a method of implementing a database engine that supports write optimized operations while provideing  efficient range querys.


---
## Architecture

The project follows a classic LSM design: write to memory first (**MemTable**), periodically flush sorted data to disk (**SSTables**), and track files in a **Manifest**.

### Core classes

- `CLI` (`src/CLI.cpp`): interactive entry point; parses user commands and forwards them to `LSMStore`.
- `LSMStore` (`src/LSMStore.cpp`): main coordinator. Owns the in-memory table, manages flushes, reads/writes manifest metadata, and resolves reads across memory + disk.
- `SkipList` (`src/SkipList.cpp`): MemTable implementation (in-memory, sorted by key). Supports insert/update/search and exports sorted entries for flush.
- `SSTable` (`src/SSTable.cpp`): immutable on-disk table format. Writes sorted key/value pairs and supports key lookup from persisted files.
- `Manifest` (`src/Manifest.cpp`): small metadata file (`MANIFEST`) that stores the ordered list of SSTable file paths so state survives restart.

### Query flow

- **Insert (`put`)**
  1. `CLI` calls `LSMStore::insert(key, value)`.
  2. `LSMStore` writes into `SkipList` (MemTable).
  3. If MemTable size reaches the flush threshold, `LSMStore` flushes it:
     - exports sorted entries from `SkipList`,
     - writes a new `sst_XXXXXX.sst` via `SSTable::write`,
     - appends that file to manifest state and persists it through `Manifest::save`,
     - clears MemTable.

- **Delete (`remove`)**
  1. `CLI` calls `LSMStore::remove(key)`.
  2. Store writes a **tombstone** (special value) into MemTable instead of physically removing old disk entries.
  3. On flush, tombstones are persisted to SSTable and hide older values for the same key during reads.

- **Search (`get`)**
  1. `CLI` calls `LSMStore::search(key)`.
  2. Store checks MemTable first (newest data).
  3. If not found, store scans SSTables from newest to oldest using manifest order.
  4. First match wins; if the match is a tombstone, result is treated as deleted (`-1`).

At startup, `LSMStore` creates/opens the data directory, loads `MANIFEST`, and rebuilds the in-memory list of known SSTables so reads can immediately see persisted data.

---

## B+ tree on-disk page (`BTreeStore`)

Each node is one **4096-byte page**. Layout is slotted: a fixed header, a **slot directory** (index), and a **record heap** toward the end of the page.

```
byte 0
│
▼  ┌──────────────────────────────────────────────────────────────────┐
   │ PageHeader                                                       │
   │  page_id, parent_id, next_page_id, prev_page_id, slot_count,     │
   │  free_ptr, is_leaf                                               │
   ├──────────────────────────────────────────────────────────────────┤
   │ Slot[0] │ Slot[1] │ ... │ Slot[slot_count - 1]                   │
   │ (offset, size) per slot — points into record bytes below         │
   ├──────────────────────────────────────────────────────────────────┤
   │                     (unused middle shrinks as slots / heap meet) │
   ├──────────────────────────────────────────────────────────────────┤
   │ record bytes … stored in [free_ptr .. 4096)                      │
   └──────────────────────────────────────────────────────────────────┘
                                                         4096-byte page ▲
```

**Leaf page** (`is_leaf = 1`): `prev_page_id` / `next_page_id` link leaves left/right at the same level. Each record is **key (4 bytes) + value length (2 bytes) + payload**.

**Internal page** (`is_leaf = 0`): `prev_page_id` holds the **leftmost child** page id; `next_page_id` is unused. Each slot stores **separator key (4 bytes) + right child page id (4 bytes)** — eight bytes per entry — so keys partition subtrees to the right.

Conceptually (keys `·` separate ranges; `L*` are child page ids):

```
internal                         leaf chain (siblings)
────────                         ─────────────────────

        ┌─ keys ··· ─┐           page A ◄──► page B ◄──► page C
        │            │              (k,v)*   (k,v)*   (k,v)*
   L0   │   L1  L2   │   L3
        └────────────┘
      ▲
      └── header.prev_page_id = L0
```

---

## Requirements

List all prerequisites:

- OS: **Linux**, **macOS**, or **Windows**. On Windows, install **g++** and **GNU Make** and run them from an environment where the Makefile runs normally (common: **MSYS2**, **MinGW-w64**, or **Git Bash**). **WSL** works too but is not required.
- Compiler: **g++** (GCC) with **C++17** support (`-std=c++17` is used by the test targets in the Makefile).
- Build tools: **GNU Make** (`make`).

---

## Limitations

Still only stores int values,should implement value interface for all storable values

---

## Roadmap

- [x] Impelment MemTable
- [x] Manifest load/store 
- [x] SSTable implementation

---

## Author

- Niko Knežević
