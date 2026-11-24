#!/usr/bin/env python3
"""Static regression checks for the pinned OpenGNM compute user-data PM4 fix."""
import re
import sys
from pathlib import Path


def function_body(source: str, signature: str) -> str:
    start = source.find(signature)
    if start < 0:
        raise AssertionError(f"missing function: {signature}")
    opening = source.find("{", start)
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    raise AssertionError(f"unterminated function: {signature}")


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: test_opengnm_compute_userdata_pm4.py <patched-drawcommandbuffer.c>")
    source = Path(sys.argv[1]).read_text(encoding="utf-8")

    internal = function_body(source, "static void setpersistentregisterrange_internal(")
    assert "PKT3_SHADER_TYPE_S(shader_type)" in internal, "SET_SH_REG header must encode shader type"

    graphics_wrapper = function_body(source, "static void setpersistentregisterrange(")
    assert "setpersistentregisterrange_internal(cmd, regaddr, regvalues, numvalues, 0)" in graphics_wrapper, (
        "legacy graphics SET_SH_REG helper must keep shader_type=0"
    )

    stage_wrapper = function_body(source, "static void setpersistentregisterrange_stage(")
    assert "stage == GNM_STAGE_CS ? 1u : 0u" in stage_wrapper, (
        "compute stage must select shader_type=1 and graphics stages shader_type=0"
    )

    setters = {
        "Vsharp": "void sceGnmDrawCmdSetVsharpUserData(",
        "Tsharp": "void sceGnmDrawCmdSetTsharpUserData(",
        "Ssharp": "void sceGnmDrawCmdSetSsharpUserData(",
        "Pointer": "void sceGnmDrawCmdSetPointerUserData(",
    }
    for label, signature in setters.items():
        body = function_body(source, signature)
        assert "setpersistentregisterrange_stage(" in body, (
            f"{label} user-data setter must pass its shader stage to the PM4 builder"
        )
        assert re.search(r"setpersistentregisterrange\s*\(", body) is None, (
            f"{label} setter still calls the stage-agnostic helper"
        )

    assert "PKT3_SHADER_TYPE_S(1)" in function_body(source, "static void setshregcompute("), (
        "existing compute shader register path must remain explicitly compute"
    )
    print("OpenGNM compute user-data PM4 source contract: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
