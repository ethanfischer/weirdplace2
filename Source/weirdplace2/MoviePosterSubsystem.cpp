#include "MoviePosterSubsystem.h"

#include "Inventory.h"
#include "MyCharacter.h"
#include "StorySubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "TimerManager.h"

namespace
{
	constexpr int32 NumPosters = 2;
	constexpr int32 PolePosterIndex = 0;
}

void UMoviePosterSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Subsystem OnWorldBeginPlay runs before GameMode->StartPlay spawns the
	// pawn; UWorld::OnWorldBeginPlay broadcasts right after it.
	InWorld.OnWorldBeginPlay.AddUObject(this, &UMoviePosterSubsystem::BindToPlayerInventory);

	if (UStorySubsystem* Story = InWorld.GetSubsystem<UStorySubsystem>())
	{
		FlagChangedHandle = Story->OnStoryFlagChanged.AddUObject(this, &UMoviePosterSubsystem::OnStoryFlagChanged);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("MoviePosterSubsystem: no UStorySubsystem; pole poster gate inert"));
	}
}

void UMoviePosterSubsystem::BindToPlayerInventory()
{
	AMyCharacter* Player = Cast<AMyCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	UInventoryComponent* Inventory = Player ? Player->GetInventoryComponent() : nullptr;
	if (!Inventory)
	{
		UE_LOG(LogTemp, Error, TEXT("MoviePosterSubsystem: no player inventory at world begin play; posters inert"));
		return;
	}
	Inventory->OnInventoryChanged.AddDynamic(this, &UMoviePosterSubsystem::OnInventoryChanged);
	ApplyPosterStates();
}

void UMoviePosterSubsystem::OnInventoryChanged(const TArray<FName>& CurrentItems)
{
	for (const FName& ItemId : CurrentItems)
	{
		if (ItemId.IsNone() || CollectedMovies.Contains(ItemId) || CollectedMovies.Num() >= NumPosters)
		{
			continue;
		}
		if (LoadCoverTexture(ItemId))
		{
			CollectedMovies.Add(ItemId);
		}
	}
	ApplyPosterStates();
}

void UMoviePosterSubsystem::OnStoryFlagChanged(EStoryFlag Flag, bool bValue)
{
	if (Flag != EStoryFlag::SeenTornadoWarning)
	{
		return;
	}
	// The phone scene's reveal propagates visibility to all its children —
	// including the PosterSheet. Reapply one tick later so the poster's own
	// state wins regardless of delegate invocation order.
	GetWorld()->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &UMoviePosterSubsystem::ApplyPosterStates));
}

void UMoviePosterSubsystem::ApplyPosterStates()
{
	UStorySubsystem* Story = GetWorld() ? GetWorld()->GetSubsystem<UStorySubsystem>() : nullptr;
	const bool bPoleRevealed = Story && Story->IsFlagSet(EStoryFlag::SeenTornadoWarning);

	for (int32 i = 0; i < NumPosters; ++i)
	{
		UStaticMeshComponent* Surface = FindPosterSurface(i);
		if (!Surface)
		{
			UE_LOG(LogTemp, Error, TEXT("MoviePosterSubsystem: no poster surface tagged MoviePoster%d"), i);
			continue;
		}

		const bool bHasMovie = CollectedMovies.IsValidIndex(i);
		if (bHasMovie)
		{
			UTexture2D* Cover = LoadCoverTexture(CollectedMovies[i]);
			UMaterialInstanceDynamic* Mid = PosterMids.FindRef(i);
			if (!Mid)
			{
				UMaterialInterface* FrontMat = LoadObject<UMaterialInterface>(
					nullptr, TEXT("/Game/Materials/M_VHSCoverFront.M_VHSCoverFront"));
				if (!FrontMat)
				{
					UE_LOG(LogTemp, Error, TEXT("MoviePosterSubsystem: M_VHSCoverFront missing; poster %d stays hidden"), i);
					continue;
				}
				Mid = UMaterialInstanceDynamic::Create(FrontMat, this);
				PosterMids.Add(i, Mid);
			}
			Mid->SetTextureParameterValue(FName("CoverTexture"), Cover);
			// Neutralize M_VHSCoverFront's letterbox (it exists only to fit the
			// portrait cover into the SQUARE inventory slot). The poster planes are
			// sized to the cover's own aspect, so CoverAspect=1 makes the remap an
			// identity and the bar mask all-1 => the front-face crop fills the plane
			// with no black bars. Inventory builds its own MID and never sets this,
			// so it keeps the 0.5763 default and its thumbnails stay letterboxed.
			Mid->SetScalarParameterValue(FName("CoverAspect"), 1.0f);
			Surface->SetMaterial(0, Mid);
		}
		const bool bShow = bHasMovie && (i != PolePosterIndex || bPoleRevealed);
		Surface->SetVisibility(bShow, true);
	}
}

UStaticMeshComponent* UMoviePosterSubsystem::FindPosterSurface(int32 PosterIndex) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const FName Tag(*FString::Printf(TEXT("MoviePoster%d"), PosterIndex));
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (!It->ActorHasTag(Tag))
		{
			continue;
		}
		TArray<UStaticMeshComponent*> Meshes;
		It->GetComponents(Meshes);
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			if (Mesh->GetName() == TEXT("PosterSheet"))
			{
				return Mesh;
			}
		}
		return Meshes.Num() == 1 ? Meshes[0] : nullptr;
	}
	return nullptr;
}

UTexture2D* UMoviePosterSubsystem::LoadCoverTexture(FName ItemId)
{
	const FString Path = FString::Printf(TEXT("/Game/VHSCovers/%s"), *ItemId.ToString());
	return LoadObject<UTexture2D>(nullptr, *Path);
}
