# Unreal MCP Tools

An [MCP (Model Context Protocol)](https://modelcontextprotocol.io/) integration for **Unreal Engine 5.7** that allows AI assistants (Claude Desktop, Cursor, Windsurf) to directly interact with the Unreal Editor — spawning actors, creating blueprints, building UMG widgets, and more.

## How It Works

```
┌──────────────────────┐       stdio       ┌──────────────────────┐    TCP/JSON     ┌──────────────────────┐
│  AI Client           │ ◄──────────────► │  Python MCP Server   │ ◄────────────► │  UE5 Plugin          │
│  (Claude Desktop,    │                   │  (FastMCP)           │  port 55557    │  (UnrealMCPTools)    │
│   Cursor, Windsurf)  │                   │                      │                │                      │
└──────────────────────┘                   └──────────────────────┘                └──────────────────────┘
```

The **Python MCP Server** exposes tools to AI clients via the MCP protocol. When a tool is called, it sends a JSON command over TCP to the **UnrealMCPTools** plugin running inside the Unreal Editor, which executes the operation and returns the result.

## Available Tools

| Category | Tools |
|----------|-------|
| **Editor** | `get_actors_in_level`, `find_actors_by_name`, `spawn_actor`, `delete_actor`, `set_actor_transform`, `get_actor_properties`, `set_actor_property`, `spawn_blueprint_actor`, `take_screenshot` |
| **Blueprint** | `create_blueprint`, `add_component_to_blueprint`, `set_component_property`, `set_static_mesh_properties`, `set_physics_properties`, `compile_blueprint`, `set_blueprint_property`, `set_pawn_properties` |
| **Blueprint Nodes** | `add_blueprint_event_node`, `add_blueprint_input_action_node`, `add_blueprint_function_node`, `connect_blueprint_nodes`, `add_blueprint_variable`, `add_blueprint_get_self_component_reference`, `add_blueprint_self_reference`, `find_blueprint_nodes` |
| **UMG Widgets** | `create_umg_widget_blueprint`, `add_text_block_to_widget`, `add_button_to_widget`, `bind_widget_event`, `set_text_block_binding`, `add_widget_to_viewport` |
| **Project** | `create_input_mapping` |

## Prerequisites

- **Unreal Engine 5.7**
- **Python 3.10+**
- **[uv](https://github.com/astral-sh/uv)** — Python package manager

## Setup

### 1. Install the Unreal Plugin

Copy the `MCPTestProject/Plugins/UnrealMCPTools` folder into your Unreal Engine project's `Plugins/` directory, then rebuild the project. Or, if using the included test project, simply open `MCPTestProject/MCPTestProject.uproject` in UE5.7.

### 2. Install Python Dependencies

```bash
cd Python
uv venv
uv pip install -e .
```

### 3. Configure Your MCP Client

Add the following to your MCP client configuration (Claude Desktop `claude_desktop_config.json`, Cursor MCP settings, etc.):

```json
{
  "mcpServers": {
    "unrealMCPTools": {
      "command": "uv",
      "args": [
        "--directory",
        "/absolute/path/to/this/repo/Python",
        "run",
        "unreal_mcp_server.py"
      ]
    }
  }
}
```

Replace the path with the absolute path to the `Python/` directory in this repository.

## Usage

1. **Open the Unreal Editor** — the UnrealMCPTools plugin starts its TCP server automatically on port 55557
2. **Start your MCP client** — it will launch the Python MCP server which connects to the Unreal plugin
3. **Ask the AI to interact with your scene** — e.g., "Spawn a point light at position (0, 0, 300)" or "Create a blueprint called MyPawn with a camera component"

## Testing

Integration test scripts are available in `Python/scripts/`. These connect directly to the Unreal plugin (no MCP server needed) and are useful for verifying the TCP bridge:

```bash
cd Python

# Test actor operations
uv run scripts/actors/test_cube.py

# Test blueprint creation
uv run scripts/blueprints/test_create_and_spawn_cube_blueprint.py

# Test blueprint nodes and input
uv run scripts/node/test_input_mapping.py
```

The Unreal Editor must be running with the plugin loaded before running these scripts.

## Project Structure

```
├── Python/
│   ├── unreal_mcp_server.py      # MCP server entry point
│   ├── tools/                    # MCP tool definitions
│   │   ├── editor_tools.py       # Actor and viewport operations
│   │   ├── blueprint_tools.py    # Blueprint creation and modification
│   │   ├── node_tools.py         # Blueprint graph node operations
│   │   ├── umg_tools.py          # UMG widget operations
│   │   └── project_tools.py      # Project-level settings
│   ├── scripts/                  # Integration test scripts
│   └── pyproject.toml
├── MCPTestProject/
│   ├── MCPTestProject.uproject
│   └── Plugins/
│       └── UnrealMCPTools/       # UE5 editor plugin
│           ├── UnrealMCPTools.uplugin
│           └── Source/UnrealMCPTools/
│               ├── Public/       # Headers
│               └── Private/      # Implementation
└── mcp-tools.json                # Example MCP client configuration
```

## Troubleshooting

- **Connection refused** — Make sure the Unreal Editor is open and the plugin is loaded before starting the MCP server. Check that port 55557 is not in use by another process.
- **Check logs** — Python server logs are written to `Python/unreal_mcp.log` with full debug output. The UE plugin logs to the Output Log under `LogTemp`.

## Acknowledgments

This project is inspired by [unreal-mcp](https://github.com/chongdashu/unreal-mcp) by chongdashu.

## License

MIT License — see [LICENSE](LICENSE) for details.
