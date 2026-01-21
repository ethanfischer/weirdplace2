#include "InventoryRoomComponent.h"
#include "Inventory.h"
#include "MyCharacter.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/TargetPoint.h"
#include "MovieBoxDisplayActor.h"
#include "Materials/MaterialInterface.h"

UInventoryRoomComponent::UInventoryRoomComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
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

	// Load default item mappings at runtime (avoids editor startup issues)
	UE_LOG(LogTemp, Log, TEXT("ItemDisplayMappings.Num() = %d"), ItemDisplayMappings.Num());
	if (ItemDisplayMappings.Num() == 0)
	{
		// BP_Envelope for InventoryItem1 (collected from MovieBox)
		UClass* EnvelopeClass = LoadClass<AActor>(nullptr, TEXT("/Game/Blueprints/BP_Envelope.BP_Envelope_C"));
		UE_LOG(LogTemp, Log, TEXT("LoadClass BP_Envelope: %s"), EnvelopeClass ? TEXT("SUCCESS") : TEXT("FAILED"));
		if (EnvelopeClass)
		{
			FInventoryItemDisplayInfo EnvelopeInfo;
			EnvelopeInfo.ItemType = EInventoryItem::InventoryItem1;
			EnvelopeInfo.DisplayActorClass = EnvelopeClass;
			EnvelopeInfo.DisplayScale = FVector(1.0f);
			ItemDisplayMappings.Add(EnvelopeInfo);
		}

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
					FRotator SpawnRotation = BaseRotation;

					FActorSpawnParameters SpawnParams;
					SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

					AActor* SpawnedActor = World->SpawnActor<AActor>(MovieBoxDisplayActorClass, SpawnLocation, SpawnRotation, SpawnParams);
					if (SpawnedActor)
					{
						SpawnedActor->SetActorEnableCollision(false);
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
						}
					else if (CoverMaterial)
					{
						if (UStaticMeshComponent* MeshComp = SpawnedActor->FindComponentByClass<UStaticMeshComponent>())
						{
							MeshComp->SetMaterial(0, CoverMaterial);
						}
					}

						SpawnedDisplayActors.Add(SpawnedActor);
						UE_LOG(LogTemp, Log, TEXT("Spawned movie cover %s at %s"), *CoverString, *SpawnLocation.ToString());
					}
				}

				bSpawnedMovieCovers = true;
				continue;
			}
		}

		// Fallback to existing mapping
		const FInventoryItemDisplayInfo* DisplayInfo = GetDisplayInfo(Item);

		if (!DisplayInfo || !DisplayInfo->DisplayActorClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("No display actor class mapped for inventory item %d"), static_cast<int32>(Item));
			continue;
		}

		FVector SpawnLocation = CalculateItemPosition(SpawnIndex++);
		FRotator SpawnRotation = BaseRotation + DisplayInfo->DisplayRotation;

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
	// Use grid layout in front of player
	int32 Row = Index / GridColumns;
	int32 Col = Index % GridColumns;

	// Center the grid horizontally
	float CenterOffset = (GridColumns - 1) * GridSpacing * 0.5f;

	// Get base location and rotation from target or fallback
	FVector BaseLocation = InventoryRoomTarget ? InventoryRoomTarget->GetActorLocation() : InventoryRoomLocation;
	FRotator BaseRotation = InventoryRoomTarget ? InventoryRoomTarget->GetActorRotation() : InventoryRoomRotation;

	// Get forward direction from room rotation
	FVector ForwardDir = BaseRotation.Vector();
	FVector RightDir = FRotationMatrix(BaseRotation).GetScaledAxis(EAxis::Y);

	FVector Position = BaseLocation;
	Position += ForwardDir * ItemDisplayDistance;                    // In front
	Position += RightDir * ((Col * GridSpacing) - CenterOffset);     // Left/right
	Position.Z += ItemDisplayHeight - (Row * GridSpacing);           // Stack rows vertically

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
