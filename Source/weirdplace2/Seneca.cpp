#include "Seneca.h"
#include "FirstPersonCharacter.h"
#include "BPFL_Utilities.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DlgSystem/DlgDialogue.h"

ASeneca::ASeneca()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create skeletal mesh for body
	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	RootComponent = Body;

	// Create dialogue trigger sphere
	DialogueTriggerSphere = CreateDefaultSubobject<USphereComponent>(TEXT("DialogueTriggerSphere"));
	DialogueTriggerSphere->SetupAttachment(RootComponent);
	DialogueTriggerSphere->SetSphereRadius(DialogueTriggerRadius);
	DialogueTriggerSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	DialogueTriggerSphere->SetGenerateOverlapEvents(true);
}

void ASeneca::BeginPlay()
{
	Super::BeginPlay();

	// Update sphere radius in case it was changed in editor
	DialogueTriggerSphere->SetSphereRadius(DialogueTriggerRadius);

	// Bind overlap events
	DialogueTriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &ASeneca::OnSphereBeginOverlap);
	DialogueTriggerSphere->OnComponentEndOverlap.AddDynamic(this, &ASeneca::OnSphereEndOverlap);
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
	// Set look-at behavior
	UBPFL_Utilities::SetShouldLookAtPlayer(true, OtherActor, Body);

	// Start dialogue with player
	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	if (AFirstPersonCharacter* FPCharacter = Cast<AFirstPersonCharacter>(PlayerCharacter))
	{
		if (Dialogue)
		{
			FPCharacter->StartDialogueWithNPC(Dialogue, this);
		}
	}
}

void ASeneca::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// Stop look-at behavior
	UBPFL_Utilities::SetShouldLookAtPlayer(false, OtherActor, Body);
}
