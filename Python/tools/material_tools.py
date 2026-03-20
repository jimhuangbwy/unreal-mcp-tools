"""
Material Tools for Unreal MCP.

This module provides tools for creating and manipulating Materials,
Material Instances, Material Functions, and Material Parameter Collections.
"""

import logging
from typing import Dict, Any, List, Optional
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")

def register_material_tools(mcp: FastMCP):
    """Register material tools with the MCP server."""

    # ── Tier 1: Core Material CRUD ──────────────────────────────────

    @mcp.tool()
    def create_material(
        ctx: Context,
        material_name: str,
        path: str = "/Game/Materials",
        blend_mode: str = "Opaque",
        shading_model: str = "DefaultLit",
        two_sided: bool = False,
        translucency_lighting_mode: str = "",
        refraction_method: str = ""
    ) -> Dict[str, Any]:
        """
        Create a new Material asset.

        Args:
            material_name: Name of the material (e.g., M_MyMaterial)
            path: Asset path (defaults to /Game/Materials)
            blend_mode: Opaque, Translucent, Additive, Modulate, or Masked
            shading_model: DefaultLit, Unlit, Subsurface, or ClearCoat
            two_sided: Whether the material renders on both sides
            translucency_lighting_mode: Surface, SurfacePerPixelLighting, VolumetricDirectional, or VolumetricNonDirectional
            refraction_method: IndexOfRefraction, PixelNormalOffset, or None
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            params = {
                "material_name": material_name,
                "path": path,
                "blend_mode": blend_mode,
                "shading_model": shading_model,
                "two_sided": two_sided
            }
            if translucency_lighting_mode:
                params["translucency_lighting_mode"] = translucency_lighting_mode
            if refraction_method:
                params["refraction_method"] = refraction_method
            return unreal.send_command("create_material", params) or {"success": False, "message": "No response"}
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    @mcp.tool()
    def set_material_properties(
        ctx: Context,
        material_name: str,
        blend_mode: str = "",
        shading_model: str = "",
        two_sided: Optional[bool] = None,
        opacity_mask_clip_value: Optional[float] = None,
        translucency_lighting_mode: str = "",
        refraction_method: str = ""
    ) -> Dict[str, Any]:
        """
        Set properties on an existing Material.

        Args:
            material_name: Name of the material to modify
            blend_mode: Opaque, Translucent, Additive, Modulate, or Masked
            shading_model: DefaultLit, Unlit, Subsurface, or ClearCoat
            two_sided: Whether the material renders on both sides
            opacity_mask_clip_value: Clip value for masked blend mode
            translucency_lighting_mode: Surface, SurfacePerPixelLighting, VolumetricDirectional, or VolumetricNonDirectional
            refraction_method: IndexOfRefraction, PixelNormalOffset, or None
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            params = {"material_name": material_name}
            if blend_mode:
                params["blend_mode"] = blend_mode
            if shading_model:
                params["shading_model"] = shading_model
            if two_sided is not None:
                params["two_sided"] = two_sided
            if opacity_mask_clip_value is not None:
                params["opacity_mask_clip_value"] = opacity_mask_clip_value
            if translucency_lighting_mode:
                params["translucency_lighting_mode"] = translucency_lighting_mode
            if refraction_method:
                params["refraction_method"] = refraction_method
            return unreal.send_command("set_material_properties", params) or {"success": False, "message": "No response"}
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    @mcp.tool()
    def get_material_info(ctx: Context, material_name: str) -> Dict[str, Any]:
        """
        Get detailed information about a Material including its expressions and properties.

        Args:
            material_name: Name of the material
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            return unreal.send_command("get_material_info", {"material_name": material_name}) or {"success": False, "message": "No response"}
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    @mcp.tool()
    def delete_material(ctx: Context, material_name: str) -> Dict[str, Any]:
        """
        Delete a Material asset.

        Args:
            material_name: Name of the material to delete
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            return unreal.send_command("delete_material", {"material_name": material_name}) or {"success": False, "message": "No response"}
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    # ── Tier 2: Material Expressions / Graph ────────────────────────

    @mcp.tool()
    def add_material_expression(
        ctx: Context,
        material_name: str,
        expression_type: str,
        x: int = 0,
        y: int = 0,
        parameter_name: str = "",
        default_value: Any = None,
        value: Any = None,
        texture_path: str = ""
    ) -> Dict[str, Any]:
        """
        Add a material expression node to a Material's graph.

        Args:
            material_name: Name of the material
            expression_type: Type of expression. Common types:
                Parameters: ScalarParameter, VectorParameter, TextureSampleParameter2D
                Constants: Constant, Constant3Vector, Constant4Vector
                Math: Add, Multiply, LinearInterpolate (Lerp), Clamp, OneMinus, Power
                Texture: TextureSample
                Utility: Panner, TextureCoordinate (TexCoord), Time, Fresnel
                Functions: MaterialFunctionCall
            x: Editor X position
            y: Editor Y position
            parameter_name: Name for parameter expressions
            default_value: Default value (float for scalar, [R,G,B,A] for vector)
            value: Value for constant expressions (float or [R,G,B])
            texture_path: Asset path for texture expressions
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            params = {"material_name": material_name, "expression_type": expression_type, "x": x, "y": y}
            if parameter_name:
                params["parameter_name"] = parameter_name
            if default_value is not None:
                params["default_value"] = default_value
            if value is not None:
                params["value"] = value
            if texture_path:
                params["texture_path"] = texture_path
            return unreal.send_command("add_material_expression", params) or {"success": False, "message": "No response"}
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    @mcp.tool()
    def connect_material_expression(
        ctx: Context,
        material_name: str,
        source_expression_id: str,
        target_expression_id: str,
        source_output_index: int = 0,
        target_input_index: int = 0
    ) -> Dict[str, Any]:
        """
        Connect one material expression's output to another expression's input.

        Args:
            material_name: Name of the material
            source_expression_id: GUID of the source expression
            target_expression_id: GUID of the target expression
            source_output_index: Output index on the source (default 0)
            target_input_index: Input index on the target (default 0)
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            params = {
                "material_name": material_name,
                "source_expression_id": source_expression_id,
                "target_expression_id": target_expression_id,
                "source_output_index": source_output_index,
                "target_input_index": target_input_index
            }
            return unreal.send_command("connect_material_expression", params) or {"success": False, "message": "No response"}
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    @mcp.tool()
    def connect_to_material_input(
        ctx: Context,
        material_name: str,
        expression_id: str,
        material_input: str,
        output_index: int = 0
    ) -> Dict[str, Any]:
        """
        Connect a material expression to a material's main input (e.g., BaseColor, Metallic).

        Args:
            material_name: Name of the material
            expression_id: GUID of the expression to connect
            material_input: Target input name: BaseColor, Metallic, Specular, Roughness,
                          Normal, EmissiveColor, Opacity, OpacityMask, AmbientOcclusion,
                          WorldPositionOffset, Refraction, SubsurfaceColor, ClearCoat,
                          ClearCoatRoughness, Anisotropy, Tangent
            output_index: Output index on the expression (default 0)
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            params = {
                "material_name": material_name,
                "expression_id": expression_id,
                "material_input": material_input,
                "output_index": output_index
            }
            return unreal.send_command("connect_to_material_input", params) or {"success": False, "message": "No response"}
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    @mcp.tool()
    def disconnect_material_expression(
        ctx: Context,
        material_name: str,
        material_input: str = "",
        target_expression_id: str = "",
        target_input_index: int = 0
    ) -> Dict[str, Any]:
        """
        Disconnect a material expression. Either disconnect a material input or an expression input.

        Args:
            material_name: Name of the material
            material_input: Material input to disconnect (e.g., BaseColor, Metallic)
            target_expression_id: Or, disconnect an expression's input by its GUID
            target_input_index: Input index on the target expression
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            params = {"material_name": material_name}
            if material_input:
                params["material_input"] = material_input
            elif target_expression_id:
                params["target_expression_id"] = target_expression_id
                params["target_input_index"] = target_input_index
            return unreal.send_command("disconnect_material_expression", params) or {"success": False, "message": "No response"}
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    @mcp.tool()
    def remove_material_expression(
        ctx: Context,
        material_name: str,
        expression_id: str
    ) -> Dict[str, Any]:
        """
        Remove a material expression from a Material's graph.

        Args:
            material_name: Name of the material
            expression_id: GUID of the expression to remove
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            return unreal.send_command("remove_material_expression", {"material_name": material_name, "expression_id": expression_id}) or {"success": False, "message": "No response"}
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    @mcp.tool()
    def get_material_connections(ctx: Context, material_name: str) -> Dict[str, Any]:
        """
        Get all connections in a Material's expression graph.

        Args:
            material_name: Name of the material
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            return unreal.send_command("get_material_connections", {"material_name": material_name}) or {"success": False, "message": "No response"}
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    # ── Tier 3: Material Instances ──────────────────────────────────

    @mcp.tool()
    def create_material_instance(
        ctx: Context,
        instance_name: str,
        parent_material: str,
        path: str = "/Game/Materials"
    ) -> Dict[str, Any]:
        """
        Create a Material Instance Constant from a parent material.

        Args:
            instance_name: Name for the material instance (e.g., MI_MyMaterial_Red)
            parent_material: Name of the parent material or material instance
            path: Asset path (defaults to /Game/Materials)
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            params = {"instance_name": instance_name, "parent_material": parent_material, "path": path}
            return unreal.send_command("create_material_instance", params) or {"success": False, "message": "No response"}
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    @mcp.tool()
    def set_material_instance_scalar_param(
        ctx: Context,
        instance_name: str,
        parameter_name: str,
        value: float
    ) -> Dict[str, Any]:
        """
        Set a scalar parameter value on a Material Instance.

        Args:
            instance_name: Name of the material instance
            parameter_name: Name of the scalar parameter
            value: Float value to set
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            params = {"instance_name": instance_name, "parameter_name": parameter_name, "value": value}
            return unreal.send_command("set_material_instance_scalar_param", params) or {"success": False, "message": "No response"}
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    @mcp.tool()
    def set_material_instance_vector_param(
        ctx: Context,
        instance_name: str,
        parameter_name: str,
        value: list
    ) -> Dict[str, Any]:
        """
        Set a vector parameter value on a Material Instance.

        Args:
            instance_name: Name of the material instance
            parameter_name: Name of the vector parameter
            value: Color/vector as [R, G, B] or [R, G, B, A] with values 0.0-1.0
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            params = {"instance_name": instance_name, "parameter_name": parameter_name, "value": value}
            return unreal.send_command("set_material_instance_vector_param", params) or {"success": False, "message": "No response"}
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    @mcp.tool()
    def set_material_instance_texture_param(
        ctx: Context,
        instance_name: str,
        parameter_name: str,
        texture_path: str
    ) -> Dict[str, Any]:
        """
        Set a texture parameter value on a Material Instance.

        Args:
            instance_name: Name of the material instance
            parameter_name: Name of the texture parameter
            texture_path: Full asset path to the texture
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            params = {"instance_name": instance_name, "parameter_name": parameter_name, "texture_path": texture_path}
            return unreal.send_command("set_material_instance_texture_param", params) or {"success": False, "message": "No response"}
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    @mcp.tool()
    def get_material_instance_info(ctx: Context, instance_name: str) -> Dict[str, Any]:
        """
        Get information about a Material Instance including its parent and parameter overrides.

        Args:
            instance_name: Name of the material instance
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            return unreal.send_command("get_material_instance_info", {"instance_name": instance_name}) or {"success": False, "message": "No response"}
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    # ── Tier 4: Material Parameter Collections ──────────────────────

    @mcp.tool()
    def create_material_parameter_collection(
        ctx: Context,
        collection_name: str,
        path: str = "/Game/Materials"
    ) -> Dict[str, Any]:
        """
        Create a Material Parameter Collection asset.

        Args:
            collection_name: Name for the collection (e.g., MPC_GlobalParams)
            path: Asset path (defaults to /Game/Materials)
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            params = {"collection_name": collection_name, "path": path}
            return unreal.send_command("create_material_parameter_collection", params) or {"success": False, "message": "No response"}
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    @mcp.tool()
    def add_mpc_scalar_parameter(
        ctx: Context,
        collection_name: str,
        parameter_name: str,
        default_value: float = 0.0
    ) -> Dict[str, Any]:
        """
        Add a scalar parameter to a Material Parameter Collection.

        Args:
            collection_name: Name of the MPC
            parameter_name: Name for the new parameter
            default_value: Default float value
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            params = {"collection_name": collection_name, "parameter_name": parameter_name, "default_value": default_value}
            return unreal.send_command("add_mpc_scalar_parameter", params) or {"success": False, "message": "No response"}
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    @mcp.tool()
    def add_mpc_vector_parameter(
        ctx: Context,
        collection_name: str,
        parameter_name: str,
        default_value: list = None
    ) -> Dict[str, Any]:
        """
        Add a vector parameter to a Material Parameter Collection.

        Args:
            collection_name: Name of the MPC
            parameter_name: Name for the new parameter
            default_value: Default color as [R, G, B] or [R, G, B, A]
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            params = {"collection_name": collection_name, "parameter_name": parameter_name}
            if default_value:
                params["default_value"] = default_value
            return unreal.send_command("add_mpc_vector_parameter", params) or {"success": False, "message": "No response"}
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    @mcp.tool()
    def get_mpc_info(ctx: Context, collection_name: str) -> Dict[str, Any]:
        """
        Get information about a Material Parameter Collection including all parameters.

        Args:
            collection_name: Name of the MPC
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            return unreal.send_command("get_mpc_info", {"collection_name": collection_name}) or {"success": False, "message": "No response"}
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    # ── Tier 5: Material Functions ──────────────────────────────────

    @mcp.tool()
    def create_material_function(
        ctx: Context,
        function_name: str,
        path: str = "/Game/Materials/Functions",
        description: str = "",
        expose_to_library: bool = False
    ) -> Dict[str, Any]:
        """
        Create a Material Function asset.

        Args:
            function_name: Name for the function (e.g., MF_BlendOverlay)
            path: Asset path (defaults to /Game/Materials/Functions)
            description: Description of the function
            expose_to_library: Whether to show in the material function library
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            params = {"function_name": function_name, "path": path}
            if description:
                params["description"] = description
            params["expose_to_library"] = expose_to_library
            return unreal.send_command("create_material_function", params) or {"success": False, "message": "No response"}
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    @mcp.tool()
    def add_material_function_input(
        ctx: Context,
        function_name: str,
        input_name: str,
        input_type: str = "Scalar",
        x: int = -300,
        y: int = 0
    ) -> Dict[str, Any]:
        """
        Add an input to a Material Function.

        Args:
            function_name: Name of the material function
            input_name: Name for the input pin
            input_type: Scalar, Vector2, Vector3, Vector4, or Texture2D
            x: Editor X position
            y: Editor Y position
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            params = {
                "function_name": function_name,
                "input_name": input_name,
                "input_type": input_type,
                "x": x, "y": y
            }
            return unreal.send_command("add_material_function_input", params) or {"success": False, "message": "No response"}
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    @mcp.tool()
    def add_material_function_output(
        ctx: Context,
        function_name: str,
        output_name: str,
        x: int = 300,
        y: int = 0
    ) -> Dict[str, Any]:
        """
        Add an output to a Material Function.

        Args:
            function_name: Name of the material function
            output_name: Name for the output pin
            x: Editor X position
            y: Editor Y position
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            params = {"function_name": function_name, "output_name": output_name, "x": x, "y": y}
            return unreal.send_command("add_material_function_output", params) or {"success": False, "message": "No response"}
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    # ── Tier 6: Application & Analysis ──────────────────────────────

    @mcp.tool()
    def apply_material_to_actor(
        ctx: Context,
        actor_name: str,
        material_name: str,
        slot_index: int = 0
    ) -> Dict[str, Any]:
        """
        Apply a material to an actor's mesh components in the level.

        Args:
            actor_name: Name or label of the actor
            material_name: Name of the material or material instance to apply
            slot_index: Material slot index (default 0)
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            params = {"actor_name": actor_name, "material_name": material_name, "slot_index": slot_index}
            return unreal.send_command("apply_material_to_actor", params) or {"success": False, "message": "No response"}
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    @mcp.tool()
    def apply_material_to_blueprint_component(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        material_name: str,
        slot_index: int = 0
    ) -> Dict[str, Any]:
        """
        Apply a material to a component in a Blueprint's construction script.

        Args:
            blueprint_name: Name of the Blueprint
            component_name: Name of the component (variable name)
            material_name: Name of the material or material instance
            slot_index: Material slot index (default 0)
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            params = {
                "blueprint_name": blueprint_name,
                "component_name": component_name,
                "material_name": material_name,
                "slot_index": slot_index
            }
            return unreal.send_command("apply_material_to_blueprint_component", params) or {"success": False, "message": "No response"}
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    @mcp.tool()
    def get_material_stats(ctx: Context, material_name: str) -> Dict[str, Any]:
        """
        Get statistics and analysis of a Material's complexity.

        Returns expression counts, parameter counts, texture sample counts,
        connection counts, connected material inputs, and expression type breakdown.

        Args:
            material_name: Name of the material to analyze
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            return unreal.send_command("get_material_stats", {"material_name": material_name}) or {"success": False, "message": "No response"}
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    logger.info("Material tools registered successfully")
