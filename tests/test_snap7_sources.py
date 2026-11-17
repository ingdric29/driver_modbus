"""Verify every vendored upstream file against its pinned manifest."""
import hashlib
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1] / "third_party" / "snap7"


class SourcesTest(unittest.TestCase):
    def test_exact_manifest(self):
        expected = {}
        for line in (ROOT / "SHA256SUMS").read_text().splitlines():
            digest, name = line.split("  ", 1)
            self.assertNotIn(name, expected)
            expected[name] = digest
        actual = {
            p.relative_to(ROOT).as_posix()
            for p in ROOT.rglob("*")
            if p.is_file() and p.name not in {"SHA256SUMS", "UPSTREAM.md"}
        }
        self.assertEqual(set(expected), actual)
        for name, digest in expected.items():
            with self.subTest(file=name):
                self.assertEqual(hashlib.sha256((ROOT / name).read_bytes()).hexdigest(), digest)


if __name__ == "__main__":
    unittest.main()
