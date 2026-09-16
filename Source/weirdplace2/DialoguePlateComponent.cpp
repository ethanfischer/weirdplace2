#include "DialoguePlateComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerController.h"
#include "UI_Dialogue.h"
#include "Tunable.h"

// Where the plate's left-middle point sits: OffsetUp above the NPC root (head
// height), then OffsetRight along the camera's right axis so the line always
// starts beside the face and grows away from it on screen.
WP_TUNABLE_FLOAT(GDialoguePlateOffsetRight, "weird.DialoguePlate.OffsetRight", 20.f,
	"Sideways offset (cm, camera right) of the dialogue plate's left edge from the head anchor.");
WP_TUNABLE_FLOAT(GDialoguePlateOffsetUp, "weird.DialoguePlate.OffsetUp", 170.f,
	"Height (cm above the NPC root) of the dialogue plate's vertical center.");

UDialoguePlateComponent::UDialoguePlateComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UDialoguePlateComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	TArray<UChildActorComponent*> ChildActorComponents;
	Owner->GetComponents<UChildActorComponent>(ChildActorComponents);
	for (UChildActorComponent* ChildActorComp : ChildActorComponents)
	{
		if (!ChildActorComp->GetName().Contains(TEXT("WorldSpace_UI_Dialogue")))
		{
			continue;
		}
		if (AActor* ChildActor = ChildActorComp->GetChildActor())
		{
			WidgetComponent = ChildActor->FindComponentByClass<UWidgetComponent>();
		}
		ChildActorComp->SetRelativeLocation(FVector(0.f, 0.f, GDialoguePlateOffsetUp));
		break;
	}

	if (!WidgetComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("DialoguePlateComponent[%s] - no WorldSpace_UI_Dialogue widget component found"), *Owner->GetName());
		SetComponentTickEnabled(false);
		return;
	}
	if (!WidgetComponent->GetAttachParent())
	{
		UE_LOG(LogTemp, Error, TEXT("DialoguePlateComponent[%s] - widget component has no attach parent to anchor to"), *Owner->GetName());
		SetComponentTickEnabled(false);
		return;
	}

	// A semi-transparent dialogue backing plate (UUI_Dialogue) needs alpha
	// blending. Force Transparent blend so BackingOpacity is a true gradient --
	// Masked blend clips it binary at the 0.333 mask threshold, which reads as
	// a hard step near ~0.35.
	WidgetComponent->SetBlendMode(EWidgetBlendMode::Transparent);

	// Supersample the render target and scale the component down by the same
	// factor: same world size, sharper text. Pivot at the left-middle edge to
	// match UUI_Dialogue's plate anchoring, so this component's location is the
	// point the text grows from.
	const float RS = UUI_Dialogue::GetRenderScale();
	WidgetComponent->SetDrawSize(WidgetComponent->GetDrawSize() * RS);
	WidgetComponent->SetRelativeScale3D(WidgetComponent->GetRelativeScale3D() / RS);
	WidgetComponent->SetPivot(FVector2D(0.f, 0.5f));
}

void UDialoguePlateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}

	FVector CamLocation;
	FRotator CamRotation;
	PC->GetPlayerViewPoint(CamLocation, CamRotation);

	// The attach parent (the child actor root) is the head anchor; step sideways
	// in camera space so the plate's left edge clears the face from any angle.
	const FVector CamRight = FRotationMatrix(CamRotation).GetUnitAxis(EAxis::Y);
	const FVector WidgetLocation = WidgetComponent->GetAttachParent()->GetComponentLocation() + CamRight * GDialoguePlateOffsetRight;
	WidgetComponent->SetWorldLocation(WidgetLocation);

	// Billboard toward the camera.
	WidgetComponent->SetWorldRotation((CamLocation - WidgetLocation).Rotation());
}
