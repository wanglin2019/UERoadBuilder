#pragma once
#include "CoreMinimal.h"

class IDetailsView;
class IStructureDetailsView;
class SWidget;
class UObject;
class UScriptStruct;
class FEdModeRoad;
struct FPropertyChangedEvent;

/** Property-changed callback. Pass nullptr if not needed. */
using FRoadPropertyChanged = TFunction<void(const FPropertyChangedEvent&)>;

/**
 * Host of the three detail views.
 *
 * SRoadEdit only lays out its three views and does not own its lifetime (ownership is in FEdModeRoad); toolbars / tools push data in through the narrow interface below,
 * nobody needs (or is allowed) to touch IDetailsView directly.
 *
 * Panel semantics — the three views are stacked in one area; of the two selection panels at most one is visible,
 * and both are hidden while nothing is selected:
 *   SettingsView       always shown; switches the USettings_* CDO by current tool
 *   ObjectDetailsView  shows a whole UObject (e.g. UMarkingPoint / UMarkingCurve)
 *   StructDetailsView  shows a struct instance (e.g. FRoadPoint / FLaneSegment)
 */
class FRoadInspector
{
public:
	~FRoadInspector();

	/** Called once by FEdModeRoad::Enter() before creating the toolkit, so the panel exists before SRoadEdit. */
	void Init(FEdModeRoad* InNotifyHook);

	TSharedRef<SWidget> GetSettingsWidget() const;
	TSharedRef<SWidget> GetObjectWidget() const;
	TSharedRef<SWidget> GetStructWidget() const;

	/** Switches the Settings panel content; pass nullptr to clear. */
	void SetSettings(UObject* Settings);

	/** Shows a struct instance. A null Struct or Data clears the struct panel. */
	void ShowStruct(UScriptStruct* Struct, void* Data, FRoadPropertyChanged OnChanged = nullptr);

	/** Shows a whole UObject. A null Object clears the object panel. */
	void ShowObject(UObject* Object, FRoadPropertyChanged OnChanged = nullptr);

	/** Clears the selection: both selection panels are emptied and hidden, leaving only the settings panel. */
	void Clear();

private:
	void SetStructVisible(bool bVisible);
	void SetObjectVisible(bool bVisible);

	TSharedPtr<IDetailsView> SettingsView;
	TSharedPtr<IDetailsView> ObjectDetailsView;
	TSharedPtr<IStructureDetailsView> StructDetailsView;
};
