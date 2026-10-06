// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleInterface.h"

/**
 * Log category of the road editing tool layer.
 *
 * Declared here, in the shared runtime module, because the tools and both of their hosts log through it:
 * the tools themselves, the UEdMode host in RoadBuilderToolsEditor and the ToolsContext host in
 * RoadBuilderToolsRuntime. One category for all of them means a single command filters the whole feature,
 * and the two hosts cannot drift apart in how they report.
 *
 * Default verbosity is Log, so the lifecycle lines (mode Enter/Exit, tool started/ended, road created)
 * are on by default and everything finer-grained is opt-in. Enable at the console with:
 *
 *   log LogRoadBuilder Verbose     // branch decisions, picking results
 *   log LogRoadBuilder VeryVerbose // every cursor poll and hit test
 *
 * or from the command line with -LogCmds="LogRoadBuilder Verbose".
 *
 * The levels the code below uses, and what belongs in each:
 *   VeryVerbose - per-event noise: a mouse poll, a hit test. Off unless chasing a specific gesture.
 *   Verbose     - the criteria a branch decided on: which object a ray found, why a path was taken.
 *   Log         - lifecycle: a mode entered, a tool started, a road created or a point inserted.
 *   Warning     - a host that failed to supply a capability the tool needs. Usually a bug, never fatal.
 *   Error       - an actual failure.
 *
 * The declaration carries ROADBUILDERTOOLS_API because the category is consumed by the editor host module
 * as well: without the export macro the symbol stays private to this DLL and RoadBuilderToolsEditor fails
 * to link on it. The definition in RoadBuilderTools.cpp needs no matching decoration.
 */
ROADBUILDERTOOLS_API DECLARE_LOG_CATEGORY_EXTERN(LogRoadBuilder, Log, All);

/**
 * Shared road editing tool layer, written against the InteractiveToolsFramework (ITF).
 *
 * The same tool implementations are driven by two hosts:
 *   - editor host:  URoadToolsMode (UEdMode) in the RoadBuilderToolsEditor module
 *   - runtime host: a game-side UInteractiveToolsContext in the RoadBuilderToolsRuntime module
 *
 * ITF itself is a Runtime module (Core / CoreUObject / InputCore / ApplicationCore / MeshDescription /
 * Engine / RHI / GeometryCore only), which is the entire reason this layer can live outside the editor.
 * Keep it free of editor-only types so that stays true.
 */
class FRoadBuilderTools : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
