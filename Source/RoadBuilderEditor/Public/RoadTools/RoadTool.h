#pragma once
#include "Widgets/SCompoundWidget.h"
#include "RoadScene.h"
#include "Settings.h"
#include "RoadTools/RoadToolType.h"

class FRoadInspector;

class FRoadTool : public FModeTool
{
public:
	/**
	 * Tool identity. Each subclass must report its own; the mode indexes by it and buttons use it for the selected state.
	 * With it, tool registration order in the mode no longer matters at all.
	 */
	virtual ERoadToolType GetToolType() const = 0;

	static inline const float Size_Point = 16.f;
	static inline const float Thickness_Road = 2.f;
	static inline const float Thickness_Line = 1.f;
	static inline const float DepthBias_Select = 10.f;
	static inline const FColor Color_Road = FColor(128, 128, 255);
	static inline const FColor Color_Line = FColor(0, 128, 0);
	static inline const FColor Color_Grey = FColor(128, 128, 128);
	static inline const FColor Color_Select = FColor::Red;
	virtual bool ShouldDrawWidget() const { return false; }
	virtual FVector GetWidgetLocation() const { return FVector::ZeroVector; }
	virtual bool GetCustomDrawingCoordinateSystem(FMatrix& InMatrix, void* InData) { return false; }
	virtual EAxisList::Type GetWidgetAxisToDraw() const { return EAxisList::None; }
//	virtual TSharedPtr<SWidget> GenerateContextMenu() { return TSharedPtr<SWidget>(); }
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) { return false; }
	virtual bool InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event);
	virtual bool EndModify();
	virtual void NotifyPreChange(FProperty* PropertyAboutToChange) {}
	virtual void Reset();

	ARoadScene* GetScene() const;
	ARoadActor*& GetSelectedRoad() const;
	AGroundActor*& GetSelectedGround() const;
	AJunctionActor*& GetSelectedJunction() const;
	/** Inspector panel host. Owned by FEdModeRoad; may be null (before the panel is built). */
	FRoadInspector* GetInspector() const;
	FRay GetRay(FEditorViewportClient* ViewportClient) const;
	FVector LineTrace(const FRay& Ray, AActor* IgnoredActor = nullptr) const;
	FVector LineTrace(FEditorViewportClient* ViewportClient, AActor* IgnoredActor = nullptr) const;
	void SelectParent();
	bool HandleClickRoad(HHitProxy* HitProxy, const FViewportClick& Click, int* PointIndex = nullptr);
	bool HandleClickJunction(HHitProxy* HitProxy, const FViewportClick& Click, int* GateIndex = nullptr, int* LinkIndex = nullptr);
	void DrawCurve(FPrimitiveDrawInterface* PDI, const FPolyline& Curve, FColor Color, float Thickness, float DepthBias = 0);
	void DrawPoint(FPrimitiveDrawInterface* PDI, URoadCurve* Curve, double Dist, FColor Color);
	void DrawDivider(FPrimitiveDrawInterface* PDI, URoadLane* Lane, double Dist, FColor Color);
	void DrawRoads(FPrimitiveDrawInterface* PDI, bool DrawLinks);
	void DrawJunction(FPrimitiveDrawInterface* PDI, AJunctionActor* Junction, FColor Color);
	void DrawJunctions(FPrimitiveDrawInterface* PDI);
	bool LazyRebuild = false;
};