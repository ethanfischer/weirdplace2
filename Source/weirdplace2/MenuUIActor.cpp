#include "MenuUIActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DeviceProfiles/DeviceProfile.h"
#include "DeviceProfiles/DeviceProfileManager.h"
#include "Engine/StaticMesh.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "WeirdplaceGameUserSettings.h"

// ---------------------------------------------------------------------------
// Settings page layout (Z increases upward)
// ---------------------------------------------------------------------------
static constexpr float kControllerHeaderZ =  18.0f;
static constexpr float kControllerLabelZ  =  12.0f;
static constexpr float kControllerValueZ  =   6.0f;
static constexpr float kMouseKBHeaderZ    =  -4.0f;
static constexpr float kMouseKBLabelZ     = -10.0f;
static constexpr float kMouseKBValueZ     = -16.0f;
static constexpr float kSettingsBackZ     = -24.0f;

// ---------------------------------------------------------------------------
// Pause page layout (Resume, Settings, Graphics, Tunables [dev], Quit)
// ---------------------------------------------------------------------------
static constexpr float kPausedHeaderZ   =  18.0f;
static constexpr float kPauseResumeZ    =   8.0f;
static constexpr float kPauseSettingsZ  =   2.0f;
static constexpr float kPauseGraphicsZ  =  -4.0f;
static constexpr float kPauseTunablesZ  = -10.0f;
static constexpr float kPauseQuitZ      = -16.0f;

// ---------------------------------------------------------------------------
// Graphics page layout (one centered row per setting)
// ---------------------------------------------------------------------------
static constexpr float kGraphicsHeaderZ =  18.0f;
static constexpr float kGraphicsGIZ     =  10.0f;
static constexpr float kGraphicsReflZ   =   4.0f;
static constexpr float kGraphicsShadowZ =  -2.0f;
static constexpr float kGraphicsVDZ     =  -8.0f;
static constexpr float kGraphicsResetZ  = -16.0f;
static constexpr float kGraphicsBackZ   = -22.0f;

// ---------------------------------------------------------------------------
// Tunables page layout (system tab bar + 7-row scrolling window of that
// system's weird.* cvars)
// ---------------------------------------------------------------------------
static constexpr float kTunablesHeaderZ   =  18.0f;
static constexpr float kTunablesTabZ      =  13.5f;
static constexpr float kTunablesMoreUpZ   =  10.8f;
static constexpr float kTunablesRowTopZ   =   9.0f;
static constexpr float kTunablesRowStepZ  =   4.0f;
static constexpr float kTunablesMoreDownZ = -16.5f;
static constexpr float kTunablesHelpZ     = -20.5f;
static constexpr float kTunablesBackZ     = -24.0f;
static constexpr float kTunablesTabSpan   =  46.0f; // panel width available for tabs

AMenuUIActor::AMenuUIActor()
{
	PrimaryActorTick.bCanEverTick = false;

	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	SetRootComponent(RootSceneComponent);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshAsset(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMeshAsset.Succeeded())
	{
		PlaneMesh = PlaneMeshAsset.Object;
	}

	BackgroundPanel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackgroundPanel"));
	BackgroundPanel->SetupAttachment(RootSceneComponent);
	if (PlaneMesh)
	{
		BackgroundPanel->SetStaticMesh(PlaneMesh);
	}
	BackgroundPanel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BackgroundPanel->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	BackgroundPanel->SetRelativeLocation(FVector(1.0f, 0.0f, 0.0f));

	PausePageRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PausePageRoot"));
	PausePageRoot->SetupAttachment(RootSceneComponent);

	SettingsPageRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SettingsPageRoot"));
	SettingsPageRoot->SetupAttachment(RootSceneComponent);

	GraphicsPageRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GraphicsPageRoot"));
	GraphicsPageRoot->SetupAttachment(RootSceneComponent);

	TunablesPageRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TunablesPageRoot"));
	TunablesPageRoot->SetupAttachment(RootSceneComponent);

	// --- Pause page items (constructor-created so they're tracked properly) ---
	PausedHeaderText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PausedHeaderText"));
	PausedHeaderText->SetupAttachment(PausePageRoot);
	PausedHeaderText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	PausedHeaderText->SetWorldSize(3.5f);
	PausedHeaderText->SetTextRenderColor(FColor::White);
	PausedHeaderText->SetHorizontalAlignment(EHTA_Center);
	PausedHeaderText->SetVerticalAlignment(EVRTA_TextCenter);
	PausedHeaderText->SetText(FText::FromString(TEXT("PAUSED")));

	PauseResumeText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PauseResumeText"));
	PauseResumeText->SetupAttachment(PausePageRoot);
	PauseResumeText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	PauseResumeText->SetWorldSize(3.0f);
	PauseResumeText->SetHorizontalAlignment(EHTA_Center);
	PauseResumeText->SetVerticalAlignment(EVRTA_TextCenter);
	PauseResumeText->SetText(FText::FromString(TEXT("Resume")));

	PauseSettingsText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PauseSettingsText"));
	PauseSettingsText->SetupAttachment(PausePageRoot);
	PauseSettingsText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	PauseSettingsText->SetWorldSize(3.0f);
	PauseSettingsText->SetHorizontalAlignment(EHTA_Center);
	PauseSettingsText->SetVerticalAlignment(EVRTA_TextCenter);
	PauseSettingsText->SetText(FText::FromString(TEXT("Settings")));

	PauseGraphicsText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PauseGraphicsText"));
	PauseGraphicsText->SetupAttachment(PausePageRoot);
	PauseGraphicsText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	PauseGraphicsText->SetWorldSize(3.0f);
	PauseGraphicsText->SetHorizontalAlignment(EHTA_Center);
	PauseGraphicsText->SetVerticalAlignment(EVRTA_TextCenter);
	PauseGraphicsText->SetText(FText::FromString(TEXT("Graphics")));

	PauseTunablesText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PauseTunablesText"));
	PauseTunablesText->SetupAttachment(PausePageRoot);
	PauseTunablesText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	PauseTunablesText->SetWorldSize(3.0f);
	PauseTunablesText->SetHorizontalAlignment(EHTA_Center);
	PauseTunablesText->SetVerticalAlignment(EVRTA_TextCenter);
	PauseTunablesText->SetText(FText::FromString(TEXT("Tunables")));
#if UE_BUILD_SHIPPING
	PauseTunablesText->SetVisibility(false);
#endif

	PauseQuitText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PauseQuitText"));
	PauseQuitText->SetupAttachment(PausePageRoot);
	PauseQuitText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	PauseQuitText->SetWorldSize(3.0f);
	PauseQuitText->SetHorizontalAlignment(EHTA_Center);
	PauseQuitText->SetVerticalAlignment(EVRTA_TextCenter);
	PauseQuitText->SetText(FText::FromString(TEXT("Quit")));

	BuildStampText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("BuildStampText"));
	BuildStampText->SetupAttachment(RootSceneComponent);
	BuildStampText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	BuildStampText->SetRelativeLocation(FVector(0.0f, 0.0f, -28.0f));
	BuildStampText->SetWorldSize(1.5f);
	BuildStampText->SetTextRenderColor(FColor(140, 140, 140));
	BuildStampText->SetHorizontalAlignment(EHTA_Center);
	BuildStampText->SetVerticalAlignment(EVRTA_TextCenter);
	BuildStampText->SetText(FText::FromString(FString::Printf(TEXT("build %s %s"), TEXT(__DATE__), TEXT(__TIME__))));

	// --- Settings page section headers ---
	ControllerHeaderText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ControllerHeaderText"));
	ControllerHeaderText->SetupAttachment(SettingsPageRoot);
	ControllerHeaderText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	ControllerHeaderText->SetWorldSize(3.5f);
	ControllerHeaderText->SetTextRenderColor(FColor::White);
	ControllerHeaderText->SetHorizontalAlignment(EHTA_Center);
	ControllerHeaderText->SetVerticalAlignment(EVRTA_TextCenter);
	ControllerHeaderText->SetText(FText::FromString(TEXT("CONTROLLER")));

	MouseKBHeaderText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("MouseKBHeaderText"));
	MouseKBHeaderText->SetupAttachment(SettingsPageRoot);
	MouseKBHeaderText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	MouseKBHeaderText->SetWorldSize(3.5f);
	MouseKBHeaderText->SetTextRenderColor(FColor::White);
	MouseKBHeaderText->SetHorizontalAlignment(EHTA_Center);
	MouseKBHeaderText->SetVerticalAlignment(EVRTA_TextCenter);
	MouseKBHeaderText->SetText(FText::FromString(TEXT("MOUSE / KEYBOARD")));

	SettingsBackText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SettingsBackText"));
	SettingsBackText->SetupAttachment(SettingsPageRoot);
	SettingsBackText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	SettingsBackText->SetWorldSize(3.0f);
	SettingsBackText->SetHorizontalAlignment(EHTA_Center);
	SettingsBackText->SetVerticalAlignment(EVRTA_TextCenter);
	SettingsBackText->SetText(FText::FromString(TEXT("Back")));

	// --- Graphics page header + back ---
	GraphicsHeaderText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("GraphicsHeaderText"));
	GraphicsHeaderText->SetupAttachment(GraphicsPageRoot);
	GraphicsHeaderText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	GraphicsHeaderText->SetWorldSize(3.5f);
	GraphicsHeaderText->SetTextRenderColor(FColor::White);
	GraphicsHeaderText->SetHorizontalAlignment(EHTA_Center);
	GraphicsHeaderText->SetVerticalAlignment(EVRTA_TextCenter);
	GraphicsHeaderText->SetText(FText::FromString(TEXT("GRAPHICS")));

	GraphicsResetText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("GraphicsResetText"));
	GraphicsResetText->SetupAttachment(GraphicsPageRoot);
	GraphicsResetText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	GraphicsResetText->SetWorldSize(2.5f);
	GraphicsResetText->SetHorizontalAlignment(EHTA_Center);
	GraphicsResetText->SetVerticalAlignment(EVRTA_TextCenter);
	GraphicsResetText->SetText(FText::FromString(TEXT("Reset to Default")));

	GraphicsBackText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("GraphicsBackText"));
	GraphicsBackText->SetupAttachment(GraphicsPageRoot);
	GraphicsBackText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	GraphicsBackText->SetWorldSize(3.0f);
	GraphicsBackText->SetHorizontalAlignment(EHTA_Center);
	GraphicsBackText->SetVerticalAlignment(EVRTA_TextCenter);
	GraphicsBackText->SetText(FText::FromString(TEXT("Back")));

	// --- Tunables page fixed items ---
	TunablesHeaderText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TunablesHeaderText"));
	TunablesHeaderText->SetupAttachment(TunablesPageRoot);
	TunablesHeaderText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	TunablesHeaderText->SetWorldSize(3.5f);
	TunablesHeaderText->SetTextRenderColor(FColor::White);
	TunablesHeaderText->SetHorizontalAlignment(EHTA_Center);
	TunablesHeaderText->SetVerticalAlignment(EVRTA_TextCenter);
	TunablesHeaderText->SetText(FText::FromString(TEXT("TUNABLES")));

	TunablesBackText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TunablesBackText"));
	TunablesBackText->SetupAttachment(TunablesPageRoot);
	TunablesBackText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	TunablesBackText->SetWorldSize(3.0f);
	TunablesBackText->SetHorizontalAlignment(EHTA_Center);
	TunablesBackText->SetVerticalAlignment(EVRTA_TextCenter);
	TunablesBackText->SetText(FText::FromString(TEXT("Back")));

	TunablesHelpText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TunablesHelpText"));
	TunablesHelpText->SetupAttachment(TunablesPageRoot);
	TunablesHelpText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	TunablesHelpText->SetWorldSize(1.4f);
	TunablesHelpText->SetTextRenderColor(FColor(150, 150, 150));
	TunablesHelpText->SetHorizontalAlignment(EHTA_Center);
	TunablesHelpText->SetVerticalAlignment(EVRTA_TextCenter);

	TunablesMoreUpText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TunablesMoreUpText"));
	TunablesMoreUpText->SetupAttachment(TunablesPageRoot);
	TunablesMoreUpText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	TunablesMoreUpText->SetWorldSize(1.6f);
	TunablesMoreUpText->SetTextRenderColor(FColor(150, 150, 150));
	TunablesMoreUpText->SetHorizontalAlignment(EHTA_Center);
	TunablesMoreUpText->SetVerticalAlignment(EVRTA_TextCenter);
	TunablesMoreUpText->SetText(FText::FromString(TEXT("^ more")));

	TunablesMoreDownText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TunablesMoreDownText"));
	TunablesMoreDownText->SetupAttachment(TunablesPageRoot);
	TunablesMoreDownText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	TunablesMoreDownText->SetWorldSize(1.6f);
	TunablesMoreDownText->SetTextRenderColor(FColor(150, 150, 150));
	TunablesMoreDownText->SetHorizontalAlignment(EHTA_Center);
	TunablesMoreDownText->SetVerticalAlignment(EVRTA_TextCenter);
	TunablesMoreDownText->SetText(FText::FromString(TEXT("v more")));
}

void AMenuUIActor::BeginPlay()
{
	Super::BeginPlay();

	SolidColorMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_SolidColor.M_SolidColor"));
	if (!SolidColorMaterial)
	{
		UE_LOG(LogTemp, Error, TEXT("AMenuUIActor: /Game/Materials/M_SolidColor not found."));
		return;
	}

	if (BackgroundPanel)
	{
		BackgroundMaterial = UMaterialInstanceDynamic::Create(SolidColorMaterial, this);
		if (BackgroundMaterial)
		{
			BackgroundMaterial->SetVectorParameterValue(FName("Color"), BackgroundColor);
			BackgroundMaterial->SetVectorParameterValue(FName("BaseColor"), BackgroundColor);
			BackgroundMaterial->SetVectorParameterValue(FName("EmissiveColor"), BackgroundColor);
			BackgroundPanel->SetMaterial(0, BackgroundMaterial);
		}
	}

	BuildPausePage();
	BuildSettingsPage();
	BuildGraphicsPage();
	BuildTunablesPage();
	UpdateBackgroundSize();
	ApplyPageVisibility();
	UpdateFocusColors();

	// Self-illuminated UI (no RectLight): unlit text material so all menu labels
	// stay readable in dark areas. MI_MenuText (M_UnlitText at EmissiveScale 0.4, so menu text sits below bloom) tints by vertex color, so
	// UpdateFocusColors' per-option highlight colors still show.
	if (UMaterialInterface* TextMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/MI_MenuText.MI_MenuText")))
	{
		TArray<UTextRenderComponent*> TextComps;
		GetComponents<UTextRenderComponent>(TextComps);
		for (UTextRenderComponent* TextComp : TextComps)
		{
			TextComp->SetTextMaterial(TextMat);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AMenuUIActor: MI_MenuText not found; menu text may be dark."));
	}
}

// ---------------------------------------------------------------------------
// Build visuals
// ---------------------------------------------------------------------------

void AMenuUIActor::BuildPausePage()
{
	if (PausedHeaderText)   PausedHeaderText->SetRelativeLocation(FVector(0.0f, 0.0f, kPausedHeaderZ));
	if (PauseResumeText)    PauseResumeText->SetRelativeLocation(FVector(0.0f, 0.0f, kPauseResumeZ));
	if (PauseSettingsText)  PauseSettingsText->SetRelativeLocation(FVector(0.0f, 0.0f, kPauseSettingsZ));
	if (PauseGraphicsText)  PauseGraphicsText->SetRelativeLocation(FVector(0.0f, 0.0f, kPauseGraphicsZ));
	if (PauseTunablesText)  PauseTunablesText->SetRelativeLocation(FVector(0.0f, 0.0f, kPauseTunablesZ));
#if UE_BUILD_SHIPPING
	// No Tunables row: Quit takes its slot so there's no gap.
	if (PauseQuitText)      PauseQuitText->SetRelativeLocation(FVector(0.0f, 0.0f, kPauseTunablesZ));
#else
	if (PauseQuitText)      PauseQuitText->SetRelativeLocation(FVector(0.0f, 0.0f, kPauseQuitZ));
#endif
}

void AMenuUIActor::BuildGraphicsPage()
{
	if (GraphicsHeaderText) GraphicsHeaderText->SetRelativeLocation(FVector(0.0f, 0.0f, kGraphicsHeaderZ));
	if (GraphicsResetText)  GraphicsResetText->SetRelativeLocation(FVector(0.0f, 0.0f, kGraphicsResetZ));
	if (GraphicsBackText)   GraphicsBackText->SetRelativeLocation(FVector(0.0f, 0.0f, kGraphicsBackZ));

	BuildGraphicsRow(EGraphicsRow::GlobalIllumination, kGraphicsGIZ,     TEXT("Lumen GI"));
	BuildGraphicsRow(EGraphicsRow::Reflection,         kGraphicsReflZ,   TEXT("Reflections"));
	BuildGraphicsRow(EGraphicsRow::Shadow,             kGraphicsShadowZ, TEXT("Shadows"));
	BuildGraphicsRow(EGraphicsRow::ViewDistance,       kGraphicsVDZ,     TEXT("View Distance"));
}

void AMenuUIActor::BuildGraphicsRow(EGraphicsRow Row, float RowZ, const FString& Label)
{
	const int32 RowIdx = static_cast<int32>(Row);
	FGraphicsRowVisuals& R = GraphicsRows[RowIdx];

	R.RowText = NewObject<UTextRenderComponent>(this);
	R.RowText->SetupAttachment(GraphicsPageRoot);
	R.RowText->RegisterComponent();
	R.RowText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	R.RowText->SetWorldSize(2.8f);
	R.RowText->SetTextRenderColor(FColor(200, 200, 200));
	R.RowText->SetHorizontalAlignment(EHTA_Center);
	R.RowText->SetVerticalAlignment(EVRTA_TextCenter);
	R.RowText->SetRelativeLocation(FVector(0.0f, 0.0f, RowZ));

	const int32 CurrentValue = GetGraphicsCVarValue(Row);
	R.SelectedIndex = CurrentValue;
	R.RowText->SetText(FText::FromString(FString::Printf(TEXT("%s: %s"),
		*Label, *GetGraphicsQualityLabel(CurrentValue))));
}

void AMenuUIActor::BuildTunablesPage()
{
	if (TunablesHeaderText)   TunablesHeaderText->SetRelativeLocation(FVector(0.0f, 0.0f, kTunablesHeaderZ));
	if (TunablesMoreUpText)   TunablesMoreUpText->SetRelativeLocation(FVector(0.0f, 0.0f, kTunablesMoreUpZ));
	if (TunablesMoreDownText) TunablesMoreDownText->SetRelativeLocation(FVector(0.0f, 0.0f, kTunablesMoreDownZ));
	if (TunablesHelpText)     TunablesHelpText->SetRelativeLocation(FVector(0.0f, 0.0f, kTunablesHelpZ));
	if (TunablesBackText)     TunablesBackText->SetRelativeLocation(FVector(0.0f, 0.0f, kTunablesBackZ));

	TunableRowTexts.Empty(MaxVisibleTunableRows);
	for (int32 i = 0; i < MaxVisibleTunableRows; i++)
	{
		UTextRenderComponent* RowText = NewObject<UTextRenderComponent>(this);
		RowText->SetupAttachment(TunablesPageRoot);
		RowText->RegisterComponent();
		RowText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
		RowText->SetWorldSize(2.0f);
		RowText->SetTextRenderColor(FColor(200, 200, 200));
		RowText->SetHorizontalAlignment(EHTA_Center);
		RowText->SetVerticalAlignment(EVRTA_TextCenter);
		RowText->SetRelativeLocation(FVector(0.0f, 0.0f, kTunablesRowTopZ - i * kTunablesRowStepZ));
		TunableRowTexts.Add(RowText);
	}
}

// The system prefix of a tunable name: "weird.Storm.DimMultiplier" -> "Storm".
// Names without a system segment group under "Misc".
static FString TunableSystemOf(const FString& Name)
{
	FString Rest = Name;
	Rest.RemoveFromStart(TEXT("weird."));
	int32 DotIdx;
	return Rest.FindChar(TEXT('.'), DotIdx) ? Rest.Left(DotIdx) : FString(TEXT("Misc"));
}

void AMenuUIActor::RebuildTunablesPage()
{
	AllTunableCVars.Reset();
	IConsoleManager::Get().ForEachConsoleObjectThatStartsWith(
		FConsoleObjectVisitor::CreateLambda([this](const TCHAR* Name, IConsoleObject* Obj)
		{
			IConsoleVariable* Var = Obj->AsVariable();
			// String cvars can't be stepped and blow out the row layout — the
			// menu is bool/int/float only; strings stay console/uq-tunable.
			if (Var && (Var->IsVariableBool() || Var->IsVariableInt() || Var->IsVariableFloat()))
			{
				AllTunableCVars.Add({ Name, Var });
			}
		}),
		TEXT("weird."));

	if (AllTunableCVars.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("AMenuUIActor::RebuildTunablesPage - no weird.* console variables found"));
	}

	AllTunableCVars.Sort([](const FTunableCVar& A, const FTunableCVar& B) { return A.Name < B.Name; });

	TunableSystems.Reset();
	for (const FTunableCVar& T : AllTunableCVars)
	{
		TunableSystems.AddUnique(TunableSystemOf(T.Name));
	}
	ActiveTunableSystem = FMath::Clamp(ActiveTunableSystem, 0, FMath::Max(TunableSystems.Num() - 1, 0));

	RefreshTunablesTabs();
	SelectedTunableIndex = 0;
	RefreshTunablesRows();
}

void AMenuUIActor::RefreshTunablesTabs()
{
	// Grow the tab text pool if a new system appeared; reuse existing comps.
	// These are created after BeginPlay's unlit-material pass, so apply
	// MI_MenuText here or the tabs render lit (dark) instead of white.
	UMaterialInterface* TextMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/MI_MenuText.MI_MenuText"));
	while (TunableTabTexts.Num() < TunableSystems.Num())
	{
		UTextRenderComponent* TabText = NewObject<UTextRenderComponent>(this);
		TabText->SetupAttachment(TunablesPageRoot);
		TabText->RegisterComponent();
		TabText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
		TabText->SetWorldSize(1.8f);
		TabText->SetHorizontalAlignment(EHTA_Center);
		TabText->SetVerticalAlignment(EVRTA_TextCenter);
		if (TextMat)
		{
			TabText->SetTextMaterial(TextMat);
		}
		TunableTabTexts.Add(TabText);
	}

	const int32 NumTabs = TunableSystems.Num();
	const float Spacing = NumTabs > 0 ? kTunablesTabSpan / NumTabs : kTunablesTabSpan;
	for (int32 i = 0; i < TunableTabTexts.Num(); i++)
	{
		UTextRenderComponent* TabText = TunableTabTexts[i];
		if (!TabText)
		{
			continue;
		}
		if (i < NumTabs)
		{
			const float Y = (i - (NumTabs - 1) * 0.5f) * Spacing;
			TabText->SetRelativeLocation(FVector(0.0f, Y, kTunablesTabZ));
			TabText->SetText(FText::FromString(TunableSystems[i]));
			TabText->SetVisibility(CurrentPage == EMenuPage::Tunables);
		}
		else
		{
			TabText->SetVisibility(false);
		}
	}
}

void AMenuUIActor::RefreshTunablesRows()
{
	TunableCVars.Reset();
	if (TunableSystems.IsValidIndex(ActiveTunableSystem))
	{
		const FString& System = TunableSystems[ActiveTunableSystem];
		for (const FTunableCVar& T : AllTunableCVars)
		{
			if (TunableSystemOf(T.Name) == System)
			{
				TunableCVars.Add(T);
			}
		}
	}
	TunableScrollOffset = 0;
	SelectedTunableIndex = FMath::Clamp(SelectedTunableIndex, 0, TunableCVars.Num() + 1);
	RefreshTunablesDisplay();
}

void AMenuUIActor::RefreshTunablesDisplay()
{
	for (int32 i = 0; i < TunableRowTexts.Num(); i++)
	{
		UTextRenderComponent* RowText = TunableRowTexts[i];
		if (!RowText)
		{
			continue;
		}
		const int32 Idx = TunableScrollOffset + i;
		if (TunableCVars.IsValidIndex(Idx))
		{
			const FTunableCVar& T = TunableCVars[Idx];
			// Leaf name only — the active tab already names the system.
			FString DisplayName = T.Name;
			DisplayName.RemoveFromStart(TEXT("weird."));
			int32 DotIdx;
			if (DisplayName.FindChar(TEXT('.'), DotIdx))
			{
				DisplayName.RightChopInline(DotIdx + 1);
			}
			RowText->SetText(FText::FromString(FString::Printf(TEXT("%s: %s"),
				*DisplayName, *T.Var->GetString())));
			RowText->SetVisibility(CurrentPage == EMenuPage::Tunables);
		}
		else
		{
			RowText->SetVisibility(false);
		}
	}

	const bool bOnPage = (CurrentPage == EMenuPage::Tunables);
	if (TunablesMoreUpText)
	{
		TunablesMoreUpText->SetVisibility(bOnPage && TunableScrollOffset > 0);
	}
	if (TunablesMoreDownText)
	{
		TunablesMoreDownText->SetVisibility(bOnPage && TunableScrollOffset + MaxVisibleTunableRows < TunableCVars.Num());
	}

	if (TunablesHelpText)
	{
		FString Help;
		if (TunableCVars.IsValidIndex(SelectedTunableIndex - 1))
		{
			Help = TunableCVars[SelectedTunableIndex - 1].Var->GetHelp();
			int32 NewlineIdx;
			if (Help.FindChar(TEXT('\n'), NewlineIdx))
			{
				Help.LeftInline(NewlineIdx);
			}
		}
		TunablesHelpText->SetText(FText::FromString(Help));
	}

	UpdateFocusColors();
}

void AMenuUIActor::AdjustTunable(int32 Dir)
{
	if (SelectedTunableIndex == 0)
	{
		// Tab bar focused: left/right switches system instead of adjusting.
		if (TunableSystems.Num() > 0)
		{
			ActiveTunableSystem = (ActiveTunableSystem + Dir + TunableSystems.Num()) % TunableSystems.Num();
			RefreshTunablesRows();
		}
		return;
	}
	if (!TunableCVars.IsValidIndex(SelectedTunableIndex - 1))
	{
		return; // Back row focused
	}
	IConsoleVariable* Var = TunableCVars[SelectedTunableIndex - 1].Var;
	if (Var->IsVariableBool())
	{
		Var->Set(!Var->GetBool(), ECVF_SetByConsole);
	}
	else if (Var->IsVariableInt())
	{
		Var->Set(Var->GetInt() + Dir, ECVF_SetByConsole);
	}
	else if (Var->IsVariableFloat())
	{
		const float V = Var->GetFloat();
		const float Step = FMath::Max(FMath::Abs(V) * 0.1f, 0.01f);
		Var->Set(V + Dir * Step, ECVF_SetByConsole);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AMenuUIActor::AdjustTunable - %s has unsupported type"),
			*TunableCVars[SelectedTunableIndex - 1].Name);
		return;
	}
	RefreshTunablesDisplay();
}

void AMenuUIActor::BuildSettingsPage()
{
	if (ControllerHeaderText)
	{
		ControllerHeaderText->SetRelativeLocation(FVector(0.0f, 0.0f, kControllerHeaderZ));
	}
	if (MouseKBHeaderText)
	{
		MouseKBHeaderText->SetRelativeLocation(FVector(0.0f, 0.0f, kMouseKBHeaderZ));
	}
	if (SettingsBackText)
	{
		SettingsBackText->SetRelativeLocation(FVector(0.0f, 0.0f, kSettingsBackZ));
	}

	BuildSettingsRow(ESettingsRow::GamepadSensitivity, kControllerLabelZ, kControllerValueZ, TEXT("Look Sensitivity"));
	BuildSettingsRow(ESettingsRow::MouseSensitivity,   kMouseKBLabelZ,   kMouseKBValueZ,    TEXT("Look Sensitivity"));
}

void AMenuUIActor::BuildSettingsRow(ESettingsRow Row, float LabelZ, float ValueZ, const FString& Label)
{
	const int32 RowIdx = static_cast<int32>(Row);
	FSettingsRowVisuals& R = SettingsRows[RowIdx];
	R.SlotCount = GetSlotCountForRow(Row);

	R.LabelText = NewObject<UTextRenderComponent>(this);
	R.LabelText->SetupAttachment(SettingsPageRoot);
	R.LabelText->RegisterComponent();
	R.LabelText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	R.LabelText->SetWorldSize(2.8f);
	R.LabelText->SetTextRenderColor(FColor(200, 200, 200));
	R.LabelText->SetHorizontalAlignment(EHTA_Center);
	R.LabelText->SetVerticalAlignment(EVRTA_TextCenter);
	R.LabelText->SetRelativeLocation(FVector(0.0f, 0.0f, LabelZ));
	R.LabelText->SetText(FText::FromString(Label));

	R.ValueText = NewObject<UTextRenderComponent>(this);
	R.ValueText->SetupAttachment(SettingsPageRoot);
	R.ValueText->RegisterComponent();
	R.ValueText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	R.ValueText->SetWorldSize(3.0f);
	R.ValueText->SetTextRenderColor(FColor(200, 200, 200));
	R.ValueText->SetHorizontalAlignment(EHTA_Center);
	R.ValueText->SetVerticalAlignment(EVRTA_TextCenter);
	R.ValueText->SetRelativeLocation(FVector(0.0f, 0.0f, ValueZ));
	R.ValueText->SetText(FText::FromString(TEXT("1.00x")));
}

// ---------------------------------------------------------------------------
// Background sizing — sized for the larger Settings page so swaps don't
// resize the panel. Pause page floats inside the same frame.
// ---------------------------------------------------------------------------

void AMenuUIActor::UpdateBackgroundSize()
{
	if (!BackgroundPanel)
	{
		return;
	}

	const float TotalWidth  = 50.0f + BackgroundPadding * 2.0f;
	const float TopZ        = kControllerHeaderZ + 6.0f;
	const float BottomZ     = kSettingsBackZ - 6.0f;
	const float TotalHeight = (TopZ - BottomZ) + BackgroundPadding * 2.0f;
	const float CenterZ     = (TopZ + BottomZ) * 0.5f;

	BackgroundPanel->SetRelativeScale3D(FVector(TotalHeight * 0.01f, TotalWidth * 0.01f, 1.0f));
	BackgroundPanel->SetRelativeLocation(FVector(1.0f, 0.0f, CenterZ));
}

void AMenuUIActor::ApplyPageVisibility()
{
	if (PausePageRoot)
	{
		PausePageRoot->SetVisibility(CurrentPage == EMenuPage::Pause, true);
	}
	if (SettingsPageRoot)
	{
		SettingsPageRoot->SetVisibility(CurrentPage == EMenuPage::Settings, true);
	}
	if (GraphicsPageRoot)
	{
		GraphicsPageRoot->SetVisibility(CurrentPage == EMenuPage::Graphics, true);
	}
	if (TunablesPageRoot)
	{
		TunablesPageRoot->SetVisibility(CurrentPage == EMenuPage::Tunables, true);
	}
}

// ---------------------------------------------------------------------------
// Focus colors — focused item is yellow, unfocused is gray
// ---------------------------------------------------------------------------

void AMenuUIActor::UpdateFocusColors()
{
	const uint8 Alpha = FMath::Clamp(static_cast<int32>(CurrentOpacity * 255), 0, 255);

	FColor Focused = FocusedValueColor.ToFColor(true);
	Focused.A = Alpha;
	FColor Unfocused = UnfocusedValueColor.ToFColor(true);
	Unfocused.A = Alpha;

	// Pause page
	if (PauseResumeText)
	{
		PauseResumeText->SetTextRenderColor(PauseSelection == EPauseMenuItem::Resume ? Focused : Unfocused);
	}
	if (PauseSettingsText)
	{
		PauseSettingsText->SetTextRenderColor(PauseSelection == EPauseMenuItem::Settings ? Focused : Unfocused);
	}
	if (PauseGraphicsText)
	{
		PauseGraphicsText->SetTextRenderColor(PauseSelection == EPauseMenuItem::Graphics ? Focused : Unfocused);
	}
	if (PauseTunablesText)
	{
		PauseTunablesText->SetTextRenderColor(PauseSelection == EPauseMenuItem::Tunables ? Focused : Unfocused);
	}
	if (PauseQuitText)
	{
		PauseQuitText->SetTextRenderColor(PauseSelection == EPauseMenuItem::Quit ? Focused : Unfocused);
	}

	// Settings page sensitivity rows
	for (int32 i = 0; i < SettingsRowCount; i++)
	{
		const ESettingsRow Row = static_cast<ESettingsRow>(i);
		if (Row == ESettingsRow::Back)
		{
			continue;
		}
		FSettingsRowVisuals& R = SettingsRows[i];
		if (!R.ValueText)
		{
			continue;
		}
		const bool bFocused = (Row == SettingsSelection);
		R.ValueText->SetTextRenderColor(bFocused ? Focused : Unfocused);
	}

	// Settings page Back row
	if (SettingsBackText)
	{
		SettingsBackText->SetTextRenderColor(SettingsSelection == ESettingsRow::Back ? Focused : Unfocused);
	}

	// Graphics page rows
	for (int32 i = 0; i < GraphicsRowCount; i++)
	{
		const EGraphicsRow Row = static_cast<EGraphicsRow>(i);
		if (Row == EGraphicsRow::Back)
		{
			continue;
		}
		FGraphicsRowVisuals& R = GraphicsRows[i];
		if (!R.RowText)
		{
			continue;
		}
		R.RowText->SetTextRenderColor(Row == GraphicsSelection ? Focused : Unfocused);
	}
	if (GraphicsResetText)
	{
		GraphicsResetText->SetTextRenderColor(GraphicsSelection == EGraphicsRow::ResetToDefault ? Focused : Unfocused);
	}
	if (GraphicsBackText)
	{
		GraphicsBackText->SetTextRenderColor(GraphicsSelection == EGraphicsRow::Back ? Focused : Unfocused);
	}

	// Tunables page: tab bar, cvar rows, Back.
	// Active tab: yellow while the tab bar row is focused, white otherwise;
	// inactive tabs gray.
	FColor ActiveTab = FColor::White;
	ActiveTab.A = Alpha;
	for (int32 i = 0; i < TunableTabTexts.Num(); i++)
	{
		if (UTextRenderComponent* TabText = TunableTabTexts[i])
		{
			const bool bActive = (i == ActiveTunableSystem);
			TabText->SetTextRenderColor(bActive ? (SelectedTunableIndex == 0 ? Focused : ActiveTab) : Unfocused);
		}
	}
	for (int32 i = 0; i < TunableRowTexts.Num(); i++)
	{
		if (UTextRenderComponent* RowText = TunableRowTexts[i])
		{
			RowText->SetTextRenderColor(TunableScrollOffset + i == SelectedTunableIndex - 1 ? Focused : Unfocused);
		}
	}
	if (TunablesBackText)
	{
		TunablesBackText->SetTextRenderColor(SelectedTunableIndex > TunableCVars.Num() ? Focused : Unfocused);
	}
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void AMenuUIActor::SetPage(EMenuPage NewPage)
{
	CurrentPage = NewPage;
	switch (NewPage)
	{
	case EMenuPage::Pause:
		PauseSelection = EPauseMenuItem::Resume;
		break;
	case EMenuPage::Settings:
		SettingsSelection = ESettingsRow::GamepadSensitivity;
		break;
	case EMenuPage::Graphics:
		GraphicsSelection = EGraphicsRow::GlobalIllumination;
		break;
	case EMenuPage::Tunables:
		SelectedTunableIndex = 0;
		TunableScrollOffset = 0;
		break;
	}
	ApplyPageVisibility();
	if (NewPage == EMenuPage::Tunables)
	{
		RebuildTunablesPage(); // also fixes row visibility after the propagate above
	}
	UpdateFocusColors();
}

int32 AMenuUIActor::GetSelectedIndex() const
{
	switch (CurrentPage)
	{
	case EMenuPage::Pause:    return static_cast<int32>(PauseSelection);
	case EMenuPage::Settings: return static_cast<int32>(SettingsSelection);
	case EMenuPage::Graphics: return static_cast<int32>(GraphicsSelection);
	case EMenuPage::Tunables: return SelectedTunableIndex;
	}
	return 0;
}

void AMenuUIActor::StepSelection(int32 Delta)
{
	switch (CurrentPage)
	{
	case EMenuPage::Pause:
	{
		int32 NewIdx = FMath::Clamp(static_cast<int32>(PauseSelection) + Delta, 0, PauseItemCount - 1);
#if UE_BUILD_SHIPPING
		// Tunables is dev-only: step over it (it's never at either end of the list).
		if (static_cast<EPauseMenuItem>(NewIdx) == EPauseMenuItem::Tunables)
		{
			NewIdx = FMath::Clamp(NewIdx + (Delta >= 0 ? 1 : -1), 0, PauseItemCount - 1);
		}
#endif
		PauseSelection = static_cast<EPauseMenuItem>(NewIdx);
		break;
	}
	case EMenuPage::Settings:
	{
		const int32 NewIdx = FMath::Clamp(static_cast<int32>(SettingsSelection) + Delta, 0, SettingsRowCount - 1);
		SettingsSelection = static_cast<ESettingsRow>(NewIdx);
		break;
	}
	case EMenuPage::Graphics:
	{
		const int32 NewIdx = FMath::Clamp(static_cast<int32>(GraphicsSelection) + Delta, 0, GraphicsRowCount - 1);
		GraphicsSelection = static_cast<EGraphicsRow>(NewIdx);
		break;
	}
	case EMenuPage::Tunables:
	{
		// 0 = tab bar, 1..Num = cvar rows, Num+1 = Back.
		SelectedTunableIndex = FMath::Clamp(SelectedTunableIndex + Delta, 0, TunableCVars.Num() + 1);
		const int32 Row = SelectedTunableIndex - 1;
		if (Row >= 0 && Row < TunableCVars.Num())
		{
			if (Row < TunableScrollOffset)
			{
				TunableScrollOffset = Row;
			}
			else if (Row >= TunableScrollOffset + MaxVisibleTunableRows)
			{
				TunableScrollOffset = Row - MaxVisibleTunableRows + 1;
			}
		}
		RefreshTunablesDisplay();
		break;
	}
	}
	UpdateFocusColors();
}

void AMenuUIActor::StepLeftRight(int32 Delta, UWeirdplaceGameUserSettings* Settings)
{
	if (CurrentPage == EMenuPage::Tunables)
	{
		AdjustTunable(Delta);
		return;
	}

	if (CurrentPage == EMenuPage::Graphics)
	{
		if (GraphicsSelection == EGraphicsRow::Back || GraphicsSelection == EGraphicsRow::ResetToDefault)
		{
			return;
		}
		const int32 RowIdx = static_cast<int32>(GraphicsSelection);
		FGraphicsRowVisuals& R = GraphicsRows[RowIdx];
		const int32 NewIndex = FMath::Clamp(R.SelectedIndex + Delta, 0, GraphicsQualityLevels - 1);
		if (NewIndex == R.SelectedIndex)
		{
			return;
		}
		R.SelectedIndex = NewIndex;
		SetGraphicsCVarValue(GraphicsSelection, NewIndex);

		if (R.RowText)
		{
			// Reconstruct "Label: Value" — extract Label from existing text.
			const FString Existing = R.RowText->Text.ToString();
			int32 ColonIdx;
			const FString LabelPart = Existing.FindChar(':', ColonIdx)
				? Existing.Left(ColonIdx)
				: Existing;
			R.RowText->SetText(FText::FromString(FString::Printf(TEXT("%s: %s"),
				*LabelPart, *GetGraphicsQualityLabel(NewIndex))));
		}
		return;
	}

	if (CurrentPage != EMenuPage::Settings)
	{
		return;
	}
	if (SettingsSelection == ESettingsRow::Back)
	{
		return;
	}
	if (!Settings)
	{
		UE_LOG(LogTemp, Error, TEXT("AMenuUIActor::StepLeftRight - null Settings"));
		return;
	}

	const int32 RowIdx = static_cast<int32>(SettingsSelection);
	FSettingsRowVisuals& R = SettingsRows[RowIdx];

	const int32 NewIndex = FMath::Clamp(R.SelectedIndex + Delta, 0, R.SlotCount - 1);
	if (NewIndex == R.SelectedIndex)
	{
		return;
	}

	R.SelectedIndex = NewIndex;
	const float NewValue = SlotIndexToValue(SettingsSelection, R.SelectedIndex);
	SetSettingValue(SettingsSelection, NewValue, Settings);
	const float Snapped = GetSettingValue(SettingsSelection, Settings);
	if (R.ValueText)
	{
		R.ValueText->SetText(FText::FromString(FString::Printf(TEXT("%.2fx"), Snapped)));
	}
}

void AMenuUIActor::SyncFromSettings(UWeirdplaceGameUserSettings* Settings)
{
	if (!Settings)
	{
		UE_LOG(LogTemp, Error, TEXT("AMenuUIActor::SyncFromSettings - null Settings"));
		return;
	}
	for (int32 i = 0; i < SettingsRowCount; i++)
	{
		const ESettingsRow Row = static_cast<ESettingsRow>(i);
		if (Row == ESettingsRow::Back)
		{
			continue;
		}
		const float Value = GetSettingValue(Row, Settings);
		SettingsRows[i].SelectedIndex = ValueToSlotIndex(Row, Value);
		if (SettingsRows[i].ValueText)
		{
			SettingsRows[i].ValueText->SetText(FText::FromString(FString::Printf(TEXT("%.2fx"), Value)));
		}
	}
	UpdateFocusColors();
}

void AMenuUIActor::SetOpacity(float Opacity)
{
	CurrentOpacity = Opacity;

	if (BackgroundMaterial)
	{
		FLinearColor Adjusted = BackgroundColor;
		Adjusted.A *= Opacity;
		BackgroundMaterial->SetVectorParameterValue(FName("BaseColor"), Adjusted);
	}

	const uint8 Alpha = FMath::Clamp(static_cast<int32>(Opacity * 255), 0, 255);
	const FColor HeaderColor(255, 255, 255, Alpha);
	const FColor DimColor(200, 200, 200, Alpha);

	if (PausedHeaderText)     PausedHeaderText->SetTextRenderColor(HeaderColor);
	if (ControllerHeaderText) ControllerHeaderText->SetTextRenderColor(HeaderColor);
	if (MouseKBHeaderText)    MouseKBHeaderText->SetTextRenderColor(HeaderColor);
	if (GraphicsHeaderText)   GraphicsHeaderText->SetTextRenderColor(HeaderColor);

	for (int32 i = 0; i < SettingsRowCount; i++)
	{
		FSettingsRowVisuals& R = SettingsRows[i];
		if (R.LabelText) R.LabelText->SetTextRenderColor(DimColor);
	}

	UpdateFocusColors();
}

// ---------------------------------------------------------------------------
// Settings read/write
// ---------------------------------------------------------------------------

float AMenuUIActor::GetSettingValue(ESettingsRow Row, UWeirdplaceGameUserSettings* Settings) const
{
	switch (Row)
	{
	case ESettingsRow::GamepadSensitivity: return Settings->GetGamepadLookSensitivity();
	case ESettingsRow::MouseSensitivity:   return Settings->GetMouseLookSensitivity();
	default: return 1.0f;
	}
}

void AMenuUIActor::SetSettingValue(ESettingsRow Row, float Value, UWeirdplaceGameUserSettings* Settings)
{
	switch (Row)
	{
	case ESettingsRow::GamepadSensitivity: Settings->SetGamepadLookSensitivity(Value); break;
	case ESettingsRow::MouseSensitivity:   Settings->SetMouseLookSensitivity(Value);   break;
	default: break;
	}
}

int32 AMenuUIActor::GetSlotCountForRow(ESettingsRow Row) const
{
	float Min, Max, Snap;
	switch (Row)
	{
	case ESettingsRow::GamepadSensitivity:
		Min  = UWeirdplaceGameUserSettings::MinGamepadLookSensitivity;
		Max  = UWeirdplaceGameUserSettings::MaxGamepadLookSensitivity;
		Snap = UWeirdplaceGameUserSettings::GamepadLookSensitivitySnap;
		break;
	case ESettingsRow::MouseSensitivity:
		Min  = UWeirdplaceGameUserSettings::MinMouseLookSensitivity;
		Max  = UWeirdplaceGameUserSettings::MaxMouseLookSensitivity;
		Snap = UWeirdplaceGameUserSettings::MouseLookSensitivitySnap;
		break;
	default:
		return 1;
	}
	return FMath::RoundToInt((Max - Min) / Snap) + 1;
}

int32 AMenuUIActor::ValueToSlotIndex(ESettingsRow Row, float Value) const
{
	float Min, Snap;
	switch (Row)
	{
	case ESettingsRow::GamepadSensitivity:
		Min  = UWeirdplaceGameUserSettings::MinGamepadLookSensitivity;
		Snap = UWeirdplaceGameUserSettings::GamepadLookSensitivitySnap;
		break;
	case ESettingsRow::MouseSensitivity:
		Min  = UWeirdplaceGameUserSettings::MinMouseLookSensitivity;
		Snap = UWeirdplaceGameUserSettings::MouseLookSensitivitySnap;
		break;
	default:
		return 0;
	}
	const int32 Index = FMath::RoundToInt((Value - Min) / Snap);
	return FMath::Clamp(Index, 0, GetSlotCountForRow(Row) - 1);
}

float AMenuUIActor::SlotIndexToValue(ESettingsRow Row, int32 Index) const
{
	float Min, Snap;
	switch (Row)
	{
	case ESettingsRow::GamepadSensitivity:
		Min  = UWeirdplaceGameUserSettings::MinGamepadLookSensitivity;
		Snap = UWeirdplaceGameUserSettings::GamepadLookSensitivitySnap;
		break;
	case ESettingsRow::MouseSensitivity:
		Min  = UWeirdplaceGameUserSettings::MinMouseLookSensitivity;
		Snap = UWeirdplaceGameUserSettings::MouseLookSensitivitySnap;
		break;
	default:
		return 1.0f;
	}
	return Min + Index * Snap;
}

// ---------------------------------------------------------------------------
// Graphics page cvar wiring
// ---------------------------------------------------------------------------

const TCHAR* AMenuUIActor::GetGraphicsCVarName(EGraphicsRow Row)
{
	switch (Row)
	{
	case EGraphicsRow::GlobalIllumination: return TEXT("sg.GlobalIlluminationQuality");
	case EGraphicsRow::Reflection:         return TEXT("sg.ReflectionQuality");
	case EGraphicsRow::Shadow:             return TEXT("sg.ShadowQuality");
	case EGraphicsRow::ViewDistance:       return TEXT("sg.ViewDistanceQuality");
	default:                               return nullptr;
	}
}

FString AMenuUIActor::GetGraphicsQualityLabel(int32 QualityLevel)
{
	switch (QualityLevel)
	{
	case 0: return TEXT("Low");
	case 1: return TEXT("Medium");
	case 2: return TEXT("High");
	case 3: return TEXT("Epic");
	default: return FString::Printf(TEXT("%d"), QualityLevel);
	}
}

int32 AMenuUIActor::GetGraphicsCVarValue(EGraphicsRow Row) const
{
	const TCHAR* CVarName = GetGraphicsCVarName(Row);
	if (!CVarName)
	{
		return 0;
	}
	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(CVarName))
	{
		return FMath::Clamp(CVar->GetInt(), 0, GraphicsQualityLevels - 1);
	}
	return 0;
}

void AMenuUIActor::SetGraphicsCVarValue(EGraphicsRow Row, int32 Value)
{
	const TCHAR* CVarName = GetGraphicsCVarName(Row);
	if (!CVarName)
	{
		return;
	}
	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(CVarName))
	{
		CVar->Set(Value, ECVF_SetByConsole);
	}
}

void AMenuUIActor::ResetGraphicsToDefaults()
{
	UDeviceProfile* Profile = UDeviceProfileManager::Get().GetActiveProfile();
	if (!Profile)
	{
		UE_LOG(LogTemp, Warning, TEXT("AMenuUIActor::ResetGraphicsToDefaults - no active device profile"));
		return;
	}

	// Only touch the four CVars this menu manages — device profiles often include
	// audio/streaming/etc. that we shouldn't blast on a "Reset Graphics" click.
	static const TCHAR* const ManagedCVars[] = {
		TEXT("sg.GlobalIlluminationQuality"),
		TEXT("sg.ReflectionQuality"),
		TEXT("sg.ShadowQuality"),
		TEXT("sg.ViewDistanceQuality"),
	};

	for (const FString& Entry : Profile->CVars)
	{
		FString Name, Value;
		if (!Entry.Split(TEXT("="), &Name, &Value))
		{
			continue;
		}
		Name.TrimStartAndEndInline();
		Value.TrimStartAndEndInline();

		bool bIsManaged = false;
		for (const TCHAR* Managed : ManagedCVars)
		{
			if (Name.Equals(Managed, ESearchCase::IgnoreCase))
			{
				bIsManaged = true;
				break;
			}
		}
		if (!bIsManaged)
		{
			continue;
		}

		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*Name))
		{
			CVar->Set(*Value, ECVF_SetByConsole);
		}
	}

	SyncGraphicsFromCVars();
}

void AMenuUIActor::SyncGraphicsFromCVars()
{
	struct FRowSpec { EGraphicsRow Row; const TCHAR* Label; };
	static const FRowSpec Specs[] = {
		{ EGraphicsRow::GlobalIllumination, TEXT("Lumen GI") },
		{ EGraphicsRow::Reflection,         TEXT("Reflections") },
		{ EGraphicsRow::Shadow,             TEXT("Shadows") },
		{ EGraphicsRow::ViewDistance,       TEXT("View Distance") },
	};
	for (const FRowSpec& Spec : Specs)
	{
		const int32 RowIdx = static_cast<int32>(Spec.Row);
		FGraphicsRowVisuals& R = GraphicsRows[RowIdx];
		const int32 Value = GetGraphicsCVarValue(Spec.Row);
		R.SelectedIndex = Value;
		if (R.RowText)
		{
			R.RowText->SetText(FText::FromString(FString::Printf(TEXT("%s: %s"),
				Spec.Label, *GetGraphicsQualityLabel(Value))));
		}
	}
	UpdateFocusColors();
}
