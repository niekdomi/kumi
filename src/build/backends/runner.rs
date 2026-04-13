// Native parallel build runner (ninja replacement).
// Walks the build graph, schedules tasks respecting dependency order,
// and executes compile/link commands with maximum parallelism.
