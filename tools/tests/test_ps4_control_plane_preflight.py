from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = (ROOT / "tools" / "ps4-control-plane-preflight.ps1").read_text(encoding="utf-8")
WORKFLOW = (ROOT / ".github" / "workflows" / "ps4-control-plane-preflight.yml").read_text(encoding="utf-8")


class ControlPlanePreflightContractTests(unittest.TestCase):
    def test_ftp_operations_are_read_only(self):
        methods = set(re.findall(r"WebRequestMethods\+Ftp\]::([A-Za-z]+)", SCRIPT))
        self.assertTrue(methods)
        self.assertLessEqual(methods, {"ListDirectoryDetails", "GetFileSize", "DownloadFile"})
        self.assertNotRegex(SCRIPT, r"UploadFile|DeleteFile|MakeDirectory|RenameTo|RNFR|RNTO")

    def test_rpi_probe_is_http_get_only_on_12800(self):
        self.assertIn(":12800/api", SCRIPT)
        self.assertIn('$request.Method="GET"', SCRIPT)
        self.assertNotRegex(SCRIPT, r'Method\s*=\s*"POST"|/api/install|/api/get_task_progress')
        self.assertNotIn("TcpClient", SCRIPT)
        self.assertNotRegex(SCRIPT, r":9090|:3232")

    def test_remote_scope_is_allowlisted_and_non_mutating(self):
        for path in (
            "/data/GoldHEN/plugins.ini",
            "/data/GoldHEN/ps4_neural_engine_install.log",
            "/data/GoldHEN/plugins/ps4_neural_engine.prx.pne-release.ini",
            "/data/GoldHEN/plugins/ps4_neural_engine.prx",
            "/user/app/PNEU00004/app.json",
            "/user/app/PNEU00004/app.xml",
            "/user/app/PNEU00001/app.json",
            "/user/app/PNEU00001/app.xml",
            "/user/app/PNEU00001/app.pkg",
            "/user/app/PNEU00004/app.pkg",
            "/user/data/pkg/PS4-Neural-Engine-015.pkg",
            "/data/pkg/PS4-Neural-Engine-015.pkg",
            "/data/pkg/PS4-Neural-Engine-015-7609b5d2b536.pkg",
            "/data/pkg/PS4-Neural-Engine-015-089684400316.pkg",
            "/data/pkg/PS4-Neural-Engine-006-dfda3a974df8.pkg",
            "/data/pkg/PS4-Neural-Engine-006-eb68c632848b.pkg",
            "/data/pkg/PS4-Neural-Engine-006-a3c9eb2b185e.pkg",
            "/data/pkg/PS4-Neural-Engine-006-0f0e7060fc3d.pkg",
            "/data/pkg/PS4-Neural-Engine-006-8d95e10d45e9.pkg",
        ):
            self.assertIn(path, SCRIPT)
        for flag in (
            "binloader_9090_probed=$false",
            "remote_files_modified=$false",
            "pkg_installed=$false",
            "prx_activated=$false",
            "goldhen_restarted=$false",
        ):
            self.assertIn(flag, SCRIPT)

    def test_workflow_is_default_branch_workflow_run_on_existing_bridge(self):
        self.assertIn("workflow_run:", WORKFLOW)
        self.assertIn('workflows: ["PS4 PRX and PKG build"]', WORKFLOW)
        self.assertIn("runs-on: [self-hosted, Windows, X64, ps4-lan]", WORKFLOW)
        self.assertIn("shell: powershell -NoProfile -ExecutionPolicy Bypass -File {0}", WORKFLOW)
        self.assertIn("id: checkout-collector", WORKFLOW)
        self.assertIn("steps.checkout-collector.outputs.commit", WORKFLOW)
        self.assertIn("ps4-control-plane-preflight-${{ github.run_id }}", WORKFLOW)
        self.assertIn("collector_commit=$env:COLLECTOR_COMMIT", SCRIPT)
        self.assertIn("collector_ref=$env:COLLECTOR_REF", SCRIPT)
        self.assertIn("timeout-minutes: 30", WORKFLOW)
        self.assertIn("ref: main", WORKFLOW)
        self.assertIn("ps4-control-plane-preflight.ps1", WORKFLOW)
        self.assertIn("[System.Net.IPAddress]::TryParse", SCRIPT)
        self.assertIn("collector_commit=$env:COLLECTOR_COMMIT", SCRIPT)
        self.assertIn('schema="nr.ps4.control-plane-preflight.v3"', SCRIPT)
        self.assertIn("16777216", SCRIPT)
        self.assertNotIn("workflow_dispatch", WORKFLOW)
        self.assertNotIn("contents: write", WORKFLOW)

if __name__ == "__main__":
    unittest.main()
