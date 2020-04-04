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
	ScaleDownRate=0.999485f;
	InverseScaleDownRate=0.000515f;
	ScaleHighThreshold=1.f;
	ScaleMidThreshold=0.5f;
	ScaleLowThreshold=0.1f;
	ScaleHighModifier=0.0099f;
	ScaleMidModifier=0.0066f;
	ScaleLowModifier=0.0033f;
	Stage = 0; //Every odd stage it shrinks, even stages player's get a break from it...
	FMath::SRandInit(FPlatformTime::Cycles());

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

		GetWorldTimerManager().SetTimer(StormSetupTimerHandle,this,&AStormActor::AdvanceStage,StormAdvanceStageRate,true);
		GetWorldTimerManager().SetTimer(StormDamageTimerHandle,this,&AStormActor::ServerSetNewDamage,StormIncreaseDamageRate,true);
	}
}

// Called when the game starts or when spawned
void AStormActor::BeginPlay()
{
	Super::BeginPlay();
	InitializeStorm();

	/*FString LogMsg = FString("storm actor constructor ") + FString::FromInt(X) + FString(" ") + FString::FromInt(Y);
	UE_LOG(LogMyGame, Warning, TEXT("%s"), *LogMsg);*/
	//after 30 seconds, start shrinking the circle at the last 30 seconds of every 2 and a half minute interval
	//FTimerHandle StormSetupTimerHandle;
	//GetWorldTimerManager().SetTimer(StormSetupTimerHandle, this, &AStormActor::ServerStartStorm, 30.0f, false);
	/*LogMsg = FString("begin play circle ") + FString::FromInt(GetNetMode());
	UE_LOG(LogMyGame, Warning, TEXT("%s"), *LogMsg);*/
	
	/*FString LogMsg = FString("begin play circle ") + FString::FromInt(GetNetMode());
	UE_LOG(LogMyGame, Warning, TEXT("%s"), *LogMsg);*/
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString("Storm Begin Play ") + FString::FromInt(GetNetMode()));
}

// Called every frame
void AStormActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if(HasAuthority() && IsShrinking)
	{
		//With the storm's performance fixed, now scale down slower as it gets smaller (because it seems like it closes up too fast towards the end)
		//Also incorporate the DeltaTime now too so that it scales down consistently always the same regardless of framerate...
		//The ScaleDownRate, Scale/Low/Mid/High/Modifier can still be tweaked to find the optimal scaling down speed for players...
		float newScaleXY=0.f;
		if(SizeScale.X <= ScaleLowThreshold)
			newScaleXY=(SizeScale.X - (ScaleDownRate * DeltaTime * ScaleLowModifier));
		else if(SizeScale.X <= ScaleMidThreshold)
			newScaleXY=(SizeScale.X - (ScaleDownRate * DeltaTime * ScaleMidModifier));
		else if(SizeScale.X <= ScaleHighThreshold)
			newScaleXY=(SizeScale.X - (ScaleDownRate * DeltaTime * ScaleHighModifier));
		else if(SizeScale.X > ScaleHighThreshold)
			newScaleXY=ScaleHighThreshold;

		if(SizeScale.X > 0.00000009f) //Only scale down until it's extremely tiny
		{
			SetActorScale3D(SizeScale=FVector(newScaleXY,newScaleXY,SizeScale.Z));
			FString LogMsg=FString::Printf(_T("Using DeltaTime incorporated scaling: Stage=%i SizeScale.XY=%0.8f"),Stage,newScaleXY);
			GEngine->AddOnScreenDebugMessage(1,5.f,FColor::Yellow,LogMsg);
		}

		/*float timeElapsed1=GetWorldTimerManager().GetTimerElapsed(StormSetupTimerHandle);
		float timeRemaining1=GetWorldTimerManager().GetTimerRemaining(StormSetupTimerHandle);
		FString timeMsg1=FString::Printf(_T("Storm Stage Advance, timer elapsed (seconds): %.02f, time remaining until next stage (seconds): %.02f"),timeElapsed1,timeRemaining1);
		GEngine->AddOnScreenDebugMessage(2,5.f,FColor::Green,timeMsg1);

		if(StormDamageTimerHandle.IsValid())
		{
			float timeElapsed2=GetWorldTimerManager().GetTimerElapsed(StormDamageTimerHandle);
			float timeRemaining2=GetWorldTimerManager().GetTimerRemaining(StormDamageTimerHandle);
			FString timeMsg2=FString::Printf(_T("Storm Damage Increase, timer elapsed (seconds): %.02f, time remaining until damage increased (seconds): %.02f"),timeElapsed2,timeRemaining2);
			GEngine->AddOnScreenDebugMessage(3,5.f,FColor::Green,timeMsg2);
		}
		*/
	}
}

void AStormActor::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AStormActor, Damage);
	DOREPLIFETIME(AStormActor, IsShrinking);
	DOREPLIFETIME(AStormActor, InitialSizeScale);
	DOREPLIFETIME(AStormActor, InitialActorLocation);
}

void AStormActor::AdvanceStage_Implementation()
{		
	//Advance stages until storm is down to almost nothing
	//Usually by this point either the game has probably ended or we reinitalize the storm and start again...
	if(SizeScale.X <= 0.0000009f)
		return InitializeStorm();
	//If 1/10th the scale make this the last stage and keep shrinking until there's a winner / end of game, or the storm re-initializes due to being extremely tiny as above
	if(SizeScale.X <= 0.1f)
		return;

	IsShrinking=((++Stage % 2) != 0) ?  true : false; //Every odd numbered stage advance the shrinking!
}

bool AStormActor::AdvanceStage_Validate() {
	return true;
}

void AStormActor::ServerSetNewDamage_Implementation() {
	Damage++;
}

bool AStormActor::ServerSetNewDamage_Validate() {
	return true;
}

void AStormActor::ServerStartStorm_Implementation()
{
	InitializeStorm();
}

bool AStormActor::ServerStartStorm_Validate()
{
	return true;
}