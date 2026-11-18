# SPDX-License-Identifier: GPL-3.0-or-later
"""Source isolation and intentional failure modes of the upstream integration."""
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "tools/assemble-source.py"
SPEC = importlib.util.spec_from_file_location("assembly", SCRIPT)
assembly = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(assembly)


class SourceAssemblyTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / "source"
        self.source.mkdir()
        self.integration = self.root / "mobile"
        patches = self.integration / "patches/core"
        patches.mkdir(parents=True)
        self.original = "int upstream(void)\n{\n  return 1;\n}\n"
        (self.source / "core.c").write_text(self.original)
        self.patch = patches / "core.patch"
        self.patch.write_text("--- a/core.c\n+++ b/core.c\n@@ -1,4 +1,5 @@\n int upstream(void)\n {\n+  adapter();\n   return 1;\n }\n")
        self.lock = self.root / "lock.json"
        self.lock.write_text(json.dumps({
            "upstream": {"commit": "reviewed"},
            "files": {"core.c": hashlib.sha256(self.original.encode()).hexdigest()},
            "inventory": ["core.c"], "submodules": {}}))
        self.output = self.root / "build/view"

    def assemble(self):
        assembly.assemble(self.source, self.output, self.lock, self.integration)

    def test_isolated_idempotent_copy_and_patch(self):
        self.assemble()
        generated = self.output / "core.c"
        self.assertIn("adapter();", generated.read_text())
        self.assertEqual((self.source / "core.c").read_text(), self.original)
        modified = generated.stat().st_mtime_ns
        self.assemble()
        self.assertEqual(generated.stat().st_mtime_ns, modified)
        generated.write_text("build writes here\n")
        self.assertEqual((self.source / "core.c").read_text(), self.original)
        self.assemble()
        self.assertIn("adapter();", generated.read_text())

    def test_upstream_changes_fail_before_writing(self):
        (self.source / "core.c").write_text("changed signature\n")
        with self.assertRaisesRegex(assembly.IntegrationError, "differs from reviewed"):
            self.assemble()
        self.assertFalse(self.output.exists())

    def test_no_fuzzy_or_offset_application(self):
        self.patch.write_text(self.patch.read_text().replace(" int upstream(void)", " int old_api(void)"))
        with self.assertRaisesRegex(assembly.IntegrationError, "context changed"):
            self.assemble()
        self.assertEqual((self.source / "core.c").read_text(), self.original)

    def test_removed_hook_does_not_survive_reconfigure(self):
        self.assemble()
        self.patch.unlink()
        self.assemble()
        self.assertEqual((self.output / "core.c").read_text(), self.original)

    def test_reject_unowned_output(self):
        self.output.mkdir(parents=True)
        (self.output / "user-file").touch()
        with self.assertRaisesRegex(assembly.IntegrationError, "unowned output"):
            self.assemble()

    def test_reject_symlink_to_checkout(self):
        self.assemble()
        generated = self.output / "core.c"
        generated.unlink()
        generated.symlink_to(self.source / "core.c")
        with self.assertRaisesRegex(assembly.IntegrationError, "symlink"):
            self.assemble()
        self.assertEqual((self.source / "core.c").read_text(), self.original)

    def test_reject_changed_source_ownership(self):
        self.assemble()
        (self.output / ".dt-mobile-source-view").write_text("/another/checkout\n")
        with self.assertRaisesRegex(assembly.IntegrationError, "ownership changed"):
            self.assemble()

    def test_reject_symlink_parent(self):
        directory = self.root / "linked"
        directory.symlink_to(self.source, target_is_directory=True)
        with self.assertRaisesRegex(assembly.IntegrationError, "symlink"):
            assembly.write_if_changed(directory / "core.c", b"overwritten\n")
        self.assertEqual((self.source / "core.c").read_text(), self.original)

    def test_reject_traversal(self):
        self.patch.write_text(self.patch.read_text().replace("a/core.c", "a/../core.c").replace("b/core.c", "b/../core.c"))
        with self.assertRaisesRegex(assembly.IntegrationError, "unsafe integration path"):
            self.assemble()

    def test_verify_only_does_not_create_output(self):
        assembly.assemble(self.source, self.output, self.lock, self.integration, verify_only=True)
        self.assertFalse(self.output.exists())

    def test_new_upstream_source_is_not_silently_omitted(self):
        subprocess.run(["git", "init", "-q", str(self.source)], check=True)
        (self.source / "src").mkdir()
        (self.source / "src/new_api.cpp").write_text("int new_api;\n")
        subprocess.run(["git", "-C", str(self.source), "add", "src/new_api.cpp"], check=True)
        with self.assertRaisesRegex(assembly.IntegrationError, "unreviewed upstream files"):
            self.assemble()
        self.assertFalse(self.output.exists())

    def test_new_translation_is_not_silently_omitted(self):
        subprocess.run(["git", "init", "-q", str(self.source)], check=True)
        (self.source / "po").mkdir()
        (self.source / "po/new.po").write_text('msgid "new language"\nmsgstr ""\n')
        subprocess.run(["git", "-C", str(self.source), "add", "po/new.po"], check=True)
        with self.assertRaisesRegex(assembly.IntegrationError, "unreviewed upstream files"):
            self.assemble()

    def test_stale_nested_mobile_directory_is_cleaned(self):
        self.assemble()
        stale = self.output / "src/mobile/obsolete.c"
        stale.parent.mkdir(parents=True)
        stale.write_text("obsolete implementation\n")
        self.assemble()
        self.assertFalse(stale.exists())

    def test_dirty_submodule_and_revision_fail(self):
        submodule = self.source / "src/external/decoder"
        submodule.mkdir(parents=True)
        subprocess.run(["git", "init", "-q", str(submodule)], check=True)
        (submodule / "decoder.cpp").write_text("int decoder_api;\n")
        subprocess.run(["git", "-C", str(submodule), "add", "decoder.cpp"], check=True)
        subprocess.run(["git", "-C", str(submodule), "-c", "user.name=integration test", "-c",
                        "user.email=test@example.invalid", "commit", "-qm", "fixture"], check=True)
        revision = subprocess.check_output(["git", "-C", str(submodule), "rev-parse", "HEAD"], text=True).strip()
        lock = json.loads(self.lock.read_text())
        lock["submodules"] = {"src/external/decoder": revision}
        self.lock.write_text(json.dumps(lock))
        self.assemble()
        self.assertEqual((self.output / "src/external/decoder/decoder.cpp").read_text(), "int decoder_api;\n")
        (submodule / "decoder.cpp").write_text("int incompatible_decoder_api;\n")
        with self.assertRaisesRegex(assembly.IntegrationError, "tracked submodule sources differ"):
            assembly.assemble(self.source, self.output, self.lock, self.integration, verify_only=True)
        (submodule / "decoder.cpp").write_text("int decoder_api;\n")
        lock["submodules"]["src/external/decoder"] = "unreviewed"
        self.lock.write_text(json.dumps(lock))
        with self.assertRaisesRegex(assembly.IntegrationError, "submodule changed"):
            self.assemble()


if __name__ == "__main__":
    unittest.main()
