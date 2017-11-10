/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#ifndef CRYINCLUDE_CRYSCRIPTSYSTEM_SCRIPTBINDINGS_SCRIPTBIND_SYSTEM_H
#define CRYINCLUDE_CRYSCRIPTSYSTEM_SCRIPTBINDINGS_SCRIPTBIND_SYSTEM_H
#pragma once


#include <IScriptSystem.h>

struct ISystem;
struct ILog;
struct IRenderer;
struct IConsole;
struct IInput;
struct ITimer;
struct IEntitySystem;
struct I3DEngine;
struct IPhysicalWorld;
struct ICVar;

#undef GetUserName

/*!
*   <description>This class implements script-functions for exposing the System functionalities.</description>
*   <remarks>This object doesn't have a global mapping(is not present as global variable into the script state).</remarks>
*/
class CScriptBind_System
    : public CScriptableBase
{
public:
    CScriptBind_System(IScriptSystem* pScriptSystem, ISystem* pSystem);
    virtual ~CScriptBind_System();
    virtual void GetMemoryUsage(ICrySizer* pSizer) const
    {
        pSizer->AddObject(this, sizeof(*this));
    }

public:
    //! <code>System.CreateDownload()</code>
    int CreateDownload(IFunctionHandler* pH);

    //! <code>System.LoadFont(pszName)</code>
    //!     <param name="pszName">Font name.</param>
    //! <description>Loads a font.</description>
    int LoadFont(IFunctionHandler* pH);

    //! <code>System.ExecuteCommand(szCmd)</code>
    //!     <param name="szCmd">Command string.</param>
    //! <description>Executes a command.</description>
    int ExecuteCommand(IFunctionHandler* pH);

    //! <code>System.LogToConsole(sText)</code>
    //!     <param name="sText">Text to be logged.</param>
    //! <description>Logs a message to the console.</description>
    int LogToConsole(IFunctionHandler* pH);

    //! <code>System.ClearConsole()</code>
    //! <description>Clears the console.</description>
    int ClearConsole(IFunctionHandler* pH);

    //! <code>System.Log(sText)</code>
    //!     <param name="sText">Text to be logged.</param>
    //! <description>Logs a message.</description>
    int Log(IFunctionHandler* pH);

    //! <code>System.LogAlways(sText)</code>
    //!     <param name="sText">Text to be logged.</param>
    //! <description>Logs important data that must be printed regardless verbosity.</description>
    int LogAlways(IFunctionHandler* pH);

    //! <code>System.Warning(sText)</code>
    //!     <param name="sText">Text to be logged.</param>
    //! <description>Shows a message text with the warning severity.</description>
    int Warning(IFunctionHandler* pH);

    //! <code>System.Error(sText)</code>
    //!     <param name="sText">Text to be logged.</param>
    //! <description>Shows a message text with the error severity.</description>
    int Error(IFunctionHandler* pH);

    //! <code>System.IsEditor()</code>
    //! <description>Checks if the system is the editor.</description>
    int IsEditor(IFunctionHandler* pH);

    //! <code>System.IsEditing()</code>
    //! <description>Checks if the system is in pure editor mode, i.e. not editor game mode.</description>
    int IsEditing(IFunctionHandler* pH);

    //! <code>System.GetCurrTime()</code>
    //! <description>Gets the current time.</description>
    int GetCurrTime(IFunctionHandler* pH);

    //! <code>System.GetCurrAsyncTime()</code>
    //! <description>Gets the current asynchronous time.</description>
    int GetCurrAsyncTime(IFunctionHandler* pH);

    //! <code>System.GetFrameTime()</code>
    //! <description>Gets the frame time.</description>
    int GetFrameTime(IFunctionHandler* pH);

    //! <code>System.GetLocalOSTime()</code>
    //! <description>Gets the local operating system time.</description>
    int GetLocalOSTime(IFunctionHandler* pH);

    //! <code>System.GetUserName()</code>
    //! <description>Gets the username on this machine.</description>
    int GetUserName(IFunctionHandler* pH);

    //! <code>System.ShowConsole(nParam)</code>
    //!     <param name="nParam">1 to show the console, 0 to hide.</param>
    //! <description>Shows/hides the console.</description>
    int ShowConsole(IFunctionHandler* pH);

    //! <code>System.CheckHeapValid(name)</code>
    //!     <param name="name">Name string.</param>
    //! <description>Checks the heap validity.</description>
    int CheckHeapValid(IFunctionHandler* pH);

    //! <code>System.GetConfigSpec()</code>
    //! <description>Gets the config specification.</description>
    int GetConfigSpec(IFunctionHandler* pH);

    //! <code>System.IsMultiplayer()</code>
    //! <description>Checks if the game is multiplayer.</description>
    int IsMultiplayer(IFunctionHandler* pH);

    //! <code>System.GetEntity(entityId)</code>
    //!     <param name="entityId">Entity identifier.</param>
    //! <description>Gets an entity from its ID.</description>
    int GetEntity(IFunctionHandler* pH);

    //! <code>System.GetEntityClass(entityId)</code>
    //!     <param name="entityId">Entity identifier.</param>
    //! <description>Gets an entity class from its ID.</description>
    int GetEntityClass(IFunctionHandler* pH);

    //! <code>System.PrepareEntityFromPool(entityId)</code>
    //!     <param name="entityId">Entity identifier.</param>
    //!     <param name="bPrepareNow">(optional) Specifies whether the pooled entity shall be prepared immediately rather than queuing a request in case there is a preparation already in progress.</param>
    //! <description>Prepares the given bookmarked entity from the pool, bringing it into existence.</description>
    int PrepareEntityFromPool(IFunctionHandler* pH);

    //! <code>System.ReturnEntityToPool(entityId)</code>
    //!     <param name="entityId">Entity identifier.</param>
    //! <returns>the bookmarked entity to the pool, destroying it.</returns>
    int ReturnEntityToPool(IFunctionHandler* pH);

    //! <code>System.ResetPoolEntity(entityId)</code>
    //!     <param name="entityId">Entity identifier.</param>
    //! <description>Resets the entity's bookmarked, which frees memory.</description>
    int ResetPoolEntity(IFunctionHandler* pH);

    //! <code>System.GetEntities(center, radius)</code>
    //!     <param name="center">Center position vector for the area where to get entities.</param>
    //!     <param name="radius">Radius of the area.</param>
    //! <description>Gets all the entities contained in the specific area of the level.</description>
    int GetEntities(IFunctionHandler* pH);

    //! <code>System.GetEntitiesByClass(EntityClass)</code>
    //!     <param name="EntityClass">Entity class name.</param>
    //! <description>Gets all the entities of the specified class.</description>
    int GetEntitiesByClass(IFunctionHandler* pH, const char* EntityClass);

    //! <code>System.GetEntitiesInSphere( centre, radius )</code>
    //!     <param name="centre">Centre position vector for the sphere where to look at entities.</param>
    //!     <param name="radius">Radius of the sphere.</param>
    //! <description>Gets all the entities contained into the specified sphere.</description>
    int GetEntitiesInSphere(IFunctionHandler* pH, Vec3 center, float radius);

    //! <code>System.GetEntitiesInSphereByClass( centre, radius, EntityClass )</code>
    //!     <param name="centre">Centre position vector for the sphere where to look at entities.</param>
    //!     <param name="radius">Radius of the sphere.</param>
    //!     <param name="EntityClass">Entity class name.</param>
    //! <description>Gets all the entities contained into the specified sphere for the specific class name.</description>
    int GetEntitiesInSphereByClass(IFunctionHandler* pH, Vec3 center, float radius, const char* EntityClass);

    //! <code>System.GetPhysicalEntitiesInBox( centre, radius )</code>
    //!     <param name="centre">Centre position vector for the area where to look at entities.</param>
    //!     <param name="radius">Radius of the sphere.</param>
    //! <description>Gets all the entities contained into the specified area.</description>
    int GetPhysicalEntitiesInBox(IFunctionHandler* pH, Vec3 center, float radius);

    //! <code>System.GetPhysicalEntitiesInBoxByClass( centre, radius, className )</code>
    //!     <param name="centre">Centre position vector for the area where to look at entities.</param>
    //!     <param name="radius">Radius of the sphere.</param>
    //!     <param name="className">Entity class name.</param>
    //! <description>Gets all the entities contained into the specified area for the specific class name.</description>
    int GetPhysicalEntitiesInBoxByClass(IFunctionHandler* pH, Vec3 center, float radius, const char* className);

    //! <code>System.GetNearestEntityByClass( centre, radius, className )</code>
    //!     <param name="centre">Centre position vector for the area where to look at entities.</param>
    //!     <param name="radius">Radius of the sphere.</param>
    //!     <param name="className">Entity class name.</param>
    //! <description>Gets the nearest entity with the specified class.</description>
    int GetNearestEntityByClass(IFunctionHandler* pH, Vec3 center, float radius, const char* className);

    //! <code>System.GetEntityByName( sEntityName )</code>
    //! <description>
    //!     Retrieve entity table for the first entity with specified name.
    //!     If multiple entities with same name exist, first one found is returned.
    //! </description>
    //!     <param name="sEntityName">Name of the entity to search.</param>
    int GetEntityByName(IFunctionHandler* pH, const char* sEntityName);

    //! <code>System.GetEntityIdByName( sEntityName )</code>
    //! <description>
    //!     Retrieve entity Id for the first entity with specified name.
    //!     If multiple entities with same name exist, first one found is returned.
    //! </description>
    //!     <param name="sEntityName">Name of the entity to search.</param>
    int GetEntityIdByName(IFunctionHandler* pH, const char* sEntityName);

    //! <code>System.DrawLabel( vPos, fSize, text [, r [, g [, b [, alpha]]]] )</code>
    //!     <param name="vPos">Position vector.</param>
    //!     <param name="fSize">Size for the label.</param>
    //!     <param name="text">Text of the label.</param>
    //!     <param name="r">Red component for the label colour. Default is 1.</param>
    //!     <param name="g">Green component for the label colour. Default is 1.</param>
    //!     <param name="b">Blue component for the label colour. Default is 1.</param>
    //!     <param name="alpha">Alpha component for the label colour. Default is 1.</param>
    //! <description>Draws a label with the specified parameter.</description>
    int DrawLabel(IFunctionHandler* pH);

    //! <code>System.DeformTerrain()</code>
    //! <description>Deforms the terrain.</description>
    int DeformTerrain(IFunctionHandler* pH);

    //! <code>System.DeformTerrainUsingMat()</code>
    //! <description>Deforms the terrain using material.</description>
    int DeformTerrainUsingMat(IFunctionHandler* pH);

    //! <code>System.ScreenToTexture()</code>
    int ScreenToTexture(IFunctionHandler* pH);

    //! <code>System.DrawTriStrip(handle, nMode, vtxs, r, g, b, alpha )</code>
    //!     <param name="handle">.</param>
    //!     <param name="nMode">.</param>
    //!     <param name="vtx">.</param>
    //!     <param name="r">Red component for the label color. Default is 1.</param>
    //!     <param name="g">Green component for the label color. Default is 1.</param>
    //!     <param name="b">Blue component for the label color. Default is 1.</param>
    //!     <param name="alpha">Alpha component for the label color. Default is 1.</param>
    //! <description>Draws a triangle strip.</description>
    int DrawTriStrip(IFunctionHandler* pH);

    //! <code>System.DrawLine( p1, p2, r, g, b, alpha )</code>
    //!     <param name="p1">Start position of the line.</param>
    //!     <param name="p2">End position of the line.</param>
    //!     <param name="r">Red component for the label color. Default is 1.</param>
    //!     <param name="g">Green component for the label color. Default is 1.</param>
    //!     <param name="b">Blue component for the label color. Default is 1.</param>
    //!     <param name="alpha">Alpha component for the label color. Default is 1.</param>
    //! <description>Draws a line.</description>
    int DrawLine(IFunctionHandler* pH);

    //! <code>System.Draw2DLine(p1x, p1y, p2x, p2y, r, g, b, alpha )</code>
    //!     <param name="p1x">X value of the start point of the line.</param>
    //!     <param name="p1y">Y value of the start point of the line.</param>
    //!     <param name="p2x">X value of the end point of the line.</param>
    //!     <param name="p2y">Y value of the end point of the line.</param>
    //!     <param name="r">Red component for the label color. Default is 1.</param>
    //!     <param name="g">Green component for the label color. Default is 1.</param>
    //!     <param name="b">Blue component for the label color. Default is 1.</param>
    //!     <param name="alpha">Alpha component for the label color. Default is 1.</param>
    //! <description>Draws a 2D line.</description>
    int Draw2DLine(IFunctionHandler* pH);

    //! <code>System.DrawText( x, y, text, font, size, p2y, r, g, b, alpha )</code>
    //!     <param name="x">X position for the text.</param>
    //!     <param name="y">Y position for the text.</param>
    //!     <param name="text">Text to be displayed.</param>
    //!     <param name="font">Font name.</param>
    //!     <param name="size">Text size.</param>
    //!     <param name="r">Red component for the label color. Default is 1.</param>
    //!     <param name="g">Green component for the label color. Default is 1.</param>
    //!     <param name="b">Blue component for the label color. Default is 1.</param>
    //!     <param name="alpha">Alpha component for the label color. Default is 1.</param>
    //! <description>Draws text.</description>
    int DrawText(IFunctionHandler* pH);

    //! <code>System.SetGammaDelta( fDelta )</code>
    //!     <param name="fDelta">Delta value.</param>
    //! <description>Sets the gamma/delta value.</description>
    int SetGammaDelta(IFunctionHandler* pH);

    //! <code>System.CachePostFxGroup( name )</code>
    //!     <param name="name">Path to the post effect group.
    //! <description>Precaches a post effect group. If this isn't called, then the XML file will be loaded on demand.</description>
    int CachePostFxGroup(IFunctionHandler* pH);

    //! <code>System.SetPostFxGroupEnable( name, enable )</code>
    //!     <param name="name">Path to the post effect group.
    //!     <param name="enable">Boolean indicating whether to enable the group.
    //! <description>Enables or disables a post effect group.</description>
    int SetPostFxGroupEnable(IFunctionHandler* pH);

    //! <code>System.GetPostFxGroupEnable( name )</code>
    //!     <param name="name">Path to the post effect group.
    //! <description>Returns true if post effect group is enabled, false if it's disabled, or nil if it isn't found.</description>
    int GetPostFxGroupEnable(IFunctionHandler* pH);

    //! <code>System.ApplyPostFxGroupAtPosition( name, position )</code>
    //!     <param name="name">Path to the post effect group.
    //!     <param name="position">Position to apply the effect at.
    //! <description>
    //!     Applies a distance-based post effect group at the specified position.
    //!     This would need to be called once each frame per distance-based post effect group while it is enabled.
    //!     If it's called multiple times on the same effect group in a single frame, the effect strength will be increased each time as if each call applies the effect from a different entity.
    //! </description>
    int ApplyPostFxGroupAtPosition(IFunctionHandler* pH);

    //! <code>System.SetPostProcessFxParam( pszEffectParam, value )</code>
    //!     <param name="pszEffectParam">Parameter for the post processing effect.</param>
    //!     <param name="value">Value for the parameter.</param>
    //! <description>Sets a post processing effect parameter value.</description>
    int SetPostProcessFxParam(IFunctionHandler* pH);

    //! <code>System.GetPostProcessFxParam( pszEffectParam, value )</code>
    //!     <param name="pszEffectParam">Parameter for the post processing effect.</param>
    //!     <param name="value">Value for the parameter.</param>
    //! <description>Gets a post processing effect parameter value.</description>
    int GetPostProcessFxParam(IFunctionHandler* pH);

    //! <code>System.SetScreenFx( pszEffectParam, value )</code>
    //!     <param name="pszEffectParam">Parameter for the post processing effect.</param>
    //!     <param name="value">Value for the parameter.</param>
    //! <description>Sets a post processing effect parameter value.</description>
    int SetScreenFx(IFunctionHandler* pH);

    //! <code>System.GetScreenFx( pszEffectParam, value )</code>
    //!     <param name="pszEffectParam">Parameter for the post processing effect.</param>
    //!     <param name="value">Value for the parameter.</param>
    //! <description>Gets a post processing effect parameter value.</description>
    int GetScreenFx(IFunctionHandler* pH);

    //! <code>System.SetCVar( sCVarName, value )</code>
    //!     <param name="sCVarName">Name of the variable.</param>
    //!     <param name="value">Value of the variable.</param>
    //! <description>Sets the value of a CVariable.</description>
    int SetCVar(IFunctionHandler* pH);

    //! <code>System.GetCVar( sCVarName)</code>
    //!     <param name="sCVarName">Name of the variable.</param>
    //! <description>Gets the value of a CVariable.</description>
    int GetCVar(IFunctionHandler* pH);

    //! <code>System.AddCCommand( sCCommandName, sCommand, sHelp)</code>
    //!     <param name="sCCommandName">C command name.</param>
    //!     <param name="sCommand">Command string.</param>
    //!     <param name="sHelp">Help for the command usage.</param>
    //! <description>Adds a C command to the system.</description>
    int AddCCommand(IFunctionHandler* pH);

    //! <code>System.SetScissor( x, y, w, h )</code>
    //!     <param name="x">X position.</param>
    //!     <param name="y -    Y position.</param>
    //!     <param name="w">Width size.</param>
    //!     <param name="h">Height size.</param>
    //! <description>Sets scissor info.</description>
    int SetScissor(IFunctionHandler* pH);

    //! <code>System.GetSystemMem()</code>
    //! <description>Gets the amount of the memory for the system.</description>
    int GetSystemMem(IFunctionHandler* pH);

    //! <code>System.IsPS20Supported()</code>
    //! <description>Checks if the PS20 is supported.</description>
    int IsPS20Supported(IFunctionHandler* pH);

    //! <code>System.IsHDRSupported()</code>
    //! <description>Checks if the HDR is supported.</description>
    int IsHDRSupported(IFunctionHandler* pH);

    //! <code>System.SetBudget(sysMemLimitInMB, videoMemLimitInMB, frameTimeLimitInMS, soundChannelsPlayingLimit, soundMemLimitInMB, numDrawCallsLimit )</code>
    //!     <param name="sysMemLimitInMB">Limit of the system memory in MB.</param>
    //!     <param name="videoMemLimitInMB">Limit of the video memory in MB.</param>
    //!     <param name="frameTimeLimitInMS">Limit in the frame time in MS.</param>
    //!     <param name="soundChannelsPlayingLimit">Limit of the sound channels playing.</param>
    //!     <param name="soundMemLimitInMB">Limit of the sound memory in MB.</param>
    //!     <param name="numDrawCallsLimit">Limit of the draw calls.</param>
    //! <description>Sets system budget.</description>
    int SetBudget(IFunctionHandler* pH);

    //! <code>System.SetVolumetricFogModifiers( gobalDensityModifier, atmosphereHeightModifier )</code>
    //!     <param name="gobalDensityModifier">Modifier for the global density.</param>
    //!     <param name="atmosphereHeightModifier">Modifier for the atmosphere height.</param>
    //! <description>Sets the volumetric fog modifiers.</description>
    int SetVolumetricFogModifiers(IFunctionHandler* pH);

    //! <code>System.SetWind( vWind )</code>
    //!     <param name="vWind">Wind direction.</param>
    //! <description>Sets the wind direction.</description>
    int SetWind(IFunctionHandler* pH);

    //! <code>System.SetWind()</code>
    //! <description>Gets the wind direction.</description>
    int GetWind(IFunctionHandler* pH);

    //! <code>System.GetSurfaceTypeIdByName( surfaceName )</code>
    //!     <param name="surfaceName">Surface name.</param>
    //! <description>Gets the surface type identifier by its name.</description>
    int GetSurfaceTypeIdByName(IFunctionHandler* pH, const char* surfaceName);

    //! <code>System.GetSurfaceTypeNameById( surfaceId )</code>
    //!     <param name="surfaceId">Surface identifier.</param>
    //! <description>Gets the surface type name by its identifier.</description>
    int GetSurfaceTypeNameById(IFunctionHandler* pH, int surfaceId);

    //! <code>System.RemoveEntity( entityId )</code>
    //!     <param name="entityId">Entity identifier.</param>
    //! <description>Removes the specified entity.</description>
    int RemoveEntity(IFunctionHandler* pH, ScriptHandle entityId);

    //! <code>System.SpawnEntity( params )</code>
    //!     <param name="params">Entity parameters.</param>
    //! <description>Spawns an entity.</description>
    int SpawnEntity(IFunctionHandler* pH, SmartScriptTable params);

    //DOC-IGNORE-BEGIN
    //! <code>System.ActivateLight(name, activate)</code>
    //! <description>NOT SUPPORTED ANYMORE.</description>
    int ActivateLight(IFunctionHandler* pH);
    //DOC-IGNORE-END

    //  int ActivateMainLight(IFunctionHandler *pH); //pos, activate
    //  int SetSkyBox(IFunctionHandler *pH); //szShaderName, fBlendTime, bUseWorldBrAndColor

    //! <code>System.SetWaterVolumeOffset()</code>
    //! <description>SetWaterLevel is not supported by 3dengine for now.</description>
    int SetWaterVolumeOffset(IFunctionHandler* pH);

    //! <code>System.IsValidMapPos( v )</code>
    //!     <param name="v">Position vector.</param>
    //! <description>Checks if the position is a valid map position.</description>
    int IsValidMapPos(IFunctionHandler* pH);

    //DOC-IGNORE-BEGIN
    //! <code>System.EnableMainView()</code>
    //! <description>Feature unimplemented.</description>
    int EnableMainView(IFunctionHandler* pH);
    //DOC-IGNORE-END

    //! <code>System.EnableOceanRendering()</code>
    //!     <param name="bOcean">True to activate the ocean rendering, false to deactivate it.</param>
    //! <description>Enables/disables ocean rendering.</description>
    int EnableOceanRendering(IFunctionHandler* pH);

    //! <code>System.ScanDirectory( pszFolderName, nScanMode )</code>
    //!     <param name="pszFolderName">Folder name.</param>
    //!     <param name="nScanMode">Scan mode for the folder. Can be:
    //!             SCANDIR_ALL
    //!             SCANDIR_FILES
    //!             SCANDIR_SUBDIRS</param>
    //! <description>Scans a directory.</description>
    int ScanDirectory(IFunctionHandler* pH);

    //! <code>System.DebugStats( cp )</code>
    int DebugStats(IFunctionHandler* pH);

    //! <code>System.ViewDistanceSet( fViewDist )</code>
    //!     <param name="fViewDist">View distance.</param>
    //! <description>Sets the view distance.</description>
    int ViewDistanceSet(IFunctionHandler* pH);

    //! <code>System.ViewDistanceSet()</code>
    //! <description>Gets the view distance.</description>
    int ViewDistanceGet(IFunctionHandler* pH);

    //! <code>System.GetTerrainElevation( v3Pos )</code>
    //!     <param name="v3Pos">Position of the terraint to be checked.</param>
    //! <description>Gets the terrain elevation of the specified position.</description>
    int GetTerrainElevation(IFunctionHandler* pH);
    //  int SetIndoorColor(IFunctionHandler *pH);

    //! <code>System.ActivatePortal( vPos, bActivate, nID )</code>
    //!     <param name="vPos">Position vector.</param>
    //!     <param name="bActivate">True to activate the portal, false to deactivate.</param>
    //!     <param name="nID">Entity identifier.</param>
    //! <description>Activates/deactivates a portal.</description>
    int ActivatePortal(IFunctionHandler* pH);

    //! <code>System.DumpMMStats()</code>
    //! <description>Dumps the MM statistics.</description>
    int DumpMMStats(IFunctionHandler* pH);

    //! <code>System.EnumAAFormats( m_Width, m_Height, m_BPP )</code>
    //! <description>Enumerates the AA formats.</description>
    int EnumAAFormats(IFunctionHandler* pH);

    //! <code>System.EnumDisplayFormats()</code>
    //! <description>Enumerates display formats.</description>
    int EnumDisplayFormats(IFunctionHandler* pH);

    //! <code>System.IsPointIndoors( vPos )</code>
    //!     <param name="vPos">Position vector.</param>
    //! <description>Checks if a point is indoors.</description>
    int IsPointIndoors(IFunctionHandler* pH);

    //! <code>System.SetConsoleImage( pszName, bRemoveCurrent )</code>
    //!     <param name="pszName">Texture image.</param>
    //!     <param name="bRemoveCurrent">True to remove the current image, false otherwise.</param>
    //! <description>Sets the console image.</description>
    int SetConsoleImage(IFunctionHandler* pH);

    //! <code>System.ProjectToScreen( vec )</code>
    //!     <param name="vec">Position vector.</param>
    //! <description>Projects to the screen (not guaranteed to work if used outside Renderer).</description>
    int ProjectToScreen(IFunctionHandler* pH, Vec3 vec);

    //DOC-IGNORE-BEGIN
    //! <code>System.EnableHeatVision()</code>
    //! <description>Is not supported anymore.</description>
    int EnableHeatVision(IFunctionHandler* pH);
    //DOC-IGNORE-END

    //! <code>System.DumpMemStats( bUseKB )</code>
    //!     <param name="bUseKB">True to use KB, false otherwise.</param>
    //! <description>Dumps memory statistics.</description>
    int DumpMemStats (IFunctionHandler* pH);

    //! <code>System.DumpMemoryCoverage( bUseKB )</code>
    //! <description>Dumps memory coverage.</description>
    int DumpMemoryCoverage(IFunctionHandler* pH);

    //! <code>System.ApplicationTest( pszParam )</code>
    //!     <param name="pszParam">Parameters.</param>
    //! <description>Test the application with the specified parameters.</description>
    int ApplicationTest (IFunctionHandler* pH);

    //! <code>System.QuitInNSeconds( fInNSeconds )</code>
    //!     <param name="fInNSeconds">Number of seconds before quitting.</param>
    //! <description>Quits the application in the specified number of seconds.</description>
    int QuitInNSeconds(IFunctionHandler* pH);

    //! <code>System.DumpWinHeaps()</code>
    //! <description>Dumps windows heaps.</description>
    int DumpWinHeaps (IFunctionHandler* pH);

    //! <code>System.Break()</code>
    //! <description>Breaks the application with a fatal error message.</description>
    int Break(IFunctionHandler* pH);

    //! <code>System.SetViewCameraFov( fov )</code>
    //! <description>Sets the view camera fov.</description>
    int SetViewCameraFov(IFunctionHandler* pH, float fov);

    //! <code>System.GetViewCameraFov()</code>
    //! <description>Gets the view camera fov.</description>
    int GetViewCameraFov(IFunctionHandler* pH);

    //! <code>System.IsPointVisible( point )</code>
    //!     <param name="point">Point vector.</param>
    //! <description>Checks if the specified point is visible.</description>
    int IsPointVisible(IFunctionHandler* pH, Vec3 point);

    //! <code>System.GetViewCameraPos()</code>
    //! <description>Gets the view camera position.</description>
    int GetViewCameraPos(IFunctionHandler* pH);

    //! <code>System.GetViewCameraDir()</code>
    //! <description>Gets the view camera direction.</description>
    int GetViewCameraDir(IFunctionHandler* pH);

    //! <code>System.GetViewCameraUpDir()</code>
    //! <description>Gets the view camera up-direction.</description>
    int GetViewCameraUpDir(IFunctionHandler* pH);

    //! <code>System.GetViewCameraAngles()</code>
    //! <description>Gets the view camera angles.</description>
    int GetViewCameraAngles(IFunctionHandler* pH);

    //! <code>System.RayWorldIntersection(vPos, vDir, nMaxHits, iEntTypes)</code>
    //!     <param name="vPos">Position vector.</param>
    //!     <param name="vDir">Direction vector.</param>
    //!     <param name="nMaxHits">Maximum number of hits.</param>
    //!     <param name="iEntTypes">.</param>
    //! <description>Shots rays into the world.</description>
    int RayWorldIntersection(IFunctionHandler* pH);

    //! <code>System.RayTraceCheck(src, dst, skipId1, skipId2)</code>
    int RayTraceCheck(IFunctionHandler* pH);

    //! <code>System.BrowseURL(szURL)</code>
    //!     <param name="szURL">URL string.</param>
    //! <description>Browses a URL address.</description>
    int BrowseURL(IFunctionHandler* pH);

    //! <code>System.IsDevModeEnable()</code>
    //! <description>
    //!     Checks if game is running in dev mode (cheat mode)
    //!     to check if we are allowed to enable certain scripts
    //!     function facilities (god mode, fly mode etc.).
    //! </description>
    int IsDevModeEnable(IFunctionHandler* pH);

    //! <code>System.SaveConfiguration()</code>
    //! <description>Saves the configuration.</description>
    int SaveConfiguration(IFunctionHandler* pH);

    //! <code>System.Quit()</code>
    //! <description>Quits the program.</description>
    int Quit(IFunctionHandler* pH);

    //! <code>System.GetFrameID()</code>
    //! <description>Gets the frame identifier.</description>
    int GetFrameID(IFunctionHandler* pH);

    //! <code>System.ClearKeyState()</code>
    //! <description>Clear the key state.</description>
    int ClearKeyState(IFunctionHandler* pH);

    //! <code>System.SetSunColor( vColor )</code>
    //! <description>Set color of the sun, only relevant outdoors.</description>
    //!     <param name="vColor">Sun Color as an {x,y,z} vector (x=r,y=g,z=b).</param>
    int SetSunColor(IFunctionHandler* pH, Vec3 vColor);

    //! <code>Vec3 System.GetSunColor()</code>
    //! <description>Retrieve color of the sun outdoors.</description>
    //! <returns>Sun Color as an {x,y,z} vector (x=r,y=g,z=b).</returns>
    int GetSunColor(IFunctionHandler* pH);

    //! <code>System.SetSkyHighlight( params )</code>
    //! <description>Set Sky highlighing parameters.</description>
    //! <seealso cref="GetSkyHighlight">
    //!     <param name="params">Table with Sky highlighing parameters.
    //!         <para>
    //!             Highligh Params     Meaning
    //!             -------------       -----------
    //!             size                Sky highlight scale.
    //!             color               Sky highlight color.
    //!             direction           Direction of the sky highlight in world space.
    //!             pod                 Position of the sky highlight in world space.
    //!          </para></param>
    int SetSkyHighlight(IFunctionHandler* pH, SmartScriptTable params);

    //! <code>System.SetSkyHighlight( params )</code>
    //! <description>
    //!     Retrieves Sky highlighing parameters.
    //!     see SetSkyHighlight for parameters description.
    //! </description>
    //! <seealso cref="SetSkyHighlight">
    int GetSkyHighlight(IFunctionHandler* pH, SmartScriptTable params);

    //! <code>System.LoadLocalizationXml( filename )</code>
    //! <description>Loads Excel exported xml file with text and dialog localization data.</description>
    int LoadLocalizationXml(IFunctionHandler* pH, const char* filename);

private:
    void MergeTable(IScriptTable* pDest, IScriptTable* pSrc);
    //log a string to console and/or to disk with support for different languages
    void    LogString(IFunctionHandler* pH, bool bToConsoleOnly);
    int DeformTerrainInternal(IFunctionHandler* pH, bool nameIsMaterial);

private:
    ISystem* m_pSystem;
    ILog* m_pLog;
    IRenderer* m_pRenderer;
    IConsole* m_pConsole;
    ITimer* m_pTimer;
    I3DEngine* m_p3DEngine;

    //Vlad is too lazy to add this to 3DEngine">so I have to put it here. It should
    //not be here, but I have to put it somewhere.....
    float   m_SkyFadeStart;     // when fogEnd less">start to fade sky to fog
    float   m_SkyFadeEnd;           // when fogEnd less">no sky, just fog

    SmartScriptTable m_pScriptTimeTable;
    SmartScriptTable m_pGetEntitiesCacheTable;
};

#endif // CRYINCLUDE_CRYSCRIPTSYSTEM_SCRIPTBINDINGS_SCRIPTBIND_SYSTEM_H
