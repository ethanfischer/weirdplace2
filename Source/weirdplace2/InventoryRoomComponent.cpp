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
			UE_LOG(LogTemp, Log, TEXT("Found InventoryRoomTarget by tag: %s"), *InventoryRoomTarget->GetName());
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
			UE_LOG(LogTemp, Log, TEXT("Found BackgroundWallActor by tag: %s"), *BackgroundWallActor->GetName());
		}
	}

	// Auto-load blur material if not set
	if (!BackgroundBlurMaterial)
	{
		BackgroundBlurMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_BlurredBackground"));
		if (BackgroundBlurMaterial)
		{
			UE_LOG(LogTemp, Log, TEXT("Loaded M_BlurredBackground as background material"));
		}
	}

	// Load BP_MovieBox as default display class (matches world appearance)
	if (!MovieBoxDisplayActorClass || MovieBoxDisplayActorClass == AMovieBoxDisplayActor::StaticClass())
	{
		UClass* MovieBoxClass = LoadClass<AActor>(nullptr, TEXT("/Game/Blueprints/BP_MovieBox.BP_MovieBox_C"));
		if (MovieBoxClass)
		{
			MovieBoxDisplayActorClass = MovieBoxClass;
			UE_LOG(LogTemp, Log, TEXT("Loaded BP_MovieBox as display class"));
		}
	}

	// Load default item mappings at runtime (avoids editor startup issues)
	UE_LOG(LogTemp, Log, TEXT("ItemDisplayMappings.Num() = %d"), ItemDisplayMappings.Num());
	if (ItemDisplayMappings.Num() == 0)
	{
		// BP_Key for InventoryItem2
		UClass* KeyClass = LoadClass<AActor>(nullptr, TEXT("/Game/Blueprints/BP_Key.BP_Key_C"));
		UE_LOG(LogTemp, Log, TEXT("LoadClass BP_Key: %s"), KeyClass ? TEXT("SUCCESS") : TEXT("FAILED"));
		if (KeyClass)
		{
			FInventoryItemDisplayInfo KeyInfo;
			KeyInfo.ItemType = EInventoryItem::InventoryItem2;
			KeyInfo.DisplayActorClass = KeyClass;
			KeyInfo.DisplayScale = FVector(1.0f);
			ItemDisplayMappings.Add(KeyInfo);
		}
	}

	// Find the inventory component on the owner (use MyCharacter's getter to get the correct one)
	AActor* Owner = GetOwner();
	UE_LOG(LogTemp, Log, TEXT("InventoryRoomComponent Owner: %s"), Owner ? *Owner->GetName() : TEXT("NULL"));
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
			UE_LOG(LogTemp, Log, TEXT("Found InventoryComponent: %s on %s (ptr: %p)"), *InventoryComponent->GetName(), *InventoryComponent->GetOwner()->GetName(), InventoryComponent);
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

	UE_LOG(LogTemp, Log, TEXT("Entered Inventory Room at %s"), *TeleportLocation.ToString());
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

	UE_LOG(LogTemp, Log, TEXT("Returned from Inventory Room to %s"), *StoredLocation.ToString());
}

void UInventoryRoomComponent::SpawnInventoryDisplayActors()
{
	UE_LOG(LogTemp, Log, TEXT("SpawnInventoryDisplayActors called"));

	if (!InventoryComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnInventoryDisplayActors: InventoryComponent is null!"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	TArray<EInventoryItem> Items = InventoryComponent->GetInventoryItems();
	UE_LOG(LogTemp, Log, TEXT("Inventory has %d items, ItemDisplayMappings has %d mappings (ptr: %p)"),
		Items.Num(), ItemDisplayMappings.Num(), InventoryComponent);

	// Get base location and rotation from target or fallback
	FVector BaseLocation = InventoryRoomTarget ? InventoryRoomTarget->GetActorLocation() : InventoryRoomLocation;
	FRotator BaseRotation = InventoryRoomTarget ? InventoryRoomTarget->GetActorRotation() : InventoryRoomRotation;

	int32 SpawnIndex = 0;
	bool bSpawnedMovieCovers = false;

	for (int32 i = 0; i < Items.Num(); i++)
	{
		EInventoryItem Item = Items[i];

		// Special handling for movie boxes: spawn one per collected cover
		if (!bSpawnedMovieCovers && Item == EInventoryItem::InventoryItem1 && InventoryComponent)
		{
			const TArray<FName>& Covers = InventoryComponent->GetMovieCovers();
			if (Covers.Num() > 0 && *MovieBoxDisplayActorClass)
			{
				for (int32 CoverIdx = 0; CoverIdx < Covers.Num(); ++CoverIdx)
				{
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

					AActor* SpawnedActor = World->SpawnActor<AActor>(MovieBoxDisplayActorClass, SpawnLocation, SpawnRotation, SpawnParams);
					if (SpawnedActor)
					{
						SpawnedActor->SetActorEnableCollision(false);

						// Disable interactivity for display actors
						SpawnedActor->SetActorTickEnabled(false);
						if (UWidgetComponent* Widget = SpawnedActor->FindComponentByClass<UWidgetComponent>())
						{
							Widget->SetVisibility(false);
						}

						#if WITH_EDITOR
						SpawnedActor->SetFolderPath(InventoryFolderPath);
						#endif

						// Apply cover material
						FString CoverString = Covers[CoverIdx].ToString();
						FString MaterialPath = FString::Printf(TEXT("/Game/CreatedMaterials/VHSCoverMaterials/MI_VHSCover_%s"), *CoverString);
						UMaterialInterface* CoverMaterial = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);

						if (AMovieBoxDisplayActor* DisplayActor = Cast<AMovieBoxDisplayActor>(SpawnedActor))
						{
							DisplayActor->SetCoverMaterial(CoverMaterial);
							DisplayActor->SetCoverName(CoverString);
						}
						else if (CoverMaterial)
						{
							if (UStaticMeshComponent* MeshComp = SpawnedActor->FindComponentByClass<UStaticMeshComponent>())
							{
								MeshComp->SetMaterial(0, CoverMaterial);
							}
						}

						// Store cover name in map for look-at detection (works even if not AMovieBoxDisplayActor)
						ActorCoverNames.Add(SpawnedActor, CoverString);

						SpawnedDisplayActors.Add(SpawnedActor);
						UE_LOG(LogTemp, Log, TEXT("Spawned movie cover %s at %s"), *CoverString, *SpawnLocation.ToString());
					}
				}

				bSpawnedMovieCovers = true;
				continue;
			}
		}

		// Skip InventoryItem1 if we've already spawned movie covers for it
		if (bSpawnedMovieCovers && Item == EInventoryItem::InventoryItem1)
		{
			continue;
		}

		// Fallback to existing mapping
		const FInventoryItemDisplayInfo* DisplayInfo = GetDisplayInfo(Item);

		if (!DisplayInfo || !DisplayInfo->DisplayActorClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("No display actor class mapped for inventory item %d"), static_cast<int32>(Item));
			continue;
		}

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
		SpawnRotation.Yaw += DisplayInfo->DisplayRotation.Yaw;

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AActor* SpawnedActor = World->SpawnActor<AActor>(DisplayInfo->DisplayActorClass, SpawnLocation, SpawnRotation, SpawnParams);

		if (SpawnedActor)
		{
			// Apply custom scale if specified
			if (DisplayInfo->DisplayScale != FVector(1.0f, 1.0f, 1.0f))
			{
				SpawnedActor->SetActorScale3D(DisplayInfo->DisplayScale);
			}

			// Disable collision so player can walk through items
			SpawnedActor->SetActorEnableCollision(false);

			// Put spawned actor in the Inventory folder in Outliner
			#if WITH_EDITOR
			SpawnedActor->SetFolderPath(InventoryFolderPath);
			#endif

			SpawnedDisplayActors.Add(SpawnedActor);
			UE_LOG(LogTemp, Log, TEXT("Spawned display actor for item %d at %s"),
				static_cast<int32>(Item), *SpawnLocation.ToString());
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
				UE_LOG(LogTemp, Log, TEXT("Created ItemNameText at %s"), *TextPosition.ToString());
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

const FInventoryItemDisplayInfo* UInventoryRoomComponent::GetDisplayInfo(EInventoryItem Item) const
{
	for (const FInventoryItemDisplayInfo& Info : ItemDisplayMappings)
	{
		if (Info.ItemType == Item)
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

void UInventoryRoomComponent::OnInventoryChanged(const TArray<EInventoryItem>& CurrentInventory)
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
		UE_LOG(LogTemp, Log, TEXT("CaptureAndApplyBackground: Missing BackgroundWallActor or BackgroundBlurMaterial"));
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

	// Rotate pixels 90° clockwise to match wall UV orientation
	int32 FinalWidth = Height;
	int32 FinalHeight = Width;
	TArray<FColor> RotatedPixels;
	RotatedPixels.SetNum(Pixels.Num());
	for (int32 y = 0; y < Height; y++)
	{
		for (int32 x = 0; x < Width; x++)
		{
			int32 SrcIdx = y * Width + x;
			int32 DstIdx = x * FinalWidth + (Height - 1 - y);
			RotatedPixels[DstIdx] = Pixels[SrcIdx];
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
			UE_LOG(LogTemp, Log, TEXT("Applied captured background to wall (%dx%d)"), FinalWidth, FinalHeight);
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
		UE_LOG(LogTemp, Log, TEXT("Restored original wall material and scale"));
	}

	// Clean up
	BackgroundMaterialInstance = nullptr;
}
