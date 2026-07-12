Size reduction baseline
=======================

[Current size-reduction baselines](../CurrentSizeReductionBaselines.md) records output sizes for the JavaScript library benchmark. It reports the original, normally minified, and mangled output sizes, but does not measure execution time or memory usage.

`test-js-libs.sh` downloads the pinned JavaScript libraries when necessary and checks that both generated outputs are valid JavaScript. Normal test runs also reject output that is larger than its input, or mangled output that is larger than normally minified output. The test does not modify documentation.

`make bench` passes `--bench` to the test scripts. For `test-js-libs.sh`, that compares the generated table and SVG with the documented baseline. A matching benchmark stays quiet. A mismatch prints the generated benchmark table and fails so that the results can be reviewed. Use `--print-sizes` to print the generated table without comparing it to the baseline. Combine it with `--bench` to print and compare the sizes. Use `-h`, `-help`, or `--help` to show the test script options.

To intentionally refresh the table and chart after a relevant change, run `update-size-reduction-baselines.sh`. It runs the benchmark, updates the baseline document, and invokes the chart utility described in [Utilities](Utilities.md). Review the resulting documentation changes before committing them.
