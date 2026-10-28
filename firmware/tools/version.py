"""PlatformIO pre-build step: GOLI_VERSION from the environment (CI sets the release tag), else "dev".

Dev builds never update themselves over Wi-Fi.
"""
import os

Import("env")  # noqa: F821  (provided by PlatformIO)

env.Append(CPPDEFINES=[("GOLI_VERSION", env.StringifyMacro(os.environ.get("GOLI_VERSION", "dev")))])  # noqa: F821
