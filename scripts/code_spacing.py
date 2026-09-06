"""Statement-level vertical spacing; never edit expression or literal contents."""

import ast


def needs_gap(previous: str, following: str) -> bool:
    if previous in ("control", "definition") or following in (
        "control",
        "definition",
        "exit",
    ):
        return True

    return (previous == "setup") != (following == "setup")


def separate(source: bytes, groups: list[list[tuple[int, int, str]]]) -> bytes:
    lines = source.splitlines(keepends=True)
    insertions = set()

    for group in groups:
        for previous, following in zip(group, group[1:]):
            _, end, before = previous
            start, _, after = following

            if start <= end or not needs_gap(before, after):
                continue

            # Preserve existing separation and keep leading comments with their statement.
            if any(not line.strip() for line in lines[end + 1 : start]):
                continue

            insertions.add(end + 1)

    for row in sorted(insertions, reverse=True):
        lines.insert(row, b"\n")

    return b"".join(lines)


def python_spacing(source: str) -> str:
    tree = ast.parse(source)

    def category(node):
        if isinstance(
            node,
            (
                ast.FunctionDef,
                ast.AsyncFunctionDef,
                ast.ClassDef,
            ),
        ):
            return "definition"

        if (
            isinstance(
                node,
                (
                    ast.If,
                    ast.For,
                    ast.AsyncFor,
                    ast.While,
                    ast.Try,
                    ast.With,
                    ast.AsyncWith,
                    ast.Match,
                ),
            )
            or type(node).__name__ == "TryStar"
        ):
            return "control"

        if isinstance(
            node,
            (
                ast.Return,
                ast.Raise,
                ast.Break,
                ast.Continue,
            ),
        ):
            return "exit"

        if isinstance(
            node,
            (
                ast.Assign,
                ast.AnnAssign,
                ast.AugAssign,
            ),
        ):
            return "setup"

        return "action"

    groups = []

    for node in ast.walk(tree):
        for _, value in ast.iter_fields(node):
            if (
                isinstance(value, list)
                and value
                and all(isinstance(item, ast.stmt) for item in value)
            ):
                group = []

                for item in value:
                    start = item.lineno

                    if isinstance(
                        item,
                        (
                            ast.FunctionDef,
                            ast.AsyncFunctionDef,
                            ast.ClassDef,
                        ),
                    ):
                        start = min(
                            [start, *(decorator.lineno for decorator in item.decorator_list)]
                        )

                    group.append((
                        start - 1,
                        item.end_lineno - 1,
                        category(item),
                    ))

                groups.append(group)

    result = separate(source.encode(), groups).decode()

    assert ast.dump(ast.parse(result)) == ast.dump(tree)

    return result


def cpp_spacing(source: bytes) -> bytes:
    from tree_sitter import Language, Parser
    import tree_sitter_cpp

    parser = Parser(Language(tree_sitter_cpp.language()))
    tree = parser.parse(source)

    if tree.root_node.has_error:
        raise ValueError("C++ syntax could not be parsed; refusing a spacing edit")

    def category(node):
        if node.type in ("function_definition", "template_declaration"):
            return "definition"

        if node.type in (
            "if_statement",
            "for_statement",
            "for_range_loop",
            "while_statement",
            "do_statement",
            "switch_statement",
            "try_statement",
        ):
            return "control"

        if node.type in (
            "return_statement",
            "co_return_statement",
            "throw_statement",
            "break_statement",
            "continue_statement",
        ):
            return "exit"

        if node.type in ("declaration", "field_declaration"):
            return "setup"

        if node.type == "expression_statement" and node.named_children:
            if node.named_children[0].type in ("assignment_expression", "update_expression"):
                return "setup"

        return "action"

    groups = []
    pending = [tree.root_node]

    while pending:
        node = pending.pop()

        if node.type in (
            "compound_statement",
            "field_declaration_list",
            "declaration_list",
            "translation_unit",
        ):
            # Preprocessor directives and access labels delimit groups rather than joining them.
            group = []

            for child in node.named_children:
                if child.type == "comment":
                    continue

                if child.type.startswith("preproc_") or child.type == "access_specifier":
                    groups.append(group)

                    group = []

                    continue

                # Compute rows from byte offsets; avoid temporary Point objects from bindings.
                start = source.count(
                    b"\n",
                    0,
                    child.start_byte,
                )
                end = source.count(
                    b"\n",
                    0,
                    child.end_byte,
                )

                group.append((
                    start,
                    end,
                    category(child),
                ))

            groups.append(group)

        pending.extend(node.named_children)

    return separate(source, groups)
