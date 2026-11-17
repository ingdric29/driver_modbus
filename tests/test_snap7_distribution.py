"""Exercise the actual packaged binary after relocation, without SQL or PLC."""
import pathlib
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
BIN = pathlib.Path(sys.argv.pop(1)) if len(sys.argv) > 1 else ROOT / "bin"


class DistributionTest(unittest.TestCase):
    def test_relocated_library_and_corresponding_sources(self):
        with tempfile.TemporaryDirectory(prefix="snap7-dist-") as directory:
            output = pathlib.Path(directory) / "original"
            subprocess.run(
                ["bash", "tools/package-runtime.sh", str(BIN), str(output)],
                cwd=ROOT, check=True, capture_output=True, text=True, timeout=20,
            )
            relocated = pathlib.Path(directory) / "relocated"
            shutil.move(str(output), relocated)
            binary = relocated / "bin" / "automonitorv1"
            library = relocated / "bin" / "lib" / "libsnap7.so"
            result = subprocess.run(
                [str(binary), "--version"], cwd=directory, capture_output=True,
                text=True, timeout=10, check=True,
            )
            self.assertIn("AUTOMONITOR", result.stdout.upper())
            linked = subprocess.run(
                ["ldd", str(binary)], capture_output=True, text=True, check=True, timeout=10,
            )
            self.assertIn(str(library), linked.stdout)
            self.assertFalse((relocated / "configdb.cfg").exists())
            self.assertFalse((relocated / "src").exists())
            self.assertTrue((relocated / "licenses/snap7/lgpl-3.0.txt").is_file())
            with tarfile.open(relocated / "share/snap7-source.tar.gz") as archive:
                names = set(archive.getnames())
                self.assertIn("snap7/UPSTREAM.md", names)
                self.assertIn("snap7/src/lib/snap7_libmain.cpp", names)
                self.assertIn("snap7/release/wrappers/c-cpp/snap7.cpp", names)
                self.assertFalse(any(".git" in pathlib.PurePosixPath(n).parts for n in names))
            # A malformed first-choice library must make the loader fail. This
            # proves the binary needs Snap7 even on hosts with a system Snap7.
            library.write_bytes(b"invalid shared library\n")
            failure = subprocess.run(
                [str(binary), "--version"], cwd=directory,
                capture_output=True, text=True, timeout=10,
            )
            self.assertNotEqual(failure.returncode, 0)
            self.assertIn("libsnap7.so", failure.stderr)


if __name__ == "__main__":
    unittest.main()
