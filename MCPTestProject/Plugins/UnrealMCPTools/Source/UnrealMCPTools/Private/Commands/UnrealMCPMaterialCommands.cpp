#include "Commands/UnrealMCPMaterialCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionPower.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Factories/MaterialFunctionFactoryNew.h"
#include "Factories/MaterialParameterCollectionFactoryNew.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "UObject/SavePackage.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "EngineUtils.h"
#include "MaterialEditingLibrary.h"

FUnrealMCPMaterialCommands::FUnrealMCPMaterialCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    // Tier 1: Core Material CRUD
    if (CommandType == TEXT("create_material")) return HandleCreateMaterial(Params);
    if (CommandType == TEXT("set_material_properties")) return HandleSetMaterialProperties(Params);
    if (CommandType == TEXT("get_material_info")) return HandleGetMaterialInfo(Params);
    if (CommandType == TEXT("delete_material")) return HandleDeleteMaterial(Params);

    // Tier 2: Material Expressions / Graph
    if (CommandType == TEXT("add_material_expression")) return HandleAddMaterialExpression(Params);
    if (CommandType == TEXT("connect_material_expression")) return HandleConnectMaterialExpression(Params);
    if (CommandType == TEXT("connect_to_material_input")) return HandleConnectToMaterialInput(Params);
    if (CommandType == TEXT("disconnect_material_expression")) return HandleDisconnectMaterialExpression(Params);
    if (CommandType == TEXT("remove_material_expression")) return HandleRemoveMaterialExpression(Params);
    if (CommandType == TEXT("get_material_connections")) return HandleGetMaterialConnections(Params);

    // Tier 3: Material Instances
    if (CommandType == TEXT("create_material_instance")) return HandleCreateMaterialInstance(Params);
    if (CommandType == TEXT("set_material_instance_scalar_param")) return HandleSetMaterialInstanceScalarParam(Params);
    if (CommandType == TEXT("set_material_instance_vector_param")) return HandleSetMaterialInstanceVectorParam(Params);
    if (CommandType == TEXT("set_material_instance_texture_param")) return HandleSetMaterialInstanceTextureParam(Params);
    if (CommandType == TEXT("get_material_instance_info")) return HandleGetMaterialInstanceInfo(Params);

    // Tier 4: Material Parameter Collections
    if (CommandType == TEXT("create_material_parameter_collection")) return HandleCreateMaterialParameterCollection(Params);
    if (CommandType == TEXT("add_mpc_scalar_parameter")) return HandleAddMPCScalarParameter(Params);
    if (CommandType == TEXT("add_mpc_vector_parameter")) return HandleAddMPCVectorParameter(Params);
    if (CommandType == TEXT("get_mpc_info")) return HandleGetMPCInfo(Params);

    // Tier 5: Material Functions
    if (CommandType == TEXT("create_material_function")) return HandleCreateMaterialFunction(Params);
    if (CommandType == TEXT("add_material_function_input")) return HandleAddMaterialFunctionInput(Params);
    if (CommandType == TEXT("add_material_function_output")) return HandleAddMaterialFunctionOutput(Params);

    // Tier 6: Application & Analysis
    if (CommandType == TEXT("apply_material_to_actor")) return HandleApplyMaterialToActor(Params);
    if (CommandType == TEXT("apply_material_to_blueprint_component")) return HandleApplyMaterialToBlueprintComponent(Params);
    if (CommandType == TEXT("get_material_stats")) return HandleGetMaterialStats(Params);

    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown material command: %s"), *CommandType));
}

// ═══════════════════════════════════════════════════════════════════════
// Helper
// ═══════════════════════════════════════════════════════════════════════

UMaterialExpression* FUnrealMCPMaterialCommands::FindExpressionById(UMaterialEditorOnlyData* EditorData, const FString& ExpressionId)
{
    if (!EditorData) return nullptr;

    for (UMaterialExpression* Expr : EditorData->ExpressionCollection.Expressions)
    {
        if (Expr && Expr->MaterialExpressionGuid.ToString() == ExpressionId)
        {
            return Expr;
        }
    }
    return nullptr;
}

// ═══════════════════════════════════════════════════════════════════════
// Tier 1: Core Material CRUD
// ═══════════════════════════════════════════════════════════════════════

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleCreateMaterial(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialName;
    if (!Params->TryGetStringField(TEXT("material_name"), MaterialName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_name' parameter"));
    }

    FString Path;
    if (!Params->TryGetStringField(TEXT("path"), Path))
    {
        Path = TEXT("/Game/Materials");
    }

    FString PackagePath = FString::Printf(TEXT("%s/%s"), *Path, *MaterialName);

    // Check if already exists
    UMaterial* ExistingMaterial = LoadObject<UMaterial>(nullptr, *FString::Printf(TEXT("%s.%s"), *PackagePath, *MaterialName));
    if (ExistingMaterial)
    {
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("material_name"), MaterialName);
        ResultObj->SetStringField(TEXT("path"), PackagePath);
        ResultObj->SetBoolField(TEXT("already_exists"), true);
        return ResultObj;
    }

    // Create using factory
    UPackage* Package = CreatePackage(*PackagePath);
    if (!Package)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package"));
    }

    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* NewMaterial = Cast<UMaterial>(Factory->FactoryCreateNew(
        UMaterial::StaticClass(), Package, FName(*MaterialName),
        RF_Public | RF_Standalone, nullptr, GWarn));

    if (!NewMaterial)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create material"));
    }

    // Set optional properties
    FString BlendModeStr;
    if (Params->TryGetStringField(TEXT("blend_mode"), BlendModeStr))
    {
        if (BlendModeStr == TEXT("Translucent")) NewMaterial->BlendMode = BLEND_Translucent;
        else if (BlendModeStr == TEXT("Additive")) NewMaterial->BlendMode = BLEND_Additive;
        else if (BlendModeStr == TEXT("Modulate")) NewMaterial->BlendMode = BLEND_Modulate;
        else if (BlendModeStr == TEXT("Masked")) NewMaterial->BlendMode = BLEND_Masked;
        else NewMaterial->BlendMode = BLEND_Opaque;
    }

    FString ShadingModelStr;
    if (Params->TryGetStringField(TEXT("shading_model"), ShadingModelStr))
    {
        if (ShadingModelStr == TEXT("Unlit")) NewMaterial->SetShadingModel(MSM_Unlit);
        else if (ShadingModelStr == TEXT("Subsurface")) NewMaterial->SetShadingModel(MSM_Subsurface);
        else if (ShadingModelStr == TEXT("ClearCoat")) NewMaterial->SetShadingModel(MSM_ClearCoat);
        else NewMaterial->SetShadingModel(MSM_DefaultLit);
    }

    if (Params->HasField(TEXT("two_sided")))
    {
        NewMaterial->TwoSided = Params->GetBoolField(TEXT("two_sided"));
    }

    FString TranslucencyLightingModeStr;
    if (Params->TryGetStringField(TEXT("translucency_lighting_mode"), TranslucencyLightingModeStr))
    {
        if (TranslucencyLightingModeStr == TEXT("Surface")) NewMaterial->TranslucencyLightingMode = TLM_Surface;
        else if (TranslucencyLightingModeStr == TEXT("SurfacePerPixelLighting")) NewMaterial->TranslucencyLightingMode = TLM_SurfacePerPixelLighting;
        else if (TranslucencyLightingModeStr == TEXT("VolumetricDirectional")) NewMaterial->TranslucencyLightingMode = TLM_VolumetricDirectional;
        else NewMaterial->TranslucencyLightingMode = TLM_VolumetricNonDirectional;
    }

    FString RefractionMethodStr;
    if (Params->TryGetStringField(TEXT("refraction_method"), RefractionMethodStr))
    {
        if (RefractionMethodStr == TEXT("IndexOfRefraction")) NewMaterial->RefractionMethod = RM_IndexOfRefraction;
        else if (RefractionMethodStr == TEXT("PixelNormalOffset")) NewMaterial->RefractionMethod = RM_PixelNormalOffset;
        else NewMaterial->RefractionMethod = RM_None;
        // Refraction input requires bUsesDistortion to be enabled
        if (NewMaterial->RefractionMethod != RM_None)
        {
            NewMaterial->bUsesDistortion = true;
        }
    }

    // Save
    FAssetRegistryModule::AssetCreated(NewMaterial);
    NewMaterial->MarkPackageDirty();
    NewMaterial->PostEditChange();

    FString PackageFileName = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    UPackage::SavePackage(Package, NewMaterial, *PackageFileName, SaveArgs);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material_name"), MaterialName);
    ResultObj->SetStringField(TEXT("path"), PackagePath);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleSetMaterialProperties(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialName;
    if (!Params->TryGetStringField(TEXT("material_name"), MaterialName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_name' parameter"));
    }

    // Find material
    FString AssetPath = FString::Printf(TEXT("/Game/Materials/%s.%s"), *MaterialName, *MaterialName);
    UMaterial* Material = LoadObject<UMaterial>(nullptr, *AssetPath);
    if (!Material)
    {
        // Try broader search
        FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        TArray<FAssetData> AssetList;
        ARM.Get().GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Material")), AssetList);
        for (const FAssetData& Asset : AssetList)
        {
            if (Asset.AssetName.ToString() == MaterialName)
            {
                Material = Cast<UMaterial>(Asset.GetAsset());
                break;
            }
        }
    }
    if (!Material)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material '%s' not found"), *MaterialName));
    }

    TArray<FString> PropertiesSet;

    FString BlendModeStr;
    if (Params->TryGetStringField(TEXT("blend_mode"), BlendModeStr))
    {
        if (BlendModeStr == TEXT("Translucent")) Material->BlendMode = BLEND_Translucent;
        else if (BlendModeStr == TEXT("Additive")) Material->BlendMode = BLEND_Additive;
        else if (BlendModeStr == TEXT("Modulate")) Material->BlendMode = BLEND_Modulate;
        else if (BlendModeStr == TEXT("Masked")) Material->BlendMode = BLEND_Masked;
        else Material->BlendMode = BLEND_Opaque;
        PropertiesSet.Add(TEXT("blend_mode"));
    }

    FString ShadingModelStr;
    if (Params->TryGetStringField(TEXT("shading_model"), ShadingModelStr))
    {
        if (ShadingModelStr == TEXT("Unlit")) Material->SetShadingModel(MSM_Unlit);
        else if (ShadingModelStr == TEXT("Subsurface")) Material->SetShadingModel(MSM_Subsurface);
        else if (ShadingModelStr == TEXT("ClearCoat")) Material->SetShadingModel(MSM_ClearCoat);
        else Material->SetShadingModel(MSM_DefaultLit);
        PropertiesSet.Add(TEXT("shading_model"));
    }

    if (Params->HasField(TEXT("two_sided")))
    {
        Material->TwoSided = Params->GetBoolField(TEXT("two_sided"));
        PropertiesSet.Add(TEXT("two_sided"));
    }

    if (Params->HasField(TEXT("opacity_mask_clip_value")))
    {
        Material->OpacityMaskClipValue = static_cast<float>(Params->GetNumberField(TEXT("opacity_mask_clip_value")));
        PropertiesSet.Add(TEXT("opacity_mask_clip_value"));
    }

    FString TranslucencyLightingModeStr;
    if (Params->TryGetStringField(TEXT("translucency_lighting_mode"), TranslucencyLightingModeStr))
    {
        if (TranslucencyLightingModeStr == TEXT("Surface")) Material->TranslucencyLightingMode = TLM_Surface;
        else if (TranslucencyLightingModeStr == TEXT("SurfacePerPixelLighting")) Material->TranslucencyLightingMode = TLM_SurfacePerPixelLighting;
        else if (TranslucencyLightingModeStr == TEXT("VolumetricDirectional")) Material->TranslucencyLightingMode = TLM_VolumetricDirectional;
        else Material->TranslucencyLightingMode = TLM_VolumetricNonDirectional;
        PropertiesSet.Add(TEXT("translucency_lighting_mode"));
    }

    FString RefractionMethodStr;
    if (Params->TryGetStringField(TEXT("refraction_method"), RefractionMethodStr))
    {
        if (RefractionMethodStr == TEXT("IndexOfRefraction")) Material->RefractionMethod = RM_IndexOfRefraction;
        else if (RefractionMethodStr == TEXT("PixelNormalOffset")) Material->RefractionMethod = RM_PixelNormalOffset;
        else Material->RefractionMethod = RM_None;
        // Refraction input requires bUsesDistortion to be enabled
        if (Material->RefractionMethod != RM_None)
        {
            Material->bUsesDistortion = true;
        }
        PropertiesSet.Add(TEXT("refraction_method"));
    }

    Material->PreEditChange(nullptr);
    Material->PostEditChange();
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material_name"), MaterialName);

    TArray<TSharedPtr<FJsonValue>> PropsArray;
    for (const FString& Prop : PropertiesSet)
    {
        PropsArray.Add(MakeShared<FJsonValueString>(Prop));
    }
    ResultObj->SetArrayField(TEXT("properties_set"), PropsArray);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleGetMaterialInfo(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialName;
    if (!Params->TryGetStringField(TEXT("material_name"), MaterialName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_name' parameter"));
    }

    // Search asset registry for Material, MaterialFunction, MaterialInstanceConstant, MaterialParameterCollection
    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    FAssetData FoundAsset;
    bool bFound = false;

    // Asset class types to search (in order of priority)
    struct FAssetClassInfo
    {
        const TCHAR* ScriptPath;
        const TCHAR* ClassName;
    };
    const FAssetClassInfo ClassesToSearch[] = {
        { TEXT("/Script/Engine"), TEXT("Material") },
        { TEXT("/Script/Engine"), TEXT("MaterialFunction") },
        { TEXT("/Script/Engine"), TEXT("MaterialInstanceConstant") },
        { TEXT("/Script/Engine"), TEXT("MaterialParameterCollection") },
    };

    for (const FAssetClassInfo& ClassInfo : ClassesToSearch)
    {
        TArray<FAssetData> AssetList;
        ARM.Get().GetAssetsByClass(FTopLevelAssetPath(ClassInfo.ScriptPath, ClassInfo.ClassName), AssetList);
        for (const FAssetData& Asset : AssetList)
        {
            if (Asset.AssetName.ToString() == MaterialName)
            {
                FoundAsset = Asset;
                bFound = true;
                break;
            }
        }
        if (bFound) break;
    }

    if (!bFound)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material '%s' not found"), *MaterialName));
    }

    UObject* AssetObj = FoundAsset.GetAsset();
    if (!AssetObj)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load asset '%s'"), *MaterialName));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("name"), AssetObj->GetName());
    ResultObj->SetStringField(TEXT("path"), AssetObj->GetPathName());
    ResultObj->SetStringField(TEXT("asset_class"), AssetObj->GetClass()->GetName());

    // ── UMaterial ──
    if (UMaterial* Material = Cast<UMaterial>(AssetObj))
    {
        ResultObj->SetStringField(TEXT("asset_type"), TEXT("Material"));

        // Blend mode
        FString BlendModeStr;
        switch (Material->BlendMode)
        {
            case BLEND_Opaque: BlendModeStr = TEXT("Opaque"); break;
            case BLEND_Translucent: BlendModeStr = TEXT("Translucent"); break;
            case BLEND_Additive: BlendModeStr = TEXT("Additive"); break;
            case BLEND_Modulate: BlendModeStr = TEXT("Modulate"); break;
            case BLEND_Masked: BlendModeStr = TEXT("Masked"); break;
            default: BlendModeStr = TEXT("Unknown"); break;
        }
        ResultObj->SetStringField(TEXT("blend_mode"), BlendModeStr);

        // Shading model
        FString ShadingModelStr;
        EMaterialShadingModel SM = Material->GetShadingModels().GetFirstShadingModel();
        switch (SM)
        {
            case MSM_DefaultLit: ShadingModelStr = TEXT("DefaultLit"); break;
            case MSM_Unlit: ShadingModelStr = TEXT("Unlit"); break;
            case MSM_Subsurface: ShadingModelStr = TEXT("Subsurface"); break;
            case MSM_ClearCoat: ShadingModelStr = TEXT("ClearCoat"); break;
            default: ShadingModelStr = TEXT("Other"); break;
        }
        ResultObj->SetStringField(TEXT("shading_model"), ShadingModelStr);
        ResultObj->SetBoolField(TEXT("two_sided"), Material->TwoSided != 0);

#if WITH_EDITORONLY_DATA
        UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData();
        if (EditorData)
        {
            int32 ExprCount = EditorData->ExpressionCollection.Expressions.Num();
            ResultObj->SetNumberField(TEXT("num_expressions"), ExprCount);

            // List expressions
            TArray<TSharedPtr<FJsonValue>> ExprsArray;
            for (UMaterialExpression* Expr : EditorData->ExpressionCollection.Expressions)
            {
                if (!Expr) continue;
                TSharedPtr<FJsonObject> ExprObj = MakeShared<FJsonObject>();
                ExprObj->SetStringField(TEXT("id"), Expr->MaterialExpressionGuid.ToString());
                ExprObj->SetStringField(TEXT("class"), Expr->GetClass()->GetName());
                ExprObj->SetNumberField(TEXT("x"), Expr->MaterialExpressionEditorX);
                ExprObj->SetNumberField(TEXT("y"), Expr->MaterialExpressionEditorY);

                // Include parameter name if it's a parameter expression
                if (UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>(Expr))
                {
                    ExprObj->SetStringField(TEXT("parameter_name"), ScalarParam->ParameterName.ToString());
                    ExprObj->SetNumberField(TEXT("default_value"), ScalarParam->DefaultValue);
                }
                else if (UMaterialExpressionVectorParameter* VectorParam = Cast<UMaterialExpressionVectorParameter>(Expr))
                {
                    ExprObj->SetStringField(TEXT("parameter_name"), VectorParam->ParameterName.ToString());
                    TSharedPtr<FJsonObject> ColorObj = MakeShared<FJsonObject>();
                    ColorObj->SetNumberField(TEXT("r"), VectorParam->DefaultValue.R);
                    ColorObj->SetNumberField(TEXT("g"), VectorParam->DefaultValue.G);
                    ColorObj->SetNumberField(TEXT("b"), VectorParam->DefaultValue.B);
                    ColorObj->SetNumberField(TEXT("a"), VectorParam->DefaultValue.A);
                    ExprObj->SetObjectField(TEXT("default_value"), ColorObj);
                }

                ExprsArray.Add(MakeShared<FJsonValueObject>(ExprObj));
            }
            ResultObj->SetArrayField(TEXT("expressions"), ExprsArray);
        }
#endif
    }
    // ── UMaterialFunction ──
    else if (UMaterialFunction* MatFunc = Cast<UMaterialFunction>(AssetObj))
    {
        ResultObj->SetStringField(TEXT("asset_type"), TEXT("MaterialFunction"));
        ResultObj->SetStringField(TEXT("description"), MatFunc->Description);

#if WITH_EDITORONLY_DATA
        TConstArrayView<TObjectPtr<UMaterialExpression>> FuncExprs = MatFunc->GetExpressions();
        {
            ResultObj->SetNumberField(TEXT("num_expressions"), FuncExprs.Num());

            TArray<TSharedPtr<FJsonValue>> ExprsArray;
            int32 InputCount = 0;
            int32 OutputCount = 0;
            for (UMaterialExpression* Expr : FuncExprs)
            {
                if (!Expr) continue;
                TSharedPtr<FJsonObject> ExprObj = MakeShared<FJsonObject>();
                ExprObj->SetStringField(TEXT("id"), Expr->MaterialExpressionGuid.ToString());
                ExprObj->SetStringField(TEXT("class"), Expr->GetClass()->GetName());
                ExprObj->SetNumberField(TEXT("x"), Expr->MaterialExpressionEditorX);
                ExprObj->SetNumberField(TEXT("y"), Expr->MaterialExpressionEditorY);

                if (UMaterialExpressionFunctionInput* FuncInput = Cast<UMaterialExpressionFunctionInput>(Expr))
                {
                    ExprObj->SetStringField(TEXT("input_name"), FuncInput->InputName.ToString());
                    InputCount++;
                }
                else if (UMaterialExpressionFunctionOutput* FuncOutput = Cast<UMaterialExpressionFunctionOutput>(Expr))
                {
                    ExprObj->SetStringField(TEXT("output_name"), FuncOutput->OutputName.ToString());
                    OutputCount++;
                }
                else if (UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>(Expr))
                {
                    ExprObj->SetStringField(TEXT("parameter_name"), ScalarParam->ParameterName.ToString());
                    ExprObj->SetNumberField(TEXT("default_value"), ScalarParam->DefaultValue);
                }
                else if (UMaterialExpressionVectorParameter* VectorParam = Cast<UMaterialExpressionVectorParameter>(Expr))
                {
                    ExprObj->SetStringField(TEXT("parameter_name"), VectorParam->ParameterName.ToString());
                    TSharedPtr<FJsonObject> ColorObj = MakeShared<FJsonObject>();
                    ColorObj->SetNumberField(TEXT("r"), VectorParam->DefaultValue.R);
                    ColorObj->SetNumberField(TEXT("g"), VectorParam->DefaultValue.G);
                    ColorObj->SetNumberField(TEXT("b"), VectorParam->DefaultValue.B);
                    ColorObj->SetNumberField(TEXT("a"), VectorParam->DefaultValue.A);
                    ExprObj->SetObjectField(TEXT("default_value"), ColorObj);
                }

                ExprsArray.Add(MakeShared<FJsonValueObject>(ExprObj));
            }
            ResultObj->SetArrayField(TEXT("expressions"), ExprsArray);
            ResultObj->SetNumberField(TEXT("num_inputs"), InputCount);
            ResultObj->SetNumberField(TEXT("num_outputs"), OutputCount);
        }
#endif
    }
    // ── UMaterialInstanceConstant ──
    else if (UMaterialInstanceConstant* MatInst = Cast<UMaterialInstanceConstant>(AssetObj))
    {
        ResultObj->SetStringField(TEXT("asset_type"), TEXT("MaterialInstance"));
        if (MatInst->Parent)
        {
            ResultObj->SetStringField(TEXT("parent_material"), MatInst->Parent->GetName());
            ResultObj->SetStringField(TEXT("parent_path"), MatInst->Parent->GetPathName());
        }

        // Scalar parameters
        TArray<TSharedPtr<FJsonValue>> ScalarParams;
        for (const FScalarParameterValue& Param : MatInst->ScalarParameterValues)
        {
            TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
            ParamObj->SetStringField(TEXT("name"), Param.ParameterInfo.Name.ToString());
            ParamObj->SetNumberField(TEXT("value"), Param.ParameterValue);
            ScalarParams.Add(MakeShared<FJsonValueObject>(ParamObj));
        }
        ResultObj->SetArrayField(TEXT("scalar_parameters"), ScalarParams);

        // Vector parameters
        TArray<TSharedPtr<FJsonValue>> VectorParams;
        for (const FVectorParameterValue& Param : MatInst->VectorParameterValues)
        {
            TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
            ParamObj->SetStringField(TEXT("name"), Param.ParameterInfo.Name.ToString());
            TSharedPtr<FJsonObject> ColorObj = MakeShared<FJsonObject>();
            ColorObj->SetNumberField(TEXT("r"), Param.ParameterValue.R);
            ColorObj->SetNumberField(TEXT("g"), Param.ParameterValue.G);
            ColorObj->SetNumberField(TEXT("b"), Param.ParameterValue.B);
            ColorObj->SetNumberField(TEXT("a"), Param.ParameterValue.A);
            ParamObj->SetObjectField(TEXT("value"), ColorObj);
            VectorParams.Add(MakeShared<FJsonValueObject>(ParamObj));
        }
        ResultObj->SetArrayField(TEXT("vector_parameters"), VectorParams);
    }
    // ── UMaterialParameterCollection ──
    else if (UMaterialParameterCollection* MPC = Cast<UMaterialParameterCollection>(AssetObj))
    {
        ResultObj->SetStringField(TEXT("asset_type"), TEXT("MaterialParameterCollection"));

        TArray<TSharedPtr<FJsonValue>> ScalarParams;
        for (const FCollectionScalarParameter& Param : MPC->ScalarParameters)
        {
            TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
            ParamObj->SetStringField(TEXT("name"), Param.ParameterName.ToString());
            ParamObj->SetNumberField(TEXT("default_value"), Param.DefaultValue);
            ScalarParams.Add(MakeShared<FJsonValueObject>(ParamObj));
        }
        ResultObj->SetArrayField(TEXT("scalar_parameters"), ScalarParams);

        TArray<TSharedPtr<FJsonValue>> VectorParams;
        for (const FCollectionVectorParameter& Param : MPC->VectorParameters)
        {
            TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
            ParamObj->SetStringField(TEXT("name"), Param.ParameterName.ToString());
            TSharedPtr<FJsonObject> ColorObj = MakeShared<FJsonObject>();
            ColorObj->SetNumberField(TEXT("r"), Param.DefaultValue.R);
            ColorObj->SetNumberField(TEXT("g"), Param.DefaultValue.G);
            ColorObj->SetNumberField(TEXT("b"), Param.DefaultValue.B);
            ColorObj->SetNumberField(TEXT("a"), Param.DefaultValue.A);
            ParamObj->SetObjectField(TEXT("default_value"), ColorObj);
            VectorParams.Add(MakeShared<FJsonValueObject>(ParamObj));
        }
        ResultObj->SetArrayField(TEXT("vector_parameters"), VectorParams);
    }

    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleDeleteMaterial(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialName;
    if (!Params->TryGetStringField(TEXT("material_name"), MaterialName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_name' parameter"));
    }

    // Find the asset
    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    TArray<FAssetData> AssetList;
    ARM.Get().GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Material")), AssetList);

    FString AssetPath;
    for (const FAssetData& Asset : AssetList)
    {
        if (Asset.AssetName.ToString() == MaterialName)
        {
            AssetPath = Asset.GetObjectPathString();
            break;
        }
    }

    if (AssetPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material '%s' not found"), *MaterialName));
    }

    // Extract package path from object path (e.g., "/Game/Materials/M_Test.M_Test" -> "/Game/Materials/M_Test")
    FString PackagePath = FPackageName::ObjectPathToPackageName(AssetPath);
    bool bDeleted = UEditorAssetLibrary::DeleteAsset(PackagePath);
    if (!bDeleted)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to delete material '%s'"), *MaterialName));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material_name"), MaterialName);
    ResultObj->SetBoolField(TEXT("deleted"), true);
    return ResultObj;
}

// ═══════════════════════════════════════════════════════════════════════
// Tier 2: Material Expressions / Graph
// ═══════════════════════════════════════════════════════════════════════

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleAddMaterialExpression(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialName;
    if (!Params->TryGetStringField(TEXT("material_name"), MaterialName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_name' parameter"));
    }

    FString ExpressionType;
    if (!Params->TryGetStringField(TEXT("expression_type"), ExpressionType))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'expression_type' parameter"));
    }

    // Find material
    UMaterial* Material = nullptr;
    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    TArray<FAssetData> AssetList;
    ARM.Get().GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Material")), AssetList);
    for (const FAssetData& Asset : AssetList)
    {
        if (Asset.AssetName.ToString() == MaterialName)
        {
            Material = Cast<UMaterial>(Asset.GetAsset());
            break;
        }
    }
    if (!Material)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material '%s' not found"), *MaterialName));
    }

#if WITH_EDITORONLY_DATA
    UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData();
    if (!EditorData)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Cannot access material editor data"));
    }

    // Map expression type string to UClass
    UClass* ExprClass = nullptr;
    // Parameter types
    if (ExpressionType == TEXT("ScalarParameter")) ExprClass = UMaterialExpressionScalarParameter::StaticClass();
    else if (ExpressionType == TEXT("VectorParameter")) ExprClass = UMaterialExpressionVectorParameter::StaticClass();
    else if (ExpressionType == TEXT("TextureSampleParameter2D")) ExprClass = UMaterialExpressionTextureSampleParameter2D::StaticClass();
    // Constant types
    else if (ExpressionType == TEXT("Constant")) ExprClass = UMaterialExpressionConstant::StaticClass();
    else if (ExpressionType == TEXT("Constant3Vector")) ExprClass = UMaterialExpressionConstant3Vector::StaticClass();
    else if (ExpressionType == TEXT("Constant4Vector")) ExprClass = UMaterialExpressionConstant4Vector::StaticClass();
    // Texture
    else if (ExpressionType == TEXT("TextureSample")) ExprClass = UMaterialExpressionTextureSample::StaticClass();
    // Math
    else if (ExpressionType == TEXT("Add")) ExprClass = UMaterialExpressionAdd::StaticClass();
    else if (ExpressionType == TEXT("Multiply")) ExprClass = UMaterialExpressionMultiply::StaticClass();
    else if (ExpressionType == TEXT("LinearInterpolate") || ExpressionType == TEXT("Lerp")) ExprClass = UMaterialExpressionLinearInterpolate::StaticClass();
    else if (ExpressionType == TEXT("Clamp")) ExprClass = UMaterialExpressionClamp::StaticClass();
    else if (ExpressionType == TEXT("OneMinus")) ExprClass = UMaterialExpressionOneMinus::StaticClass();
    else if (ExpressionType == TEXT("Power")) ExprClass = UMaterialExpressionPower::StaticClass();
    else if (ExpressionType == TEXT("Sine") || ExpressionType == TEXT("Sin")) ExprClass = UMaterialExpressionSine::StaticClass();
    else if (ExpressionType == TEXT("AppendVector") || ExpressionType == TEXT("Append")) ExprClass = UMaterialExpressionAppendVector::StaticClass();
    // Utility
    else if (ExpressionType == TEXT("Panner")) ExprClass = UMaterialExpressionPanner::StaticClass();
    else if (ExpressionType == TEXT("TextureCoordinate") || ExpressionType == TEXT("TexCoord")) ExprClass = UMaterialExpressionTextureCoordinate::StaticClass();
    else if (ExpressionType == TEXT("Time")) ExprClass = UMaterialExpressionTime::StaticClass();
    else if (ExpressionType == TEXT("Fresnel")) ExprClass = UMaterialExpressionFresnel::StaticClass();
    else if (ExpressionType == TEXT("MaterialFunctionCall")) ExprClass = UMaterialExpressionMaterialFunctionCall::StaticClass();
    else
    {
        // Try to find by full class name
        FString FullClassName = FString::Printf(TEXT("MaterialExpression%s"), *ExpressionType);
        ExprClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.U%s"), *FullClassName));
        if (!ExprClass)
        {
            ExprClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.UMaterialExpression%s"), *ExpressionType));
        }
    }

    if (!ExprClass)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown expression type: %s"), *ExpressionType));
    }

    // Get position
    int32 NodePosX = 0, NodePosY = 0;
    if (Params->HasField(TEXT("x")))
    {
        NodePosX = static_cast<int32>(Params->GetNumberField(TEXT("x")));
    }
    if (Params->HasField(TEXT("y")))
    {
        NodePosY = static_cast<int32>(Params->GetNumberField(TEXT("y")));
    }

    // Use UMaterialEditingLibrary to create expression (creates graph node too)
    UMaterialExpression* NewExpr = UMaterialEditingLibrary::CreateMaterialExpression(Material, ExprClass, NodePosX, NodePosY);
    if (!NewExpr)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create material expression"));
    }

    // Set type-specific properties
    FString ParameterName;
    if (Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
    {
        if (UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>(NewExpr))
        {
            ScalarParam->ParameterName = FName(*ParameterName);
            if (Params->HasField(TEXT("default_value")))
            {
                ScalarParam->DefaultValue = static_cast<float>(Params->GetNumberField(TEXT("default_value")));
            }
        }
        else if (UMaterialExpressionVectorParameter* VectorParam = Cast<UMaterialExpressionVectorParameter>(NewExpr))
        {
            VectorParam->ParameterName = FName(*ParameterName);
            const TArray<TSharedPtr<FJsonValue>>* ColorArray;
            if (Params->TryGetArrayField(TEXT("default_value"), ColorArray) && ColorArray->Num() >= 3)
            {
                VectorParam->DefaultValue.R = static_cast<float>((*ColorArray)[0]->AsNumber());
                VectorParam->DefaultValue.G = static_cast<float>((*ColorArray)[1]->AsNumber());
                VectorParam->DefaultValue.B = static_cast<float>((*ColorArray)[2]->AsNumber());
                if (ColorArray->Num() >= 4)
                {
                    VectorParam->DefaultValue.A = static_cast<float>((*ColorArray)[3]->AsNumber());
                }
            }
        }
        else if (UMaterialExpressionTextureSampleParameter2D* TexParam = Cast<UMaterialExpressionTextureSampleParameter2D>(NewExpr))
        {
            TexParam->ParameterName = FName(*ParameterName);
        }
    }

    // Set constant value
    if (UMaterialExpressionConstant* ConstExpr = Cast<UMaterialExpressionConstant>(NewExpr))
    {
        if (Params->HasField(TEXT("value")))
        {
            ConstExpr->R = static_cast<float>(Params->GetNumberField(TEXT("value")));
        }
    }
    else if (UMaterialExpressionConstant3Vector* Const3Expr = Cast<UMaterialExpressionConstant3Vector>(NewExpr))
    {
        const TArray<TSharedPtr<FJsonValue>>* ColorArray;
        if (Params->TryGetArrayField(TEXT("value"), ColorArray) && ColorArray->Num() >= 3)
        {
            Const3Expr->Constant.R = static_cast<float>((*ColorArray)[0]->AsNumber());
            Const3Expr->Constant.G = static_cast<float>((*ColorArray)[1]->AsNumber());
            Const3Expr->Constant.B = static_cast<float>((*ColorArray)[2]->AsNumber());
        }
    }

    // Set texture
    FString TexturePath;
    if (Params->TryGetStringField(TEXT("texture_path"), TexturePath))
    {
        UTexture* Texture = LoadObject<UTexture>(nullptr, *TexturePath);
        if (Texture)
        {
            if (UMaterialExpressionTextureSample* TexSample = Cast<UMaterialExpressionTextureSample>(NewExpr))
            {
                TexSample->Texture = Texture;
            }
        }
    }

    // Set Sine period
    if (UMaterialExpressionSine* SineExpr = Cast<UMaterialExpressionSine>(NewExpr))
    {
        if (Params->HasField(TEXT("period")))
        {
            SineExpr->Period = static_cast<float>(Params->GetNumberField(TEXT("period")));
        }
    }

    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material_name"), MaterialName);
    ResultObj->SetStringField(TEXT("expression_type"), ExpressionType);
    ResultObj->SetStringField(TEXT("expression_id"), NewExpr->MaterialExpressionGuid.ToString());
    return ResultObj;
#else
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only feature"));
#endif
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleConnectMaterialExpression(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialName;
    if (!Params->TryGetStringField(TEXT("material_name"), MaterialName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_name' parameter"));
    }

    FString SourceId, TargetId;
    if (!Params->TryGetStringField(TEXT("source_expression_id"), SourceId))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_expression_id' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("target_expression_id"), TargetId))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'target_expression_id' parameter"));
    }

    int32 SourceOutputIndex = 0;
    if (Params->HasField(TEXT("source_output_index")))
    {
        SourceOutputIndex = static_cast<int32>(Params->GetNumberField(TEXT("source_output_index")));
    }

    int32 TargetInputIndex = 0;
    if (Params->HasField(TEXT("target_input_index")))
    {
        TargetInputIndex = static_cast<int32>(Params->GetNumberField(TEXT("target_input_index")));
    }

    // Find material
    UMaterial* Material = nullptr;
    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    TArray<FAssetData> AssetList;
    ARM.Get().GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Material")), AssetList);
    for (const FAssetData& Asset : AssetList)
    {
        if (Asset.AssetName.ToString() == MaterialName)
        {
            Material = Cast<UMaterial>(Asset.GetAsset());
            break;
        }
    }
    if (!Material)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material '%s' not found"), *MaterialName));
    }

#if WITH_EDITORONLY_DATA
    UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData();
    if (!EditorData)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Cannot access material editor data"));
    }

    UMaterialExpression* SourceExpr = FindExpressionById(EditorData, SourceId);
    UMaterialExpression* TargetExpr = FindExpressionById(EditorData, TargetId);

    if (!SourceExpr)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Source expression '%s' not found"), *SourceId));
    }
    if (!TargetExpr)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Target expression '%s' not found"), *TargetId));
    }

    // Get output name from source expression by index
    FString OutputName;
    TArray<FExpressionOutput>& Outputs = SourceExpr->GetOutputs();
    if (SourceOutputIndex < Outputs.Num())
    {
        OutputName = Outputs[SourceOutputIndex].OutputName.ToString();
    }

    // Get input name from target expression by index
    FString InputName = TargetExpr->GetInputName(TargetInputIndex).ToString();

    // Use UMaterialEditingLibrary to make the connection (updates graph nodes)
    bool bConnected = UMaterialEditingLibrary::ConnectMaterialExpressions(SourceExpr, OutputName, TargetExpr, InputName);
    if (!bConnected)
    {
        // Fallback: try direct connection if name-based connection fails
        FExpressionInput* TargetInput = TargetExpr->GetInput(TargetInputIndex);
        if (TargetInput)
        {
            SourceExpr->ConnectExpression(TargetInput, SourceOutputIndex);
            bConnected = true;
        }
    }

    Material->PreEditChange(nullptr);
    Material->PostEditChange();
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material_name"), MaterialName);
    ResultObj->SetBoolField(TEXT("connected"), bConnected);
    ResultObj->SetStringField(TEXT("source_expression_id"), SourceId);
    ResultObj->SetStringField(TEXT("target_expression_id"), TargetId);
    return ResultObj;
#else
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only feature"));
#endif
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleConnectToMaterialInput(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialName;
    if (!Params->TryGetStringField(TEXT("material_name"), MaterialName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_name' parameter"));
    }

    FString ExpressionId;
    if (!Params->TryGetStringField(TEXT("expression_id"), ExpressionId))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'expression_id' parameter"));
    }

    FString MaterialInput;
    if (!Params->TryGetStringField(TEXT("material_input"), MaterialInput))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_input' parameter (e.g. BaseColor, Metallic, Roughness, Normal, EmissiveColor, Opacity, OpacityMask)"));
    }

    int32 OutputIndex = 0;
    if (Params->HasField(TEXT("output_index")))
    {
        OutputIndex = static_cast<int32>(Params->GetNumberField(TEXT("output_index")));
    }

    // Find material
    UMaterial* Material = nullptr;
    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    TArray<FAssetData> AssetList;
    ARM.Get().GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Material")), AssetList);
    for (const FAssetData& Asset : AssetList)
    {
        if (Asset.AssetName.ToString() == MaterialName)
        {
            Material = Cast<UMaterial>(Asset.GetAsset());
            break;
        }
    }
    if (!Material)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material '%s' not found"), *MaterialName));
    }

#if WITH_EDITORONLY_DATA
    UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData();
    if (!EditorData)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Cannot access material editor data"));
    }

    UMaterialExpression* Expr = FindExpressionById(EditorData, ExpressionId);
    if (!Expr)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Expression '%s' not found"), *ExpressionId));
    }

    // Map material_input string to EMaterialProperty
    EMaterialProperty Property;
    if (MaterialInput == TEXT("BaseColor")) Property = MP_BaseColor;
    else if (MaterialInput == TEXT("Metallic")) Property = MP_Metallic;
    else if (MaterialInput == TEXT("Specular")) Property = MP_Specular;
    else if (MaterialInput == TEXT("Roughness")) Property = MP_Roughness;
    else if (MaterialInput == TEXT("Normal")) Property = MP_Normal;
    else if (MaterialInput == TEXT("EmissiveColor")) Property = MP_EmissiveColor;
    else if (MaterialInput == TEXT("Opacity")) Property = MP_Opacity;
    else if (MaterialInput == TEXT("OpacityMask")) Property = MP_OpacityMask;
    else if (MaterialInput == TEXT("WorldPositionOffset")) Property = MP_WorldPositionOffset;
    else if (MaterialInput == TEXT("AmbientOcclusion")) Property = MP_AmbientOcclusion;
    else if (MaterialInput == TEXT("Refraction")) Property = MP_Refraction;
    else if (MaterialInput == TEXT("SubsurfaceColor")) Property = MP_SubsurfaceColor;
    else if (MaterialInput == TEXT("Anisotropy")) Property = MP_Anisotropy;
    else if (MaterialInput == TEXT("Tangent")) Property = MP_Tangent;
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown material input: %s"), *MaterialInput));
    }

    // Get output name from expression by index
    FString OutputName;
    TArray<FExpressionOutput>& Outputs = Expr->GetOutputs();
    if (OutputIndex < Outputs.Num())
    {
        OutputName = Outputs[OutputIndex].OutputName.ToString();
    }

    // Use UMaterialEditingLibrary to connect to material property (updates graph nodes)
    bool bConnected = UMaterialEditingLibrary::ConnectMaterialProperty(Expr, OutputName, Property);
    if (!bConnected)
    {
        // Fallback: direct connection
        FExpressionInput* Input = nullptr;
        if (MaterialInput == TEXT("BaseColor")) Input = &EditorData->BaseColor;
        else if (MaterialInput == TEXT("Metallic")) Input = &EditorData->Metallic;
        else if (MaterialInput == TEXT("Specular")) Input = &EditorData->Specular;
        else if (MaterialInput == TEXT("Roughness")) Input = &EditorData->Roughness;
        else if (MaterialInput == TEXT("Normal")) Input = &EditorData->Normal;
        else if (MaterialInput == TEXT("EmissiveColor")) Input = &EditorData->EmissiveColor;
        else if (MaterialInput == TEXT("Opacity")) Input = &EditorData->Opacity;
        else if (MaterialInput == TEXT("OpacityMask")) Input = &EditorData->OpacityMask;
        else if (MaterialInput == TEXT("AmbientOcclusion")) Input = &EditorData->AmbientOcclusion;
        else if (MaterialInput == TEXT("Refraction")) Input = &EditorData->Refraction;

        if (Input)
        {
            Expr->ConnectExpression(Input, OutputIndex);
            bConnected = true;
        }
    }

    Material->PreEditChange(nullptr);
    Material->PostEditChange();
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material_name"), MaterialName);
    ResultObj->SetStringField(TEXT("expression_id"), ExpressionId);
    ResultObj->SetStringField(TEXT("material_input"), MaterialInput);
    ResultObj->SetBoolField(TEXT("connected"), bConnected);
    return ResultObj;
#else
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only feature"));
#endif
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleDisconnectMaterialExpression(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialName;
    if (!Params->TryGetStringField(TEXT("material_name"), MaterialName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_name' parameter"));
    }

    // Find material
    UMaterial* Material = nullptr;
    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    TArray<FAssetData> AssetList;
    ARM.Get().GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Material")), AssetList);
    for (const FAssetData& Asset : AssetList)
    {
        if (Asset.AssetName.ToString() == MaterialName)
        {
            Material = Cast<UMaterial>(Asset.GetAsset());
            break;
        }
    }
    if (!Material)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material '%s' not found"), *MaterialName));
    }

#if WITH_EDITORONLY_DATA
    UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData();
    if (!EditorData)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Cannot access material editor data"));
    }

    // Can disconnect either a material input or an expression-to-expression connection
    FString MaterialInput;
    if (Params->TryGetStringField(TEXT("material_input"), MaterialInput))
    {
        // Disconnect a material input
        FExpressionInput* Input = nullptr;
        if (MaterialInput == TEXT("BaseColor")) Input = &EditorData->BaseColor;
        else if (MaterialInput == TEXT("Metallic")) Input = &EditorData->Metallic;
        else if (MaterialInput == TEXT("Specular")) Input = &EditorData->Specular;
        else if (MaterialInput == TEXT("Roughness")) Input = &EditorData->Roughness;
        else if (MaterialInput == TEXT("Normal")) Input = &EditorData->Normal;
        else if (MaterialInput == TEXT("EmissiveColor")) Input = &EditorData->EmissiveColor;
        else if (MaterialInput == TEXT("Opacity")) Input = &EditorData->Opacity;
        else if (MaterialInput == TEXT("OpacityMask")) Input = &EditorData->OpacityMask;
        else if (MaterialInput == TEXT("AmbientOcclusion")) Input = &EditorData->AmbientOcclusion;

        if (!Input)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown material input: %s"), *MaterialInput));
        }

        Input->Expression = nullptr;
        Input->OutputIndex = 0;
    }
    else
    {
        // Disconnect an expression input
        FString TargetId;
        if (!Params->TryGetStringField(TEXT("target_expression_id"), TargetId))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_input' or 'target_expression_id' parameter"));
        }

        int32 TargetInputIndex = 0;
        if (Params->HasField(TEXT("target_input_index")))
        {
            TargetInputIndex = static_cast<int32>(Params->GetNumberField(TEXT("target_input_index")));
        }

        UMaterialExpression* TargetExpr = FindExpressionById(EditorData, TargetId);
        if (!TargetExpr)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Target expression '%s' not found"), *TargetId));
        }

        FExpressionInput* DisconnInput = TargetExpr->GetInput(TargetInputIndex);
        if (DisconnInput)
        {
            DisconnInput->Expression = nullptr;
            DisconnInput->OutputIndex = 0;
        }
    }

    Material->PostEditChange();
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material_name"), MaterialName);
    ResultObj->SetBoolField(TEXT("disconnected"), true);
    return ResultObj;
#else
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only feature"));
#endif
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleRemoveMaterialExpression(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialName;
    if (!Params->TryGetStringField(TEXT("material_name"), MaterialName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_name' parameter"));
    }

    FString ExpressionId;
    if (!Params->TryGetStringField(TEXT("expression_id"), ExpressionId))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'expression_id' parameter"));
    }

    UMaterial* Material = nullptr;
    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    TArray<FAssetData> AssetList;
    ARM.Get().GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Material")), AssetList);
    for (const FAssetData& Asset : AssetList)
    {
        if (Asset.AssetName.ToString() == MaterialName)
        {
            Material = Cast<UMaterial>(Asset.GetAsset());
            break;
        }
    }
    if (!Material)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material '%s' not found"), *MaterialName));
    }

#if WITH_EDITORONLY_DATA
    UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData();
    if (!EditorData)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Cannot access material editor data"));
    }

    UMaterialExpression* Expr = FindExpressionById(EditorData, ExpressionId);
    if (!Expr)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Expression '%s' not found"), *ExpressionId));
    }

    UMaterialEditingLibrary::DeleteMaterialExpression(Material, Expr);
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material_name"), MaterialName);
    ResultObj->SetStringField(TEXT("expression_id"), ExpressionId);
    ResultObj->SetBoolField(TEXT("removed"), true);
    return ResultObj;
#else
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only feature"));
#endif
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleGetMaterialConnections(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialName;
    if (!Params->TryGetStringField(TEXT("material_name"), MaterialName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_name' parameter"));
    }

    UMaterial* Material = nullptr;
    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    TArray<FAssetData> AssetList;
    ARM.Get().GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Material")), AssetList);
    for (const FAssetData& Asset : AssetList)
    {
        if (Asset.AssetName.ToString() == MaterialName)
        {
            Material = Cast<UMaterial>(Asset.GetAsset());
            break;
        }
    }
    if (!Material)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material '%s' not found"), *MaterialName));
    }

#if WITH_EDITORONLY_DATA
    UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData();
    if (!EditorData)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Cannot access material editor data"));
    }

    TArray<TSharedPtr<FJsonValue>> ConnectionsArray;

    // Check material inputs
    auto AddMaterialInputConnection = [&](const FString& InputName, const FExpressionInput& Input)
    {
        if (Input.Expression)
        {
            TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
            ConnObj->SetStringField(TEXT("source_expression_id"), Input.Expression->MaterialExpressionGuid.ToString());
            ConnObj->SetStringField(TEXT("source_class"), Input.Expression->GetClass()->GetName());
            ConnObj->SetNumberField(TEXT("source_output_index"), Input.OutputIndex);
            ConnObj->SetStringField(TEXT("target"), InputName);
            ConnObj->SetStringField(TEXT("target_type"), TEXT("material_input"));
            ConnectionsArray.Add(MakeShared<FJsonValueObject>(ConnObj));
        }
    };

    AddMaterialInputConnection(TEXT("BaseColor"), EditorData->BaseColor);
    AddMaterialInputConnection(TEXT("Metallic"), EditorData->Metallic);
    AddMaterialInputConnection(TEXT("Specular"), EditorData->Specular);
    AddMaterialInputConnection(TEXT("Roughness"), EditorData->Roughness);
    AddMaterialInputConnection(TEXT("Normal"), EditorData->Normal);
    AddMaterialInputConnection(TEXT("EmissiveColor"), EditorData->EmissiveColor);
    AddMaterialInputConnection(TEXT("Opacity"), EditorData->Opacity);
    AddMaterialInputConnection(TEXT("OpacityMask"), EditorData->OpacityMask);
    AddMaterialInputConnection(TEXT("AmbientOcclusion"), EditorData->AmbientOcclusion);
    AddMaterialInputConnection(TEXT("SubsurfaceColor"), EditorData->SubsurfaceColor);
    AddMaterialInputConnection(TEXT("Refraction"), EditorData->Refraction);

    // Check expression-to-expression connections
    for (UMaterialExpression* Expr : EditorData->ExpressionCollection.Expressions)
    {
        if (!Expr) continue;

        for (int32 i = 0; ; ++i)
        {
            FExpressionInput* Input = Expr->GetInput(i);
            if (!Input) break;
            if (Input->Expression)
            {
                TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
                ConnObj->SetStringField(TEXT("source_expression_id"), Input->Expression->MaterialExpressionGuid.ToString());
                ConnObj->SetStringField(TEXT("source_class"), Input->Expression->GetClass()->GetName());
                ConnObj->SetNumberField(TEXT("source_output_index"), Input->OutputIndex);
                ConnObj->SetStringField(TEXT("target_expression_id"), Expr->MaterialExpressionGuid.ToString());
                ConnObj->SetStringField(TEXT("target_class"), Expr->GetClass()->GetName());
                ConnObj->SetNumberField(TEXT("target_input_index"), i);
                ConnObj->SetStringField(TEXT("target_input_name"), Expr->GetInputName(i).ToString());
                ConnObj->SetStringField(TEXT("target_type"), TEXT("expression"));
                ConnectionsArray.Add(MakeShared<FJsonValueObject>(ConnObj));
            }
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material_name"), MaterialName);
    ResultObj->SetNumberField(TEXT("num_connections"), ConnectionsArray.Num());
    ResultObj->SetArrayField(TEXT("connections"), ConnectionsArray);
    return ResultObj;
#else
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only feature"));
#endif
}

// ═══════════════════════════════════════════════════════════════════════
// Tier 3: Material Instances
// ═══════════════════════════════════════════════════════════════════════

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleCreateMaterialInstance(const TSharedPtr<FJsonObject>& Params)
{
    FString InstanceName;
    if (!Params->TryGetStringField(TEXT("instance_name"), InstanceName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'instance_name' parameter"));
    }

    FString ParentName;
    if (!Params->TryGetStringField(TEXT("parent_material"), ParentName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parent_material' parameter"));
    }

    FString Path;
    if (!Params->TryGetStringField(TEXT("path"), Path))
    {
        Path = TEXT("/Game/Materials");
    }

    // Find parent material
    UMaterialInterface* ParentMaterial = nullptr;
    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

    // Search for Material
    TArray<FAssetData> MaterialAssets;
    ARM.Get().GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Material")), MaterialAssets);
    for (const FAssetData& Asset : MaterialAssets)
    {
        if (Asset.AssetName.ToString() == ParentName)
        {
            ParentMaterial = Cast<UMaterialInterface>(Asset.GetAsset());
            break;
        }
    }

    // Also search MaterialInstanceConstant as parent
    if (!ParentMaterial)
    {
        TArray<FAssetData> MIAssets;
        ARM.Get().GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("MaterialInstanceConstant")), MIAssets);
        for (const FAssetData& Asset : MIAssets)
        {
            if (Asset.AssetName.ToString() == ParentName)
            {
                ParentMaterial = Cast<UMaterialInterface>(Asset.GetAsset());
                break;
            }
        }
    }

    if (!ParentMaterial)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Parent material '%s' not found"), *ParentName));
    }

    FString PackagePath = FString::Printf(TEXT("%s/%s"), *Path, *InstanceName);

    // Check if already exists
    UMaterialInstanceConstant* ExistingMI = LoadObject<UMaterialInstanceConstant>(nullptr, *FString::Printf(TEXT("%s.%s"), *PackagePath, *InstanceName));
    if (ExistingMI)
    {
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("instance_name"), InstanceName);
        ResultObj->SetStringField(TEXT("path"), PackagePath);
        ResultObj->SetBoolField(TEXT("already_exists"), true);
        return ResultObj;
    }

    UPackage* Package = CreatePackage(*PackagePath);
    if (!Package)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package"));
    }

    UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
    Factory->InitialParent = ParentMaterial;

    UMaterialInstanceConstant* NewMI = Cast<UMaterialInstanceConstant>(Factory->FactoryCreateNew(
        UMaterialInstanceConstant::StaticClass(), Package, FName(*InstanceName),
        RF_Public | RF_Standalone, nullptr, GWarn));

    if (!NewMI)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create material instance"));
    }

    FAssetRegistryModule::AssetCreated(NewMI);
    NewMI->MarkPackageDirty();

    FString PackageFileName = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    UPackage::SavePackage(Package, NewMI, *PackageFileName, SaveArgs);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("instance_name"), InstanceName);
    ResultObj->SetStringField(TEXT("parent_material"), ParentName);
    ResultObj->SetStringField(TEXT("path"), PackagePath);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleSetMaterialInstanceScalarParam(const TSharedPtr<FJsonObject>& Params)
{
    FString InstanceName;
    if (!Params->TryGetStringField(TEXT("instance_name"), InstanceName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'instance_name' parameter"));
    }

    FString ParameterName;
    if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parameter_name' parameter"));
    }

    if (!Params->HasField(TEXT("value")))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value' parameter"));
    }
    float Value = static_cast<float>(Params->GetNumberField(TEXT("value")));

    // Find material instance
    UMaterialInstanceConstant* MI = nullptr;
    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    TArray<FAssetData> AssetList;
    ARM.Get().GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("MaterialInstanceConstant")), AssetList);
    for (const FAssetData& Asset : AssetList)
    {
        if (Asset.AssetName.ToString() == InstanceName)
        {
            MI = Cast<UMaterialInstanceConstant>(Asset.GetAsset());
            break;
        }
    }
    if (!MI)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material instance '%s' not found"), *InstanceName));
    }

    FMaterialParameterInfo ParamInfo{FName(*ParameterName)};
    MI->SetScalarParameterValueEditorOnly(ParamInfo, Value);
    MI->PostEditChange();
    MI->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("instance_name"), InstanceName);
    ResultObj->SetStringField(TEXT("parameter_name"), ParameterName);
    ResultObj->SetNumberField(TEXT("value"), Value);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleSetMaterialInstanceVectorParam(const TSharedPtr<FJsonObject>& Params)
{
    FString InstanceName;
    if (!Params->TryGetStringField(TEXT("instance_name"), InstanceName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'instance_name' parameter"));
    }

    FString ParameterName;
    if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parameter_name' parameter"));
    }

    const TArray<TSharedPtr<FJsonValue>>* ValueArray;
    if (!Params->TryGetArrayField(TEXT("value"), ValueArray) || ValueArray->Num() < 3)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or invalid 'value' parameter (expected array [R, G, B] or [R, G, B, A])"));
    }

    FLinearColor Color;
    Color.R = static_cast<float>((*ValueArray)[0]->AsNumber());
    Color.G = static_cast<float>((*ValueArray)[1]->AsNumber());
    Color.B = static_cast<float>((*ValueArray)[2]->AsNumber());
    Color.A = ValueArray->Num() >= 4 ? static_cast<float>((*ValueArray)[3]->AsNumber()) : 1.0f;

    UMaterialInstanceConstant* MI = nullptr;
    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    TArray<FAssetData> AssetList;
    ARM.Get().GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("MaterialInstanceConstant")), AssetList);
    for (const FAssetData& Asset : AssetList)
    {
        if (Asset.AssetName.ToString() == InstanceName)
        {
            MI = Cast<UMaterialInstanceConstant>(Asset.GetAsset());
            break;
        }
    }
    if (!MI)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material instance '%s' not found"), *InstanceName));
    }

    FMaterialParameterInfo ParamInfo{FName(*ParameterName)};
    MI->SetVectorParameterValueEditorOnly(ParamInfo, Color);
    MI->PostEditChange();
    MI->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("instance_name"), InstanceName);
    ResultObj->SetStringField(TEXT("parameter_name"), ParameterName);
    TSharedPtr<FJsonObject> ColorObj = MakeShared<FJsonObject>();
    ColorObj->SetNumberField(TEXT("r"), Color.R);
    ColorObj->SetNumberField(TEXT("g"), Color.G);
    ColorObj->SetNumberField(TEXT("b"), Color.B);
    ColorObj->SetNumberField(TEXT("a"), Color.A);
    ResultObj->SetObjectField(TEXT("value"), ColorObj);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleSetMaterialInstanceTextureParam(const TSharedPtr<FJsonObject>& Params)
{
    FString InstanceName;
    if (!Params->TryGetStringField(TEXT("instance_name"), InstanceName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'instance_name' parameter"));
    }

    FString ParameterName;
    if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parameter_name' parameter"));
    }

    FString TexturePath;
    if (!Params->TryGetStringField(TEXT("texture_path"), TexturePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'texture_path' parameter"));
    }

    UTexture* Texture = LoadObject<UTexture>(nullptr, *TexturePath);
    if (!Texture)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Texture '%s' not found"), *TexturePath));
    }

    UMaterialInstanceConstant* MI = nullptr;
    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    TArray<FAssetData> AssetList;
    ARM.Get().GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("MaterialInstanceConstant")), AssetList);
    for (const FAssetData& Asset : AssetList)
    {
        if (Asset.AssetName.ToString() == InstanceName)
        {
            MI = Cast<UMaterialInstanceConstant>(Asset.GetAsset());
            break;
        }
    }
    if (!MI)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material instance '%s' not found"), *InstanceName));
    }

    FMaterialParameterInfo ParamInfo{FName(*ParameterName)};
    MI->SetTextureParameterValueEditorOnly(ParamInfo, Texture);
    MI->PostEditChange();
    MI->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("instance_name"), InstanceName);
    ResultObj->SetStringField(TEXT("parameter_name"), ParameterName);
    ResultObj->SetStringField(TEXT("texture_path"), TexturePath);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleGetMaterialInstanceInfo(const TSharedPtr<FJsonObject>& Params)
{
    FString InstanceName;
    if (!Params->TryGetStringField(TEXT("instance_name"), InstanceName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'instance_name' parameter"));
    }

    UMaterialInstanceConstant* MI = nullptr;
    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    TArray<FAssetData> AssetList;
    ARM.Get().GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("MaterialInstanceConstant")), AssetList);
    for (const FAssetData& Asset : AssetList)
    {
        if (Asset.AssetName.ToString() == InstanceName)
        {
            MI = Cast<UMaterialInstanceConstant>(Asset.GetAsset());
            break;
        }
    }
    if (!MI)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material instance '%s' not found"), *InstanceName));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("instance_name"), MI->GetName());
    ResultObj->SetStringField(TEXT("path"), MI->GetPathName());

    if (MI->Parent)
    {
        ResultObj->SetStringField(TEXT("parent"), MI->Parent->GetName());
        ResultObj->SetStringField(TEXT("parent_path"), MI->Parent->GetPathName());
    }

    // Scalar parameters
    TArray<TSharedPtr<FJsonValue>> ScalarParams;
    for (const auto& Param : MI->ScalarParameterValues)
    {
        TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
        ParamObj->SetStringField(TEXT("name"), Param.ParameterInfo.Name.ToString());
        ParamObj->SetNumberField(TEXT("value"), Param.ParameterValue);
        ScalarParams.Add(MakeShared<FJsonValueObject>(ParamObj));
    }
    ResultObj->SetArrayField(TEXT("scalar_parameters"), ScalarParams);

    // Vector parameters
    TArray<TSharedPtr<FJsonValue>> VectorParams;
    for (const auto& Param : MI->VectorParameterValues)
    {
        TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
        ParamObj->SetStringField(TEXT("name"), Param.ParameterInfo.Name.ToString());
        TSharedPtr<FJsonObject> ColorObj = MakeShared<FJsonObject>();
        ColorObj->SetNumberField(TEXT("r"), Param.ParameterValue.R);
        ColorObj->SetNumberField(TEXT("g"), Param.ParameterValue.G);
        ColorObj->SetNumberField(TEXT("b"), Param.ParameterValue.B);
        ColorObj->SetNumberField(TEXT("a"), Param.ParameterValue.A);
        ParamObj->SetObjectField(TEXT("value"), ColorObj);
        VectorParams.Add(MakeShared<FJsonValueObject>(ParamObj));
    }
    ResultObj->SetArrayField(TEXT("vector_parameters"), VectorParams);

    // Texture parameters
    TArray<TSharedPtr<FJsonValue>> TextureParams;
    for (const auto& Param : MI->TextureParameterValues)
    {
        TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
        ParamObj->SetStringField(TEXT("name"), Param.ParameterInfo.Name.ToString());
        ParamObj->SetStringField(TEXT("texture"), Param.ParameterValue ? Param.ParameterValue->GetPathName() : TEXT("None"));
        TextureParams.Add(MakeShared<FJsonValueObject>(ParamObj));
    }
    ResultObj->SetArrayField(TEXT("texture_parameters"), TextureParams);

    return ResultObj;
}

// ═══════════════════════════════════════════════════════════════════════
// Tier 4: Material Parameter Collections
// ═══════════════════════════════════════════════════════════════════════

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleCreateMaterialParameterCollection(const TSharedPtr<FJsonObject>& Params)
{
    FString CollectionName;
    if (!Params->TryGetStringField(TEXT("collection_name"), CollectionName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'collection_name' parameter"));
    }

    FString Path;
    if (!Params->TryGetStringField(TEXT("path"), Path))
    {
        Path = TEXT("/Game/Materials");
    }

    FString PackagePath = FString::Printf(TEXT("%s/%s"), *Path, *CollectionName);

    // Check if exists
    UMaterialParameterCollection* Existing = LoadObject<UMaterialParameterCollection>(nullptr, *FString::Printf(TEXT("%s.%s"), *PackagePath, *CollectionName));
    if (Existing)
    {
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("collection_name"), CollectionName);
        ResultObj->SetStringField(TEXT("path"), PackagePath);
        ResultObj->SetBoolField(TEXT("already_exists"), true);
        return ResultObj;
    }

    UPackage* Package = CreatePackage(*PackagePath);
    if (!Package)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package"));
    }

    UMaterialParameterCollectionFactoryNew* Factory = NewObject<UMaterialParameterCollectionFactoryNew>();
    UMaterialParameterCollection* NewMPC = Cast<UMaterialParameterCollection>(Factory->FactoryCreateNew(
        UMaterialParameterCollection::StaticClass(), Package, FName(*CollectionName),
        RF_Public | RF_Standalone, nullptr, GWarn));

    if (!NewMPC)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create material parameter collection"));
    }

    FAssetRegistryModule::AssetCreated(NewMPC);
    NewMPC->MarkPackageDirty();

    FString PackageFileName = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    UPackage::SavePackage(Package, NewMPC, *PackageFileName, SaveArgs);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("collection_name"), CollectionName);
    ResultObj->SetStringField(TEXT("path"), PackagePath);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleAddMPCScalarParameter(const TSharedPtr<FJsonObject>& Params)
{
    FString CollectionName;
    if (!Params->TryGetStringField(TEXT("collection_name"), CollectionName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'collection_name' parameter"));
    }

    FString ParameterName;
    if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parameter_name' parameter"));
    }

    float DefaultValue = 0.0f;
    if (Params->HasField(TEXT("default_value")))
    {
        DefaultValue = static_cast<float>(Params->GetNumberField(TEXT("default_value")));
    }

    // Find MPC
    UMaterialParameterCollection* MPC = nullptr;
    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    TArray<FAssetData> AssetList;
    ARM.Get().GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("MaterialParameterCollection")), AssetList);
    for (const FAssetData& Asset : AssetList)
    {
        if (Asset.AssetName.ToString() == CollectionName)
        {
            MPC = Cast<UMaterialParameterCollection>(Asset.GetAsset());
            break;
        }
    }
    if (!MPC)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material parameter collection '%s' not found"), *CollectionName));
    }

    FCollectionScalarParameter NewParam;
    NewParam.ParameterName = FName(*ParameterName);
    NewParam.DefaultValue = DefaultValue;
    MPC->ScalarParameters.Add(NewParam);

    MPC->PostEditChange();
    MPC->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("collection_name"), CollectionName);
    ResultObj->SetStringField(TEXT("parameter_name"), ParameterName);
    ResultObj->SetNumberField(TEXT("default_value"), DefaultValue);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleAddMPCVectorParameter(const TSharedPtr<FJsonObject>& Params)
{
    FString CollectionName;
    if (!Params->TryGetStringField(TEXT("collection_name"), CollectionName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'collection_name' parameter"));
    }

    FString ParameterName;
    if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parameter_name' parameter"));
    }

    FLinearColor DefaultValue(0.0f, 0.0f, 0.0f, 1.0f);
    const TArray<TSharedPtr<FJsonValue>>* ValueArray;
    if (Params->TryGetArrayField(TEXT("default_value"), ValueArray) && ValueArray->Num() >= 3)
    {
        DefaultValue.R = static_cast<float>((*ValueArray)[0]->AsNumber());
        DefaultValue.G = static_cast<float>((*ValueArray)[1]->AsNumber());
        DefaultValue.B = static_cast<float>((*ValueArray)[2]->AsNumber());
        if (ValueArray->Num() >= 4)
        {
            DefaultValue.A = static_cast<float>((*ValueArray)[3]->AsNumber());
        }
    }

    UMaterialParameterCollection* MPC = nullptr;
    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    TArray<FAssetData> AssetList;
    ARM.Get().GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("MaterialParameterCollection")), AssetList);
    for (const FAssetData& Asset : AssetList)
    {
        if (Asset.AssetName.ToString() == CollectionName)
        {
            MPC = Cast<UMaterialParameterCollection>(Asset.GetAsset());
            break;
        }
    }
    if (!MPC)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material parameter collection '%s' not found"), *CollectionName));
    }

    FCollectionVectorParameter NewParam;
    NewParam.ParameterName = FName(*ParameterName);
    NewParam.DefaultValue = DefaultValue;
    MPC->VectorParameters.Add(NewParam);

    MPC->PostEditChange();
    MPC->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("collection_name"), CollectionName);
    ResultObj->SetStringField(TEXT("parameter_name"), ParameterName);
    TSharedPtr<FJsonObject> ColorObj = MakeShared<FJsonObject>();
    ColorObj->SetNumberField(TEXT("r"), DefaultValue.R);
    ColorObj->SetNumberField(TEXT("g"), DefaultValue.G);
    ColorObj->SetNumberField(TEXT("b"), DefaultValue.B);
    ColorObj->SetNumberField(TEXT("a"), DefaultValue.A);
    ResultObj->SetObjectField(TEXT("default_value"), ColorObj);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleGetMPCInfo(const TSharedPtr<FJsonObject>& Params)
{
    FString CollectionName;
    if (!Params->TryGetStringField(TEXT("collection_name"), CollectionName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'collection_name' parameter"));
    }

    UMaterialParameterCollection* MPC = nullptr;
    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    TArray<FAssetData> AssetList;
    ARM.Get().GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("MaterialParameterCollection")), AssetList);
    for (const FAssetData& Asset : AssetList)
    {
        if (Asset.AssetName.ToString() == CollectionName)
        {
            MPC = Cast<UMaterialParameterCollection>(Asset.GetAsset());
            break;
        }
    }
    if (!MPC)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material parameter collection '%s' not found"), *CollectionName));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("collection_name"), MPC->GetName());
    ResultObj->SetStringField(TEXT("path"), MPC->GetPathName());

    TArray<TSharedPtr<FJsonValue>> ScalarParams;
    for (const FCollectionScalarParameter& Param : MPC->ScalarParameters)
    {
        TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
        ParamObj->SetStringField(TEXT("name"), Param.ParameterName.ToString());
        ParamObj->SetNumberField(TEXT("default_value"), Param.DefaultValue);
        ScalarParams.Add(MakeShared<FJsonValueObject>(ParamObj));
    }
    ResultObj->SetArrayField(TEXT("scalar_parameters"), ScalarParams);

    TArray<TSharedPtr<FJsonValue>> VectorParams;
    for (const FCollectionVectorParameter& Param : MPC->VectorParameters)
    {
        TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
        ParamObj->SetStringField(TEXT("name"), Param.ParameterName.ToString());
        TSharedPtr<FJsonObject> ColorObj = MakeShared<FJsonObject>();
        ColorObj->SetNumberField(TEXT("r"), Param.DefaultValue.R);
        ColorObj->SetNumberField(TEXT("g"), Param.DefaultValue.G);
        ColorObj->SetNumberField(TEXT("b"), Param.DefaultValue.B);
        ColorObj->SetNumberField(TEXT("a"), Param.DefaultValue.A);
        ParamObj->SetObjectField(TEXT("default_value"), ColorObj);
        VectorParams.Add(MakeShared<FJsonValueObject>(ParamObj));
    }
    ResultObj->SetArrayField(TEXT("vector_parameters"), VectorParams);

    return ResultObj;
}

// ═══════════════════════════════════════════════════════════════════════
// Tier 5: Material Functions
// ═══════════════════════════════════════════════════════════════════════

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleCreateMaterialFunction(const TSharedPtr<FJsonObject>& Params)
{
    FString FunctionName;
    if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
    }

    FString Path;
    if (!Params->TryGetStringField(TEXT("path"), Path))
    {
        Path = TEXT("/Game/Materials/Functions");
    }

    FString PackagePath = FString::Printf(TEXT("%s/%s"), *Path, *FunctionName);

    UMaterialFunction* Existing = LoadObject<UMaterialFunction>(nullptr, *FString::Printf(TEXT("%s.%s"), *PackagePath, *FunctionName));
    if (Existing)
    {
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("function_name"), FunctionName);
        ResultObj->SetStringField(TEXT("path"), PackagePath);
        ResultObj->SetBoolField(TEXT("already_exists"), true);
        return ResultObj;
    }

    UPackage* Package = CreatePackage(*PackagePath);
    if (!Package)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package"));
    }

    UMaterialFunctionFactoryNew* Factory = NewObject<UMaterialFunctionFactoryNew>();
    UMaterialFunction* NewFunc = Cast<UMaterialFunction>(Factory->FactoryCreateNew(
        UMaterialFunction::StaticClass(), Package, FName(*FunctionName),
        RF_Public | RF_Standalone, nullptr, GWarn));

    if (!NewFunc)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create material function"));
    }

    // Set optional description
    FString Description;
    if (Params->TryGetStringField(TEXT("description"), Description))
    {
        NewFunc->Description = Description;
    }

    if (Params->HasField(TEXT("expose_to_library")))
    {
        NewFunc->bExposeToLibrary = Params->GetBoolField(TEXT("expose_to_library"));
    }

    FAssetRegistryModule::AssetCreated(NewFunc);
    NewFunc->MarkPackageDirty();

    FString PackageFileName = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    UPackage::SavePackage(Package, NewFunc, *PackageFileName, SaveArgs);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("function_name"), FunctionName);
    ResultObj->SetStringField(TEXT("path"), PackagePath);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleAddMaterialFunctionInput(const TSharedPtr<FJsonObject>& Params)
{
    FString FunctionName;
    if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
    }

    FString InputName;
    if (!Params->TryGetStringField(TEXT("input_name"), InputName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'input_name' parameter"));
    }

    FString InputTypeStr;
    if (!Params->TryGetStringField(TEXT("input_type"), InputTypeStr))
    {
        InputTypeStr = TEXT("Scalar");
    }

    // Map input type string
    EFunctionInputType InputType = FunctionInput_Scalar;
    if (InputTypeStr == TEXT("Vector2")) InputType = FunctionInput_Vector2;
    else if (InputTypeStr == TEXT("Vector3")) InputType = FunctionInput_Vector3;
    else if (InputTypeStr == TEXT("Vector4")) InputType = FunctionInput_Vector4;
    else if (InputTypeStr == TEXT("Texture2D")) InputType = FunctionInput_Texture2D;

    // Find function
    UMaterialFunction* Func = nullptr;
    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    TArray<FAssetData> AssetList;
    ARM.Get().GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("MaterialFunction")), AssetList);
    for (const FAssetData& Asset : AssetList)
    {
        if (Asset.AssetName.ToString() == FunctionName)
        {
            Func = Cast<UMaterialFunction>(Asset.GetAsset());
            break;
        }
    }
    if (!Func)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material function '%s' not found"), *FunctionName));
    }

#if WITH_EDITORONLY_DATA
    UMaterialFunctionEditorOnlyData* EditorData = Func->GetEditorOnlyData();
    if (!EditorData)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Cannot access function editor data"));
    }

    int32 NodePosX = -300, NodePosY = 0;
    if (Params->HasField(TEXT("x")))
    {
        NodePosX = static_cast<int32>(Params->GetNumberField(TEXT("x")));
    }
    if (Params->HasField(TEXT("y")))
    {
        NodePosY = static_cast<int32>(Params->GetNumberField(TEXT("y")));
    }

    UMaterialExpression* NewExpr = UMaterialEditingLibrary::CreateMaterialExpressionInFunction(
        Func, UMaterialExpressionFunctionInput::StaticClass(), NodePosX, NodePosY);
    UMaterialExpressionFunctionInput* NewInput = Cast<UMaterialExpressionFunctionInput>(NewExpr);
    if (!NewInput)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create function input expression"));
    }

    NewInput->InputName = FName(*InputName);
    NewInput->InputType = InputType;
    Func->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("function_name"), FunctionName);
    ResultObj->SetStringField(TEXT("input_name"), InputName);
    ResultObj->SetStringField(TEXT("input_type"), InputTypeStr);
    ResultObj->SetStringField(TEXT("expression_id"), NewInput->MaterialExpressionGuid.ToString());
    return ResultObj;
#else
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only feature"));
#endif
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleAddMaterialFunctionOutput(const TSharedPtr<FJsonObject>& Params)
{
    FString FunctionName;
    if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
    }

    FString OutputName;
    if (!Params->TryGetStringField(TEXT("output_name"), OutputName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'output_name' parameter"));
    }

    UMaterialFunction* Func = nullptr;
    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    TArray<FAssetData> AssetList;
    ARM.Get().GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("MaterialFunction")), AssetList);
    for (const FAssetData& Asset : AssetList)
    {
        if (Asset.AssetName.ToString() == FunctionName)
        {
            Func = Cast<UMaterialFunction>(Asset.GetAsset());
            break;
        }
    }
    if (!Func)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material function '%s' not found"), *FunctionName));
    }

#if WITH_EDITORONLY_DATA
    UMaterialFunctionEditorOnlyData* EditorData = Func->GetEditorOnlyData();
    if (!EditorData)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Cannot access function editor data"));
    }

    int32 NodePosX = 300, NodePosY = 0;
    if (Params->HasField(TEXT("x")))
    {
        NodePosX = static_cast<int32>(Params->GetNumberField(TEXT("x")));
    }
    if (Params->HasField(TEXT("y")))
    {
        NodePosY = static_cast<int32>(Params->GetNumberField(TEXT("y")));
    }

    UMaterialExpression* NewExpr = UMaterialEditingLibrary::CreateMaterialExpressionInFunction(
        Func, UMaterialExpressionFunctionOutput::StaticClass(), NodePosX, NodePosY);
    UMaterialExpressionFunctionOutput* NewOutput = Cast<UMaterialExpressionFunctionOutput>(NewExpr);
    if (!NewOutput)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create function output expression"));
    }

    NewOutput->OutputName = FName(*OutputName);
    Func->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("function_name"), FunctionName);
    ResultObj->SetStringField(TEXT("output_name"), OutputName);
    ResultObj->SetStringField(TEXT("expression_id"), NewOutput->MaterialExpressionGuid.ToString());
    return ResultObj;
#else
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only feature"));
#endif
}

// ═══════════════════════════════════════════════════════════════════════
// Tier 6: Application & Analysis
// ═══════════════════════════════════════════════════════════════════════

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleApplyMaterialToActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    FString MaterialName;
    if (!Params->TryGetStringField(TEXT("material_name"), MaterialName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_name' parameter"));
    }

    int32 SlotIndex = 0;
    if (Params->HasField(TEXT("slot_index")))
    {
        SlotIndex = static_cast<int32>(Params->GetNumberField(TEXT("slot_index")));
    }

    // Find material
    UMaterialInterface* MaterialInterface = nullptr;
    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

    // Search Materials
    TArray<FAssetData> MatAssets;
    ARM.Get().GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Material")), MatAssets);
    for (const FAssetData& Asset : MatAssets)
    {
        if (Asset.AssetName.ToString() == MaterialName)
        {
            MaterialInterface = Cast<UMaterialInterface>(Asset.GetAsset());
            break;
        }
    }

    // Search Material Instances
    if (!MaterialInterface)
    {
        TArray<FAssetData> MIAssets;
        ARM.Get().GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("MaterialInstanceConstant")), MIAssets);
        for (const FAssetData& Asset : MIAssets)
        {
            if (Asset.AssetName.ToString() == MaterialName)
            {
                MaterialInterface = Cast<UMaterialInterface>(Asset.GetAsset());
                break;
            }
        }
    }

    if (!MaterialInterface)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material '%s' not found"), *MaterialName));
    }

    // Find actor
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    AActor* FoundActor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName || It->GetName() == ActorName)
        {
            FoundActor = *It;
            break;
        }
    }

    if (!FoundActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
    }

    // Apply to mesh components
    TArray<UPrimitiveComponent*> Components;
    FoundActor->GetComponents<UPrimitiveComponent>(Components);

    int32 AppliedCount = 0;
    for (UPrimitiveComponent* Comp : Components)
    {
        if (Comp)
        {
            Comp->SetMaterial(SlotIndex, MaterialInterface);
            AppliedCount++;
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor_name"), ActorName);
    ResultObj->SetStringField(TEXT("material_name"), MaterialName);
    ResultObj->SetNumberField(TEXT("slot_index"), SlotIndex);
    ResultObj->SetNumberField(TEXT("components_updated"), AppliedCount);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleApplyMaterialToBlueprintComponent(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    FString MaterialName;
    if (!Params->TryGetStringField(TEXT("material_name"), MaterialName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_name' parameter"));
    }

    int32 SlotIndex = 0;
    if (Params->HasField(TEXT("slot_index")))
    {
        SlotIndex = static_cast<int32>(Params->GetNumberField(TEXT("slot_index")));
    }

    // Find material
    UMaterialInterface* MaterialInterface = nullptr;
    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

    TArray<FAssetData> MatAssets;
    ARM.Get().GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Material")), MatAssets);
    for (const FAssetData& Asset : MatAssets)
    {
        if (Asset.AssetName.ToString() == MaterialName)
        {
            MaterialInterface = Cast<UMaterialInterface>(Asset.GetAsset());
            break;
        }
    }
    if (!MaterialInterface)
    {
        TArray<FAssetData> MIAssets;
        ARM.Get().GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("MaterialInstanceConstant")), MIAssets);
        for (const FAssetData& Asset : MIAssets)
        {
            if (Asset.AssetName.ToString() == MaterialName)
            {
                MaterialInterface = Cast<UMaterialInterface>(Asset.GetAsset());
                break;
            }
        }
    }
    if (!MaterialInterface)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material '%s' not found"), *MaterialName));
    }

    // Find blueprint
    UBlueprint* Blueprint = nullptr;
    TArray<FAssetData> BPAssets;
    ARM.Get().GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Blueprint")), BPAssets);
    for (const FAssetData& Asset : BPAssets)
    {
        if (Asset.AssetName.ToString() == BlueprintName)
        {
            Blueprint = Cast<UBlueprint>(Asset.GetAsset());
            break;
        }
    }
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName));
    }

    // Find component in SCS
    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (!SCS)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint has no SimpleConstructionScript"));
    }

    USCS_Node* TargetNode = nullptr;
    for (USCS_Node* Node : SCS->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            TargetNode = Node;
            break;
        }
    }
    if (!TargetNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component '%s' not found in blueprint"), *ComponentName));
    }

    UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(TargetNode->ComponentTemplate);
    if (!PrimComp)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Component is not a primitive component"));
    }

    PrimComp->SetMaterial(SlotIndex, MaterialInterface);
    Blueprint->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
    ResultObj->SetStringField(TEXT("component_name"), ComponentName);
    ResultObj->SetStringField(TEXT("material_name"), MaterialName);
    ResultObj->SetNumberField(TEXT("slot_index"), SlotIndex);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleGetMaterialStats(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialName;
    if (!Params->TryGetStringField(TEXT("material_name"), MaterialName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_name' parameter"));
    }

    UMaterial* Material = nullptr;
    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    TArray<FAssetData> AssetList;
    ARM.Get().GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Material")), AssetList);
    for (const FAssetData& Asset : AssetList)
    {
        if (Asset.AssetName.ToString() == MaterialName)
        {
            Material = Cast<UMaterial>(Asset.GetAsset());
            break;
        }
    }
    if (!Material)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material '%s' not found"), *MaterialName));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material_name"), MaterialName);

    // Blend mode
    FString BlendModeStr;
    switch (Material->BlendMode)
    {
        case BLEND_Opaque: BlendModeStr = TEXT("Opaque"); break;
        case BLEND_Translucent: BlendModeStr = TEXT("Translucent"); break;
        case BLEND_Additive: BlendModeStr = TEXT("Additive"); break;
        case BLEND_Modulate: BlendModeStr = TEXT("Modulate"); break;
        case BLEND_Masked: BlendModeStr = TEXT("Masked"); break;
        default: BlendModeStr = TEXT("Unknown"); break;
    }
    ResultObj->SetStringField(TEXT("blend_mode"), BlendModeStr);
    ResultObj->SetBoolField(TEXT("two_sided"), Material->TwoSided != 0);

#if WITH_EDITORONLY_DATA
    UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData();
    if (EditorData)
    {
        int32 TotalExpressions = EditorData->ExpressionCollection.Expressions.Num();
        ResultObj->SetNumberField(TEXT("total_expressions"), TotalExpressions);

        // Count expression types
        TMap<FString, int32> ExprTypeCounts;
        int32 ParameterCount = 0;
        int32 TextureSampleCount = 0;
        int32 ConnectionCount = 0;

        for (UMaterialExpression* Expr : EditorData->ExpressionCollection.Expressions)
        {
            if (!Expr) continue;

            FString ClassName = Expr->GetClass()->GetName();
            ExprTypeCounts.FindOrAdd(ClassName)++;

            if (Expr->IsA<UMaterialExpressionScalarParameter>() ||
                Expr->IsA<UMaterialExpressionVectorParameter>() ||
                Expr->IsA<UMaterialExpressionTextureSampleParameter2D>())
            {
                ParameterCount++;
            }

            if (Expr->IsA<UMaterialExpressionTextureSample>())
            {
                TextureSampleCount++;
            }

            // Count connections (inputs that are connected)
            for (int32 i = 0; ; ++i)
            {
                FExpressionInput* Input = Expr->GetInput(i);
                if (!Input) break;
                if (Input->Expression)
                {
                    ConnectionCount++;
                }
            }
        }

        ResultObj->SetNumberField(TEXT("parameter_count"), ParameterCount);
        ResultObj->SetNumberField(TEXT("texture_sample_count"), TextureSampleCount);
        ResultObj->SetNumberField(TEXT("connection_count"), ConnectionCount);

        // Connected material inputs
        TArray<FString> ConnectedInputs;
        if (EditorData->BaseColor.Expression) ConnectedInputs.Add(TEXT("BaseColor"));
        if (EditorData->Metallic.Expression) ConnectedInputs.Add(TEXT("Metallic"));
        if (EditorData->Specular.Expression) ConnectedInputs.Add(TEXT("Specular"));
        if (EditorData->Roughness.Expression) ConnectedInputs.Add(TEXT("Roughness"));
        if (EditorData->Normal.Expression) ConnectedInputs.Add(TEXT("Normal"));
        if (EditorData->EmissiveColor.Expression) ConnectedInputs.Add(TEXT("EmissiveColor"));
        if (EditorData->Opacity.Expression) ConnectedInputs.Add(TEXT("Opacity"));
        if (EditorData->OpacityMask.Expression) ConnectedInputs.Add(TEXT("OpacityMask"));
        if (EditorData->AmbientOcclusion.Expression) ConnectedInputs.Add(TEXT("AmbientOcclusion"));

        TArray<TSharedPtr<FJsonValue>> ConnectedArray;
        for (const FString& Input : ConnectedInputs)
        {
            ConnectedArray.Add(MakeShared<FJsonValueString>(Input));
        }
        ResultObj->SetArrayField(TEXT("connected_material_inputs"), ConnectedArray);

        // Expression type breakdown
        TArray<TSharedPtr<FJsonValue>> TypesArray;
        for (const auto& Pair : ExprTypeCounts)
        {
            TSharedPtr<FJsonObject> TypeObj = MakeShared<FJsonObject>();
            TypeObj->SetStringField(TEXT("type"), Pair.Key);
            TypeObj->SetNumberField(TEXT("count"), Pair.Value);
            TypesArray.Add(MakeShared<FJsonValueObject>(TypeObj));
        }
        ResultObj->SetArrayField(TEXT("expression_types"), TypesArray);
    }
#endif

    return ResultObj;
}
