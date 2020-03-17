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
	ScaleHighThreshold=1.0f;
	ScaleMidThreshold=0.7f;
	ScaleLowThreshold=0.4f;
	ScaleHighModifier=0.000110f;
	ScaleMidModifier=0.000320f;
	ScaleLowModifier=0.000100f;
	Stage = 0; //Stages 0, 1, 2, 3, the circle is not shrinking, stage 4, 5, 6 the circle is shrinking and sets back to 0 afterwards
	FMath::SRandInit(FPlatformTime::Cycles());

	IConsoleVariable *RestartStormCmd=IConsoleManager::Get().RegisterConsoleVariable(TEXT("RestartStorm"),
		2,
		TEXT("AdminCommand: Run 'RestartStorm 1' to restart the storm on demand.\n"),
		ECVF_Scalability | ECVF_RenderThreadSafe);

	RestartStormCmd->AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateLambda([&] (IConsoleVariable* Var){ InitializeStorm(); }));
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
		SetActorLocation(FVector(X,Y,GetActorLocation().Z));

		SetActorScale3D(InitialSizeScale);
		FVector ScaleDown=InitialSizeScale;
		ScaleTotalCount=131072; //(131072 * 12) == (1024 * 1024 * 1.5) We'll go up to 1.5MB worth of scale downs but it's more than enough and we'll stop early anyway...
		ScaleIndex=0;
		SizeScales.Reset();
		for(int32 i=0; i < ScaleTotalCount; i++)
		{
			//More like the original / uniform scale down
			//ScaleDown.X=(ScaleDown.Y*=ScaleDownRate);

			//Experimental variable shrink down rate (to try and get past frame dip points faster)
			if(ScaleDown.X <= ScaleHighThreshold)
				ScaleDown.X=(ScaleDown.Y*=(ScaleDownRate - ScaleHighModifier));
			else if(ScaleDown.X <= ScaleMidThreshold)
				ScaleDown.X=(ScaleDown.Y*=(ScaleDownRate - ScaleMidModifier));
			else if(ScaleDown.X <= ScaleLowThreshold)
				ScaleDown.X=(ScaleDown.Y*=(ScaleDownRate + ScaleLowModifier));
			
			SizeScales.Emplace(ScaleDown);
			FString LogMsg=FString::Printf(_T("Pre-calculating: ScaleDown.X=%.08f ScaleDown.Y=%.08f ScaleTotalCount=%i"),ScaleDown.X,ScaleDown.Y,i);
			GEngine->AddOnScreenDebugMessage(0,5.0f,FColor::Yellow,LogMsg);
			if(ScaleDown.X <= 0.0000009f && ScaleDown.Y <= 0.0000009f)
			{
				ScaleTotalCount=i+1; //When we have enough to reach basically zero then we're done pre-calculating
				break;
			}
		}
	}
}

// Called when the game starts or when spawned
void AStormActor::BeginPlay()
{
	Super::BeginPlay();
	InitializeStorm();
	if(HasAuthority())
	{
		FTimerHandle StormSetupTimerHandle;
		GetWorldTimerManager().SetTimer(StormSetupTimerHandle,this,&AStormActor::ServerStartStorm,StormAdvanceStageRate,true);
	}

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
		if(++ScaleIndex < ScaleTotalCount)
		{
			SetActorScale3D(SizeScales[ScaleIndex]);
			FString LogMsg=FString::Printf(_T("Using Pre-calculated: SizeScale.X=%.08f SizeScale.Y=%.08f ScaleIndex=%i ScaleTotalCount=%i Stage=%i"),SizeScales[ScaleIndex].X,SizeScales[ScaleIndex].Y,ScaleIndex,ScaleTotalCount,Stage);
			GEngine->AddOnScreenDebugMessage(1,5.f,FColor::Green,LogMsg);
		}

		/*FVector SizeScale=FVector(SizeScale.X * 0.999485,SizeScale.Y * 0.999485,SizeScale.Z);
		SetActorScale3D(SizeScale);*/
	}
}

void AStormActor::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AStormActor, Damage);
	DOREPLIFETIME(AStormActor, IsShrinking);
	DOREPLIFETIME(AStormActor, InitialSizeScale);
	DOREPLIFETIME(AStormActor, InitialActorLocation);
	DOREPLIFETIME(AStormActor, SizeScales);
	DOREPLIFETIME(AStormActor, ScaleIndex);
	DOREPLIFETIME(AStormActor, ScaleTotalCount);
}

void AStormActor::AdvanceStage_Implementation()
{		
	//Advance stages until storm is down to almost nothing (no longer visible) or ScaleIndex is outside of precalculated shrinkdowns,
	//Usually by this point either the game has probably ended or we reinitalize the storm and start again...
	if(GetActorScale3D().X <= 0.0000009f && GetActorScale3D().Y <= 0.0000009f || ScaleIndex >= ScaleTotalCount)
		return InitializeStorm();
	//If 1/10th the scale make this the last stage and keep shrinking until there's a winner / end of game, or the storm re-initializes due to being extremely tiny as above
	if(GetActorScale3D().X <= 0.1f && GetActorScale3D().Y <= 0.1f)
		return;

	if((++Stage % 2) != 0) //Every odd numbered stage advance the shrinking!
		IsShrinking=true;
	else
		IsShrinking=false;
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

void AStormActor::ServerStartStorm_Implementation() {
	
	GetWorldTimerManager().SetTimer(StormStateTimerHandle,this,&AStormActor::AdvanceStage,0.001f,false);
	GetWorldTimerManager().SetTimer(StormDamageTimerHandle,this,&AStormActor::ServerSetNewDamage,StormIncreaseDamageRate+0.001f,false);
}

bool AStormActor::ServerStartStorm_Validate() {
	return true;
}
