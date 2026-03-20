#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Material-related MCP commands.
 * Supports creation and manipulation of Materials, Material Instances,
 * Material Functions, and Material Parameter Collections.
 */
class UNREALMCPTOOLS_API FUnrealMCPMaterialCommands
{
public:
    FUnrealMCPMaterialCommands();

    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // ── Tier 1: Core Material CRUD ──────────────────────────────────
    TSharedPtr<FJsonObject> HandleCreateMaterial(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetMaterialProperties(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetMaterialInfo(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeleteMaterial(const TSharedPtr<FJsonObject>& Params);

    // ── Tier 2: Material Expressions / Graph ────────────────────────
    TSharedPtr<FJsonObject> HandleAddMaterialExpression(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleConnectMaterialExpression(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleConnectToMaterialInput(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDisconnectMaterialExpression(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRemoveMaterialExpression(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetMaterialConnections(const TSharedPtr<FJsonObject>& Params);

    // ── Tier 3: Material Instances ──────────────────────────────────
    TSharedPtr<FJsonObject> HandleCreateMaterialInstance(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetMaterialInstanceScalarParam(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetMaterialInstanceVectorParam(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetMaterialInstanceTextureParam(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetMaterialInstanceInfo(const TSharedPtr<FJsonObject>& Params);

    // ── Tier 4: Material Parameter Collections ──────────────────────
    TSharedPtr<FJsonObject> HandleCreateMaterialParameterCollection(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddMPCScalarParameter(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddMPCVectorParameter(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetMPCInfo(const TSharedPtr<FJsonObject>& Params);

    // ── Tier 5: Material Functions ──────────────────────────────────
    TSharedPtr<FJsonObject> HandleCreateMaterialFunction(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddMaterialFunctionInput(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddMaterialFunctionOutput(const TSharedPtr<FJsonObject>& Params);

    // ── Tier 6: Application & Analysis ──────────────────────────────
    TSharedPtr<FJsonObject> HandleApplyMaterialToActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleApplyMaterialToBlueprintComponent(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetMaterialStats(const TSharedPtr<FJsonObject>& Params);

    // ── Helpers ─────────────────────────────────────────────────────
    UMaterialExpression* FindExpressionById(class UMaterialEditorOnlyData* EditorData, const FString& ExpressionId);
};
