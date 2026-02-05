// Copyright Meborg. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Commandlets/Commandlet.h"
#include "BatchBlueprintToCodeCommandlet.generated.h"

struct FAssetData;
class UBlueprint;

/**
 * Commandlet for batch converting Blueprints to C++ using NodeToCode plugin.
 *
 * Usage:
 *   UnrealEditor-Cmd.exe Project.uproject -run=BatchBlueprintToCode [options]
 *
 * Options:
 *   -DryRun           List Blueprints without converting
 *   -Path=/Game/...   Root path to search (default: /Game/Blueprints/)
 *   -Include=BP1,BP2  Only convert these Blueprints (comma-separated names)
 *   -Exclude=BP1,BP2  Skip these Blueprints (comma-separated names)
 */
UCLASS()
class UBatchBlueprintToCodeCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UBatchBlueprintToCodeCommandlet();
    virtual int32 Main(const FString& Params) override;

private:
    /** Parse command line parameters */
    void ParseParams(const FString& InParams);

    /** Discover Blueprint assets in the search path */
    TArray<FAssetData> DiscoverBlueprints();

    /** Check if a Blueprint should be excluded based on filters */
    bool ShouldExcludeBlueprint(const FAssetData& AssetData) const;

    /** Process a single Blueprint through NodeToCode */
    bool ProcessBlueprint(UBlueprint* Blueprint);

    /** Wait for all pending LLM requests to complete */
    void WaitForPendingRequests();

    /** Dry run mode - list files without converting */
    bool bDryRun = false;

    /** Root path to search for Blueprints */
    FString SearchPath = TEXT("/Game/Blueprints/");

    /** Explicit include list (empty means include all) */
    TArray<FString> IncludeList;

    /** Explicit exclude list */
    TArray<FString> ExcludeList;

    /** Counter for pending async LLM requests */
    FThreadSafeCounter PendingRequests;

    /** Counter for successful translations */
    FThreadSafeCounter SuccessCount;

    /** Counter for failed translations */
    FThreadSafeCounter FailureCount;

    /** API key for Anthropic */
    FString AnthropicApiKey;

    /** Make HTTP request via curl subprocess (workaround for commandlet mode) */
    FString CallCurlForLLMRequest(const FString& JsonPayload, const FString& SystemPrompt);

    /** Save translation result to disk */
    void SaveTranslationResult(const FString& BlueprintName, const FString& GraphName, const FString& Response);
};

#endif // WITH_EDITOR
