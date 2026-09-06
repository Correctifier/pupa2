import ast
import unittest

from format_code import cpp_layout, format_python


class FormattingTests(unittest.TestCase):
    def test_python_three_arguments_and_small_calls(self):
        result = format_python("f(a, b)\nf(a, b, c)\n")

        self.assertIn("f(a, b)", result)
        self.assertIn("f(\n    a,\n    b,\n    c,\n)", result)

    def test_python_parameters_ignore_implicit_self(self):
        result = format_python("def f(self, a, b):\n    pass\ndef g(self, a, b, c):\n    pass\n")

        self.assertIn("def f(self, a, b):", result)
        self.assertIn("def g(\n    self,\n    a,\n    b,\n    c,\n):", result)

    def test_python_type_annotations_are_not_container_literals(self):
        source = "Alias = tuple[str, str, list[tuple[float, float]]]\n"

        self.assertEqual(ast.dump(ast.parse(source)), ast.dump(ast.parse(format_python(source))))

    def test_python_nested_delimiters_and_unicode(self):
        source = 'name = "Ω"; f({"a": 1, "b": 2, "c": 3})\n'
        result = format_python(source)

        self.assertIn("f({\n", result)
        self.assertIn("\n})", result)
        self.assertEqual(result, format_python(result))

    def test_cpp_parameters_and_nested_calls(self):
        source = b"void f(int a, int b, int c) { f(a, g(a, b, c), c); small(a, b); }"
        result = cpp_layout(source)

        self.assertIn(b"f(\n    int a,\n    int b,\n    int c\n)", result)
        self.assertIn(b"g(\n        a,\n        b,\n        c\n    )", result)
        self.assertIn(b"small(a, b)", result)
        self.assertEqual(result, cpp_layout(result))

    def test_cpp_template_and_string_commas_do_not_count(self):
        source = b'void f() { g(std::pair<int, int>{1, 2}, "a,b,c"); }'

        self.assertEqual(cpp_layout(source), source)

    def test_cpp_braced_argument_keeps_delimiters_together(self):
        result = cpp_layout(b"void f() { g({a, b, c}); }")

        self.assertIn(b"g({\n    a,\n    b,\n    c\n});", result)

    def test_cpp_comments_remain_intact(self):
        source = b"void f() { g(a, // comma, in comment\n b, c); }"
        result = cpp_layout(source)

        self.assertIn(b"// comma, in comment\n", result)
        self.assertEqual(result, cpp_layout(result))


if __name__ == "__main__":
    unittest.main()
