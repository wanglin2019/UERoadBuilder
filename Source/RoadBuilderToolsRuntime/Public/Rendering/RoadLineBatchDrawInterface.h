// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SceneManagement.h"

class ULineBatchComponent;

/**
 * Runtime PrimitiveDrawInterface backed by a ULineBatchComponent.
 *
 * Tools draw through FPrimitiveDrawInterface, which the editor supplies from the viewport's scene
 * rendering pass. A game has no such pass, so the runtime host has to supply one itself - and the
 * engine ships nothing public to build on: the only in-tree implementation, FViewElementPDI, lives in
 * Renderer/Private and FSimpleElementCollector is bound to FCanvas. This class is therefore the piece
 * of host plumbing the runtime editing host cannot do without, and proving it can exist is half of what
 * S0.5 set out to establish.
 *
 * It is also where FPrimitiveDrawInterface's editor-centric assumptions get resolved, each one
 * explicitly rather than silently:
 *   - hit proxies  -> there are none at runtime, so SetHitProxy() is accepted and discarded. Tools that
 *                     picked through proxies in the editor pick by ray test instead (see RoadHeight);
 *   - DrawMesh     -> unsupported, and reported as such through its return value;
 *   - DrawSprite   -> unsupported: a line batch has no sprite concept.
 *
 * Drawing is frame-scoped: Reset() before the tools render, Flush() after.
 */
class ROADBUILDERTOOLSRUNTIME_API FRoadLineBatchDrawInterface : public FPrimitiveDrawInterface
{
public:
	FRoadLineBatchDrawInterface(const FSceneView* InView, ULineBatchComponent* InLineBatchComponent);

	/** Drops what the previous frame drew. Call once per frame, before rendering the tools. */
	void Reset();

	/** Publishes what the tools drew. Call once per frame, after rendering the tools. */
	void Flush();

	/** Batch this adapter owns, so clearing it cannot disturb lines the game drew through the same component. */
	static constexpr uint32 OverlayBatchId = 0x1D0A;

	/** FPrimitiveDrawInterface implementation */
	virtual bool IsHitTesting() override;
	virtual void SetHitProxy(HHitProxy* HitProxy) override;
	virtual void RegisterDynamicResource(FDynamicPrimitiveResource* DynamicResource) override;
	virtual void AddReserveLines(uint8 DepthPriorityGroup, int32 NumLines, bool bDepthBiased = false, bool bThickLines = false) override;
	virtual void DrawSprite(
		const FVector& Position,
		float SizeX,
		float SizeY,
		const FTexture* Sprite,
		const FLinearColor& Color,
		uint8 DepthPriorityGroup,
		float U,
		float UL,
		float V,
		float VL,
		uint8 BlendMode = 1,
		float OpacityMaskRefVal = 0.5f) override;
	virtual void DrawLine(
		const FVector& Start,
		const FVector& End,
		const FLinearColor& Color,
		uint8 DepthPriorityGroup,
		float Thickness = 0.0f,
		float DepthBias = 0.0f,
		bool bScreenSpace = false) override;
	virtual void DrawTranslucentLine(
		const FVector& Start,
		const FVector& End,
		const FLinearColor& Color,
		uint8 DepthPriorityGroup,
		float Thickness = 0.0f,
		float DepthBias = 0.0f,
		bool bScreenSpace = false) override;
	virtual void DrawPoint(
		const FVector& Position,
		const FLinearColor& Color,
		float PointSize,
		uint8 DepthPriorityGroup) override;
	virtual int32 DrawMesh(const FMeshBatch& Mesh) override;

	/** Primitives this adapter has accepted since the last Reset(); used by tests and diagnostics. */
	int32 GetDrawnPrimitiveCount() const { return DrawnPrimitiveCount; }

private:
	/** Weak: the component belongs to whichever actor the game hung it on. */
	TWeakObjectPtr<ULineBatchComponent> LineBatch;

	int32 DrawnPrimitiveCount = 0;
};
