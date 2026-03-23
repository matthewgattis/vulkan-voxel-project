# ECS Scaling: Beyond Sparse Sets

Our current ECS uses a **sparse-set** per component type. This is a great fit at moderate entity counts, but there are known scaling limits and alternative designs worth understanding.

## Sparse-Set Tradeoffs

**Strengths:**
- O(1) add, remove, get, has
- Dense arrays are packed and contiguous — cache-friendly iteration over a single component type
- Swap-and-pop removal keeps arrays packed with no gaps
- Simple to implement and reason about

**Weaknesses:**
- Sparse array is sized to the highest entity index ever issued, not the number of living entities. Doesn't shrink.
- Each component type has its own pool, so multi-component queries (`View<A, B, C>`) jump between separate dense arrays — less cache-friendly across types.
- At very high entity counts (millions) with frequent create/destroy, the sparse array memory and index space can become a concern.

## Paged Sparse Arrays

Instead of one flat `vector<optional<size_t>>` for the sparse side, you can **page** it — allocate fixed-size pages (e.g., 4096 slots each) on demand. Pages that correspond to unused index ranges are never allocated.

- Reduces memory waste when entity indices are sparse or the high-water mark is large
- Same O(1) lookup: `page[index / PAGE_SIZE][index % PAGE_SIZE]`
- Pages can be freed when all their entities are destroyed
- EnTT (a popular C++ ECS library) uses this approach

## Archetype-Based Storage

This is what **Unity DOTS** (and Flecs, another popular ECS) uses. Instead of storing each component type in its own pool, entities are grouped by their **archetype** — the exact set of component types they have.

An archetype is essentially a table:
- Columns are component types (e.g., Transform, GeometryComponent, MaterialComponent)
- Rows are entities with exactly that combination
- All data for a row is stored contiguously or in parallel arrays within the archetype

**How it works:**
1. Entity with components `{A, B, C}` lives in the `{A, B, C}` archetype table
2. Adding component `D` moves the entity to the `{A, B, C, D}` archetype table
3. Queries like `View<A, B>` find all archetypes that contain both A and B, then iterate their tables directly

**Strengths:**
- Multi-component iteration is maximally cache-friendly — components you're querying are co-located in the same table
- No per-entity filtering needed during queries — if an entity is in an archetype table, it has all the components by definition
- Memory is dense and well-packed

**Weaknesses:**
- Adding or removing components is expensive — it moves the entire entity between archetype tables (memcpy of all components)
- More complex to implement (archetype graph, table management, component move logic)
- Many unique component combinations can lead to many small tables ("archetype fragmentation")

## When to Consider Switching

Our sparse-set ECS is the right choice for now. Consider alternatives if:
- Entity counts reach tens/hundreds of thousands with tight frame budgets
- Multi-component queries become a profiled bottleneck (cache misses across pools)
- Memory profiling shows sparse array waste is significant

## Further Reading

- **EnTT** — mature C++ sparse-set ECS library (good reference for paged sparse sets)
- **Flecs** — C/C++ archetype ECS with query caching and a rich feature set
- **Unity DOTS / ECS** — archetype-based, designed for millions of entities
- **"ECS Back and Forth"** (Michele Caini / skypjack blog series) — deep dives on sparse-set internals from the EnTT author
