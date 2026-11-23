from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
QA = (ROOT / ".github" / "workflows" / "ps4-base-pkg-qa-006.yml").read_text(encoding="utf-8")
STAGE = (ROOT / ".github" / "workflows" / "ps4-stage-full-installer-006.yml").read_text(encoding="utf-8")

class QaStageRunIdentityTests(unittest.TestCase):
    def test_qa_report_binds_to_triggering_build_run(self):
        self.assertIn("BUILD_RUN_ID: ${{ github.event.workflow_run.id }}", QA)
        self.assertIn('"BUILD_RUN_ID=$env:BUILD_RUN_ID"', QA)

    def test_stage_downloads_build_artifact_from_qa_report_run_id(self):
        self.assertIn("run-id: ${{ github.event.workflow_run.id }}", STAGE)
        self.assertIn("run-id: ${{ steps.source.outputs.build_run_id }}", STAGE)
        self.assertIn(r"(?m)^BUILD_RUN_ID=(\d+)\r?$", STAGE)
        self.assertNotIn("44662999645", STAGE)
        self.assertNotIn("44663222188", STAGE)
        self.assertIn("STAGING_SKIPPED_NEWER_RELEASE_PRESENT=true", STAGE)
        self.assertIn("[int]$m.Groups[1].Value -gt 6", STAGE)

    def test_stage_only_runs_after_successful_qa_workflow(self):
        self.assertIn('workflows: ["PS4 Base PKG 006 Independent QA"]', STAGE)
        self.assertIn("github.event.workflow_run.conclusion == 'success'", STAGE)
        self.assertNotIn("  push:", STAGE)
        self.assertNotIn("workflow_dispatch", STAGE)

if __name__ == "__main__":
    unittest.main()
