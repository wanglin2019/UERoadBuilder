// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RoadActor.h"
#include "Tools/RoadInteractiveTool.h"

#include "RoadTool_JunctionLink.generated.h"

class AJunctionActor;
class FArrayProperty;
struct FJunctionLink;

/**
 * Settings of URoadTool_JunctionLink: a live mirror of the selected junction link.
 *
 * The legacy tool pushed FJunctionLink straight into a struct details view. Of its four fields only the
 * turn radius is editable - the three roads are what the link *is*, decided when the junction is built -
 * so the mirror keeps them visible and read-only, which is what the struct view showed.
 */
UCLASS(Transient)
class ROADBUILDERTOOLS_API URoadTool_JunctionLinkProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	/** Gate the selected link belongs to; INDEX_NONE when no link is selected. Owned by the tool. */
	UPROPERTY(VisibleAnywhere, Category = "Junction Link")
	int32 GateIndex = INDEX_NONE;

	/** Position of the link within that gate; INDEX_NONE when no link is selected. Owned by the tool. */
	UPROPERTY(VisibleAnywhere, Category = "Junction Link")
	int32 LinkIndex = INDEX_NONE;

	/** Road the link curve belongs to. */
	UPROPERTY(VisibleAnywhere, Category = "Junction Link")
	TObjectPtr<ARoadActor> Road = nullptr;

	/** Road the link takes traffic from. */
	UPROPERTY(VisibleAnywhere, Category = "Junction Link")
	TObjectPtr<ARoadActor> InputRoad = nullptr;

	/** Road the link hands traffic to. */
	UPROPERTY(VisibleAnywhere, Category = "Junction Link")
	TObjectPtr<ARoadActor> OutputRoad = nullptr;

	/** Turn radius of the link curve. */
	UPROPERTY(EditAnywhere, Category = "Junction Link")
	double Radius = 1000.0;
};

/** Builder for URoadTool_JunctionLink. */
UCLASS()
class ROADBUILDERTOOLS_API URoadTool_JunctionLinkBuilder : public URoadInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

/**
 * Inspects and tunes the link curves a junction generates between the roads meeting there.
 *
 * Legacy gesture, preserved: before a junction is picked the tool draws every junction in the level, and a
 * left click picks one. From then on it draws that junction's link curves, and a left click picks one of
 * them. The tool has no other gesture - no right-button behaviour, no keyboard binding, and no transform
 * gizmo - because a link is positioned by the junction, not by the user.
 *
 * | legacy (FModeTool)                                | here                                    |
 * |---------------------------------------------------|-----------------------------------------|
 * | HJunctionProxy (with and without gate/link indices)| RoadPicking, one collector per form     |
 * | ShowStruct(FJunctionLink)                          | URoadTool_JunctionLinkProperties        |
 * | DrawJunctions / DrawJunction(PDI) + hit proxies    | Render(RenderAPI), no hit proxies       |
 *
 * The tool owns the junction drawing pass. It was the only legacy tool that used DrawJunction() - every
 * other tool either drew roads or cross-sections - so the pass moved here rather than into the shared
 * layer, and the same walk feeds both the drawing and the picking, which is what keeps the two from
 * drifting apart.
 */
UCLASS()
class ROADBUILDERTOOLS_API URoadTool_JunctionLink : public URoadInteractiveTool
{
	GENERATED_BODY()

public:
	virtual void Setup() override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;
	virtual void OnRoadClicked(const FInputDeviceRay& ClickPos, bool bRightButton) override;

	/** Escape: drop the link selection, then let the base step the junction selection up a level. */
	virtual void SelectParent() override;

protected:
	UPROPERTY()
	TObjectPtr<URoadTool_JunctionLinkProperties> Properties;

private:
	/** The link being edited, or null when the selection no longer resolves. */
	FJunctionLink* GetCurrentLink() const;

	/** Makes the given link the selection and refreshes the panel and the view. */
	void SelectLink(AJunctionActor* Junction, int32 Gate, int32 Link);

	/** Copies the selected link into Properties, so the panel shows live values. */
	void SyncProperties();

	/** Writes the edited radius back into the link, with one undo entry. */
	void ApplyProperties(FProperty* Property);

	/** Junction, gate and link under the ray; OutJunction is null when the ray missed everything. */
	void PickUnderRay(const FRay& Ray, AJunctionActor*& OutJunction, int32& OutGate, int32& OutLink) const;

	/**
	 * The Gates array property of a junction, resolved per edit rather than cached.
	 *
	 * A link is a struct nested inside a struct inside that array, and a single-property undo record
	 * addresses a property of its owner UObject - a nested member has no such address. Snapshotting the
	 * gate array instead is the primitive that does reach it, and the array holds a handful of gates.
	 */
	static FArrayProperty* GetGatesProperty();

	/** Gate the selected link belongs to, or INDEX_NONE. */
	int32 GateIndex = INDEX_NONE;

	/** Position of the selected link within that gate, or INDEX_NONE. */
	int32 LinkIndex = INDEX_NONE;
};
