#!/usr/bin/env python3
"""Apply the project style, including the three-item multiline threshold."""

from __future__ import annotations

import argparse
import ast
import io
from pathlib import Path
import subprocess
import tokenize

from code_spacing import cpp_spacing, python_spacing

ROOT = Path(__file__).resolve().parent.parent


def python_layout(source: str) -> str:
    """Trailing commas make Black retain three-item argument/container layouts."""

    tree = ast.parse(source)
    lines = source.splitlines(keepends=True)
    offsets = [0]

    for line in lines:
        offsets.append(offsets[-1] + len(line))

    def position(row, column):
        return offsets[row - 1] + column

    pairs = {}
    previous = {}
    stack = []
    last = None

    for token in tokenize.generate_tokens(io.StringIO(source).readline):
        if token.type == tokenize.OP:
            start = position(*token.start)

            if token.string in (
                "(",
                "[",
                "{",
            ):
                stack.append(start)
            elif token.string in (
                ")",
                "]",
                "}",
            ):
                pairs[stack.pop()] = start
                previous[start] = last

        if token.type not in (
            tokenize.NL,
            tokenize.NEWLINE,
            tokenize.COMMENT,
            tokenize.INDENT,
            tokenize.DEDENT,
            tokenize.ENCODING,
        ):
            last = token

    insertions = set()

    for node in ast.walk(tree):
        count = 0
        opening = None

        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            args = node.args
            positional = [*args.posonlyargs, *args.args]
            count = len(positional) + len(args.kwonlyargs) + bool(args.vararg) + bool(args.kwarg)

            if positional and positional[0].arg in ("self", "cls"):
                count -= 1

            opening = source.index("(", offsets[node.lineno - 1])
        elif isinstance(node, ast.Call):
            count = len(node.args) + len(node.keywords)
        elif isinstance(
            node,
            (
                ast.List,
                ast.Tuple,
                ast.Set,
            ),
        ):
            count = len(node.elts)
        elif isinstance(node, ast.Dict):
            count = len(node.keys)

        if count < 3:
            continue

        if opening is not None:
            closing = pairs[opening]
        else:
            # AST columns are UTF-8 byte offsets, while tokenize uses characters.
            line = lines[node.end_lineno - 1]
            column = len(line.encode()[: node.end_col_offset].decode())
            closing = position(node.end_lineno, column) - 1

            if isinstance(node, ast.Tuple):
                start_line = lines[node.lineno - 1]
                start_column = len(start_line.encode()[: node.col_offset].decode())
                start = position(node.lineno, start_column)

                if source[start] != "(" or pairs.get(start) != closing:
                    continue  # Bare tuples and generic type subscriptions are not call lists.

        token = previous.get(closing)

        if token is not None and token.string != ",":
            insertions.add(position(*token.end))

    for offset in sorted(insertions, reverse=True):
        source = source[:offset] + "," + source[offset:]

    assert ast.dump(ast.parse(source)) == ast.dump(tree)

    return source


def cpp_layout(source: bytes) -> bytes:
    """Expand actual syntax lists, ignoring commas in strings/templates/comments."""
    from tree_sitter import Language, Parser
    import tree_sitter_cpp

    parser = Parser(Language(tree_sitter_cpp.language()))

    def tokens(root):
        pending = [root]
        result = []

        while pending:
            node = pending.pop()

            if node.type == "comment":
                continue

            if node.child_count:
                pending.extend(reversed(node.children))
            else:
                result.append((node.type, node.text))

        return result

    original_tokens = tokens(parser.parse(source).root_node)

    def indentation(offset):
        start = (
            source.rfind(
                b"\n",
                0,
                offset,
            )
            + 1
        )
        line = source[start:offset]

        return len(line) - len(line.lstrip(b" \t"))

    def expanded(node):
        commas = [child for child in node.children if child.type == ","]
        start = node.start_byte + 1
        segments = []

        for comma in commas:
            segments.append((start, comma.start_byte))

            start = comma.end_byte

        end = node.end_byte - 1
        trailing = not source[start:end].strip()

        if not trailing:
            segments.append((start, end))

        if len(segments) < 3:
            return None

        base = indentation(node.start_byte)
        prefix = b" " * (base + 4)
        parts = []

        for start, end in segments:
            raw = source[start:end]
            first = start + len(raw) - len(raw.lstrip())
            old_indent = indentation(first)
            lines = raw.strip().splitlines()

            parts.append(prefix + lines[0])

            for line in lines[1:]:
                if line.strip():
                    shift = base + 4 - old_indent
                    line = (
                        b" " * shift + line
                        if shift >= 0
                        else line[min(-shift, len(line) - len(line.lstrip())) :]
                    )

                parts.append(line)

            parts[-1] += b","

        if not trailing:
            parts[-1] = parts[-1][:-1]

        return (
            source[node.start_byte : node.start_byte + 1]
            + b"\n"
            + b"\n".join(parts)
            + b"\n"
            + b" " * base
            + source[node.end_byte - 1 : node.end_byte]
        )

    # Reparse after each outer-list edit so nested indentation uses current positions.
    for _ in range(10000):
        tree = parser.parse(source)

        if tree.root_node.has_error:
            raise ValueError("C++ syntax could not be parsed; refusing a layout edit")

        pending = [tree.root_node]
        replacement = None

        while pending:
            node = pending.pop()

            if node.type in (
                "argument_list",
                "parameter_list",
                "initializer_list",
            ):
                replacement = expanded(node)

                if replacement is not None and replacement != node.text:
                    source = source[: node.start_byte] + replacement + source[node.end_byte :]

                    break

            pending.extend(reversed(node.children))
        else:
            if tokens(tree.root_node) != original_tokens:
                raise ValueError(
                    "Layout would alter C++ tokens or string contents; refusing the edit"
                )

            return source

    raise RuntimeError("C++ layout did not converge")


def format_python(source: str) -> str:
    import black

    mode = black.Mode(
        line_length=100,
        target_versions={black.TargetVersion.PY310},
        preview=True,
        enabled_features={black.mode.Preview.hug_parens_with_braces_and_square_brackets},
    )
    result = python_spacing(black.format_str(python_layout(source), mode=mode))

    assert ast.dump(ast.parse(result)) == ast.dump(ast.parse(source))

    return result


def main():
    import clang_format

    parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument(
        "--check",
        action="store_true",
        help="report files needing formatting",
    )

    args = parser.parse_args()
    files = sorted(
        path
        for folder in (
            "pc",
            "target",
            "tests",
            "scripts",
        )
        for path in (ROOT / folder).rglob("*")
        if path.suffix
        in (
            ".py",
            ".cpp",
            ".hpp",
            ".c",
            ".h",
        )
    )
    changed = {}

    for path in files:
        source = path.read_bytes()

        if path.suffix == ".py":
            result = format_python(source.decode()).encode()
        else:
            formatted = subprocess.run(
                [clang_format.get_executable("clang-format"), f"--assume-filename={path}"],
                input=source,
                capture_output=True,
                check=True,
            ).stdout
            result = cpp_spacing(cpp_layout(formatted))

        if result != source:
            changed[path] = result

    for path, result in changed.items():
        if not args.check:
            path.write_bytes(result)

        print(path.relative_to(ROOT))

    print(f"{'Would format' if args.check else 'Formatted'} {len(changed)} of {len(files)} files")

    return bool(changed) if args.check else 0


if __name__ == "__main__":
    raise SystemExit(main())
