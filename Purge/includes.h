#pragma once

#include "backend/logger/logger.h"
#include "backend/classes/classes.h"

#include <cstdint>
#include <iostream>

// Offsets updated from https://offsets.imtheo.lol/Offsets.hpp
// Roblox version-4ea8e85481e34f46 (dumped 10/11/2026, 08:13 GMT)
namespace offsets {
    // The client build every value below was dumped from. The loader compares this
    // against the running client, so a stale offset set is reported rather than
    // silently resolving to the wrong fields.
    inline constexpr const char* ClientVersion = "version-4ea8e85481e34f46";

    inline constexpr uintptr_t Adornee = 0xD8;
    inline constexpr uintptr_t Anchored = 0x1BE;
    inline constexpr uintptr_t AnchoredMask = 0x2;
    inline constexpr uintptr_t AnimationId = 0xA8;
    inline constexpr uintptr_t AttributeToNext = 0x58;
    inline constexpr uintptr_t AttributeToValue = 0x18;
    inline constexpr uintptr_t AutoJumpEnabled = 0x1CC;
    inline constexpr uintptr_t BeamBrightness = 0x168;
    inline constexpr uintptr_t BeamColor = 0x120;
    inline constexpr uintptr_t BeamLightEmission = 0x174;
    inline constexpr uintptr_t BeamLightInfuence = 0x178;
    inline constexpr uintptr_t CFrame = 0xB0;
    inline constexpr uintptr_t Camera = 0x4A0;
    inline constexpr uintptr_t CameraMaxZoomDistance = 0x370;
    inline constexpr uintptr_t CameraMinZoomDistance = 0x374;
    inline constexpr uintptr_t CameraMode = 0x378;
    inline constexpr uintptr_t CameraPos = 0xE4;
    inline constexpr uintptr_t CameraRotation = 0xC0;
    inline constexpr uintptr_t CameraSubject = 0xB0;
    inline constexpr uintptr_t CameraType = 0x120;
    inline constexpr uintptr_t CanCollide = 0x1BE;
    inline constexpr uintptr_t CanCollideMask = 0x8;
    inline constexpr uintptr_t CanTouch = 0x1BE;
    inline constexpr uintptr_t CanTouchMask = 0x10;
    inline constexpr uintptr_t CharacterAppearanceId = 0x298;
    inline constexpr uintptr_t Children = 0x70;
    inline constexpr uintptr_t ChildrenEnd = 0x8;
    inline constexpr uintptr_t ClassDescriptor = 0x18;
    inline constexpr uintptr_t ClassDescriptorToClassName = 0x8;
    inline constexpr uintptr_t ClickDetectorMaxActivationDistance = 0xD0;
    inline constexpr uintptr_t ClockTime = 0xB0;
    inline constexpr uintptr_t CreatorId = 0x190;
    inline constexpr uintptr_t DataModelDeleterPointer = 0x759FD20;
    inline constexpr uintptr_t DataModelPrimitiveCount = 0x430;
    inline constexpr uintptr_t DataModelToRenderView1 = 0x1D8;
    inline constexpr uintptr_t DataModelToRenderView2 = 0x8;
    inline constexpr uintptr_t DataModelToRenderView3 = 0x28;
    inline constexpr uintptr_t DecalTexture = 0x1C8;
    inline constexpr uintptr_t Deleter = 0x10;
    inline constexpr uintptr_t DeleterBack = 0x18;
    inline constexpr uintptr_t Dimensions = 0xB10;
    inline constexpr uintptr_t DisplayName = 0x120;
    inline constexpr uintptr_t EvaluateStateMachine = 0x1D1;
    inline constexpr uintptr_t FOV = 0x128;
    inline constexpr uintptr_t FakeDataModelPointer = 0x8F8E968;
    inline constexpr uintptr_t FakeDataModelToDataModel = 0x1F8;
    inline constexpr uintptr_t FogColor = 0xDC;
    inline constexpr uintptr_t FogEnd = 0x114;
    inline constexpr uintptr_t FogStart = 0x118;
    inline constexpr uintptr_t ForceNewAFKDuration = 0x1F8;
    inline constexpr uintptr_t FramePositionOffsetX = 0x2AC;
    inline constexpr uintptr_t FramePositionOffsetY = 0x2B4;
    inline constexpr uintptr_t FramePositionX = 0x2A8;
    inline constexpr uintptr_t FramePositionY = 0x2B0;
    inline constexpr uintptr_t FrameRotation = 0xD0;
    inline constexpr uintptr_t FrameSizeOffsetX = 0x2D0;
    inline constexpr uintptr_t FrameSizeOffsetY = 0x2D4;
    inline constexpr uintptr_t FrameSizeX = 0x2C8;
    inline constexpr uintptr_t FrameSizeY = 0x2CC;
    inline constexpr uintptr_t GameId = 0x198;
    inline constexpr uintptr_t GameLoaded = 0x5B8;
    inline constexpr uintptr_t Gravity = 0x218;
    inline constexpr uintptr_t Health = 0x184;
    inline constexpr uintptr_t HealthDisplayDistance = 0x3A0;
    inline constexpr uintptr_t HipHeight = 0x188;
    inline constexpr uintptr_t HumanoidDisplayName = 0xA0;
    inline constexpr uintptr_t HumanoidState = 0x8B0;
    inline constexpr uintptr_t HumanoidStateId = 0x20;
    inline constexpr uintptr_t InputObject = 0xE8;
    inline constexpr uintptr_t InsetMaxX = 0x100;
    inline constexpr uintptr_t InsetMaxY = 0x104;
    inline constexpr uintptr_t InsetMinX = 0xF8;
    inline constexpr uintptr_t InsetMinY = 0xFC;
    inline constexpr uintptr_t InstanceAttributePointer1 = 0x40;
    inline constexpr uintptr_t InstanceAttributePointer2 = 0x18;
    inline constexpr uintptr_t InstanceCapabilities = 0x588;
    inline constexpr uintptr_t JobEnd = 0xD0;
    inline constexpr uintptr_t JobId = 0x108;
    inline constexpr uintptr_t JobStart = 0xC8;
    inline constexpr uintptr_t Job_Name = 0x18;
    inline constexpr uintptr_t JobsPointer = 0x8F1E408;
    inline constexpr uintptr_t JumpPower = 0x19C;
    inline constexpr uintptr_t LocalPlayer = 0x118;
    inline constexpr uintptr_t LocalScriptByteCode = 0x178;
    inline constexpr uintptr_t LocalScriptBytecodePointer = 0x18;
    inline constexpr uintptr_t LocalScriptHash = 0x188;
    inline constexpr uintptr_t MaterialType = 0x24E;
    inline constexpr uintptr_t MaxHealth = 0x1A0;
    inline constexpr uintptr_t MaxSlopeAngle = 0x1A4;
    inline constexpr uintptr_t MeshPartColor3 = 0x12;
    inline constexpr uintptr_t MeshPartTexture = 0x328;
    inline constexpr uintptr_t MeshPartMeshId = 0x2F8;
    inline constexpr uintptr_t ModelInstance = 0x2A0;
    inline constexpr uintptr_t ModuleScriptByteCode = 0x120;
    inline constexpr uintptr_t ModuleScriptBytecodePointer = 0x18;
    inline constexpr uintptr_t ModuleScriptHash = 0x348;
    inline constexpr uintptr_t MoonTextureId = 0xB0;
    inline constexpr uintptr_t MousePosition = 0xBC;
    inline constexpr uintptr_t MouseSensitivity = 0x7677810;
    inline constexpr uintptr_t MoveDirection = 0x134;
    inline constexpr uintptr_t Name = 0x68;
    inline constexpr uintptr_t NameDisplayDistance = 0x3B0;
    inline constexpr uintptr_t NameSize = 0x10;
    inline constexpr uintptr_t OnDemandInstance = 0x38;
    inline constexpr uintptr_t OutdoorAmbient = 0xE8;
    inline constexpr uintptr_t Parent = 0x60;
    inline constexpr uintptr_t PartSize = 0x1C4;
    inline constexpr uintptr_t Ping = 0xC8;
    inline constexpr uintptr_t PlaceId = 0x1A0;
    inline constexpr uintptr_t PlayerConfigurerPointer = 0x757C588;
    inline constexpr uintptr_t PlayerMouse = 0x11F0;
    inline constexpr uintptr_t Position = 0xD4;
    inline constexpr uintptr_t Primitive = 0x178;
    inline constexpr uintptr_t PrimitiveValidateValue = 0x6;
    inline constexpr uintptr_t PrimitivesPointer1 = 0x3F8;
    inline constexpr uintptr_t PrimitivesPointer2 = 0x2A0;
    inline constexpr uintptr_t ProximityPromptActionText = 0x98;
    inline constexpr uintptr_t ProximityPromptEnabled = 0x11E;
    inline constexpr uintptr_t ProximityPromptGamepadKeyCode = 0x104;
    inline constexpr uintptr_t ProximityPromptHoldDuraction = 0x108;
    inline constexpr uintptr_t ProximityPromptMaxActivationDistance = 0x110;
    inline constexpr uintptr_t ProximityPromptMaxObjectText = 0xB8;
    inline constexpr uintptr_t RenderJobToDataModel = 0x1F0;
    inline constexpr uintptr_t RenderJobToFakeDataModel = 0x38;
    inline constexpr uintptr_t RenderJobToRenderView = 0x1E0;
    inline constexpr uintptr_t RequireBypass = 0x8E0;
    inline constexpr uintptr_t RigType = 0x1B8;
    inline constexpr uintptr_t Rotation = 0xB0;
    inline constexpr uintptr_t RunContext = 0x168;
    inline constexpr uintptr_t ScriptContext = 0x440;
    inline constexpr uintptr_t Sit = 0x1D7;
    inline constexpr uintptr_t SkyboxBk = 0xE0;
    inline constexpr uintptr_t SkyboxDn = 0x110;
    inline constexpr uintptr_t SkyboxFt = 0x140;
    inline constexpr uintptr_t SkyboxLf = 0x170;
    inline constexpr uintptr_t SkyboxRt = 0x1A0;
    inline constexpr uintptr_t SkyboxUp = 0x1D0;
    inline constexpr uintptr_t SoundId = 0xB0;
    inline constexpr uintptr_t StarCount = 0x230;
    inline constexpr uintptr_t StringLength = 0x10;
    inline constexpr uintptr_t SunTextureId = 0x200;
    inline constexpr uintptr_t TagList = 0x110;
    inline constexpr uintptr_t TaskSchedulerMaxFPS = 0xB0;
    inline constexpr uintptr_t TaskSchedulerPointer = 0x8F1E408;
    inline constexpr uintptr_t Team = 0x2E0;
    inline constexpr uintptr_t TeamColor = 0x3B8;
    inline constexpr uintptr_t TextLabelText = 0xA30;
    inline constexpr uintptr_t TextLabelVisible = 0x72D;
    inline constexpr uintptr_t Tool_Grip_Position = 0x494;
    inline constexpr uintptr_t Transparency = 0x118;
    inline constexpr uintptr_t UserId = 0xB8;
    inline constexpr uintptr_t Value = 0xA0;
    inline constexpr uintptr_t Velocity = 0xE0;
    inline constexpr uintptr_t ViewportSize = 0x2C4;
    inline constexpr uintptr_t VisualEngine = 0x10;
    inline constexpr uintptr_t VisualEnginePointer = 0x8A11808;
    inline constexpr uintptr_t VisualEngineToDataModel1 = 0xAF0;
    inline constexpr uintptr_t VisualEngineToDataModel2 = 0x1F8;
    inline constexpr uintptr_t WalkSpeed = 0x1C8;
    inline constexpr uintptr_t WalkSpeedCheck = 0x3A4;
    inline constexpr uintptr_t Workspace = 0x168;
    inline constexpr uintptr_t WorkspaceToWorld = 0x3F8;
    inline constexpr uintptr_t viewmatrix = 0x1B0;

    // todo : update below ones
    inline constexpr uintptr_t PhysicsSenderMaxBandwidthBps = 0x5e96f10;
    inline constexpr uintptr_t DebugEnableInterpThrottle = 0x5ed1f90;
    inline constexpr uintptr_t PhysicsSenderMaxBandwidthBpsScaling = 0x5e96f0c;
}