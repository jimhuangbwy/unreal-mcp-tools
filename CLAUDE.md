# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

An MCP (Model Context Protocol) server that enables AI tools (Claude Desktop, Cursor, Windsurf) to interact with Unreal Engine 5.7. Two components communicate over TCP/JSON:

- **Python MCP Server** (`Python/`) — FastMCP-based server exposing tools via stdio transport
- **Unreal Engine Plugin** (`MCPTestProject/Plugins/UnrealMCPTools/`) — C++ editor plugin running a TCP server inside UE5.7

## Architecture

```
AI Client (stdio) → Python MCP Server (TCP client) → UE5 Plugin TCP Server (port 55557)
```

The Python server receives MCP tool calls, translates them to JSON commands `{"type": "command_name", "params": {...}}`, sends them over TCP to Unreal, and returns the JSON response. The Unreal plugin receives commands on a dedicated thread, executes them on the game thread via `AsyncTask`, and sends back results. The connection is **non-persistent** — reconnects for each command.

## Build Commands

### Build the UE plugin (includes all project modules)
```
"C:/Program Files/Epic Games/UE_5.7/Engine/Build/BatchFiles/Build.bat" MCPTestProjectEditor Win64 Development "D:/UnrealProjects/unreal-mcp-tools/MCPTestProject/MCPTestProject.uproject" -WaitMutex
```

### Run the Python MCP server
```
cd Python && uv run unreal_mcp_server.py
```

### Run Python test scripts (requires UE editor running with plugin loaded)
```
cd Python && uv run scripts/actors/test_cube.py
```

## Python Server (`Python/`)

- **Entry point**: `unreal_mcp_server.py` — initializes FastMCP server, manages `UnrealConnection` TCP client
- **Tools**: `tools/` directory with 5 modules:
  - `editor_tools.py` — actor CRUD, transforms, properties, viewport
  - `blueprint_tools.py` — blueprint creation, components, compilation
  - `node_tools.py` — blueprint graph nodes, pin connections, variables
  - `umg_tools.py` — UMG widget blueprints, text blocks, buttons, bindings
  - `project_tools.py` — input action mappings
- **Dependencies**: managed via `pyproject.toml`, uses `uv` as the package manager
- **Test scripts**: `scripts/` directory with integration tests (not a test framework, just standalone scripts)

## Unreal Plugin (`MCPTestProject/Plugins/UnrealMCPTools/`)

- **Module name**: `UnrealMCPTools` (API macro: `UNREALMCPTOOLS_API`)
- **UnrealMCPBridge** (`UEditorSubsystem`) — initializes TCP server, routes commands to 5 handler classes
- **MCPServerRunnable** (`FRunnable`) — threaded TCP listener on 127.0.0.1:55557, parses JSON, bridges to game thread
- **Command handlers** (in `Private/Commands/`):
  - `UnrealMCPEditorCommands` — actor spawning, deletion, transforms, properties, viewport
  - `UnrealMCPBlueprintCommands` — blueprint creation, components, physics, compilation
  - `UnrealMCPBlueprintNodeCommands` — event/function/variable nodes, pin connections
  - `UnrealMCPUMGCommands` — widget blueprint creation, widget addition, event/property bindings
  - `UnrealMCPProjectCommands` — input mapping creation
- **UnrealMCPCommonUtils** — shared utilities for JSON parsing, response building, actor serialization, blueprint/node helpers

## Command Protocol

Commands are JSON objects with `type` and `params`. Responses use `{"success": true, "data": {...}}` for success and `{"status": "error", "error": "message"}` for errors.

## MCP Client Configuration

Configure in Claude Desktop/Cursor settings using `mcp.json`:
```json
{
  "mcpServers": {
    "unrealMCPTools": {
      "command": "uv",
      "args": ["--directory", "<absolute-path-to-repo>/Python", "run", "unreal_mcp_server.py"]
    }
  }
}
```
