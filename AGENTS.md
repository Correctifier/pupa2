# Project conventions

Follow `docs/coding-style.md` for project-owned code; leave `external/` formatting alone.

- Expand calls, declarations, and initializer/container lists with three or more
  items to one item per line. Do not count Python's implicit `self` or `cls`.
- Keep naturally adjacent delimiters together, such as `({` and `});`.
- Always use braces for C++ `if` and `else` bodies, including single statements.
- Separate control-flow blocks, setup/calculation groups, and actions with a blank
  line. Keep related assignments together and never split an `if`/`else` chain.
- Use `python scripts/format_code.py` and its `--check` mode for the complete style;
  clangd 22 alone cannot enforce the three-item threshold.
