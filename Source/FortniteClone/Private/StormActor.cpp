// Fill out your copyright notice in the Description page of Project Settings.

#include "StormActor.h"
#include "FortniteCloneCharacter.h"
#include "UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
AStormActor::AStormActor()
{
	bReplicates = true;
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	Damage=1;
	StormAdvanceStageRate=30.f; //Default every 30 seconds
	StormIncreaseDamageRate=120.f; //Default every 120 seconds
	IsShrinking=false;
	InitialSizeScale=GetActorScale3D();
	InitialActorLocation=GetActorLocation();
	Stage=0; //Every odd stage it shrinks, even stages players get a break from it...
	FMath::SRandInit(FPlatformTime::Cycles());

	//Auto generate thresholds and modifiers with specified resolution: ex. 10.f = 10 linear scaling progression levels, etc...
	StormAdvancement.AutoGenerateThresholds(7.f);

	//3.f generates -> {0.3333f, 0.0033f}, {0.6666f, 0.0066f}, {1.f, 0.01f};
	//StormAdvancement.AutoGenerateThresholds(3.f);
	//4.f generates -> {0.25f, 0.0025f}, {0.5f, 0.005f}, {0.75f, 0.0075f}, {1.f, 0.01f};
	//StormAdvancement.AutoGenerateThresholds(4.f); //etc...

	//Or manual thresholds and modifiers setup #1
	/*StormAdvancement.Add(1.f,0.0099f);
	StormAdvancement.Add(0.5f,0.0066f);
	StormAdvancement.Add(0.1f,0.0033f);*/

	//manual thresholds and modifiers setup #2, etc
	/*StormAdvancement.Add(1.f,0.0088f);
	StormAdvancement.Add(0.66f,0.0044f);
	StormAdvancement.Add(0.33f,0.0022f);
	StormAdvancement.Add(0.1f,0.0011f);*/

	IConsoleVariable *StormRestartCmd=IConsoleManager::Get().RegisterConsoleVariable(TEXT("StormRestart"),
		2,
		TEXT("AdminCommand: Run 'StormRestart 1' to restart the storm on demand.\n"),
		ECVF_Scalability | ECVF_RenderThreadSafe);

	StormRestartCmd->AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateLambda([&] (IConsoleVariable* Var){ InitializeStorm(); }));
}

void AStormActor::InitializeStorm()
{
	if(HasAuthority())
	{
		Damage=1;
		IsShrinking=false;
		Stage=0;
		//move the storm to a random location on the map
		int32 X=FMath::RandRange(-7000 + 10000,60000 - 10000);
		int32 Y=FMath::RandRange(-60000 + 10000,17000 - 10000);
		SetActorLocation(FVector(X,Y,InitialActorLocation.Z));

		SizeScale=InitialSizeScale;
		SetActorScale3D(InitialSizeScale);

		GetWorldTimerManager().SetTimer(StormStageTimerHandle,this,&AStormActor::ServerAdvanceStage,StormAdvanceStageRate,true);
		GetWorldTimerManager().SetTimer(StormDamageTimerHandle,this,&AStormActor::ServerSetNewDamage,StormIncreaseDamageRate,true);

		FString LogMsg="StormProgression=";
		for(auto& progression : StormAdvancement.Progressions)
			LogMsg+=FString::Printf(_T("{%.04f, %.04f}, "),progression.ScaleThreshold,progression.ScaleModifier);
		GEngine->AddOnScreenDebugMessage(1,5.f,FColor::Yellow,LogMsg);
	}
}

// Called when the game starts or when spawned
void AStormActor::BeginPlay()
{
	Super::BeginPlay();
	InitializeStorm();
}

// Called every frame
void AStormActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if(HasAuthority() && IsShrinking)
	{
		//With the storm's performance fixed, now scale down slower as it gets smaller (because it seems like it closes up too fast towards the end)
		//Also incorporate the DeltaTime now too so that it scales down consistently always the same regardless of framerate...
		SetActorScale3D(SizeScale=StormAdvancement.AdvanceStorm(SizeScale.X,SizeScale.Z,DeltaTime));

		FString LogMsg=FString::Printf(_T("Using DeltaTime incorporated scaling: Stage=%i SizeScale.XY=%.08f"),Stage,SizeScale.X);
		GEngine->AddOnScreenDebugMessage(1,5.f,FColor::Green,LogMsg);
	}
}

void AStormActor::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AStormActor, Damage);
	DOREPLIFETIME(AStormActor, StormAdvanceStageRate);
	DOREPLIFETIME(AStormActor, StormIncreaseDamageRate);
	DOREPLIFETIME(AStormActor, IsShrinking);
	DOREPLIFETIME(AStormActor, InitialSizeScale);
	DOREPLIFETIME(AStormActor, InitialActorLocation);
	DOREPLIFETIME(AStormActor, SizeScale);
}

void AStormActor::ServerAdvanceStage_Implementation()
{		
	//Advance stages until storm is down to almost nothing
	//Usually by this point either the game has probably ended or we reinitalize the storm and start again...
	if(SizeScale.X <= StormAdvancement.ExtremelyLowScale)
		return InitializeStorm();
	//If 1/10th the scale make this the last stage and keep shrinking until there's a winner / end of game, or the storm re-initializes due to being extremely tiny as above
	if(SizeScale.X <= 0.1f)
		return;

	IsShrinking=(++Stage % 2); //Every odd numbered stage advance the shrinking!
}

bool AStormActor::ServerAdvanceStage_Validate() {
	return true;
}

void AStormActor::ServerSetNewDamage_Implementation() {
	Damage++;
}

bool AStormActor::ServerSetNewDamage_Validate() {
	return true;
}

void AStormActor::ServerStartStorm_Implementation() {
	InitializeStorm();
}

bool AStormActor::ServerStartStorm_Validate() {
	return true;
}