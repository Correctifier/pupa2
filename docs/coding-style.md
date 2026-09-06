# Coding style

For function declarations and calls with **three or more** parameters/arguments,
put each one on its own line, even if the whole list would fit on one line.
Python's implicit `self`/`cls` does not count toward the threshold.
Apply the same three-item threshold to dictionary, list, and braced initializer
contents. Start the list on the line after the opening delimiter, indent one
continuation level, and put the closing parenthesis on its own line at the
original indentation. Keep lists with fewer than three items on one line when they fit comfortably
within 100 columns. Use trailing commas in multiline Python lists of arguments
and parameters; C++ function argument lists do not permit trailing commas.
Use trailing commas in multiline C++ initializer lists as well, so the formatter
preserves their expanded layout and can attach a single initializer to its call.

Keep delimiters together when they wrap the same expression, such as a single
dictionary or initializer passed to a function: `function({ ... })`, closing
with `})` or `});`. This is often called delimiter "hugging". Nested blocks with
separate logical structure still retain their own indentation and closing lines.

Always use curly braces around C++ `if` and `else` bodies, including single
statements. `else if` chains are fine when every branch has a braced body. The
formatter also adds braces to loops for consistency. Python uses its required
indentation-based blocks.

## Vertical whitespace

Use blank lines to mark logical sections, following the principle in
[PEP 8](https://peps.python.org/pep-0008/#blank-lines) and
[Google's C++ style guide](https://google.github.io/styleguide/cppguide.html#Vertical_Whitespace).
Those guides leave the exact boundaries to judgment. This project makes the
following statement-padding rules automatic:

- Put one blank line before and after an `if`/`else` block, loop, or exception
  handling block when another statement is adjacent at the same nesting level.
- Separate runs of declarations, initialization, assignments, and calculations
  from actions such as function calls. Keep consecutive assignments/calculations
  together; the formatter does not try to infer separate mathematical ideas.
- Put a blank line before a return, throw/raise, break, or continue when another
  statement precedes it in the same block.
- Separate function/constructor definitions from neighboring declarations and
  definitions. Preserve Python's normal two blank lines around top-level definitions.
- Keep `if`/`else`, `try`/`catch`/`finally`, and `do`/`while` chains together.
  Do not add blank lines just inside braces or between an `if` and its body.
- Keep leading comments attached to the section they explain, preserve manual
  logical separators, and never insert blank lines inside expressions or literals.

```cpp
const float voltage = sample * scale;
const float impedance = voltage / current;

if (!std::isfinite(impedance)) {
  return;
}

publish(impedance);

for (auto& channel : channels) {
  channel.reset();
}
```

These are project rules built on established style guidance, not a claim that
every language has a single universal whitespace standard. The complete rules,
including whitespace, are enforced by `scripts/format_code.py` and `--check`.

```python
result = adaptive_sweep(
    client,
    start_hz,
    stop_hz,
    settings,
    strategy,
    cancelled,
    on_point,
)
```

```cpp
send_message(
    transport,
    endpoint,
    message
);

pickup::Application app({
    transport,
    frontend,
    {"PC virtual target", "Guitar Pickup Impedance Analyzer", PICKUP_APPLICATION_VERSION},
});
```

The root `.clang-format` configures C++ using clang-format/clangd 22 or newer.
Python uses Black with four-space indentation and the settings in
`pc/pyproject.toml`. Black's delimiter-hugging option is currently an opt-in
preview feature, so the formatter version is pinned for consistent results.

Install the formatting tools in a development virtual environment with
`python -m pip install -e './pc[format]'`, then run:

```sh
python scripts/format_code.py
python scripts/format_code.py --check
```

The project command applies Black and clang-format, then enforces the item-count
rule using Python's AST and a C++ syntax parser. Commas inside templates, strings,
and nested calls are not mistaken for additional arguments. It also formats
its own source and tests. Run formatter regression tests with
`python -m unittest discover -s scripts -p 'test_*.py'`.

The `.clang-format` `InsertBraces` setting also enforces braced control bodies in
clangd's Format Document action. clangd 22 alone does not enforce the three-item
cutoff and may compact shorter calls; use the project command for the complete
style.

Apply the convention across project sources, tests, and examples. Vendored
dependencies retain their upstream formatting.
