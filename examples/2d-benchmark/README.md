# MNGL 2D benchmark

The benchmark is an opt-in target in the root CMake project. Configure with
`-DMNGL_BUILD_2D_BENCHMARK=ON`, build `mngl_2d_benchmark`, then run it from its
build output directory so it can find the copied `shaders/` directory.

Use `1`, `2`, and `3` to switch between sprite-heavy, geometry-heavy, and mixed
scenes. Press `F2` to toggle GPU culling and `Escape` to quit. The default scene
uses 20,000 sprites and 20,000 triangles, with 70% placed outside the view.
Arguments can change the counts and offscreen percentage; pass `--help` for the
syntax.

The scene discards 120 warm-up frames after startup, resize, or mode changes. It
then prints average and 95th-percentile CPU timings over the most recent 120
frames. See the [2D profiling guide](../../docs/2d/benchmarking.md) for timing
definitions and limits.
