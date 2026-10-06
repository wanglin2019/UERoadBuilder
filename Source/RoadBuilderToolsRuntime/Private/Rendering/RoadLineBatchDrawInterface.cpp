// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Rendering/RoadLineBatchDrawInterface.h"

#include "Components/LineBatchComponent.h"

FRoadLineBatchDrawInterface::FRoadLineBatchDrawInterface(const FSceneView* InView, ULineBatchComponent* InLineBatchComponent)
	: FPrimitiveDrawInterface(InView)
	, LineBatch(InLineBatchComponent)
{
}

void FRoadLineBatchDrawInterface::Reset()
{
	DrawnPrimitiveCount = 0;

	if (ULineBatchComponent* Component = LineBatch.Get())
	{
		// Only our own batch: the game may be drawing through the same component.
		Component->ClearBatch(OverlayBatchId);
	}
}

void FRoadLineBatchDrawInterface::Flush()
{
	if (ULineBatchComponent* Component = LineBatch.Get())
	{
		// The batch is rebuilt in place every frame, so the proxy has to be told to re-read it.
		Component->MarkRenderStateDirty();
	}
}

bool FRoadLineBatchDrawInterface::IsHitTesting()
{
	// There is no hit proxy pass at runtime: nothing ever asks this adapter to render for picking.
	return false;
}

void FRoadLineBatchDrawInterface::SetHitProxy(HHitProxy* HitProxy)
{
	// Deliberately discarded. Tools must not rely on hit proxies outside the editor; the shared tool
	// layer picks by ray test instead, which is the only mechanism both hosts can offer.
}

void FRoadLineBatchDrawInterface::RegisterDynamicResource(FDynamicPrimitiveResource* DynamicResource)
{
	// Nothing here holds GPU resources with a frame lifetime.
}

void FRoadLineBatchDrawInterface::AddReserveLines(uint8 DepthPriorityGroup, int32 NumLines, bool bDepthBiased, bool bThickLines)
{
	// A reservation hint for the editor's line batcher; ULineBatchComponent grows its arrays itself.
}

void FRoadLineBatchDrawInterface::DrawSprite(const FVector& Position, float SizeX, float SizeY, const FTexture* Sprite,
	const FLinearColor& Color, uint8 DepthPriorityGroup, float U, float UL, float V, float VL, uint8 BlendMode,
	float OpacityMaskRefVal)
{
	// Unsupported: a line batch has no sprite concept, and the road tools draw only lines and points.
	// Dropping it is preferable to a silent approximation at the wrong size.
}

void FRoadLineBatchDrawInterface::DrawLine(const FVector& Start, const FVector& End, const FLinearColor& Color,
	uint8 DepthPriorityGroup, float Thickness, float DepthBias, bool bScreenSpace)
{
	if (ULineBatchComponent* Component = LineBatch.Get())
	{
		// LifeTime 0 means "the component's default", which keeps the line alive long enough to be seen
		// and is refreshed every frame by the tool's own Render().
		Component->DrawLine(Start, End, Color, DepthPriorityGroup, Thickness, /*LifeTime*/ 0.0f, OverlayBatchId);
		++DrawnPrimitiveCount;
	}
}

void FRoadLineBatchDrawInterface::DrawTranslucentLine(const FVector& Start, const FVector& End,
	const FLinearColor& Color, uint8 DepthPriorityGroup, float Thickness, float DepthBias, bool bScreenSpace)
{
	// ULineBatchComponent has no separate translucency path; batched lines blend by alpha already, so
	// the alpha component of Color carries the transparency through unchanged.
	DrawLine(Start, End, Color, DepthPriorityGroup, Thickness, DepthBias, bScreenSpace);
}

void FRoadLineBatchDrawInterface::DrawPoint(const FVector& Position, const FLinearColor& Color, float PointSize,
	uint8 DepthPriorityGroup)
{
	if (ULineBatchComponent* Component = LineBatch.Get())
	{
		Component->DrawPoint(Position, Color, PointSize, DepthPriorityGroup, /*LifeTime*/ 0.0f, OverlayBatchId);
		++DrawnPrimitiveCount;
	}
}

int32 FRoadLineBatchDrawInterface::DrawMesh(const FMeshBatch& Mesh)
{
	// Unsupported, and honest about it: 0 passes rendered. A road tool that needs meshes needs a
	// component, not the overlay adapter.
	return 0;
}
