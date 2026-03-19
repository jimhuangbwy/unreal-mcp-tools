#!/usr/bin/env python
"""
Test script for creating and manipulating cube actors in Unreal Engine via MCP.

Creates two cube actors under a "test" folder in the World Outliner.
Automatically increments name suffixes (cube_1, cube_2, ...) to avoid duplicates.
"""

import sys
import os
import re
import socket
import json
import logging
from typing import Dict, Any, Optional

sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger("TestCube")

FOLDER_NAME = "test"


def send_command(command: str, params: Dict[str, Any]) -> Optional[Dict[str, Any]]:
    """Send a command to the Unreal MCP server and get the response."""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))

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
        finally:
            sock.close()

    except Exception as e:
        logger.error(f"Error sending command: {e}")
        return None


def get_existing_cube_names() -> set[str]:
    """Get names of all existing actors that match the cube_N pattern."""
    response = send_command("find_actors_by_name", {"pattern": "cube_"})
    if not response or response.get("status") != "success":
        return set()

    names = set()
    result = response.get("result", {})
    actors = result.get("actors", [])
    for actor in actors:
        if isinstance(actor, dict):
            name = actor.get("name", "")
        else:
            name = ""
        if re.match(r"^cube_\d+$", name):
            names.add(name)
    return names


def next_cube_name(existing: set[str]) -> str:
    """Return the next available cube_N name."""
    n = 1
    while f"cube_{n}" in existing:
        n += 1
    return f"cube_{n}"


def create_cube(name: str, location: list[float]) -> Optional[Dict[str, Any]]:
    """Create a cube actor in the test folder."""
    params = {
        "name": name,
        "type": "StaticMeshActor",
        "location": location,
        "rotation": [0.0, 0.0, 0.0],
        "scale": [1.0, 1.0, 1.0],
        "folder": FOLDER_NAME,
    }

    response = send_command("spawn_actor", params)
    if not response or response.get("status") != "success":
        logger.error(f"Failed to create cube '{name}': {response}")
        return None

    logger.info(f"Created cube '{name}' at {location} in folder '{FOLDER_NAME}'")
    return response


def main():
    """Create two cubes with auto-incrementing names under the test folder."""
    try:
        existing = get_existing_cube_names()
        logger.info(f"Found existing cubes: {sorted(existing) if existing else 'none'}")

        # Create two cubes
        for i, location in enumerate([[0.0, 0.0, 100.0], [200.0, 0.0, 100.0]]):
            name = next_cube_name(existing)
            result = create_cube(name, location)
            if not result:
                logger.error(f"Failed to create cube #{i+1}")
                return
            existing.add(name)

        logger.info("All test operations completed successfully!")

    except Exception as e:
        logger.error(f"Error in main: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
