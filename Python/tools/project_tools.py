"""
Project Tools for Unreal MCP.

This module provides tools for managing project-wide settings and configuration.
"""

import logging
from typing import Dict, Any
from mcp.server.fastmcp import FastMCP, Context

# Get logger
logger = logging.getLogger("UnrealMCP")

def register_project_tools(mcp: FastMCP):
    """Register project tools with the MCP server."""
    
    @mcp.tool()
    def create_input_mapping(
        ctx: Context,
        action_name: str,
        key: str,
        input_type: str = "Action"
    ) -> Dict[str, Any]:
        """
        Create an input mapping for the project.
        
        Args:
            action_name: Name of the input action
            key: Key to bind (SpaceBar, LeftMouseButton, etc.)
            input_type: Type of input mapping (Action or Axis)
            
        Returns:
            Response indicating success or failure
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "action_name": action_name,
                "key": key,
                "input_type": input_type
            }
            
            logger.info(f"Creating input mapping '{action_name}' with key '{key}'")
            response = unreal.send_command("create_input_mapping", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Input mapping creation response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error creating input mapping: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def create_input_action(
        ctx: Context,
        action_name: str,
        value_type: str = "Boolean"
    ) -> Dict[str, Any]:
        """
        Create an Enhanced Input Action asset.

        Args:
            action_name: Name of the input action (e.g., IA_Jump)
            value_type: Value type - Boolean, Axis1D/Float, Axis2D/Vector2D, Axis3D/Vector

        Returns:
            Response containing the asset path and success status
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "action_name": action_name,
                "value_type": value_type
            }

            logger.info(f"Creating input action '{action_name}' with value type '{value_type}'")
            response = unreal.send_command("create_input_action", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Input action creation response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error creating input action: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def create_input_mapping_context(
        ctx: Context,
        context_name: str,
        mappings: list
    ) -> Dict[str, Any]:
        """
        Create an Enhanced Input Mapping Context asset with key-to-action mappings.

        Args:
            context_name: Name of the mapping context (e.g., IMC_Default)
            mappings: List of dicts with 'action_name' and 'key' fields
                     e.g., [{"action_name": "IA_Jump", "key": "SpaceBar"}]

        Returns:
            Response containing the asset path and success status
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "context_name": context_name,
                "mappings": mappings
            }

            logger.info(f"Creating input mapping context '{context_name}' with {len(mappings)} mappings")
            response = unreal.send_command("create_input_mapping_context", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Input mapping context creation response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error creating input mapping context: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def list_assets(
        ctx: Context,
        path: str = "/Game/",
        class_filter: str = "",
        recursive: bool = True
    ) -> Dict[str, Any]:
        """
        List assets in the project using the Asset Registry.

        Args:
            path: Asset path to search (defaults to "/Game/")
            class_filter: Optional class name filter (e.g., "Blueprint", "InputAction")
            recursive: Whether to search subdirectories (defaults to True)

        Returns:
            List of assets with their names, paths, and classes
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {"path": path, "recursive": recursive}
            if class_filter:
                params["class_filter"] = class_filter

            response = unreal.send_command("list_assets", params)
            if not response:
                return {"success": False, "message": "No response from Unreal Engine"}
            return response
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    @mcp.tool()
    def get_input_actions(ctx: Context) -> Dict[str, Any]:
        """
        List all Enhanced Input Action assets in the project.

        Returns:
            List of InputAction assets with their names, paths, and value types
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("get_input_actions", {})
            if not response:
                return {"success": False, "message": "No response from Unreal Engine"}
            return response
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    @mcp.tool()
    def get_level_info(ctx: Context) -> Dict[str, Any]:
        """
        Get information about the current level including actor counts by class.

        Returns:
            Level info including name, path, total actors, actor class breakdown, and sub-levels
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("get_level_info", {})
            if not response:
                return {"success": False, "message": "No response from Unreal Engine"}
            return response
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    logger.info("Project tools registered successfully")