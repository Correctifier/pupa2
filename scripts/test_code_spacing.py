import ast
import unittest

from code_spacing import cpp_spacing, python_spacing


class SpacingTests(unittest.TestCase):
    def test_python_sections_and_chains(self):
        source = (
            "def f():\n"
            "    x = 1\n"
            "    y = x * 2\n"
            "    if y:\n"
            "        act()\n"
            "    else:\n"
            "        other()\n"
            "    for item in items:\n"
            "        act()\n"
            "    return y\n"
        )
        result = python_spacing(source)

        self.assertIn("x = 1\n    y = x * 2\n\n    if y:", result)
        self.assertIn("act()\n    else:\n        other()\n\n    for", result)
        self.assertIn("act()\n\n    return y", result)
        self.assertEqual(result, python_spacing(result))

    def test_python_comments_and_decorators_stay_attached(self):
        source = "x = 1\n# Explain this action.\nact()\n@decorate\ndef f():\n    pass\n"
        result = python_spacing(source)

        self.assertIn("x = 1\n\n# Explain this action.\nact()", result)
        self.assertIn("act()\n\n@decorate\ndef f():", result)

    def test_python_does_not_change_multiline_strings(self):
        source = 'def f():\n    text = """one\ntwo\nthree"""\n    use(text)\n'
        result = python_spacing(source)

        self.assertEqual(ast.dump(ast.parse(source)), ast.dump(ast.parse(result)))
        self.assertIn('"""one\ntwo\nthree"""', result)

    def test_cpp_calculations_control_flow_and_return(self):
        source = (
            b"void f() {\n"
            b"  int x = 1;\n"
            b"  int y = x * 2;\n"
            b"  if (y) {\n    act();\n  } else {\n    other();\n  }\n"
            b"  while (ready()) {\n    act();\n  }\n"
            b"  return;\n}\n"
        )
        result = cpp_spacing(source)

        self.assertIn(b"x = 1;\n  int y = x * 2;\n\n  if", result)
        self.assertIn(b"} else {\n", result)
        self.assertIn(b"  }\n\n  while", result)
        self.assertIn(b"  }\n\n  return;", result)
        self.assertEqual(result, cpp_spacing(result))

    def test_cpp_comments_directives_and_do_while(self):
        source = (
            b"void f() {\n  int x = 1;\n  // Process x.\n  act();\n"
            b"  do {\n    act();\n  } while (ready());\n"
            b"#ifdef DEBUG\n  debug();\n#endif\n  done();\n}\n"
        )
        result = cpp_spacing(source)

        self.assertIn(b"x = 1;\n\n  // Process x.\n  act();", result)
        self.assertIn(b"} while (ready());\n#ifdef DEBUG", result)
        self.assertIn(b"debug();\n#endif\n  done();", result)

    def test_cpp_definitions_separated_from_fields(self):
        result = cpp_spacing(b"struct S {\n  int x;\n  S() : x(0) {}\n  void f() {}\n};\n")

        self.assertIn(b"int x;\n\n  S() : x(0) {}\n\n  void f()", result)

    def test_cpp_raw_strings_unchanged(self):
        source = b'void f() {\n  auto s = R"(one\ntwo\nthree)";\n  use(s);\n}\n'
        result = cpp_spacing(source)

        self.assertIn(b'R"(one\ntwo\nthree)";', result)

    def test_cpp_repeated_large_traversal(self):
        source = b"void f() {\n" + b"int x = 1;\nif (x) { act(); }\nact();\n" * 100 + b"}\n"
        expected = cpp_spacing(source)

        for _ in range(20):
            self.assertEqual(cpp_spacing(source), expected)


if __name__ == "__main__":
    unittest.main()
