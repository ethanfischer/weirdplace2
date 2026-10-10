#include "NpcPersonalSpaceComponent.h"
#include "Components/CapsuleComponent.h"
#include "Tunable.h"

// Horizontal radius (cm) the player is held away from the NPC root. Dialogue
// text is sized to read at this range, so keep it in step with
// weird.Dialogue.WalkAwayDistance (must be larger than this).
WP_TUNABLE_FLOAT(GNpcPersonalSpaceRadius, "weird.Npc.PersonalSpaceRadius", 100.f,
	"Radius (cm) of the player-only blocking capsule around each NPC.");

void UNpcPersonalSpaceComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	USceneComponent* Root = Owner->GetRootComponent();
	if (!Root)
	{
		UE_LOG(LogTemp, Error, TEXT("NpcPersonalSpaceComponent[%s] - owner has no root component"), *Owner->GetName());
		return;
	}

	const float HalfHeight = 100.f;
	Capsule = NewObject<UCapsuleComponent>(Owner, TEXT("PersonalSpaceCapsule"));
	Capsule->InitCapsuleSize(GNpcPersonalSpaceRadius, HalfHeight);
	Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Capsule->SetCollisionObjectType(ECC_Pawn);
	Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Capsule->SetGenerateOverlapEvents(false);
	Capsule->SetupAttachment(Root);
	// MetaHuman roots sit at the feet; lift the capsule so it spans the body.
	Capsule->SetRelativeLocation(FVector(0.f, 0.f, HalfHeight));
	Capsule->RegisterComponent();
}
