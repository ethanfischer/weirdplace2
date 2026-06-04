#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShadowMan.generated.h"

class USkeletalMeshComponent;
class UCarRideComponent;

UCLASS()
class WEIRDPLACE2_API AShadowMan : public AActor
{
	GENERATED_BODY()

public:
	AShadowMan();

	// Body mesh — assign MetaHuman SK + fully-black unlit material in BP defaults.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShadowMan")
	USkeletalMeshComponent* BodyMesh;

	// Distance behind the player camera to teleport to on appearance (cm).
	UPROPERTY(EditAnywhere, Category = "ShadowMan")
	float SpawnDistanceBehindPlayer = 400.f;

	// Cumulative gaze seconds before kill fires.
	UPROPERTY(EditAnywhere, Category = "ShadowMan")
	float StareTimeUntilKill = 2.f;

	// Level-placed actor that owns a UCarRideComponent (e.g. BP_Car). Set on the
	// level instance — kill triggers a car-ride replay on that component.
	UPROPERTY(EditInstanceOnly, Category = "ShadowMan")
	AActor* CarRideOwner = nullptr;

	void StartHaunting();
	void StopHaunting();

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

private:
	bool bHaunting = false;
	float StareElapsed = 0.f;

	bool IsPlayerLookingAt(const FVector& Position) const;
	bool IsPlayerLookingAtMe() const;
	void TriggerKill();
	void TeleportBehindPlayer();
	void SetMeshVisible(bool bVisible);
};
