// Fill out your copyright notice in the Description page of Project Settings.


#include "MovieBox.h"

// Sets default values
AMovieBox::AMovieBox()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AMovieBox::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AMovieBox::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AMovieBox::InteractWithObject(AActor* Actor)
{
	Actor->SetActorHiddenInGame(true);
	 if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(
                -1,                    // Unique key (-1 means auto-remove)
                5.f,                   // Duration (seconds)
                FColor::Green,         // Text color
                FString::Printf(TEXT("%s has been hidden"), *Actor->GetName()) // Message
            );
        }
}
