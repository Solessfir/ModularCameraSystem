// Copyright Solessfir. All Rights Reserved.

#include "Camera/CameraMode_FirstPerson.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(CameraMode_FirstPerson)

FVector UCameraMode_FirstPerson::GetPivotLocation_Implementation() const
{
	const AActor* TargetActor = GetTargetActor();
	check(TargetActor);

	if (const APawn* TargetPawn = Cast<APawn>(TargetActor))
	{
		if (const ACharacter* TargetCharacter = Cast<ACharacter>(TargetPawn))
		{
			if (TargetCharacter->GetMesh()->DoesSocketExist(HeadSocketName))
			{
				return TargetCharacter->GetMesh()->GetSocketLocation(HeadSocketName);
			}
		}
		return TargetPawn->GetPawnViewLocation();
	}
	return TargetActor->GetActorLocation();
}
