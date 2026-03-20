#!/usr/bin/env python
"""
Test script for Enhanced Input System via MCP.
This script creates InputAction assets, an InputMappingContext, and wires
enhanced input action event nodes in a Blueprint.
"""

import sys
import os
import socket
import json
import logging
from typing import Dict, Any, Optional

# Add the parent directory to the path so we can import the server module
sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

# Set up logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger("TestEnhancedInput")

HOST = "127.0.0.1"
PORT = 55557
BLUEPRINT_NAME = "EnhancedInputBP"


def new_connection() -> socket.socket:
    """Create a new TCP connection to the Unreal MCP server."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5.0)
    sock.connect((HOST, PORT))
    return sock


def send_command(command: str, params: Dict[str, Any]) -> Optional[Dict[str, Any]]:
    """Send a command using a fresh connection and return the response."""
    sock = new_connection()
    try:
        command_obj = {"type": command, "params": params}
        command_json = json.dumps(command_obj)
        logger.info(f"Sending command: {command_json}")
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
        logger.info(f"Received response: {response}")
        return response

    except Exception as e:
        logger.error(f"Error sending command: {e}")
        return None
    finally:
        sock.close()


def expect_success(response: Optional[Dict], step: str) -> Dict:
    """Check response is successful, log and exit if not."""
    if not response or response.get("status") != "success":
        logger.error(f"Failed at step '{step}': {response}")
        sys.exit(1)
    return response.get("result", {})


def main():
    """Main function to test Enhanced Input System in blueprints."""

    # ── Step 1: Create blueprint ──────────────────────────────────────
    result = expect_success(
        send_command("create_blueprint", {"name": BLUEPRINT_NAME, "parent_class": "Actor"}),
        "create blueprint"
    )
    if result.get("already_exists"):
        logger.info(f"Blueprint '{BLUEPRINT_NAME}' already exists, reusing it")
    else:
        logger.info("Blueprint created successfully!")

    # ── Step 2: Add variables ─────────────────────────────────────────
    variables = [
        {"variable_name": "Score", "variable_type": "Integer", "default_value": 0, "is_exposed": True},
        {"variable_name": "IsGameActive", "variable_type": "Boolean", "default_value": True, "is_exposed": True},
        {"variable_name": "PlayerName", "variable_type": "String", "default_value": "Player1", "is_exposed": True},
    ]
    for var in variables:
        var["blueprint_name"] = BLUEPRINT_NAME
        expect_success(send_command("add_blueprint_variable", var), f"add variable {var['variable_name']}")
        logger.info(f"Variable {var['variable_name']} added!")

    # ── Step 3: Create InputAction assets (Enhanced Input) ────────────
    actions = [
        ("IA_Jump", "Boolean"),
        ("IA_Pause", "Boolean"),
        ("IA_Restart", "Boolean"),
        ("IA_MoveForward", "Axis1D"),
        ("IA_MoveRight", "Axis1D"),
    ]
    for action_name, value_type in actions:
        expect_success(
            send_command("create_input_action", {"action_name": action_name, "value_type": value_type}),
            f"create input action {action_name}"
        )
        logger.info(f"InputAction '{action_name}' created!")

    # ── Step 4: Create InputMappingContext with key bindings ──────────
    mappings = [
        {"action_name": "IA_Jump", "key": "SpaceBar"},
        {"action_name": "IA_Pause", "key": "P"},
        {"action_name": "IA_Restart", "key": "R"},
        {"action_name": "IA_MoveForward", "key": "W"},
        {"action_name": "IA_MoveRight", "key": "D"},
    ]
    expect_success(
        send_command("create_input_mapping_context", {"context_name": "IMC_Default", "mappings": mappings}),
        "create input mapping context"
    )
    logger.info("InputMappingContext 'IMC_Default' created with all mappings!")

    # ── Step 5: Add BeginPlay event node ──────────────────────────────
    result = expect_success(
        send_command("add_blueprint_event_node", {
            "blueprint_name": BLUEPRINT_NAME,
            "event_name": "BeginPlay",
            "node_position": [0, 0]
        }),
        "add BeginPlay event"
    )
    begin_play_id = result.get("node_id")
    logger.info("BeginPlay event node added!")

    # ── Step 6: Add PrintString for BeginPlay ─────────────────────────
    result = expect_success(
        send_command("add_blueprint_function_node", {
            "blueprint_name": BLUEPRINT_NAME,
            "target": "KismetSystemLibrary",
            "function_name": "PrintString",
            "params": {"InString": "Enhanced Input Controller Initialized", "Duration": 5.0},
            "node_position": [300, 0]
        }),
        "add BeginPlay PrintString"
    )
    print_init_id = result.get("node_id")
    logger.info("PrintString node for BeginPlay added!")

    # ── Step 6b: Add mapping context setup in BeginPlay ─────────────
    # GetPlayerController(0) - pure node, no exec pins
    result = expect_success(
        send_command("add_blueprint_function_node", {
            "blueprint_name": BLUEPRINT_NAME,
            "target": "GameplayStatics",
            "function_name": "GetPlayerController",
            "params": {"PlayerIndex": 0},
            "node_position": [300, 100]
        }),
        "add GetPlayerController"
    )
    get_pc_id = result.get("node_id")
    logger.info("GetPlayerController node added!")

    # Get Enhanced Input Subsystem from Player Controller (K2Node_GetSubsystemFromPC)
    result = expect_success(
        send_command("add_blueprint_get_subsystem_node", {
            "blueprint_name": BLUEPRINT_NAME,
            "subsystem_class": "EnhancedInputLocalPlayerSubsystem",
            "node_position": [550, 100]
        }),
        "add GetSubsystem"
    )
    get_subsystem_id = result.get("node_id")
    logger.info("GetSubsystem node added!")

    # AddMappingContext - has exec pins
    result = expect_success(
        send_command("add_blueprint_function_node", {
            "blueprint_name": BLUEPRINT_NAME,
            "target": "EnhancedInputLocalPlayerSubsystem",
            "function_name": "AddMappingContext",
            "params": {
                "MappingContext": "/Game/Input/IMC_Default",
                "Priority": 0
            },
            "node_position": [800, 0]
        }),
        "add AddMappingContext"
    )
    add_imc_id = result.get("node_id")
    logger.info("AddMappingContext node added!")

    # Connect execution: BeginPlay -> PrintString -> AddMappingContext
    expect_success(
        send_command("connect_blueprint_nodes", {
            "blueprint_name": BLUEPRINT_NAME,
            "source_node_id": begin_play_id,
            "source_pin": "Then",
            "target_node_id": print_init_id,
            "target_pin": "execute"
        }),
        "connect BeginPlay to PrintString"
    )
    logger.info("BeginPlay connected to PrintString!")

    expect_success(
        send_command("connect_blueprint_nodes", {
            "blueprint_name": BLUEPRINT_NAME,
            "source_node_id": print_init_id,
            "source_pin": "Then",
            "target_node_id": add_imc_id,
            "target_pin": "execute"
        }),
        "connect PrintString to AddMappingContext"
    )
    logger.info("PrintString connected to AddMappingContext!")

    # Connect data: GetPlayerController -> GetSubsystem (PlayerController pin)
    expect_success(
        send_command("connect_blueprint_nodes", {
            "blueprint_name": BLUEPRINT_NAME,
            "source_node_id": get_pc_id,
            "source_pin": "ReturnValue",
            "target_node_id": get_subsystem_id,
            "target_pin": "PlayerController"
        }),
        "connect GetPlayerController to GetSubsystem"
    )
    logger.info("GetPlayerController -> GetSubsystem connected!")

    # Connect data: GetSubsystem -> AddMappingContext (self/target pin)
    expect_success(
        send_command("connect_blueprint_nodes", {
            "blueprint_name": BLUEPRINT_NAME,
            "source_node_id": get_subsystem_id,
            "source_pin": "ReturnValue",
            "target_node_id": add_imc_id,
            "target_pin": "self"
        }),
        "connect GetSubsystem to AddMappingContext"
    )

    # ── Step 6c: Enable input on this Actor so it receives Enhanced Input events ──
    result = expect_success(
        send_command("add_blueprint_function_node", {
            "blueprint_name": BLUEPRINT_NAME,
            "target": "Actor",
            "function_name": "EnableInput",
            "node_position": [1100, 0]
        }),
        "add EnableInput"
    )
    enable_input_id = result.get("node_id")
    logger.info("EnableInput node added!")

    # Connect execution: AddMappingContext -> EnableInput
    expect_success(
        send_command("connect_blueprint_nodes", {
            "blueprint_name": BLUEPRINT_NAME,
            "source_node_id": add_imc_id,
            "source_pin": "Then",
            "target_node_id": enable_input_id,
            "target_pin": "execute"
        }),
        "connect AddMappingContext to EnableInput"
    )
    logger.info("AddMappingContext -> EnableInput connected!")

    # Connect data: GetPlayerController -> EnableInput (PlayerController pin)
    expect_success(
        send_command("connect_blueprint_nodes", {
            "blueprint_name": BLUEPRINT_NAME,
            "source_node_id": get_pc_id,
            "source_pin": "ReturnValue",
            "target_node_id": enable_input_id,
            "target_pin": "PlayerController"
        }),
        "connect GetPlayerController to EnableInput"
    )
    logger.info("GetPlayerController -> EnableInput connected!")
    logger.info("GetSubsystem -> AddMappingContext connected!")

    # ── Step 7: Add Enhanced Input Action nodes + PrintString for each action ──
    action_events = [
        ("IA_Jump", [0, 200], [300, 200]),
        ("IA_Pause", [0, 400], [300, 400]),
        ("IA_Restart", [0, 600], [300, 600]),
        ("IA_MoveForward", [0, 800], [300, 800]),
        ("IA_MoveRight", [0, 1000], [300, 1000]),
    ]

    for action_name, event_pos, func_pos in action_events:
        # Create Enhanced Input Action event node
        result = expect_success(
            send_command("add_blueprint_enhanced_input_action_node", {
                "blueprint_name": BLUEPRINT_NAME,
                "action_name": action_name,
                "node_position": event_pos
            }),
            f"add enhanced input node {action_name}"
        )
        event_node_id = result.get("node_id")
        logger.info(f"EnhancedInputAction node for {action_name} added!")

        # Create PrintString function node
        display_name = action_name.replace("IA_", "")
        result = expect_success(
            send_command("add_blueprint_function_node", {
                "blueprint_name": BLUEPRINT_NAME,
                "target": "KismetSystemLibrary",
                "function_name": "PrintString",
                "params": {"InString": f"{display_name} Action Triggered!", "Duration": 2.0},
                "node_position": func_pos
            }),
            f"add PrintString for {action_name}"
        )
        func_node_id = result.get("node_id")
        logger.info(f"PrintString node for {action_name} added!")

        # Connect Enhanced Input Action "Triggered" pin -> PrintString
        expect_success(
            send_command("connect_blueprint_nodes", {
                "blueprint_name": BLUEPRINT_NAME,
                "source_node_id": event_node_id,
                "source_pin": "Triggered",
                "target_node_id": func_node_id,
                "target_pin": "execute"
            }),
            f"connect {action_name} to PrintString"
        )
        logger.info(f"Connected {action_name} Triggered -> PrintString!")

    # ── Step 8: Compile the blueprint ─────────────────────────────────
    expect_success(
        send_command("compile_blueprint", {"blueprint_name": BLUEPRINT_NAME}),
        "compile blueprint"
    )
    logger.info("Blueprint compiled successfully!")

    # ── Step 9: Spawn the blueprint actor ─────────────────────────────
    expect_success(
        send_command("spawn_blueprint_actor", {
            "blueprint_name": BLUEPRINT_NAME,
            "actor_name": "EnhancedInputController",
            "location": [0.0, 0.0, 100.0],
            "rotation": [0.0, 0.0, 0.0],
            "scale": [1.0, 1.0, 1.0]
        }),
        "spawn blueprint actor"
    )
    logger.info("Enhanced input controller spawned successfully!")

    # ── Summary ───────────────────────────────────────────────────────
    logger.info("=== Enhanced Input System Test Complete ===")
    logger.info("Created assets:")
    for action_name, _ in actions:
        logger.info(f"  InputAction: /Game/Input/{action_name}")
    logger.info("  InputMappingContext: /Game/Input/IMC_Default")
    logger.info("Blueprint nodes:")
    logger.info("  BeginPlay -> PrintString('Enhanced Input Controller Initialized')")
    for action_name, _, _ in action_events:
        display_name = action_name.replace("IA_", "")
        logger.info(f"  {action_name} [Triggered] -> PrintString('{display_name} Action Triggered!')")


if __name__ == "__main__":
    main()
