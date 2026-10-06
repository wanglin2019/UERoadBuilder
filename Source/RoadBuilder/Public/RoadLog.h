// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RoadBuilder.h"
#include "Logging/LogMacros.h"

/**
 * Logging front end for the whole plugin.
 *
 * Deliberate wrappers over UE_LOG on the plugin's own category (LogRoadBuilder, declared in
 * RoadBuilder.h). They exist for two reasons:
 *
 *   1. One place to change. Every layer of the plugin - the runtime model, the shared tool layer and both
 *      editor hosts - logs through these five entry points, so the category and anything else that should
 *      be uniform can be decided here instead of at each of the ~50 call sites.
 *   2. A vocabulary that matches how the levels are meant to be used. UE_LOG takes a verbosity enum whose
 *      names (VeryVerbose / Verbose / Log / Warning / Error) say where a message sits in the ordering but
 *      not what belongs there. Trace/Debug/Info/Warn/Error are the words the team actually thinks in.
 *
 * The mapping is one to one and adds nothing on top:
 *
 *   RoadLog_Trace -> ELogVerbosity::VeryVerbose   per-event noise: a mouse poll, a hit test
 *   RoadLog_Debug -> ELogVerbosity::Verbose       the criteria a branch decided on
 *   RoadLog_Info  -> ELogVerbosity::Log           lifecycle: a mode entered, a tool started
 *   RoadLog_Warn  -> ELogVerbosity::Warning       a host failed to supply a capability
 *   RoadLog_Error -> ELogVerbosity::Error         an actual failure
 *
 * Only Info and above are on by default (the category's default verbosity is Log). Raise it at the console
 * to see the rest:
 *
 *   log LogRoadBuilder Verbose       // Debug and above
 *   log LogRoadBuilder VeryVerbose   // everything, including Trace
 *
 * or on the command line with -LogCmds="LogRoadBuilder Verbose".
 *
 * WHY THESE ARE MACROS AND NOT FUNCTIONS - and why the names are flat.
 *
 * Macros: UE_LOG expands its __VA_ARGS__ at the point of use, and arguments that reached a function
 * through its own ellipsis sit in a va_list that cannot be spliced back into a macro. A real function
 * could only forward by formatting first (FString::Printf) and passing the result as a "%s" argument,
 * which would give up the engine's compile-time format-string checking and add an allocation per call.
 * Wrapping the macro keeps both, at the cost of a call site that looks like a function but is not one -
 * worth knowing when debugging.
 *
 * Flat names: "RoadLog::Info" would read better, but the C++ grammar forbids '::' in a macro name
 * (MSVC: "error C2008: unexpected ':' in macro definition"; it is rejected even without /permissive-,
 * and UE enables /permissive- from BuildSettingsVersion V4). So the namespace separator became an
 * underscore. Read RoadLog_Info as RoadLog::Info.
 *
 * A runtime control surface (an editor panel listing categories and their levels) is a planned follow-up.
 * It will want to change the category's verbosity rather than route messages, so it does not need anything
 * added here; the five names are the stable part.
 */

/** Per-event noise. Off unless the category is raised to VeryVerbose. */
#define RoadLog_Trace(...) UE_LOG(LogRoadBuilder, VeryVerbose, __VA_ARGS__)

/** Branch criteria: what a decision was made on. Shown at Verbose. */
#define RoadLog_Debug(...) UE_LOG(LogRoadBuilder, Verbose, __VA_ARGS__)

/** Lifecycle: something the user would recognise happened. On by default. */
#define RoadLog_Info(...) UE_LOG(LogRoadBuilder, Log, __VA_ARGS__)

/** A host failed to supply a capability the caller needs. Usually a bug, never fatal. */
#define RoadLog_Warn(...) UE_LOG(LogRoadBuilder, Warning, __VA_ARGS__)

/** An actual failure. */
#define RoadLog_Error(...) UE_LOG(LogRoadBuilder, Error, __VA_ARGS__)
