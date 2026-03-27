#include "LookAtPlayerComponent.h"
#include "BPFL_Utilities.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

ULookAtPlayerComponent::ULookAtPlayerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	InitSphereRadius(200.0f);
	ShapeColor = FColor::Cyan;
}

void ULookAtPlayerComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		UE_LOG(LogTemp, Error, TEXT("ULookAtPlayerComponent::BeginPlay - No owner actor"));
		return;
	}

	SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	SetGenerateOverlapEvents(true);

	// Auto-find the Body skeletal mesh by component name
	TArray<USkeletalMeshComponent*> SkelMeshes;
	Owner->GetComponents<USkeletalMeshComponent>(SkelMeshes);
	for (USkeletalMeshComponent* SM : SkelMeshes)
	{
		if (SM->GetFName() == BodyMeshComponentName)
		{
			BodyMesh = SM;
			break;
		}
	}
	if (!BodyMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("ULookAtPlayerComponent on '%s': Could not find skeletal mesh component named '%s'"),
			*Owner->GetName(), *BodyMeshComponentName.ToString());
		return;
	}

	OnComponentBeginOverlap.AddDynamic(this, &ULookAtPlayerComponent::OnSphereBeginOverlap);
	OnComponentEndOverlap.AddDynamic(this, &ULookAtPlayerComponent::OnSphereEndOverlap);
}

void ULookAtPlayerComponent::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!BodyMesh || OtherActor != UGameplayStatics::GetPlayerCharacter(GetWorld(), 0))
	{
		return;
	}
	UBPFL_Utilities::SetShouldLookAtPlayer(true, OtherActor, BodyMesh);
}

void ULookAtPlayerComponent::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!BodyMesh || OtherActor != UGameplayStatics::GetPlayerCharacter(GetWorld(), 0))
	{
		return;
	}
	UBPFL_Utilities::SetShouldLookAtPlayer(false, OtherActor, BodyMesh);
}
