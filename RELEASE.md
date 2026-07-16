RELEASE_TYPE: patch

This patch adds pools for storing previously generated values. It also adds two
generators, `values_reusable` and `values_consumed`, for drawing previously generated
values with and without replacement, respectively.

Pools will be more useful once stateful testing is added in a later release.
