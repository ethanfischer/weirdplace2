#include "LookAtPlayerComponent.h"
#include "BPFL_Utilities.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"

ULookAtPlayerComponent::ULookAtPlayerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
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
	UE_LOG(LogTemp, Log, TEXT("ULookAtPlayerComponent on '%s': Found BodyMesh '%s', AnimInstance=%s"),
		*Owner->GetName(), *BodyMesh->GetName(),
		BodyMesh->GetAnimInstance() ? *BodyMesh->GetAnimInstance()->GetClass()->GetName() : TEXT("null"));

	TriggerSphere = NewObject<USphereComponent>(Owner, TEXT("LookAtTriggerSphere"));
	TriggerSphere->RegisterComponent();
	TriggerSphere->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	TriggerSphere->SetSphereRadius(TriggerRadius);
	TriggerSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	TriggerSphere->SetGenerateOverlapEvents(true);
	TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &ULookAtPlayerComponent::OnSphereBeginOverlap);
	TriggerSphere->OnComponentEndOverlap.AddDynamic(this, &ULookAtPlayerComponent::OnSphereEndOverlap);
	UE_LOG(LogTemp, Log, TEXT("ULookAtPlayerComponent::BeginPlay - Sphere created on '%s', radius=%.0f"), *Owner->GetName(), TriggerRadius);
}

void ULookAtPlayerComponent::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!BodyMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("ULookAtPlayerComponent on '%s': BodyMesh is not assigned"), *GetOwner()->GetName());
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("ULookAtPlayerComponent on '%s': BeginOverlap with '%s'"), *GetOwner()->GetName(), *OtherActor->GetName());
	UBPFL_Utilities::SetShouldLookAtPlayer(true, OtherActor, BodyMesh);
}

void ULookAtPlayerComponent::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!BodyMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("ULookAtPlayerComponent on '%s': BodyMesh is not assigned"), *GetOwner()->GetName());
		return;
	}
	UBPFL_Utilities::SetShouldLookAtPlayer(false, OtherActor, BodyMesh);
}
