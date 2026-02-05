// Copyright Meborg. All Rights Reserved.

#include "BatchBlueprintToCodeCommandlet.h"

#if WITH_EDITOR

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "K2Node.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// NodeToCode includes
#include "Core/N2CNodeCollector.h"
#include "Core/N2CNodeTranslator.h"
#include "Core/N2CSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogBatchBlueprintToCode, Log, All);

UBatchBlueprintToCodeCommandlet::UBatchBlueprintToCodeCommandlet()
{
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
}

int32 UBatchBlueprintToCodeCommandlet::Main(const FString& Params)
{
    UE_LOG(LogBatchBlueprintToCode, Display, TEXT("=== Batch Blueprint to Code Conversion ==="));
    UE_LOG(LogBatchBlueprintToCode, Display, TEXT("Parameters: %s"), *Params);

    // Parse command line
    ParseParams(Params);

    UE_LOG(LogBatchBlueprintToCode, Display, TEXT("Search Path: %s"), *SearchPath);
    UE_LOG(LogBatchBlueprintToCode, Display, TEXT("Dry Run: %s"), bDryRun ? TEXT("Yes") : TEXT("No"));

    if (IncludeList.Num() > 0)
    {
        UE_LOG(LogBatchBlueprintToCode, Display, TEXT("Include Filter: %s"), *FString::Join(IncludeList, TEXT(", ")));
    }
    if (ExcludeList.Num() > 0)
    {
        UE_LOG(LogBatchBlueprintToCode, Display, TEXT("Exclude Filter: %s"), *FString::Join(ExcludeList, TEXT(", ")));
    }

    // Discover Blueprints
    TArray<FAssetData> BlueprintAssets = DiscoverBlueprints();

    if (BlueprintAssets.Num() == 0)
    {
        UE_LOG(LogBatchBlueprintToCode, Warning, TEXT("No Blueprints found matching criteria"));
        return 0;
    }

    UE_LOG(LogBatchBlueprintToCode, Display, TEXT("Found %d Blueprint(s) to process:"), BlueprintAssets.Num());
    for (const FAssetData& Asset : BlueprintAssets)
    {
        UE_LOG(LogBatchBlueprintToCode, Display, TEXT("  - %s"), *Asset.AssetName.ToString());
    }

    // Dry run mode - just list and exit
    if (bDryRun)
    {
        UE_LOG(LogBatchBlueprintToCode, Display, TEXT("Dry run complete. Use without -DryRun to convert."));
        return 0;
    }

    // Load API key from secrets file
    FString SecretsFilePath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NodeToCode"), TEXT("User"), TEXT("secrets.json"));
    FString SecretsJson;
    if (FFileHelper::LoadFileToString(SecretsJson, *SecretsFilePath))
    {
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SecretsJson);
        if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
        {
            AnthropicApiKey = JsonObject->GetStringField(TEXT("Anthropic_API_Key"));
        }
    }

    if (AnthropicApiKey.IsEmpty())
    {
        UE_LOG(LogBatchBlueprintToCode, Error, TEXT("No Anthropic API key found in %s"), *SecretsFilePath);
        return 1;
    }

    UE_LOG(LogBatchBlueprintToCode, Display, TEXT("Loaded API key from secrets file"));

    // Process each Blueprint (synchronous using curl subprocess)
    for (const FAssetData& AssetData : BlueprintAssets)
    {
        UBlueprint* Blueprint = Cast<UBlueprint>(AssetData.GetAsset());
        if (!Blueprint)
        {
            UE_LOG(LogBatchBlueprintToCode, Warning, TEXT("Failed to load Blueprint: %s"), *AssetData.AssetName.ToString());
            FailureCount.Increment();
            continue;
        }

        UE_LOG(LogBatchBlueprintToCode, Display, TEXT("Processing: %s"), *Blueprint->GetName());

        if (ProcessBlueprint(Blueprint))
        {
            UE_LOG(LogBatchBlueprintToCode, Display, TEXT("  Translation complete"));
        }
        else
        {
            UE_LOG(LogBatchBlueprintToCode, Warning, TEXT("  Failed to process"));
            FailureCount.Increment();
        }
    }

    // Summary
    FString OutputDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NodeToCode"), TEXT("Translations"));
    UE_LOG(LogBatchBlueprintToCode, Display, TEXT("=== Conversion Complete ==="));
    UE_LOG(LogBatchBlueprintToCode, Display, TEXT("Successful: %d"), SuccessCount.GetValue());
    UE_LOG(LogBatchBlueprintToCode, Display, TEXT("Failed: %d"), FailureCount.GetValue());
    UE_LOG(LogBatchBlueprintToCode, Display, TEXT("Output: %s"), *OutputDir);

    return FailureCount.GetValue() > 0 ? 1 : 0;
}

void UBatchBlueprintToCodeCommandlet::ParseParams(const FString& InParams)
{
    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamMap;

    UCommandlet::ParseCommandLine(*InParams, Tokens, Switches, ParamMap);

    // Check for switches
    bDryRun = Switches.Contains(TEXT("DryRun")) || Switches.Contains(TEXT("dryrun"));

    // Check for parameters
    if (const FString* PathValue = ParamMap.Find(TEXT("Path")))
    {
        SearchPath = *PathValue;
    }

    if (const FString* IncludeValue = ParamMap.Find(TEXT("Include")))
    {
        IncludeValue->ParseIntoArray(IncludeList, TEXT(","), true);
        for (FString& Name : IncludeList)
        {
            Name.TrimStartAndEndInline();
        }
    }

    if (const FString* ExcludeValue = ParamMap.Find(TEXT("Exclude")))
    {
        ExcludeValue->ParseIntoArray(ExcludeList, TEXT(","), true);
        for (FString& Name : ExcludeList)
        {
            Name.TrimStartAndEndInline();
        }
    }
}

TArray<FAssetData> UBatchBlueprintToCodeCommandlet::DiscoverBlueprints()
{
    TArray<FAssetData> Result;

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    // Ensure asset registry is ready
    AssetRegistry.SearchAllAssets(true);

    // Query for Blueprint assets
    FARFilter Filter;
    Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
    Filter.bRecursiveClasses = true;
    Filter.bRecursivePaths = true;
    Filter.PackagePaths.Add(FName(*SearchPath));

    TArray<FAssetData> AllBlueprints;
    AssetRegistry.GetAssets(Filter, AllBlueprints);

    // Apply filters
    for (const FAssetData& AssetData : AllBlueprints)
    {
        if (!ShouldExcludeBlueprint(AssetData))
        {
            Result.Add(AssetData);
        }
    }

    return Result;
}

bool UBatchBlueprintToCodeCommandlet::ShouldExcludeBlueprint(const FAssetData& AssetData) const
{
    FString AssetName = AssetData.AssetName.ToString();
    FString PackagePath = AssetData.PackagePath.ToString();

    // Check explicit include list first
    if (IncludeList.Num() > 0)
    {
        bool bInIncludeList = false;
        for (const FString& IncludeName : IncludeList)
        {
            if (AssetName.Equals(IncludeName, ESearchCase::IgnoreCase))
            {
                bInIncludeList = true;
                break;
            }
        }
        if (!bInIncludeList)
        {
            return true; // Not in include list, exclude it
        }
    }

    // Check explicit exclude list
    for (const FString& ExcludeName : ExcludeList)
    {
        if (AssetName.Equals(ExcludeName, ESearchCase::IgnoreCase))
        {
            return true;
        }
    }

    // Exclude Widget Blueprints (WBP_* prefix)
    if (AssetName.StartsWith(TEXT("WBP_"), ESearchCase::IgnoreCase))
    {
        return true;
    }

    // Exclude Animation Blueprints (ABP_* prefix)
    if (AssetName.StartsWith(TEXT("ABP_"), ESearchCase::IgnoreCase))
    {
        return true;
    }

    // Exclude third-party plugin content
    if (PackagePath.Contains(TEXT("/DlgSystem/")))
    {
        return true;
    }

    // Exclude engine content
    if (PackagePath.StartsWith(TEXT("/Engine/")))
    {
        return true;
    }

    // Check parent class for Widget/Animation types
    FAssetTagValueRef ParentClassTag = AssetData.TagsAndValues.FindTag(FName(TEXT("ParentClass")));
    if (ParentClassTag.IsSet())
    {
        FString ParentClassName = ParentClassTag.AsString();
        if (ParentClassName.Contains(TEXT("UserWidget")) ||
            ParentClassName.Contains(TEXT("AnimInstance")))
        {
            return true;
        }
    }

    return false;
}

bool UBatchBlueprintToCodeCommandlet::ProcessBlueprint(UBlueprint* Blueprint)
{
    if (!Blueprint)
    {
        return false;
    }

    // Get all graphs from the Blueprint
    TArray<UEdGraph*> AllGraphs;
    Blueprint->GetAllGraphs(AllGraphs);

    if (AllGraphs.Num() == 0)
    {
        UE_LOG(LogBatchBlueprintToCode, Warning, TEXT("  No graphs found in Blueprint"));
        return false;
    }

    UE_LOG(LogBatchBlueprintToCode, Display, TEXT("  Found %d graph(s)"), AllGraphs.Num());

    // Process each graph
    FN2CNodeCollector& Collector = FN2CNodeCollector::Get();
    FN2CNodeTranslator& Translator = FN2CNodeTranslator::Get();
    FString BlueprintName = Blueprint->GetName();
    bool bAnySuccess = false;

    // System prompt for code generation
    FString SystemPrompt = TEXT("You are an expert Unreal Engine C++ programmer. Convert the following Blueprint graph JSON to equivalent C++ code. Output clean, production-ready code with appropriate UPROPERTY and UFUNCTION macros. Include both header declarations and implementation.");

    for (UEdGraph* Graph : AllGraphs)
    {
        if (!Graph)
        {
            continue;
        }

        FString GraphName = Graph->GetName();
        UE_LOG(LogBatchBlueprintToCode, Display, TEXT("  Processing graph: %s"), *GraphName);

        // Collect nodes from the graph
        TArray<UK2Node*> CollectedNodes;
        if (!Collector.CollectNodesFromGraph(Graph, CollectedNodes))
        {
            UE_LOG(LogBatchBlueprintToCode, Warning, TEXT("    Failed to collect nodes"));
            continue;
        }

        if (CollectedNodes.Num() == 0)
        {
            UE_LOG(LogBatchBlueprintToCode, Display, TEXT("    No K2 nodes in graph, skipping"));
            continue;
        }

        UE_LOG(LogBatchBlueprintToCode, Display, TEXT("    Collected %d node(s)"), CollectedNodes.Num());

        // Translate nodes to N2C format
        if (!Translator.GenerateN2CStruct(CollectedNodes))
        {
            UE_LOG(LogBatchBlueprintToCode, Warning, TEXT("    Failed to translate nodes"));
            continue;
        }

        const FN2CBlueprint& N2CBlueprint = Translator.GetN2CBlueprint();
        if (!N2CBlueprint.IsValid())
        {
            UE_LOG(LogBatchBlueprintToCode, Warning, TEXT("    Invalid N2C Blueprint structure"));
            continue;
        }

        // Serialize to JSON
        FN2CSerializer::SetPrettyPrint(false);
        FString JsonOutput = FN2CSerializer::ToJson(N2CBlueprint);

        if (JsonOutput.IsEmpty())
        {
            UE_LOG(LogBatchBlueprintToCode, Warning, TEXT("    JSON serialization failed"));
            continue;
        }

        // Call LLM via curl subprocess (synchronous)
        FString Response = CallCurlForLLMRequest(JsonOutput, SystemPrompt);

        if (Response.IsEmpty())
        {
            UE_LOG(LogBatchBlueprintToCode, Error, TEXT("    Empty response for %s::%s"), *BlueprintName, *GraphName);
            FailureCount.Increment();
        }
        else
        {
            UE_LOG(LogBatchBlueprintToCode, Display, TEXT("    Received response (%d chars)"), Response.Len());
            SaveTranslationResult(BlueprintName, GraphName, Response);
            SuccessCount.Increment();
            bAnySuccess = true;
        }
    }

    return bAnySuccess;
}

void UBatchBlueprintToCodeCommandlet::WaitForPendingRequests()
{
    // No longer needed - using synchronous curl subprocess instead
}

FString UBatchBlueprintToCodeCommandlet::CallCurlForLLMRequest(const FString& JsonPayload, const FString& SystemPrompt)
{
    // Create temp file for the request payload
    FString TempPayloadFile = FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("n2c_"), TEXT(".json"));

    // Build the Anthropic API request body
    // Escape the JSON payload for embedding in the request
    FString EscapedPayload = JsonPayload.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\"")).Replace(TEXT("\n"), TEXT("\\n")).Replace(TEXT("\r"), TEXT(""));
    FString EscapedSystemPrompt = SystemPrompt.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\"")).Replace(TEXT("\n"), TEXT("\\n")).Replace(TEXT("\r"), TEXT(""));

    FString RequestBody = FString::Printf(
        TEXT("{\"model\":\"claude-sonnet-4-20250514\",\"max_tokens\":8192,\"system\":\"%s\",\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}]}"),
        *EscapedSystemPrompt,
        *EscapedPayload
    );

    // Write request to temp file
    if (!FFileHelper::SaveStringToFile(RequestBody, *TempPayloadFile))
    {
        UE_LOG(LogBatchBlueprintToCode, Error, TEXT("Failed to write temp payload file"));
        return TEXT("");
    }

    // Build curl command - output directly to file
    FString TempOutputFile = FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("n2c_out_"), TEXT(".json"));

    // Use curl with output to file
    FString CurlCmd = FString::Printf(
        TEXT("curl -s -o \"%s\" -X POST \"https://api.anthropic.com/v1/messages\" -H \"x-api-key: %s\" -H \"anthropic-version: 2023-06-01\" -H \"content-type: application/json\" -d @\"%s\""),
        *TempOutputFile,
        *AnthropicApiKey,
        *TempPayloadFile
    );

    // Run curl via cmd.exe
    FString CmdExe = TEXT("cmd.exe");
    FString CmdArgs = FString::Printf(TEXT("/c %s"), *CurlCmd);

    UE_LOG(LogBatchBlueprintToCode, Display, TEXT("Calling Anthropic API..."));

    FString StdOut, StdErr;
    int32 ReturnCode = -1;

    // Execute curl
    bool bSuccess = FPlatformProcess::ExecProcess(
        *CmdExe,
        *CmdArgs,
        &ReturnCode,
        &StdOut,
        &StdErr
    );

    // Clean up payload file
    IFileManager::Get().Delete(*TempPayloadFile);

    if (!bSuccess)
    {
        UE_LOG(LogBatchBlueprintToCode, Error, TEXT("curl failed to execute: %s"), *StdErr);
        IFileManager::Get().Delete(*TempOutputFile);
        return TEXT("");
    }

    // Read the output file
    FString ResponseContent;
    if (!FFileHelper::LoadFileToString(ResponseContent, *TempOutputFile))
    {
        UE_LOG(LogBatchBlueprintToCode, Error, TEXT("Failed to read curl output file: %s"), *TempOutputFile);
        IFileManager::Get().Delete(*TempOutputFile);
        return TEXT("");
    }

    // Clean up output file
    IFileManager::Get().Delete(*TempOutputFile);

    if (ResponseContent.IsEmpty())
    {
        UE_LOG(LogBatchBlueprintToCode, Error, TEXT("Empty response from API"));
        return TEXT("");
    }

    // Parse the response to extract the content
    TSharedPtr<FJsonObject> JsonResponse;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);
    if (!FJsonSerializer::Deserialize(Reader, JsonResponse) || !JsonResponse.IsValid())
    {
        UE_LOG(LogBatchBlueprintToCode, Error, TEXT("Failed to parse API response: %s"), *ResponseContent.Left(500));
        return TEXT("");
    }

    // Check for error
    if (JsonResponse->HasField(TEXT("error")))
    {
        TSharedPtr<FJsonObject> ErrorObj = JsonResponse->GetObjectField(TEXT("error"));
        FString ErrorMessage = ErrorObj->GetStringField(TEXT("message"));
        UE_LOG(LogBatchBlueprintToCode, Error, TEXT("API error: %s"), *ErrorMessage);
        return TEXT("");
    }

    // Extract content from response
    const TArray<TSharedPtr<FJsonValue>>* ContentArray;
    if (JsonResponse->TryGetArrayField(TEXT("content"), ContentArray) && ContentArray->Num() > 0)
    {
        TSharedPtr<FJsonObject> ContentObj = (*ContentArray)[0]->AsObject();
        if (ContentObj.IsValid())
        {
            FString ContentText = ContentObj->GetStringField(TEXT("text"));
            UE_LOG(LogBatchBlueprintToCode, Display, TEXT("Received response (%d chars)"), ContentText.Len());
            return ContentText;
        }
    }

    UE_LOG(LogBatchBlueprintToCode, Error, TEXT("Unexpected response format"));
    return TEXT("");
}

void UBatchBlueprintToCodeCommandlet::SaveTranslationResult(const FString& BlueprintName, const FString& GraphName, const FString& Response)
{
    // Create output directory
    FString OutputDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NodeToCode"), TEXT("Translations"), BlueprintName);
    IFileManager::Get().MakeDirectory(*OutputDir, true);

    // Save the response
    FString OutputFile = FPaths::Combine(OutputDir, GraphName + TEXT(".txt"));
    if (FFileHelper::SaveStringToFile(Response, *OutputFile))
    {
        UE_LOG(LogBatchBlueprintToCode, Display, TEXT("Saved translation to: %s"), *OutputFile);
    }
    else
    {
        UE_LOG(LogBatchBlueprintToCode, Warning, TEXT("Failed to save translation to: %s"), *OutputFile);
    }
}

#endif // WITH_EDITOR
