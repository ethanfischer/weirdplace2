#include "Seneca.h"
#include "FirstPersonCharacter.h"
#include "BPFL_Utilities.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/ChildActorComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DlgSystem/DlgDialogue.h"

ASeneca::ASeneca()
{
	PrimaryActorTick.bCanEverTick = false;
	// Components are created in Blueprint to preserve MetaHuman setup
}

void ASeneca::BeginPlay()
{
	Super::BeginPlay();

	// Find the dialogue widget component - it's inside a Child Actor Component
	TArray<UChildActorComponent*> ChildActorComponents;
	GetComponents<UChildActorComponent>(ChildActorComponents);
	for (UChildActorComponent* ChildActorComp : ChildActorComponents)
	{
		if (ChildActorComp->GetName().Contains(TEXT("WorldSpace_UI_Dialogue")))
		{
			if (AActor* ChildActor = ChildActorComp->GetChildActor())
			{
				DialogueWidgetComponent = ChildActor->FindComponentByClass<UWidgetComponent>();
			}
			break;
		}
	}

	if (TriggerSphere)
	{
		// Update sphere radius in case it was changed in editor
		TriggerSphere->SetSphereRadius(DialogueTriggerRadius);

		// Bind overlap events
		TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &ASeneca::OnSphereBeginOverlap);
		TriggerSphere->OnComponentEndOverlap.AddDynamic(this, &ASeneca::OnSphereEndOverlap);
	}
}

void ASeneca::Interact_Implementation()
{
	// Start dialogue when interacted with
	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);

	if (AFirstPersonCharacter* FPCharacter = Cast<AFirstPersonCharacter>(PlayerCharacter))
	{
		if (Dialogue)
		{
			FPCharacter->StartDialogueWithNPC(Dialogue, this);
		}
	}
}

void ASeneca::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Set look-at behavior (applies to any actor)
	if (BodyMesh)
	{
		UBPFL_Utilities::SetShouldLookAtPlayer(true, OtherActor, BodyMesh);
	}

	// Start dialogue only if OtherActor is the player
	AFirstPersonCharacter* FPCharacter = Cast<AFirstPersonCharacter>(OtherActor);
	if (FPCharacter && Dialogue)
	{
		FPCharacter->StartDialogueWithNPC(Dialogue, this);
	}
}

void ASeneca::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// Stop look-at behavior
	if (BodyMesh)
	{
		UBPFL_Utilities::SetShouldLookAtPlayer(false, OtherActor, BodyMesh);
	}
}
