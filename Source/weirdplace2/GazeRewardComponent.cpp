// Fill out your copyright notice in the Description page of Project Settings.

#include "GazeRewardComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "FirstPersonCharacter.h"
#include "GazeUtils.h"
#include "Inventory.h"
#include "ItemDefinition.h"
#include "IXRTrackingSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace GazeRewardInternal
{
	static const FName IntensityParamName(TEXT("Intensity"));
	static const TCHAR* DefaultEffectMaterialPath = TEXT("/Game/CreatedMaterials/M_GazeReward.M_GazeReward");

	// Drive a single, stable weighted-blendable entry on the camera's post
	// process (same pattern as UBladderUrgencyComponent).
	static void SetBlendableWeight(FPostProcessSettings& PostProcessSettings, UObject* BlendableObject, float Weight)
	{
		if (!BlendableObject)
		{
			return;
		}
		FWeightedBlendables& WeightedBlendables = PostProcessSettings.WeightedBlendables;
		const float ClampedWeight = FMath::Clamp(Weight, 0.f, 1.f);
		int32 FoundIndex = INDEX_NONE;
		for (int32 Index = 0; Index < WeightedBlendables.Array.Num(); ++Index)
		{
			if (WeightedBlendables.Array[Index].Object == BlendableObject)
			{
				FoundIndex = Index;
				break;
			}
		}
		if (FoundIndex == INDEX_NONE)
		{
			WeightedBlendables.Array.Add(FWeightedBlendable(ClampedWeight, BlendableObject));
			return;
		}
		WeightedBlendables.Array[FoundIndex].Weight = ClampedWeight;
		for (int32 Index = WeightedBlendables.Array.Num() - 1; Index >= 0; --Index)
		{
			if (Index != FoundIndex && WeightedBlendables.Array[Index].Object == BlendableObject)
			{
				WeightedBlendables.Array.RemoveAt(Index);
			}
		}
	}
}

UGazeRewardComponent::UGazeRewardComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UGazeRewardComponent::BeginPlay()
{
	Super::BeginPlay();

	// An ActorComponent can't CreateDefaultSubobject (constructor-only), so build
	// the hum audio at runtime, attached to the owner. Stays silent until gazed at.
	AActor* Owner = GetOwner();
	if (Owner && Owner->GetRootComponent())
	{
		HumComponent = NewObject<UAudioComponent>(Owner, TEXT("GazeRewardHum"));
		HumComponent->SetupAttachment(Owner->GetRootComponent());
		HumComponent->bAutoActivate = false;
		HumComponent->RegisterComponent();
	}
}

void UGazeRewardComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Don't leave the effect baked onto the player camera's post process.
	if (CachedPlayerCamera && GazeEffectMID)
	{
		GazeRewardInternal::SetBlendableWeight(CachedPlayerCamera->PostProcessSettings, GazeEffectMID, 0.f);
		CachedPlayerCamera->PostProcessSettings.WeightedBlendables.Array.RemoveAll(
			[this](const FWeightedBlendable& B) { return B.Object == GazeEffectMID; });
	}
	CurrentEffectWeight = 0.f;
	ResetFOV();
	Super::EndPlay(EndPlayReason);
}

void UGazeRewardComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bRewardGranted)
	{
		return;
	}

	if (!IsPlayerGazingAtOwner())
	{
		if (GazeSeconds > 0.f)
		{
			GazeSeconds = 0.f;
			if (HumComponent) { HumComponent->Stop(); }
			ApplyEffectWeight(0.f);
			ResetFOV();
		}
		return;
	}

	GazeSeconds += DeltaTime;
	const float Progress = RequiredLookSeconds > 0.f
		? FMath::Clamp(GazeSeconds / RequiredLookSeconds, 0.f, 1.f)
		: 1.f;

	if (HumSound && HumComponent)
	{
		if (!HumComponent->IsPlaying())
		{
			HumComponent->SetSound(HumSound);
			HumComponent->Play();
		}
		// Swell toward the payout, curved so it stays near-silent early and
		// builds late — no floor, so it isn't audible from the first frame.
		HumComponent->SetVolumeMultiplier(FMath::Pow(Progress, AudioRampExponent));
	}

	// Screen-space effect swells on the player's view as the gaze builds. The
	// gentler exponent lets the player notice it before the hum comes in.
	ApplyEffectWeight(FMath::Pow(Progress, VisualRampExponent) * MaxEffectWeight);
	ApplyFOVZoom(Progress);

	if (GazeSeconds >= RequiredLookSeconds)
	{
		GrantReward();
	}
}

bool UGazeRewardComponent::IsPlayerGazingAtOwner() const
{
	// Shared gaze geometry (box-crossing + optional line-of-sight). The 30m
	// canopy bar is why this is a box test, not a cone or a hit test.
	return UGazeUtils::IsActorInPlayerGaze(GetOwner(), GetWorld(), BoxExpand, MaxGazeDistance, bRequireLineOfSight);
}

void UGazeRewardComponent::GrantReward()
{
	// All failure paths below are misconfiguration: log and stop ticking.
	SetComponentTickEnabled(false);

	AActor* Owner = GetOwner();
	if (!RewardItem || RewardItem->ItemID.IsNone())
	{
		UE_LOG(LogTemp, Error, TEXT("GazeRewardComponent on '%s': RewardItem missing or has no ItemID"),
			Owner ? *Owner->GetName() : TEXT("?"));
		return;
	}

	AFirstPersonCharacter* Player = Cast<AFirstPersonCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	if (!Player)
	{
		UE_LOG(LogTemp, Error, TEXT("GazeRewardComponent on '%s': no AFirstPersonCharacter player"),
			Owner ? *Owner->GetName() : TEXT("?"));
		return;
	}

	UInventoryComponent* Inventory = Player->GetInventoryComponent();
	if (!Inventory)
	{
		UE_LOG(LogTemp, Error, TEXT("GazeRewardComponent on '%s': player has no InventoryComponent"),
			Owner ? *Owner->GetName() : TEXT("?"));
		return;
	}

	const FInventoryItemData ItemData = RewardItem->ToInventoryItemData();
	Inventory->AddItemWithData(ItemData);
	Player->ShowItemNotification(ItemData, RewardItem->NotificationRotation);

	bRewardGranted = true;
	if (HumComponent) { HumComponent->Stop(); }
	ApplyEffectWeight(0.f);
	ResetFOV();
	UE_LOG(LogTemp, Log, TEXT("GazeRewardComponent on '%s': granted '%s' after %.1fs gaze"),
		Owner ? *Owner->GetName() : TEXT("?"), *RewardItem->ItemID.ToString(), GazeSeconds);

	// Re-arm for non-one-shot owners.
	if (!bOneShot)
	{
		GazeSeconds = 0.f;
		bRewardGranted = false;
		SetComponentTickEnabled(true);
	}
}

void UGazeRewardComponent::ApplyEffectWeight(float Weight)
{
	CurrentEffectWeight = FMath::Clamp(Weight, 0.f, 1.f);

	// Lazily resolve the player camera + effect material — the player may not
	// exist at this component's BeginPlay (it lives on a level fixture).
	if (!CachedPlayerCamera)
	{
		if (AFirstPersonCharacter* Player = Cast<AFirstPersonCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0)))
		{
			CachedPlayerCamera = Player->GetFirstPersonCamera();
		}
	}
	if (!CachedPlayerCamera)
	{
		return;
	}
	if (!GazeEffectMID)
	{
		if (!GazeEffectMaterial)
		{
			GazeEffectMaterial = LoadObject<UMaterialInterface>(nullptr, GazeRewardInternal::DefaultEffectMaterialPath);
		}
		if (!GazeEffectMaterial)
		{
			return;
		}
		GazeEffectMID = UMaterialInstanceDynamic::Create(GazeEffectMaterial, this);
		if (!GazeEffectMID)
		{
			return;
		}
	}

	GazeRewardInternal::SetBlendableWeight(CachedPlayerCamera->PostProcessSettings, GazeEffectMID, CurrentEffectWeight);
	GazeEffectMID->SetScalarParameterValue(GazeRewardInternal::IntensityParamName, CurrentEffectWeight);
}

void UGazeRewardComponent::ApplyFOVZoom(float Progress)
{
	if (!bEnableFOVZoom)
	{
		return;
	}
	// VR owns the FOV and headset zoom is nauseating — never touch it there.
	if (!bVRChecked)
	{
		bIsVR = GEngine && GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsHeadTrackingAllowed();
		bVRChecked = true;
	}
	if (bIsVR)
	{
		return;
	}
	if (!CachedPlayerCamera)
	{
		if (AFirstPersonCharacter* Player = Cast<AFirstPersonCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0)))
		{
			CachedPlayerCamera = Player->GetFirstPersonCamera();
		}
	}
	if (!CachedPlayerCamera)
	{
		return;
	}
	if (!bBaseFOVCached)
	{
		BaseFOV = CachedPlayerCamera->FieldOfView;
		bBaseFOVCached = true;
	}
	CachedPlayerCamera->SetFieldOfView(BaseFOV - MaxFOVZoom * FMath::Pow(Progress, VisualRampExponent));
}

void UGazeRewardComponent::ResetFOV()
{
	if (bBaseFOVCached && !bIsVR && CachedPlayerCamera)
	{
		CachedPlayerCamera->SetFieldOfView(BaseFOV);
	}
}

float UGazeRewardComponent::GetCurrentFOV() const
{
	return CachedPlayerCamera ? CachedPlayerCamera->FieldOfView : -1.f;
}
