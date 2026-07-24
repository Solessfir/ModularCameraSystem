// Copyright Solessfir. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorldCollision.h"
#include "CameraPenetrationAvoidanceFeeler.generated.h"

// Struct defining a feeler ray used for camera penetration avoidance.
USTRUCT(BlueprintType)
struct FCameraPenetrationAvoidanceFeeler
{
	GENERATED_BODY()

	FCameraPenetrationAvoidanceFeeler()
		: AdjustmentRot(FRotator::ZeroRotator)
		, WorldWeight(0.f)
		, PawnWeight(0.f)
		, Extent(0.f)
		, TraceInterval(0)
		, FramesUntilNextTrace(0)
	{
	}

	FCameraPenetrationAvoidanceFeeler(const FRotator& InAdjustmentRot, const float& InWorldWeight, const float& InPawnWeight, const float& InExtent, const int32& InTraceInterval = 0, const int32& InFramesUntilNextTrace = 0)
		: AdjustmentRot(InAdjustmentRot)
		, WorldWeight(InWorldWeight)
		, PawnWeight(InPawnWeight)
		, Extent(InExtent)
		, TraceInterval(InTraceInterval)
		, FramesUntilNextTrace(InFramesUntilNextTrace)
	{
	}

	// FRotator describing deviance from main ray.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PenetrationAvoidanceFeeler")
	FRotator AdjustmentRot;

	// How much this feeler affects the final position if it hits the world.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ClampMin = 0.f, ClampMax = 1.f), Category = "PenetrationAvoidanceFeeler")
	float WorldWeight;

	// How much this feeler affects the final position if it hits a APawn (setting to 0 will not attempt to collide with pawns at all).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ClampMin = 0.f, ClampMax = 1.f), Category = "PenetrationAvoidanceFeeler")
	float PawnWeight;

	// Extent to use for collision when tracing this feeler.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ClampMin = 0.f, UIMin = 0.f, UIMax = 100.f), Category = "PenetrationAvoidanceFeeler")
	float Extent;

	// Minimum frame interval between traces with this feeler if nothing was hit last frame.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ClampMin = 0, UIMin = 0, UIMax = 10), Category = "PenetrationAvoidanceFeeler")
	int32 TraceInterval;

	// Number of frames since this feeler was used.
	UPROPERTY(Transient)
	int32 FramesUntilNextTrace;

	FTraceHandle AsyncTraceHandle;

	// Endpoints used by AsyncTraceHandle. Async results arrive a frame later, after the live camera ray may have moved.
	FVector AsyncTraceStart = FVector::ZeroVector;
	FVector AsyncTraceEnd = FVector::ZeroVector;

	// Cached debug data stays present in all configurations so public-header layout does not depend
	// on whether DrawDebugHelpers.h happened to define ENABLE_DRAW_DEBUG before this header.
	bool bLastDrawResultValid = false;
	bool bLastBlocked = false;
	FVector LastStartPoint = FVector::ZeroVector;
	FVector LastEndPoint = FVector::ZeroVector;
};
