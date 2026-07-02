#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MoviePosterSubsystem.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UTexture2D;
enum class EStoryFlag : uint8;

// Shows the player's collected movies as posters in the world: the first
// collected movie on the telephone pole's PosterSheet (actor tagged
// "MoviePoster0"), the second on the bathroom wall plane ("MoviePoster1").
// Posters are hidden until their movie exists and persist once shown — giving
// a tape to Seneca doesn't take its poster down. The pole poster additionally
// stays hidden until the phone scene's SeenTornadoWarning reveal.
UCLASS()
class WEIRDPLACE2_API UMoviePosterSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	// Deferred to UWorld::OnWorldBeginPlay (after GameMode StartPlay) so the
	// player pawn exists; subsystem OnWorldBeginPlay itself runs before spawn.
	void BindToPlayerInventory();

	UFUNCTION()
	void OnInventoryChanged(const TArray<FName>& CurrentItems);

	void OnStoryFlagChanged(EStoryFlag Flag, bool bValue);

	// Reconciles both poster surfaces against CollectedMovies + the pole gate.
	void ApplyPosterStates();

	// The poster surface for PosterIndex: the actor tagged
	// "MoviePoster<PosterIndex>", its component named "PosterSheet"
	// (BP_TelephoneScene) or its sole static mesh component (plain planes).
	UStaticMeshComponent* FindPosterSurface(int32 PosterIndex) const;

	// The raw VHS cover texture (/Game/VHSCovers/<ItemID>) — the same source
	// the inventory thumbnails use. The MI_VHSCover_* materials are full box
	// wraps and show back+spine+front on a flat surface, so posters instead
	// run the texture through M_VHSCoverFront (front-face crop, square target).
	static UTexture2D* LoadCoverTexture(FName ItemId);

	// Distinct movie ItemIDs in collection order. Movies are identified by
	// their /Game/VHSCovers/<ItemID> cover texture existing — non-movie items
	// (Money, Key, BlankVHS) have none.
	TArray<FName> CollectedMovies;

	// One front-face MID per poster surface, keyed by poster index.
	UPROPERTY()
	TMap<int32, TObjectPtr<UMaterialInstanceDynamic>> PosterMids;

	FDelegateHandle FlagChangedHandle;
};
