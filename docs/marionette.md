# Marionette harness quick reference

Marionette is the in-repo C++ test harness used by DragonGod tests.

## Core test declaration

### `FACT`

```cpp
FACT(MyTestName)
{
    ASSERT_TRUE(true, "example");
}
```

Registers one test case.

### `THEORY` + `RunTheoryCases`

`THEORY` is currently an alias for `FACT`, and `RunTheoryCases` lives on `TestContext`.

```cpp
struct CaseData { const char* name; int value; };

THEORY(ExampleTheory)
{
    const std::vector<CaseData> cases = {
        {"zero", 0},
        {"one", 1}
    };

    context.RunTheoryCases(cases, [](::marionette::tests::TestContext& theoryContext, const CaseData& testCase) {
        ASSERT_TRUE(testCase.value >= 0, "case value should be non-negative");
    });
}
```

## Assertions

### `ASSERT_TRUE` / `ASSERT_FALSE`

```cpp
ASSERT_TRUE(condition, "must be true");
ASSERT_FALSE(condition, "must be false");
```

### `ASSERT_EQUAL` / `ASSERT_NOT_EQUAL`

```cpp
ASSERT_EQUAL(expected, actual, "values should match");
ASSERT_NOT_EQUAL(expected, actual, "values should differ");
```

### `ASSERT_SEQUENCE_EQUAL`

```cpp
ASSERT_SEQUENCE_EQUAL(expectedVector, actualVector, "ordered sequence should match");
```

### `ASSERT_NEAR`

```cpp
ASSERT_NEAR(10.0, measured, 0.1, "difference should stay in tolerance");
```

### `FAIL`

```cpp
if (fatalCondition) {
    FAIL("fatal condition encountered");
}
```

### `SKIP`

```cpp
if (!preconditionAvailable) {
    SKIP("precondition unavailable in this environment");
}
```

## Artifacts

Write test diagnostics through the context:

```cpp
ASSERT_TRUE(context.WriteTextArtifact("trace_summary", "...details..."), "artifact should be written");
```

Artifacts are materialized under repository `out/test-artifacts/...` via the harness.

## Benchmarks (separate from correctness tests)

Benchmarks register in a separate benchmark registry and are executed via bench mode.

### `BENCHMARK`

```cpp
BENCHMARK(MyBenchmark)
{
    volatile std::uint64_t value = context.iteration;
    (void)value;
}
```

Uses default iteration count.

### `BENCHMARK_WITH_ITERATIONS`

```cpp
BENCHMARK_WITH_ITERATIONS(MyBenchmarkFixed, 128)
{
    volatile std::uint64_t value = context.iteration + 1;
    (void)value;
}
```

Use when you need explicit fixed iteration count.

> Guidance: benchmarks measure performance behavior; they are not substitutes for `FACT`/`THEORY` correctness checks.

## Doom module (quarantined)

Use this only for intentional subprocess-abnormal-termination envelope tests.

### `DOOM_FACT`

```cpp
DOOM_FACT(MyDoomCase)
{
    FORETELL_DOOM("intentional abort path");
    std::abort();
}
```

### `FORETELL_DOOM`

Attaches expected catastrophe context for envelope diagnostics.

### `ASSERT_DOOM`

```cpp
FACT(DoomEnvelopeRecovered)
{
    ASSERT_DOOM(MyDoomCase);
}
```

Asserts the doom subprocess terminated abnormally and produced expected diagnostic envelope artifacts.

> Guidance: doom tests are intentionally quarantined behavior. Do not use doom patterns casually in greenfield runtime or ordinary unit tests.

## Anti-patterns

- Do **not** use benchmarks as pass/fail correctness tests.
- Do **not** use doom macros for ordinary negative tests.
- Do **not** bypass harness assertions with silent logging-only checks.
- Do **not** skip writing artifacts when debugging replay/trace mismatches; capture bounded evidence.
