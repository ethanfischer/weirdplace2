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
// Unlit white text at full emissive blooms into a glow; the widget material's
// tint scales it down.
WP_TUNABLE_FLOAT(GDialoguePlateBrightness, "weird.DialoguePlate.Brightness", 0.45f,
	"Emissive multiplier (0..1) for the dialogue plate; lower = less bloom glow.");

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
	// blending. Force Transparent blend so text alpha is a true gradient --
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
	SetLayout(EDialoguePlateLayout::BesideHead);
}

void UDialoguePlateComponent::SetLayout(EDialoguePlateLayout InLayout)
{
	Layout = InLayout;
	if (!WidgetComponent)
	{
		return;
	}
	// SetPivot only writes the field; the scene proxy reads it on (re)creation.
	WidgetComponent->SetPivot(Layout == EDialoguePlateLayout::BesideHead ? FVector2D(0.f, 0.5f) : FVector2D(0.5f, 0.5f));
	WidgetComponent->MarkRenderStateDirty();
	// The car ride authored its own text colour for its lighting; only the
	// beside-head plate gets the anti-bloom tint.
	const float Tint = Layout == EDialoguePlateLayout::BesideHead ? GDialoguePlateBrightness : 1.f;
	WidgetComponent->SetTintColorAndOpacity(FLinearColor(Tint, Tint, Tint, 1.f));
	if (Layout == EDialoguePlateLayout::Centered)
	{
		// Drop the beside-head side step so the plate sits exactly where its
		// owner parks the child actor.
		WidgetComponent->SetRelativeLocation(FVector::ZeroVector);
	}
	if (UUI_Dialogue* Dialogue = Cast<UUI_Dialogue>(WidgetComponent->GetUserWidgetObject()))
	{
		Dialogue->SetPlateLayout(Layout);
	}
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

	// BesideHead: the attach parent (the child actor root) is the head anchor;
	// step sideways in camera space so the plate's left edge clears the face
	// from any angle. Centered: leave the widget where its owner parked it.
	FVector WidgetLocation = WidgetComponent->GetComponentLocation();
	if (Layout == EDialoguePlateLayout::BesideHead)
	{
		const FVector CamRight = FRotationMatrix(CamRotation).GetUnitAxis(EAxis::Y);
		WidgetLocation = WidgetComponent->GetAttachParent()->GetComponentLocation() + CamRight * GDialoguePlateOffsetRight;
		WidgetComponent->SetWorldLocation(WidgetLocation);
	}

	// Billboard toward the camera.
	WidgetComponent->SetWorldRotation((CamLocation - WidgetLocation).Rotation());
}
