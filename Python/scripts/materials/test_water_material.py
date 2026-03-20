#!/usr/bin/env python
"""
Test script for creating a realistic water material with adjustable parameters,
animated wave effects, and applying it to a cuboid static mesh actor in the level.

Creates:
  - M_Water: A translucent water material with:
      - BaseColor: deep/shallow color blend via animated Fresnel
      - Animated color shimmer via Panner + Sine modulating the blend alpha
      - Vertex wave displacement via Sine → WorldPositionOffset
      - Opacity, Roughness, Refraction, Emissive highlights
  - water_cuboid: A scaled cuboid StaticMeshActor with the material applied.

The material graph:

  Color blend:
    VectorParameter "DeepColor"    ──┐
    VectorParameter "ShallowColor" ──┤── Lerp(A=Deep, B=Shallow, Alpha) ── BaseColor
    Fresnel ──┐                      │
    Power(Fresnel, FresnelPower) ────┤
    Add(Power, ColorShimmer) ────────┘
      where ColorShimmer = Sine(Panner.R * 2)

  Wave animation (color shimmer):
    TexCoord → Panner(Speed=WaveSpeed*Time) → .R → Sine → * 0.3 → Add to Fresnel Alpha

  Wave animation (vertex displacement):
    Time * WaveSpeed → Sine(period=1) → * WaveAmplitude → AppendVector(0, result) → WorldPositionOffset

  Emissive:
    ShallowColor * EmissiveStrength → EmissiveColor

  Direct connections:
    ScalarParameter "Opacity"    → Opacity
    ScalarParameter "Roughness"  → Roughness
    ScalarParameter "Refraction" → Refraction
"""

import sys
import os
import socket
import json
import logging
from typing import Dict, Any, Optional

sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger("TestWaterMaterial")


def send_command(command: str, params: Dict[str, Any]) -> Optional[Dict[str, Any]]:
    """Send a command to the Unreal MCP server and get the response."""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(10.0)
        sock.connect(("127.0.0.1", 55557))

        try:
            command_obj = {"type": command, "params": params}
            command_json = json.dumps(command_obj)
            logger.info(f"Sending: {command}")
            sock.sendall(command_json.encode('utf-8'))

            chunks = []
            while True:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                chunks.append(chunk)
                try:
                    data = b''.join(chunks)
                    json.loads(data.decode('utf-8'))
                    break
                except json.JSONDecodeError:
                    continue

            data = b''.join(chunks)
            response = json.loads(data.decode('utf-8'))
            if response.get("status") == "error":
                logger.error(f"  Error: {response.get('error')}")
            else:
                logger.info(f"  OK")
            return response
        finally:
            sock.close()

    except Exception as e:
        logger.error(f"Error sending command: {e}")
        return None


def require_success(response, step_name):
    """Assert a command succeeded, exit otherwise."""
    if not response or response.get("status") == "error":
        error = response.get("error", "No response") if response else "No response"
        logger.error(f"FAILED at '{step_name}': {error}")
        sys.exit(1)
    return response.get("result", response)


def get_expr_id(response):
    """Extract expression_id from a successful add_material_expression response."""
    result = response.get("result", response)
    return result.get("expression_id")


def add_expr(name, params):
    """Add a material expression, require success, return its ID."""
    resp = send_command("add_material_expression", params)
    require_success(resp, f"add {name}")
    return get_expr_id(resp)


def connect_exprs(src_id, tgt_id, src_out=0, tgt_in=0, label=""):
    """Connect two expressions."""
    resp = send_command("connect_material_expression", {
        "material_name": "M_Water",
        "source_expression_id": src_id,
        "target_expression_id": tgt_id,
        "source_output_index": src_out,
        "target_input_index": tgt_in,
    })
    require_success(resp, label or f"connect {src_id}->{tgt_id}")


def connect_to_input(expr_id, material_input, label=""):
    """Connect expression to material input."""
    resp = send_command("connect_to_material_input", {
        "material_name": "M_Water",
        "expression_id": expr_id,
        "material_input": material_input,
    })
    require_success(resp, label or f"connect ->{material_input}")


MAT = "M_Water"


def main():
    logger.info("=== Creating Water Material with Wave Animation ===")

    # ── Step 1: Create the base material ─────────────────────────────
    logger.info("Step 1: Creating M_Water material")
    resp = send_command("create_material", {
        "material_name": MAT,
        "path": "/Game/Materials",
        "blend_mode": "Translucent",
        "shading_model": "DefaultLit",
        "two_sided": True,
        "translucency_lighting_mode": "Surface",
        "refraction_method": "IndexOfRefraction",
    })
    require_success(resp, "create_material")

    # ── Step 2: Add all expression nodes ─────────────────────────────
    logger.info("Step 2: Adding expression nodes")

    # --- Color parameters ---
    deep_color_id = add_expr("DeepColor", {
        "material_name": MAT, "expression_type": "VectorParameter",
        "parameter_name": "DeepColor", "default_value": [0.0, 0.05, 0.15, 1.0],
        "x": -800, "y": -200,
    })
    shallow_color_id = add_expr("ShallowColor", {
        "material_name": MAT, "expression_type": "VectorParameter",
        "parameter_name": "ShallowColor", "default_value": [0.0, 0.4, 0.5, 1.0],
        "x": -800, "y": -50,
    })

    # --- Fresnel ---
    fresnel_power_id = add_expr("FresnelPower", {
        "material_name": MAT, "expression_type": "ScalarParameter",
        "parameter_name": "FresnelPower", "default_value": 3.0,
        "x": -800, "y": 100,
    })
    fresnel_id = add_expr("Fresnel", {
        "material_name": MAT, "expression_type": "Fresnel",
        "x": -600, "y": 100,
    })
    power_id = add_expr("Power", {
        "material_name": MAT, "expression_type": "Power",
        "x": -400, "y": 100,
    })

    # --- Color shimmer: Panner-driven sine modulates Lerp alpha ---
    wave_speed_id = add_expr("WaveSpeed", {
        "material_name": MAT, "expression_type": "ScalarParameter",
        "parameter_name": "WaveSpeed", "default_value": 0.1,
        "x": -1200, "y": 400,
    })
    texcoord_id = add_expr("TexCoord", {
        "material_name": MAT, "expression_type": "TextureCoordinate",
        "x": -1200, "y": 300,
    })
    time_id = add_expr("Time", {
        "material_name": MAT, "expression_type": "Time",
        "x": -1200, "y": 500,
    })
    # Time * WaveSpeed = animated time value
    time_mult_id = add_expr("TimeMult", {
        "material_name": MAT, "expression_type": "Multiply",
        "x": -1000, "y": 450,
    })
    panner_id = add_expr("Panner", {
        "material_name": MAT, "expression_type": "Panner",
        "x": -800, "y": 350,
    })
    # Sine of panned UV for color shimmer
    shimmer_sine_id = add_expr("ShimmerSine", {
        "material_name": MAT, "expression_type": "Sine",
        "period": 1.0,
        "x": -600, "y": 350,
    })
    # Scale shimmer amplitude (constant 0.3)
    shimmer_scale_id = add_expr("ShimmerScale", {
        "material_name": MAT, "expression_type": "Constant",
        "value": 0.3,
        "x": -600, "y": 450,
    })
    shimmer_mult_id = add_expr("ShimmerMult", {
        "material_name": MAT, "expression_type": "Multiply",
        "x": -400, "y": 350,
    })
    # Add shimmer to Fresnel alpha
    alpha_add_id = add_expr("AlphaAdd", {
        "material_name": MAT, "expression_type": "Add",
        "x": -250, "y": 200,
    })
    # Clamp combined alpha to 0..1
    alpha_clamp_id = add_expr("AlphaClamp", {
        "material_name": MAT, "expression_type": "Clamp",
        "x": -100, "y": 200,
    })

    # --- Lerp for blending deep/shallow ---
    lerp_id = add_expr("Lerp", {
        "material_name": MAT, "expression_type": "LinearInterpolate",
        "x": 100, "y": -100,
    })

    # --- Direct material parameters ---
    opacity_id = add_expr("Opacity", {
        "material_name": MAT, "expression_type": "ScalarParameter",
        "parameter_name": "Opacity", "default_value": 0.7,
        "x": -300, "y": 600,
    })
    roughness_id = add_expr("Roughness", {
        "material_name": MAT, "expression_type": "ScalarParameter",
        "parameter_name": "Roughness", "default_value": 0.05,
        "x": -300, "y": 700,
    })
    refraction_id = add_expr("Refraction", {
        "material_name": MAT, "expression_type": "ScalarParameter",
        "parameter_name": "Refraction", "default_value": 1.333,
        "x": -300, "y": 800,
    })

    # --- Vertex wave displacement ---
    wave_amp_id = add_expr("WaveAmplitude", {
        "material_name": MAT, "expression_type": "ScalarParameter",
        "parameter_name": "WaveAmplitude", "default_value": 5.0,
        "x": -800, "y": 900,
    })
    wave_sine_id = add_expr("WaveSine", {
        "material_name": MAT, "expression_type": "Sine",
        "period": 1.0,
        "x": -600, "y": 850,
    })
    wave_disp_mult_id = add_expr("WaveDispMult", {
        "material_name": MAT, "expression_type": "Multiply",
        "x": -400, "y": 870,
    })
    # Need a zero constant for X component of AppendVector
    zero_const_id = add_expr("Zero", {
        "material_name": MAT, "expression_type": "Constant",
        "value": 0.0,
        "x": -400, "y": 950,
    })
    # AppendVector(0, WaveDisp) → makes (0, WaveDisp) = 2D
    append1_id = add_expr("Append1", {
        "material_name": MAT, "expression_type": "AppendVector",
        "x": -200, "y": 900,
    })
    # AppendVector((0, WaveDisp), 0) → makes (0, WaveDisp, 0) = 3D
    # Actually we want (0, 0, WaveDisp) for Z-up displacement
    # So: AppendVector(0, 0) → (0,0), then AppendVector((0,0), WaveDisp) → (0, 0, WaveDisp)
    append2_id = add_expr("Append2", {
        "material_name": MAT, "expression_type": "AppendVector",
        "x": -200, "y": 1000,
    })
    append3_id = add_expr("AppendFinal", {
        "material_name": MAT, "expression_type": "AppendVector",
        "x": 0, "y": 950,
    })

    # --- Emissive ---
    emissive_strength_id = add_expr("EmissiveStrength", {
        "material_name": MAT, "expression_type": "ScalarParameter",
        "parameter_name": "EmissiveStrength", "default_value": 0.1,
        "x": -400, "y": -350,
    })
    emissive_mult_id = add_expr("EmissiveMult", {
        "material_name": MAT, "expression_type": "Multiply",
        "x": -150, "y": -350,
    })

    # ── Step 3: Wire up expression-to-expression connections ─────────
    logger.info("Step 3: Connecting expression graph")

    # --- Fresnel + Power ---
    connect_exprs(fresnel_id, power_id, 0, 0, "Fresnel->Power.Base")
    connect_exprs(fresnel_power_id, power_id, 0, 1, "FresnelPower->Power.Exp")

    # --- Wave speed animation: Time * WaveSpeed ---
    connect_exprs(time_id, time_mult_id, 0, 0, "Time->TimeMult.A")
    connect_exprs(wave_speed_id, time_mult_id, 0, 1, "WaveSpeed->TimeMult.B")

    # --- Panner: TexCoord + animated time ---
    connect_exprs(texcoord_id, panner_id, 0, 0, "TexCoord->Panner.Coord")
    connect_exprs(time_mult_id, panner_id, 0, 1, "TimeMult->Panner.Time")

    # --- Color shimmer: Sine(Panner.R) * 0.3 ---
    connect_exprs(panner_id, shimmer_sine_id, 0, 0, "Panner->ShimmerSine.Input")
    connect_exprs(shimmer_sine_id, shimmer_mult_id, 0, 0, "ShimmerSine->ShimmerMult.A")
    connect_exprs(shimmer_scale_id, shimmer_mult_id, 0, 1, "ShimmerScale->ShimmerMult.B")

    # --- Combined alpha: Power(Fresnel) + ShimmerMult → Clamp ---
    connect_exprs(power_id, alpha_add_id, 0, 0, "Power->AlphaAdd.A")
    connect_exprs(shimmer_mult_id, alpha_add_id, 0, 1, "ShimmerMult->AlphaAdd.B")
    connect_exprs(alpha_add_id, alpha_clamp_id, 0, 0, "AlphaAdd->AlphaClamp.Input")

    # --- Lerp: Deep/Shallow blend with animated alpha ---
    connect_exprs(deep_color_id, lerp_id, 0, 0, "DeepColor->Lerp.A")
    connect_exprs(shallow_color_id, lerp_id, 0, 1, "ShallowColor->Lerp.B")
    connect_exprs(alpha_clamp_id, lerp_id, 0, 2, "AlphaClamp->Lerp.Alpha")

    # --- Emissive: ShallowColor * EmissiveStrength ---
    connect_exprs(shallow_color_id, emissive_mult_id, 0, 0, "ShallowColor->EmissiveMult.A")
    connect_exprs(emissive_strength_id, emissive_mult_id, 0, 1, "EmissiveStrength->EmissiveMult.B")

    # --- Vertex wave: Time*Speed → Sine → *Amplitude → (0,0,Z) ---
    connect_exprs(time_mult_id, wave_sine_id, 0, 0, "TimeMult->WaveSine.Input")
    connect_exprs(wave_sine_id, wave_disp_mult_id, 0, 0, "WaveSine->WaveDispMult.A")
    connect_exprs(wave_amp_id, wave_disp_mult_id, 0, 1, "WaveAmplitude->WaveDispMult.B")

    # Build (0, 0, WaveDisp) vector for WorldPositionOffset
    # Append(0, 0) → (0,0)
    connect_exprs(zero_const_id, append2_id, 0, 0, "Zero->Append2.A")
    connect_exprs(zero_const_id, append2_id, 0, 1, "Zero->Append2.B")
    # Append((0,0), WaveDisp) → (0, 0, WaveDisp)
    connect_exprs(append2_id, append3_id, 0, 0, "Append2->AppendFinal.A")
    connect_exprs(wave_disp_mult_id, append3_id, 0, 1, "WaveDispMult->AppendFinal.B")

    # ── Step 4: Connect to material inputs ───────────────────────────
    logger.info("Step 4: Connecting to material inputs")

    connect_to_input(lerp_id, "BaseColor", "Lerp->BaseColor")
    connect_to_input(opacity_id, "Opacity", "Opacity->Opacity")
    connect_to_input(roughness_id, "Roughness", "Roughness->Roughness")
    connect_to_input(refraction_id, "Refraction", "Refraction->Refraction")
    connect_to_input(emissive_mult_id, "EmissiveColor", "EmissiveMult->EmissiveColor")
    connect_to_input(append3_id, "WorldPositionOffset", "AppendFinal->WorldPositionOffset")

    # ── Step 5: Verify material ──────────────────────────────────────
    logger.info("Step 5: Verifying material")
    resp = send_command("get_material_stats", {"material_name": MAT})
    require_success(resp, "get_material_stats")
    result = resp.get("result", resp)
    logger.info(f"  Expressions: {result.get('total_expressions')}")
    logger.info(f"  Parameters:  {result.get('parameter_count')}")
    logger.info(f"  Connections: {result.get('connection_count')}")
    logger.info(f"  Inputs:      {result.get('connected_material_inputs')}")

    # ── Step 6: Create cuboid actor ──────────────────────────────────
    logger.info("Step 6: Creating water_cuboid actor")
    resp = send_command("spawn_actor", {
        "name": "water_cuboid",
        "type": "StaticMeshActor",
        "location": [0.0, 0.0, 50.0],
        "rotation": [0.0, 0.0, 0.0],
        "scale": [3.0, 5.0, 0.5],
        "folder": "water_test",
    })
    require_success(resp, "spawn water_cuboid")

    # ── Step 7: Apply material to actor ──────────────────────────────
    logger.info("Step 7: Applying M_Water to water_cuboid")
    resp = send_command("apply_material_to_actor", {
        "actor_name": "water_cuboid",
        "material_name": MAT,
        "slot_index": 0,
    })
    require_success(resp, "apply_material_to_actor")

    # ── Step 8: Focus viewport ───────────────────────────────────────
    logger.info("Step 8: Focusing viewport on water cuboid")
    resp = send_command("focus_viewport", {
        "target": "water_cuboid",
        "distance": 500.0,
    })
    if resp and resp.get("status") != "error":
        logger.info("  Viewport focused")

    logger.info("")
    logger.info("=== Water Material Test Complete ===")
    logger.info("Adjustable parameters (tweak in Material Instance or editor):")
    logger.info("  DeepColor        - Deep water color        [0.0, 0.05, 0.15]")
    logger.info("  ShallowColor     - Shallow/edge color      [0.0, 0.4, 0.5]")
    logger.info("  FresnelPower     - Edge-vs-center blend    3.0")
    logger.info("  Opacity          - Overall transparency    0.7")
    logger.info("  Roughness        - Surface shininess       0.05")
    logger.info("  Refraction       - IOR (water=1.333)       1.333")
    logger.info("  EmissiveStrength - Glow intensity          0.1")
    logger.info("  WaveSpeed        - Animation speed         0.1")
    logger.info("  WaveAmplitude    - Vertex wave height      5.0")


if __name__ == "__main__":
    main()
