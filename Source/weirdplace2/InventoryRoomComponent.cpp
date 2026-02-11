#include "InventoryRoomComponent.h"
#include "Inventory.h"
#include "MyCharacter.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/TargetPoint.h"
#include "MovieBoxDisplayActor.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/WidgetComponent.h"
#include "Components/StaticMeshComponent.h"

UInventoryRoomComponent::UInventoryRoomComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	GridSpacing = 30.0f;
	GridVerticalSpacing = 30.0f;
	WallOffset = 20.0f;
	ItemNameText = nullptr;
	BackgroundMaterialInstance = nullptr;
	OriginalWallMaterial = nullptr;
}

void UInventoryRoomComponent::BeginPlay()
{
	Super::BeginPlay();

	// Auto-find InventoryRoomTarget by tag if not set
	if (!InventoryRoomTarget)
	{
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("InventoryRoom"), FoundActors);
		if (FoundActors.Num() > 0)
		{
			InventoryRoomTarget = FoundActors[0];
		}
	}

	// Auto-find BackgroundWallActor by tag if not set
	if (!BackgroundWallActor)
	{
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("InventoryBackgroundWall"), FoundActors);
		if (FoundActors.Num() > 0)
		{
			BackgroundWallActor = FoundActors[0];
		}
	}

	// Auto-load blur material if not set
	if (!BackgroundBlurMaterial)
	{
		BackgroundBlurMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_BlurredBackground"));
	}

	// Load BP_MovieBox as default display class (matches world appearance)
	if (!MovieBoxDisplayActorClass || MovieBoxDisplayActorClass == AMovieBoxDisplayActor::StaticClass())
	{
		UClass* MovieBoxClass = LoadClass<AActor>(nullptr, TEXT("/Game/Blueprints/BP_MovieBox.BP_MovieBox_C"));
		if (MovieBoxClass)
		{
			MovieBoxDisplayActorClass = MovieBoxClass;
		}
	}

	// Load default item mappings at runtime (avoids editor startup issues)
	if (ItemDisplayMappings.Num() == 0)
	{
		// BP_Key for KEY_BASEMENT
		UClass* KeyClass = LoadClass<AActor>(nullptr, TEXT("/Game/Blueprints/BP_Key.BP_Key_C"));
		if (KeyClass)
		{
			FInventoryItemDisplayInfo KeyInfo;
			KeyInfo.ItemID = FName("KEY_BASEMENT");
			KeyInfo.DisplayActorClass = KeyClass;
			KeyInfo.DisplayScale = FVector(1.0f);
			ItemDisplayMappings.Add(KeyInfo);
		}
	}

	// Find the inventory component on the owner (use MyCharacter's getter to get the correct one)
	AActor* Owner = GetOwner();
	if (Owner)
	{
		// Cast to AMyCharacter and use the explicit getter to avoid duplicate component issues
		if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(Owner))
		{
			InventoryComponent = MyCharacter->GetInventoryComponent();
		}
		else
		{
			// Fallback for non-MyCharacter owners
			InventoryComponent = Owner->FindComponentByClass<UInventoryComponent>();
		}

		if (InventoryComponent)
		{
			// Bind to inventory changes to refresh display while in room
			InventoryComponent->OnInventoryChanged.AddDynamic(this, &UInventoryRoomComponent::OnInventoryChanged);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("UInventoryRoomComponent: No UInventoryComponent found on owner!"));
		}
	}
}

void UInventoryRoomComponent::ToggleInventoryRoom()
{
	if (bIsInInventoryRoom)
	{
		TeleportBack();
	}
	else
	{
		TeleportToInventoryRoom();
	}
}

void UInventoryRoomComponent::TeleportToInventoryRoom()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	// Store current transform
	StoredLocation = Owner->GetActorLocation();
	StoredRotation = Owner->GetActorRotation();

	// Capture the player's view BEFORE teleporting
	CaptureAndApplyBackground();

	// Get teleport destination from target point or fallback to hardcoded location
	FVector TeleportLocation = InventoryRoomLocation;
	FRotator TeleportRotation = InventoryRoomRotation;

	if (InventoryRoomTarget)
	{
		TeleportLocation = InventoryRoomTarget->GetActorLocation();
		TeleportRotation = InventoryRoomTarget->GetActorRotation();
	}

	// Teleport to inventory room
	Owner->SetActorLocation(TeleportLocation);
	Owner->SetActorRotation(TeleportRotation);

	// Update controller rotation for first-person view
	if (ACharacter* Character = Cast<ACharacter>(Owner))
	{
		if (AController* Controller = Character->GetController())
		{
			Controller->SetControlRotation(TeleportRotation);
		}
	}

	bIsInInventoryRoom = true;

	// Spawn inventory display actors
	SpawnInventoryDisplayActors();
}

void UInventoryRoomComponent::TeleportBack()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	// Clean up spawned actors first
	DestroyInventoryDisplayActors();

	// Restore original wall material
	RestoreWallMaterial();

	// Teleport back to stored location
	Owner->SetActorLocation(StoredLocation);
	Owner->SetActorRotation(StoredRotation);

	// Update controller rotation
	if (ACharacter* Character = Cast<ACharacter>(Owner))
	{
		if (AController* Controller = Character->GetController())
		{
			Controller->SetControlRotation(StoredRotation);
		}
	}

	bIsInInventoryRoom = false;
}

void UInventoryRoomComponent::SpawnInventoryDisplayActors()
{
	if (!InventoryComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnInventoryDisplayActors: InventoryComponent is null!"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	TArray<FName> Items = InventoryComponent->GetItems();

	// Get base location and rotation from target or fallback
	FVector BaseLocation = InventoryRoomTarget ? InventoryRoomTarget->GetActorLocation() : InventoryRoomLocation;
	FRotator BaseRotation = InventoryRoomTarget ? InventoryRoomTarget->GetActorRotation() : InventoryRoomRotation;

	int32 SpawnIndex = 0;

	for (int32 i = 0; i < Items.Num(); i++)
	{
		const FName& ItemID = Items[i];

		// Check if there's a custom display mapping for this item
		const FInventoryItemDisplayInfo* DisplayInfo = GetDisplayInfo(ItemID);

		// Calculate spawn location
		FVector SpawnLocation = CalculateItemPosition(SpawnIndex++);

		// Face the player (or room target) so displays are oriented toward the viewer
		FVector LookAtTarget = BaseLocation;
		if (ACharacter* CharacterOwner = Cast<ACharacter>(GetOwner()))
		{
			LookAtTarget = CharacterOwner->GetActorLocation();
			if (AController* Controller = CharacterOwner->GetController())
			{
				FVector CameraLoc;
				FRotator CameraRot;
				Controller->GetPlayerViewPoint(CameraLoc, CameraRot);
				LookAtTarget = CameraLoc;
			}
		}

		FVector ToTarget = LookAtTarget - SpawnLocation;
		FRotator SpawnRotation = ToTarget.IsNearlyZero() ? BaseRotation : ToTarget.Rotation();
		SpawnRotation.Pitch = 0.0f;
		SpawnRotation.Roll = 0.0f;

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AActor* SpawnedActor = nullptr;

		if (DisplayInfo && DisplayInfo->DisplayActorClass)
		{
			// Use custom display actor for this item
			SpawnRotation.Yaw += DisplayInfo->DisplayRotation.Yaw;

			SpawnedActor = World->SpawnActor<AActor>(DisplayInfo->DisplayActorClass, SpawnLocation, SpawnRotation, SpawnParams);

			if (SpawnedActor && DisplayInfo->DisplayScale != FVector(1.0f, 1.0f, 1.0f))
			{
				SpawnedActor->SetActorScale3D(DisplayInfo->DisplayScale);
			}
		}
		else if (MovieBoxDisplayActorClass)
		{
			// Default: use movie box display for VHS covers
			SpawnedActor = World->SpawnActor<AActor>(MovieBoxDisplayActorClass, SpawnLocation, SpawnRotation, SpawnParams);

			if (SpawnedActor)
			{
				// Disable interactivity for display actors
				SpawnedActor->SetActorTickEnabled(false);
				if (UWidgetComponent* Widget = SpawnedActor->FindComponentByClass<UWidgetComponent>())
				{
					Widget->SetVisibility(false);
				}

				// Apply cover material
				FString ItemString = ItemID.ToString();
				FString MaterialPath = FString::Printf(TEXT("/Game/CreatedMaterials/VHSCoverMaterials/MI_VHSCover_%s"), *ItemString);
				UMaterialInterface* CoverMaterial = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);

				if (AMovieBoxDisplayActor* DisplayActor = Cast<AMovieBoxDisplayActor>(SpawnedActor))
				{
					DisplayActor->SetCoverMaterial(CoverMaterial);
					DisplayActor->SetCoverName(ItemString);
				}
				else if (CoverMaterial)
				{
					if (UStaticMeshComponent* MeshComp = SpawnedActor->FindComponentByClass<UStaticMeshComponent>())
					{
						MeshComp->SetMaterial(0, CoverMaterial);
					}
				}

				// Store cover name in map for look-at detection
				ActorCoverNames.Add(SpawnedActor, ItemString);
			}
		}

		if (SpawnedActor)
		{
			// Disable collision so player can walk through items
			SpawnedActor->SetActorEnableCollision(false);

			// Put spawned actor in the Inventory folder in Outliner
			#if WITH_EDITOR
			SpawnedActor->SetFolderPath(InventoryFolderPath);
			#endif

			SpawnedDisplayActors.Add(SpawnedActor);
		}
	}

	// Create the item name text component below the grid center
	if (SpawnIndex > 0)
	{
		// Calculate center position of the grid and offset downward
		FVector TextPosition = CalculateItemPosition(0);
		// Center horizontally based on total columns used
		int32 NumCols = FMath::Min(SpawnIndex, GridColumns);
		float HorizontalCenter = (NumCols - 1) * GridSpacing * 0.5f;

		FVector RightDir = FRotationMatrix(BaseRotation).GetScaledAxis(EAxis::Y);

		// Position at the first column's X/Z but centered horizontally
		TextPosition = TextPosition + RightDir * HorizontalCenter;
		TextPosition.Z -= TextVerticalOffset;

		// Create text render component attached to owner
		AActor* Owner = GetOwner();
		if (Owner)
		{
			ItemNameText = NewObject<UTextRenderComponent>(Owner);
			if (ItemNameText)
			{
				ItemNameText->RegisterComponent();
				ItemNameText->SetWorldLocation(TextPosition);
				// Rotate 180 degrees so text faces the player
				ItemNameText->SetWorldRotation(FRotator(0.0f, BaseRotation.Yaw + 180.0f, 0.0f));
				ItemNameText->SetText(FText::GetEmpty());
				ItemNameText->SetWorldSize(TextWorldSize);
				ItemNameText->SetTextRenderColor(TextColor);
				ItemNameText->SetHorizontalAlignment(EHTA_Center);
				ItemNameText->SetVerticalAlignment(EVRTA_TextCenter);

				// Apply custom material if set (use an unlit material to avoid shadows)
				if (TextMaterial)
				{
					ItemNameText->SetTextMaterial(TextMaterial);
				}

				CurrentLookedAtItem = TEXT("");
			}
		}
	}
}

void UInventoryRoomComponent::DestroyInventoryDisplayActors()
{
	for (AActor* Actor : SpawnedDisplayActors)
	{
		if (Actor && IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	SpawnedDisplayActors.Empty();
	ActorCoverNames.Empty();

	// Clean up the text component
	if (ItemNameText)
	{
		ItemNameText->DestroyComponent();
		ItemNameText = nullptr;
	}
	CurrentLookedAtItem = TEXT("");
}

const FInventoryItemDisplayInfo* UInventoryRoomComponent::GetDisplayInfo(const FName& ItemID) const
{
	for (const FInventoryItemDisplayInfo& Info : ItemDisplayMappings)
	{
		if (Info.ItemID == ItemID)
		{
			return &Info;
		}
	}
	return nullptr;
}

FVector UInventoryRoomComponent::CalculateItemPosition(int32 Index) const
{
	int32 Row = Index / GridColumns;
	int32 Col = Index % GridColumns;
	float CenterOffset = (GridColumns - 1) * GridSpacing * 0.5f;

	// Get base location and rotation from InventoryRoomTarget
	FVector BaseLocation = InventoryRoomTarget ? InventoryRoomTarget->GetActorLocation() : InventoryRoomLocation;
	FRotator BaseRotation = InventoryRoomTarget ? InventoryRoomTarget->GetActorRotation() : InventoryRoomRotation;

	FVector ForwardDir = BaseRotation.Vector();
	FVector RightDir = FRotationMatrix(BaseRotation).GetScaledAxis(EAxis::Y);
	FVector UpDir = FVector::UpVector;

	// Raycast from TargetPoint forward to find wall
	FVector TraceStart = BaseLocation;
	FVector TraceEnd = BaseLocation + ForwardDir * 10000.0f;

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());

	float WallDistance = ItemDisplayDistance; // Fallback

	if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
	{
		WallDistance = HitResult.Distance - WallOffset;
	}

	// Calculate position: X from raycast, Y from TargetPoint, Z for grid layout
	FVector Position;
	Position.X = BaseLocation.X + ForwardDir.X * WallDistance;
	Position.Y = BaseLocation.Y + RightDir.Y * ((Col * GridSpacing) - CenterOffset);
	Position.Z = BaseLocation.Z + ItemDisplayHeight - (Row * GridVerticalSpacing);

	return Position;
}

void UInventoryRoomComponent::OnInventoryChanged(const TArray<FName>& CurrentItems)
{
	// If we're in the inventory room, refresh the display
	if (bIsInInventoryRoom)
	{
		DestroyInventoryDisplayActors();
		SpawnInventoryDisplayActors();
	}
}

void UInventoryRoomComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Only update when in inventory room
	if (bIsInInventoryRoom)
	{
		UpdateLookedAtItem();
	}
}

void UInventoryRoomComponent::UpdateLookedAtItem()
{
	if (!ItemNameText) return;

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character) return;

	AController* Controller = Character->GetController();
	if (!Controller) return;

	// Get camera view point
	FVector CameraLocation;
	FRotator CameraRotation;
	Controller->GetPlayerViewPoint(CameraLocation, CameraRotation);

	FVector LookDirection = CameraRotation.Vector();
	FVector TraceEnd = CameraLocation + LookDirection * 500.0f;

	FString NewLookedAtItem = TEXT("");
	float BestDistance = FLT_MAX;

	// Check each spawned display actor using the cover name map
	for (const auto& Pair : ActorCoverNames)
	{
		AActor* Actor = Pair.Key;
		const FString& CoverName = Pair.Value;

		if (!Actor || CoverName.IsEmpty()) continue;

		// Get actor bounds
		FVector Origin, Extent;
		Actor->GetActorBounds(false, Origin, Extent);

		// Create a box for the bounds
		FBox ActorBox(Origin - Extent, Origin + Extent);

		// Check if look ray intersects the bounding box
		FVector HitLocation, HitNormal;
		float HitTime;
		if (FMath::LineExtentBoxIntersection(ActorBox, CameraLocation, TraceEnd, FVector::ZeroVector, HitLocation, HitNormal, HitTime))
		{
			float Distance = FVector::Dist(CameraLocation, HitLocation);
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				NewLookedAtItem = CoverName;
			}
		}
	}

	// Only update text if it changed
	if (NewLookedAtItem != CurrentLookedAtItem)
	{
		CurrentLookedAtItem = NewLookedAtItem;
		ItemNameText->SetText(FText::FromString(CurrentLookedAtItem));
	}
}

void UInventoryRoomComponent::CaptureAndApplyBackground()
{
	// Need wall actor and blur material to proceed
	if (!BackgroundWallActor || !BackgroundBlurMaterial)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	UGameViewportClient* ViewportClient = World->GetGameViewport();
	if (!ViewportClient) return;

	FViewport* Viewport = ViewportClient->Viewport;
	if (!Viewport) return;

	// Read pixels from the current frame (exactly what player sees)
	TArray<FColor> Pixels;
	if (!Viewport->ReadPixels(Pixels))
	{
		UE_LOG(LogTemp, Warning, TEXT("CaptureAndApplyBackground: Failed to read viewport pixels"));
		return;
	}

	int32 Width = Viewport->GetSizeXY().X;
	int32 Height = Viewport->GetSizeXY().Y;

	if (Width == 0 || Height == 0 || Pixels.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("CaptureAndApplyBackground: Invalid viewport size"));
		return;
	}

	// Apply Gaussian blur (two-pass separable blur for efficiency)
	int32 BlurRadius = 15;
	TArray<float> Kernel;
	Kernel.SetNum(BlurRadius * 2 + 1);
	float Sigma = BlurRadius / 2.0f;
	float Sum = 0.0f;
	for (int32 i = -BlurRadius; i <= BlurRadius; i++)
	{
		float Value = FMath::Exp(-(i * i) / (2 * Sigma * Sigma));
		Kernel[i + BlurRadius] = Value;
		Sum += Value;
	}
	for (int32 i = 0; i < Kernel.Num(); i++) Kernel[i] /= Sum;

	// Horizontal pass
	TArray<FColor> TempPixels;
	TempPixels.SetNum(Pixels.Num());
	for (int32 y = 0; y < Height; y++)
	{
		for (int32 x = 0; x < Width; x++)
		{
			float r = 0, g = 0, b = 0;
			for (int32 k = -BlurRadius; k <= BlurRadius; k++)
			{
				int32 SampleX = FMath::Clamp(x + k, 0, Width - 1);
				FColor& Sample = Pixels[y * Width + SampleX];
				float Weight = Kernel[k + BlurRadius];
				r += Sample.R * Weight;
				g += Sample.G * Weight;
				b += Sample.B * Weight;
			}
			TempPixels[y * Width + x] = FColor(r, g, b, 255);
		}
	}

	// Vertical pass
	TArray<FColor> BlurredPixels;
	BlurredPixels.SetNum(Pixels.Num());
	for (int32 y = 0; y < Height; y++)
	{
		for (int32 x = 0; x < Width; x++)
		{
			float r = 0, g = 0, b = 0;
			for (int32 k = -BlurRadius; k <= BlurRadius; k++)
			{
				int32 SampleY = FMath::Clamp(y + k, 0, Height - 1);
				FColor& Sample = TempPixels[SampleY * Width + x];
				float Weight = Kernel[k + BlurRadius];
				r += Sample.R * Weight;
				g += Sample.G * Weight;
				b += Sample.B * Weight;
			}
			BlurredPixels[y * Width + x] = FColor(r, g, b, 255);
		}
	}

	// Rotate blurred pixels 90° clockwise to match wall UV orientation
	int32 FinalWidth = Height;
	int32 FinalHeight = Width;
	TArray<FColor> RotatedPixels;
	RotatedPixels.SetNum(BlurredPixels.Num());
	for (int32 y = 0; y < Height; y++)
	{
		for (int32 x = 0; x < Width; x++)
		{
			int32 SrcIdx = y * Width + x;
			int32 DstIdx = x * FinalWidth + (Height - 1 - y);
			RotatedPixels[DstIdx] = BlurredPixels[SrcIdx];
		}
	}

	// Create texture from rotated pixels
	UTexture2D* CapturedTexture = UTexture2D::CreateTransient(FinalWidth, FinalHeight, PF_B8G8R8A8);
	if (!CapturedTexture) return;

	// Copy pixel data to texture
	void* TextureData = CapturedTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(TextureData, RotatedPixels.GetData(), RotatedPixels.Num() * sizeof(FColor));
	CapturedTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
	CapturedTexture->UpdateResource();

	// Scale wall to match viewport aspect ratio (wall is rotated 90° so scale Z for width)
	float ViewportAspect = (float)FinalWidth / (float)FinalHeight;
	OriginalWallScale = BackgroundWallActor->GetActorScale3D();
	BackgroundWallActor->SetActorScale3D(FVector(OriginalWallScale.X * ViewportAspect, OriginalWallScale.Y, OriginalWallScale.Z) * 2);

	// Store original material and create dynamic material instance
	UStaticMeshComponent* WallMesh = BackgroundWallActor->FindComponentByClass<UStaticMeshComponent>();
	if (WallMesh)
	{
		// Store original material (only first time)
		if (!OriginalWallMaterial)
		{
			OriginalWallMaterial = WallMesh->GetMaterial(0);
		}

		// Create dynamic material instance
		BackgroundMaterialInstance = UMaterialInstanceDynamic::Create(BackgroundBlurMaterial, this);
		if (BackgroundMaterialInstance)
		{
			// Set the captured texture as parameter
			BackgroundMaterialInstance->SetTextureParameterValue(FName("BackgroundTexture"), CapturedTexture);

			// Apply to wall
			WallMesh->SetMaterial(0, BackgroundMaterialInstance);
		}
	}
}

void UInventoryRoomComponent::RestoreWallMaterial()
{
	if (!BackgroundWallActor) return;

	// Restore original scale
	if (!OriginalWallScale.IsZero())
	{
		BackgroundWallActor->SetActorScale3D(OriginalWallScale);
	}

	UStaticMeshComponent* WallMesh = BackgroundWallActor->FindComponentByClass<UStaticMeshComponent>();
	if (WallMesh && OriginalWallMaterial)
	{
		WallMesh->SetMaterial(0, OriginalWallMaterial);
	}

	// Clean up
	BackgroundMaterialInstance = nullptr;
}
