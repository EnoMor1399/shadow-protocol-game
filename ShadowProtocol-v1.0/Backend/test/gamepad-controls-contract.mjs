import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { test } from 'node:test';

async function source(relativePath) {
  return readFile(new URL(relativePath, import.meta.url), 'utf8');
}

function escapeRegex(value) {
  return value.replace(/[.*+?^$()|[\]\\]/g, '\\$&');
}

test('stock Unreal input config provides a native gamepad combat profile', async () => {
  const input = await source('../../Config/DefaultInput.ini');

  for (const expected of [
    'AxisName="MoveForward",Scale=1.0,Key=Gamepad_LeftY',
    'AxisName="MoveRight",Scale=1.0,Key=Gamepad_LeftX',
    'AxisName="TurnGamepad",Scale=1.0,Key=Gamepad_RightX',
    'AxisName="LookUpGamepad",Scale=-1.0,Key=Gamepad_RightY',
    'ActionName="Fire",Key=Gamepad_RightTrigger',
    'ActionName="Aim",Key=Gamepad_LeftTrigger',
    'ActionName="Reload",Key=Gamepad_FaceButton_Left',
    'ActionName="Sprint",Key=Gamepad_LeftThumbstick',
    'ActionName="Crouch",Key=Gamepad_RightThumbstick',
    'ActionName="Vault",Key=Gamepad_FaceButton_Bottom',
    'ActionName="Controls",Key=Gamepad_Special_Right',
    'ActionName="Scoreboard",Key=Gamepad_Special_Left'
  ]) {
    assert.match(input, new RegExp(escapeRegex(expected)));
  }
});

test('gamepad look is separated from mouse delta input and remains gameplay gated', async () => {
  const header = await source('../../Source/ShadowProtocol/Public/SPCharacter.h');
  const cpp = await source('../../Source/ShadowProtocol/Private/SPCharacter.cpp');

  assert.match(header, /TurnGamepad\(float V\)/);
  assert.match(header, /LookUpGamepad\(float V\)/);
  assert.match(cpp, /BindAxis\("TurnGamepad"[\s\S]*TurnGamepad/);
  assert.match(cpp, /BindAxis\("LookUpGamepad"[\s\S]*LookUpGamepad/);
  assert.match(cpp, /TurnGamepad[\s\S]*CanUseLocalControls\(\)/);
  assert.match(cpp, /LookUpGamepad[\s\S]*CanUseLocalControls\(\)/);
  assert.match(cpp, /TurnGamepad[\s\S]*GetDeltaSeconds\(\)[\s\S]*AddControllerYawInput/);
  assert.match(cpp, /LookUpGamepad[\s\S]*GetDeltaSeconds\(\)[\s\S]*AddControllerPitchInput/);
  const turnGamepad = cpp.split('void ASPCharacter::TurnGamepad(float V)')[1]
    .split('void ASPCharacter::LookUpGamepad(float V)')[0];
  const lookGamepad = cpp.split('void ASPCharacter::LookUpGamepad(float V)')[1]
    .split('void ASPCharacter::Fire()')[0];

  assert.match(turnGamepad, /ApplyGamepadDeadZone/);
  assert.match(turnGamepad, /GetSafeGamepadLookSensitivity/);
  assert.match(turnGamepad, /GetSafeGamepadAimSensitivityMultiplier/);
  assert.doesNotMatch(turnGamepad, /GetMouseSensitivity/);
  assert.match(lookGamepad, /ApplyGamepadDeadZone/);
  assert.match(lookGamepad, /bInvertGamepadY/);
  assert.doesNotMatch(lookGamepad, /IsMouseYInverted|GetMouseSensitivity/);
});

test('controller tuning is persisted, bounded and independent from mouse settings', async () => {
  const header = await source('../../Source/ShadowProtocol/Public/SPControlSettings.h');
  const settings = await source('../../Source/ShadowProtocol/Private/SPControlSettings.cpp');
  const controls = await source('../../Source/ShadowProtocol/Private/SPControlsWidget.cpp');

  for (const name of [
    'GamepadLookSensitivity',
    'GamepadAimSensitivityMultiplier',
    'GamepadDeadZone',
    'bInvertGamepadY'
  ]) assert.ok(header.includes(name));

  assert.match(settings, /GetSafeGamepadLookSensitivity/);
  assert.match(settings, /FMath::Clamp\(GamepadLookSensitivity, 0\.25f, 3\.f\)/);
  assert.match(settings, /GetSafeGamepadAimSensitivityMultiplier/);
  assert.match(settings, /FMath::Clamp\(GamepadAimSensitivityMultiplier, 0\.1f, 1\.f\)/);
  assert.match(settings, /GetSafeGamepadDeadZone/);
  assert.match(settings, /FMath::Clamp\(GamepadDeadZone, 0\.05f, 0\.5f\)/);
  assert.match(settings, /ApplyGamepadDeadZone/);
  assert.match(settings, /Magnitude <= DeadZone/);

  assert.match(controls, /Controller look sensitivity/);
  assert.match(controls, /Controller ADS sensitivity/);
  assert.match(controls, /Right-stick dead zone/);
  assert.match(controls, /Invert vertical controller look/);
  assert.match(controls, /Reset controller settings/);
  assert.match(controls, /SetGamepadSensitivity/);
  assert.match(controls, /SetGamepadAimSensitivity/);
  assert.match(controls, /SetGamepadDeadZone/);
  assert.match(controls, /SetGamepadInvert/);
  assert.match(controls, /GamepadAimSensitivitySlider->SetValue\(0\.75f\)/);
});

test('keyboard override machinery preserves controller mappings', async () => {
  const cpp = await source('../../Source/ShadowProtocol/Private/SPControlSettings.cpp');

  assert.match(cpp, /retain mouse look, analog and gamepad axes/);
  assert.match(cpp, /Seen\.Contains\(Mapping\.ActionName\) && !Mapping\.Key\.IsGamepadKey\(\)/);
  assert.match(cpp, /if \(Mapping\.Key\.IsGamepadKey\(\)\) continue/);
});

test('controller action remapping is isolated and conflict safe', async () => {
  const header = await source('../../Source/ShadowProtocol/Public/SPControlSettings.h');
  const settings = await source('../../Source/ShadowProtocol/Private/SPControlSettings.cpp');
  const controls = await source('../../Source/ShadowProtocol/Private/SPControlsWidget.cpp');

  assert.match(header, /GamepadActionOverrides/);
  assert.match(header, /RebindGamepadAction/);
  assert.match(header, /FindGamepadActionUsingKey/);
  assert.match(header, /ResetGamepadBindings/);

  assert.match(settings, /GamepadSeen/);
  assert.match(settings, /Mapping\.Key\.IsGamepadKey\(\)/);
  assert.match(settings, /controller input is used by movement or camera look/);
  assert.match(settings, /FindGamepadActionUsingKey\(Key, Action, Conflict\)/);
  assert.match(settings, /GamepadActionOverrides = Previous/);
  assert.match(settings, /ResetGamepadBindings/);
  assert.match(settings, /GamepadActionOverrides\.Reset\(\)/);

  assert.match(controls, /CONTROLLER ACTION BUTTONS/);
  assert.match(controls, /SetAllowGamepadKeys\(true\)/);
  assert.match(controls, /CaptureGamepadBinding/);
  assert.match(controls, /RebindGamepadAction/);
  assert.match(controls, /Restore original controller buttons/);
  assert.match(controls, /Original controller action buttons restored/);
});

test('native UI exposes controller shortcuts while keeping keyboard and controller remapping separate', async () => {
  const settings = await source('../../Source/ShadowProtocol/Private/SPControlSettings.cpp');
  const hud = await source('../../Source/ShadowProtocol/Private/SPCombatHUD.cpp');
  const controls = await source('../../Source/ShadowProtocol/Private/SPControlsWidget.cpp');
  const readyRoom = await source('../../Source/ShadowProtocol/Private/SPReadyRoomWidget.cpp');

  assert.doesNotMatch(settings, /Mapping\.ActionName != Action \|\| Mapping\.Key\.IsGamepadKey\(\)/);
  assert.match(hud, /GetActionKeyLabel\(TEXT\("Controls"\)\)/);
  assert.match(controls, /EKeys::Gamepad_Special_Right/);
  assert.match(controls, /TurnGamepad/);
  assert.match(controls, /LookUpGamepad/);
  assert.match(readyRoom, /EKeys::Gamepad_Special_Right/);
  assert.match(readyRoom, /Controls & input settings/);
  assert.match(controls, /SetAllowGamepadKeys\(false\)/);
  assert.match(controls, /SetAllowGamepadKeys\(true\)/);
});
