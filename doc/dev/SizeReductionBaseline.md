Size reduction baseline
=======================

[Current size-reduction baselines](../CurrentSizeReductionBaselines.md) records output sizes for the JavaScript library benchmark. It reports the original, normally minified, and mangled output sizes, but does not measure execution time or memory usage.

`test-js-libs.sh` downloads the pinned JavaScript libraries when necessary, checks that both generated outputs are valid JavaScript, and verifies that the documented table and SVG remain current. The test does not modify documentation.

To intentionally refresh the table and chart after a relevant change, run `update-size-reduction-baselines.sh`. It runs the benchmark, updates the baseline document, and invokes the chart utility described in [Utilities](Utilities.md). Review the resulting documentation changes before committing them.
