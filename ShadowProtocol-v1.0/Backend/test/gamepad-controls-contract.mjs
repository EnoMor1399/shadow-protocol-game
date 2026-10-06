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
  assert.match(cpp, /TurnGamepad[\s\S]*GetSafeAimSensitivityMultiplier/);
  assert.match(cpp, /LookUpGamepad[\s\S]*IsMouseYInverted/);
});

test('keyboard override machinery preserves controller mappings', async () => {
  const cpp = await source('../../Source/ShadowProtocol/Private/SPControlSettings.cpp');

  assert.match(cpp, /retain mouse look, analog and gamepad axes/);
  assert.match(cpp, /Seen\.Contains\(Mapping\.ActionName\) && !Mapping\.Key\.IsGamepadKey\(\)/);
  assert.match(cpp, /if \(Mapping\.Key\.IsGamepadKey\(\)\) continue/);
});
