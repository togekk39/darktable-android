#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Expose small, current upstream numerical routines without retaining copies.

Run from CMake after the integration's reviewed-source compatibility check. The
outputs belong exclusively in the build tree and must never be committed.
"""
import argparse
from pathlib import Path
import re


def once(pattern, source, description, flags=0):
    matches = list(re.finditer(pattern, source, flags))
    if len(matches) != 1:
        raise ValueError(f"upstream API changed: expected one {description}, found {len(matches)}")
    return matches[0]


def function(source, name, signature):
    match = once(r"^static(?: inline)? [^\n]*\b" + re.escape(name)
                 + r"\([^;]*?\)\n\{.*?^\}", source, name, re.M | re.S)
    definition = match.group()
    actual = " ".join(definition[:definition.index("{")].split())
    if actual != " ".join(signature.split()):
        raise ValueError(f"upstream API changed: unsupported signature for {name}: {actual}")
    return definition


def write_if_changed(path, content):
    if not path.exists() or path.read_text() != content:
        path.write_text(content)


def generate(source_root, output_dir):
    temperature = (source_root / "src/iop/temperature.c").read_text()
    zonesystem = (source_root / "src/iop/zonesystem.c").read_text()
    colorspaces = (source_root / "src/common/colorspaces.h").read_text()
    for name, value in (("INITIALBLACKBODYTEMPERATURE", "4000"),
                        ("DT_IOP_LOWEST_TEMPERATURE", "1901"),
                        ("DT_IOP_HIGHEST_TEMPERATURE", "25000"),
                        ("DT_IOP_LOWEST_TINT", "0.135"),
                        ("DT_IOP_HIGHEST_TINT", "2.326")):
        once(r"^#define " + name + " " + re.escape(value) + r"$", temperature, name, re.M)

    # The entire live spectral block is reused. No spectral implementation is
    # stored in mobile/, and changes to its internal functions flow through.
    start = once(r"^typedef double\(\(\*spd\)\(unsigned long int wavelength, double TempK\)\);$",
                 temperature, "spectral-function typedef", re.M).start()
    stop = once(r"^static void _xyz2mul\(", temperature, "spectral-block end", re.M).start()
    if stop <= start:
        raise ValueError("upstream API changed: invalid spectral-block boundaries")
    spectral = temperature[start:stop]
    for name, signature in (
        ("_temperature_to_XYZ", "static cmsCIEXYZ _temperature_to_XYZ(double TempK)"),
        ("_temperature_tint_to_XYZ", "static cmsCIEXYZ _temperature_tint_to_XYZ(double TempK, double tint)"),
        ("_XYZ_to_temperature", "static void _XYZ_to_temperature(cmsCIEXYZ XYZ, float *TempK, float *tint)")):
        function(temperature, name, signature)
    xyz2mul = function(temperature, "_xyz2mul",
        "static void _xyz2mul(dt_iop_module_t *self, cmsCIEXYZ xyz, double mul[4])")
    mul2xyz = function(temperature, "_mul2xyz",
        "static cmsCIEXYZ _mul2xyz(dt_iop_module_t *self, const dt_iop_temperature_params_t *p)")
    array = function(temperature, "_temp_array_from_params",
        "static inline void _temp_array_from_params(double a[4], const dt_iop_temperature_params_t *p)")
    d65 = once(r"^static const cmsCIExyY D65xyY = \{[^\n]*\};$", colorspaces, "D65 white point", re.M).group()
    matrices = []
    for name, dimensions in (("RGB_to_XYZ", r"\[3\]\[4\]"), ("XYZ_to_RGB", r"\[4\]\[3\]")):
        matrices.append(once(r"  const double " + name + dimensions + r" = \{.*?\};",
                             temperature, name + " matrix", re.S).group())
    prelude = """/* Generated from current upstream temperature.c; do not edit. */
#include <math.h>
#include <stddef.h>
#include <string.h>
#include "external/cie_colorimetric_tables.c"
#define INITIALBLACKBODYTEMPERATURE 4000
#define for_four_channels(c) for(int c = 0; c < 4; c++)
typedef struct { double XYZ_to_CAM[4][3], CAM_to_XYZ[3][4]; } dt_iop_temperature_gui_data_t;
typedef struct { void *gui_data; } dt_iop_module_t;
typedef struct { float red, green, blue, various; } dt_iop_temperature_params_t;
"""
    output_dir.mkdir(parents=True, exist_ok=True)
    write_if_changed(output_dir / "whitebalance_upstream.inc",
        prelude + d65 + "\n" + spectral + array + "\n" + xyz2mul + "\n" + mul2xyz + "\n"
        + "#undef for_four_channels\n"
        + "static void _dt_mobile_upstream_srgb_matrices(double XYZ_to_CAM[4][3], double CAM_to_XYZ[3][4])\n{\n"
        + "\n".join(matrices)
        + "\n  memcpy(XYZ_to_CAM, XYZ_to_RGB, sizeof(XYZ_to_RGB));\n"
        + "  memcpy(CAM_to_XYZ, RGB_to_XYZ, sizeof(RGB_to_XYZ));\n}\n")
    zone = function(zonesystem, "_iop_zonesystem_calculate_zonemap",
        "static inline void _iop_zonesystem_calculate_zonemap(dt_iop_zonesystem_params_t *p, float *zonemap)")
    # Only the argument wrapper changes; the interpolation body is upstream's.
    zone = zone.replace("_iop_zonesystem_calculate_zonemap(dt_iop_zonesystem_params_t *p, float *zonemap)",
                        "_dt_mobile_upstream_zonemap(dt_mobile_zonemap_source_params *p, float *zonemap)", 1)
    write_if_changed(output_dir / "zonemap_upstream.inc",
        "/* Generated from current upstream zonesystem.c; do not edit. */\n"
        "typedef struct { int size; const float *zone; } dt_mobile_zonemap_source_params;\n"
        + zone + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    try:
        generate(args.source_root, args.output_dir)
    except (ValueError, OSError) as error:
        parser.exit(1, f"Android numerical adapter: {error}\n")


if __name__ == "__main__":
    main()
