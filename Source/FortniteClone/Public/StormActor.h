// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StormActor.generated.h"

UCLASS()
class FORTNITECLONE_API AStormActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AStormActor();
	void InitializeStorm();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY()
	FTimerHandle StormSetupTimerHandle;
	
	UPROPERTY()
	FTimerHandle StormDamageTimerHandle;

	UPROPERTY(Replicated)
	float Damage;

	UPROPERTY(Replicated)
	float StormAdvanceStageRate;

	UPROPERTY(Replicated)
	float StormIncreaseDamageRate;

	UPROPERTY(Replicated)
	bool IsShrinking;

	UPROPERTY(Replicated)
	FVector InitialSizeScale;
	
	UPROPERTY(Replicated)
	FVector InitialActorLocation;

	UPROPERTY(Replicated)
	FVector SizeScale;

	UPROPERTY()
	float ScaleDownRate;

	UPROPERTY()
	float InverseScaleDownRate;

	UPROPERTY()
	float ScaleHighThreshold;

	UPROPERTY()
	float ScaleMidThreshold;

	UPROPERTY()
	float ScaleLowThreshold;

	UPROPERTY()
	float ScaleHighModifier;

	UPROPERTY()
	float ScaleMidModifier;

	UPROPERTY()
	float ScaleLowModifier;

	UPROPERTY()
	int Stage;

	virtual bool IsSupportedForNetworking() const override
	{
		return true;
	}

	UFUNCTION(Server, Reliable, WithValidation)
	void AdvanceStage();

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSetNewDamage();

	UFUNCTION(Server,Reliable,WithValidation)
	void ServerStartStorm();

};
