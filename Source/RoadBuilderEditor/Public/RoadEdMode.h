// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once
#include "EditorModeTools.h"
#include "EditorModeManager.h"
#include "EdMode.h"
#include "RoadActor.h"
#include "RoadScene.h"
#include "RoadTools/RoadToolType.h"

class SRoadEdit;
class FRoadInspector;
class FRoadTool;

struct HRoadProxy : public HHitProxy
{
	DECLARE_HIT_PROXY();
	HRoadProxy(ARoadActor* R, int I = INDEX_NONE) : HHitProxy(HPP_Wireframe), Road(R), Index(I) {}
	virtual EMouseCursor::Type GetMouseCursor() { return EMouseCursor::Crosshairs; }
	ARoadActor* Road;
	int Index;	//INDEX_NONE indicates the whole road
};

struct HGroundProxy : public HHitProxy
{
	DECLARE_HIT_PROXY();
	HGroundProxy(AGroundActor* G, int I = INDEX_NONE) : HHitProxy(HPP_Wireframe), Ground(G), Index(I) {}
	virtual EMouseCursor::Type GetMouseCursor() { return EMouseCursor::Crosshairs; }
	AGroundActor* Ground;
	int Index;
};

struct HJunctionProxy : public HHitProxy
{
	DECLARE_HIT_PROXY();
	HJunctionProxy(AJunctionActor* J, int I = INDEX_NONE, int S = INDEX_NONE) : HHitProxy(HPP_Wireframe), Junction(J), Index(I), SubId(S) {}
	virtual EMouseCursor::Type GetMouseCursor() { return EMouseCursor::Crosshairs; }
	AJunctionActor* Junction;
	int Index;	//INDEX_NONE indicates the whole line
	int SubId;
};

struct HRoadCurveProxy : public HHitProxy
{
	DECLARE_HIT_PROXY();
	HRoadCurveProxy(URoadCurve* C, int I = INDEX_NONE) : HHitProxy(HPP_Wireframe), Curve(C), Index(I) {}
	virtual EMouseCursor::Type GetMouseCursor() { return EMouseCursor::Crosshairs; }
	URoadCurve* Curve;
	int Index;	//INDEX_NONE indicates the whole line
};

struct HRoadMarkingProxy : public HHitProxy
{
	DECLARE_HIT_PROXY();
	HRoadMarkingProxy(URoadMarking* M, int I = INDEX_NONE, int S = INDEX_NONE) : HHitProxy(HPP_Wireframe), Marking(M), Index(I), SubId(S) {}
	virtual EMouseCursor::Type GetMouseCursor() { return EMouseCursor::Crosshairs; }
	URoadMarking* Marking;
	int Index;	//INDEX_NONE indicates the whole object
	int SubId;
};



class FEdModeRoad : public FEdMode, public FNotifyHook
{
public:
	static FEditorModeID GetModeID()
	{
		return FEditorModeID(TEXT("EM_Road"));
	}
	static FEdModeRoad* Get()
	{
		return (FEdModeRoad*)GLevelEditorModeTools().GetActiveMode(GetModeID());
	}

	FEdModeRoad();
	virtual ~FEdModeRoad();

	/** Identity of the current tool. Returns ERoadToolType::None when there is no current tool. */
	ERoadToolType GetCurrentToolType() const;
	/** Switches the current tool by identity. Distinct from the base SetCurrentTool(FModeTool*): this takes a tool identity, not a tool instance. */
	void SetCurrentToolByType(ERoadToolType ToolType);
	/** Finds a tool by identity; returns nullptr if not found. */
	FRoadTool* FindRoadTool(ERoadToolType ToolType) const;

	/**
	 * Maps a tool to the object shown for it in the settings panel. Returns nullptr for tools without a settings panel.
	 * The mapping is tool metadata, so it lives in the mode rather than in the panel.
	 */
	static UObject* GetToolSettings(ERoadToolType ToolType);

	/** Inspector panel. Owned by this class (created in Enter, released in Exit); tools reach it via FRoadTool::GetInspector(). */
	FRoadInspector* GetInspector() const { return Inspector.Get(); }

	void OnUndo(const FTransactionContext& InTransactionContext, bool bSucceeded);
	void OnRedo(const FTransactionContext& InTransactionContext, bool bSucceeded);

	/** FEdMode: Called when the mode is entered */
	virtual void Enter() override;

	/** FEdMode: Called when the mode is exited */
	virtual void Exit() override;
	virtual void NotifyPreChange(FProperty* PropertyAboutToChange);
	virtual void PostUndo();
	virtual bool GetCursor(EMouseCursor::Type& OutCursor) const;
	virtual bool AllowWidgetMove() { return true; }
	virtual bool ShouldDrawWidget() const;
	virtual FVector GetWidgetLocation() const;
	virtual bool GetCustomDrawingCoordinateSystem(FMatrix& InMatrix, void* InData);
	virtual EAxisList::Type GetWidgetAxisToDraw(UE::Widget::EWidgetMode InWidgetMode) const;
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click);
	virtual bool IsSelectionAllowed(AActor* InActor, bool bInSelection) const override { return false; }

	ARoadScene* Scene = nullptr;
	ARoadActor* SelectedRoad = nullptr;
	AGroundActor* SelectedGround = nullptr;
	AJunctionActor* SelectedJunction = nullptr;

private:
	/** Applies the current tool's settings panel once: call right after the panel is created. */
	void RefreshInspectorSettings();

	/**
	 * Inspector panel. **Owned by the mode**: created in Enter() (before the toolkit, since SRoadEdit borrows it for layout),
	 * released in Exit(), exactly spanning one editing session.
	 * The panel does not own the mode and the mode does not depend on the panel's lifetime, so there is no raw pointer and no need
	 * for a patch that clears the mode pointer back when the panel is destroyed before the mode.
	 */
	TSharedPtr<FRoadInspector> Inspector;

	/**
	 * Tool identity -> tool instance, indexed by the GetToolType() each tool self-reports.
	 * The base class Tools array only serves as the ownership container (for destruction); all lookups go here — registration order is meaningless.
	 */
	TMap<ERoadToolType, FRoadTool*> ToolsById;
};