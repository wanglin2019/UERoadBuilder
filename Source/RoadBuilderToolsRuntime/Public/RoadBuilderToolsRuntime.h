// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/**
 * Runtime editing host for the road tools.
 *
 * This module owns everything that only a game needs: the game-side InteractiveToolsContext, the world
 * subsystem that starts and stops editing, the input bridge from the player controller into
 * UInputRouter, the runtime property UI and the undo stack. The tools themselves are not here - they
 * live in RoadBuilderTools, which this module depends on and never the other way round.
 *
 * S0 ships the module as an empty skeleton on purpose. It exists this early so the dependency boundary
 * is enforced by the build from day one: UI and input dependencies are declared here, so the compiler
 * rejects any attempt to pull them into the shared tool layer.
 *
 * Note on load behaviour (verified in engine source): a Runtime module of an enabled plugin is loaded
 * in packaged games regardless of whether anything references it - "ExplicitlyLoaded" is a
 * FPluginDescriptor flag, not a per-module one. A project that does not want the host can therefore
 * only opt out by moving this module into its own .uplugin, or by gating the subsystem through
 * USubsystem::ShouldCreateSubsystem().
 */
class FRoadBuilderToolsRuntime : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
