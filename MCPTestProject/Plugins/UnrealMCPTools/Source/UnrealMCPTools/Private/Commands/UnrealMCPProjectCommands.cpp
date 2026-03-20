#include "Commands/UnrealMCPProjectCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "GameFramework/InputSettings.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "UObject/SavePackage.h"
#include "EnhancedInputComponent.h"

FUnrealMCPProjectCommands::FUnrealMCPProjectCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("create_input_mapping"))
    {
        return HandleCreateInputMapping(Params);
    }
    else if (CommandType == TEXT("create_input_action"))
    {
        return HandleCreateInputAction(Params);
    }
    else if (CommandType == TEXT("create_input_mapping_context"))
    {
        return HandleCreateInputMappingContext(Params);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown project command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleCreateInputMapping(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString ActionName;
    if (!Params->TryGetStringField(TEXT("action_name"), ActionName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'action_name' parameter"));
    }

    FString Key;
    if (!Params->TryGetStringField(TEXT("key"), Key))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'key' parameter"));
    }

    // Get the input settings
    UInputSettings* InputSettings = GetMutableDefault<UInputSettings>();
    if (!InputSettings)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get input settings"));
    }

    // Create the input action mapping
    FInputActionKeyMapping ActionMapping;
    ActionMapping.ActionName = FName(*ActionName);
    ActionMapping.Key = FKey(*Key);

    // Add modifiers if provided
    if (Params->HasField(TEXT("shift")))
    {
        ActionMapping.bShift = Params->GetBoolField(TEXT("shift"));
    }
    if (Params->HasField(TEXT("ctrl")))
    {
        ActionMapping.bCtrl = Params->GetBoolField(TEXT("ctrl"));
    }
    if (Params->HasField(TEXT("alt")))
    {
        ActionMapping.bAlt = Params->GetBoolField(TEXT("alt"));
    }
    if (Params->HasField(TEXT("cmd")))
    {
        ActionMapping.bCmd = Params->GetBoolField(TEXT("cmd"));
    }

    // Add the mapping
    InputSettings->AddActionMapping(ActionMapping);
    InputSettings->SaveConfig();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("action_name"), ActionName);
    ResultObj->SetStringField(TEXT("key"), Key);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleCreateInputAction(const TSharedPtr<FJsonObject>& Params)
{
    FString ActionName;
    if (!Params->TryGetStringField(TEXT("action_name"), ActionName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'action_name' parameter"));
    }

    // Get optional value type (default: Boolean for simple button presses)
    FString ValueTypeStr;
    if (!Params->TryGetStringField(TEXT("value_type"), ValueTypeStr))
    {
        ValueTypeStr = TEXT("Boolean");
    }

    // Map string to EInputActionValueType
    EInputActionValueType ValueType = EInputActionValueType::Boolean;
    if (ValueTypeStr == TEXT("Axis1D") || ValueTypeStr == TEXT("Float"))
    {
        ValueType = EInputActionValueType::Axis1D;
    }
    else if (ValueTypeStr == TEXT("Axis2D") || ValueTypeStr == TEXT("Vector2D"))
    {
        ValueType = EInputActionValueType::Axis2D;
    }
    else if (ValueTypeStr == TEXT("Axis3D") || ValueTypeStr == TEXT("Vector"))
    {
        ValueType = EInputActionValueType::Axis3D;
    }

    // Create the package path
    FString PackagePath = FString::Printf(TEXT("/Game/Input/%s"), *ActionName);

    // Check if asset already exists
    UInputAction* ExistingAction = LoadObject<UInputAction>(nullptr, *FString::Printf(TEXT("%s.%s"), *PackagePath, *ActionName));
    if (ExistingAction)
    {
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("action_name"), ActionName);
        ResultObj->SetStringField(TEXT("path"), PackagePath);
        ResultObj->SetBoolField(TEXT("already_exists"), true);
        return ResultObj;
    }

    // Create the package
    UPackage* Package = CreatePackage(*PackagePath);
    if (!Package)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package for InputAction"));
    }

    // Create the InputAction asset
    UInputAction* NewAction = NewObject<UInputAction>(Package, *ActionName, RF_Public | RF_Standalone);
    if (!NewAction)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create InputAction object"));
    }

    NewAction->ValueType = ValueType;

    // Register with asset registry and save
    FAssetRegistryModule::AssetCreated(NewAction);
    NewAction->MarkPackageDirty();

    FString PackageFileName = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    UPackage::SavePackage(Package, NewAction, *PackageFileName, SaveArgs);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("action_name"), ActionName);
    ResultObj->SetStringField(TEXT("path"), PackagePath);
    ResultObj->SetStringField(TEXT("value_type"), ValueTypeStr);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleCreateInputMappingContext(const TSharedPtr<FJsonObject>& Params)
{
    FString ContextName;
    if (!Params->TryGetStringField(TEXT("context_name"), ContextName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'context_name' parameter"));
    }

    // Get mappings array
    const TArray<TSharedPtr<FJsonValue>>* MappingsArray;
    if (!Params->TryGetArrayField(TEXT("mappings"), MappingsArray))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'mappings' parameter"));
    }

    // Create the package path
    FString PackagePath = FString::Printf(TEXT("/Game/Input/%s"), *ContextName);

    // Check if asset already exists
    UInputMappingContext* ExistingContext = LoadObject<UInputMappingContext>(nullptr, *FString::Printf(TEXT("%s.%s"), *PackagePath, *ContextName));
    if (ExistingContext)
    {
        // Remove existing mappings and re-create
        ExistingContext->UnmapAll();
    }

    UInputMappingContext* MappingContext = ExistingContext;

    if (!MappingContext)
    {
        // Create the package
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package for InputMappingContext"));
        }

        // Create the InputMappingContext asset
        MappingContext = NewObject<UInputMappingContext>(Package, *ContextName, RF_Public | RF_Standalone);
        if (!MappingContext)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create InputMappingContext object"));
        }

        FAssetRegistryModule::AssetCreated(MappingContext);
    }

    // Add mappings
    TArray<TSharedPtr<FJsonValue>> AddedMappings;
    for (const TSharedPtr<FJsonValue>& MappingValue : *MappingsArray)
    {
        const TSharedPtr<FJsonObject>& MappingObj = MappingValue->AsObject();
        if (!MappingObj.IsValid())
        {
            continue;
        }

        FString ActionName;
        if (!MappingObj->TryGetStringField(TEXT("action_name"), ActionName))
        {
            continue;
        }

        FString Key;
        if (!MappingObj->TryGetStringField(TEXT("key"), Key))
        {
            continue;
        }

        // Load the InputAction asset
        FString ActionPath = FString::Printf(TEXT("/Game/Input/%s.%s"), *ActionName, *ActionName);
        UInputAction* InputAction = LoadObject<UInputAction>(nullptr, *ActionPath);
        if (!InputAction)
        {
            UE_LOG(LogTemp, Warning, TEXT("InputAction not found: %s"), *ActionPath);
            continue;
        }

        // Map the key to the action
        FEnhancedActionKeyMapping& KeyMapping = MappingContext->MapKey(InputAction, FKey(*Key));
        (void)KeyMapping; // Suppress unused warning

        TSharedPtr<FJsonObject> MappingResult = MakeShared<FJsonObject>();
        MappingResult->SetStringField(TEXT("action_name"), ActionName);
        MappingResult->SetStringField(TEXT("key"), Key);
        AddedMappings.Add(MakeShared<FJsonValueObject>(MappingResult));
    }

    // Save the asset
    MappingContext->MarkPackageDirty();
    FString PackageFileName = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    UPackage::SavePackage(MappingContext->GetPackage(), MappingContext, *PackageFileName, SaveArgs);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("context_name"), ContextName);
    ResultObj->SetStringField(TEXT("path"), PackagePath);
    ResultObj->SetArrayField(TEXT("mappings"), AddedMappings);
    return ResultObj;
} 