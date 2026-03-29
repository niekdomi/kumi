// Wrapper around the external ccache tool.
// Manages compilation caching by intercepting compiler invocations
// and reusing previously cached object files when inputs haven't changed.
