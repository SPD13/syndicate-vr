/******************************************************************************/
// Bullfrog Engine Emulation Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet or Dungeon Keeper.
/******************************************************************************/
/** @file bfjoyst.h
 *     Header file for sjoyst.c.
 * @par Purpose:
 *     Joystick support.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   Tomasz Lis
 * @date     11 Feb 2010 - 11 Oct 2020
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef BFLIB_JOYST_H
#define BFLIB_JOYST_H

#include "bftypes.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
#pragma pack(1)

 #define MAX_JOYSTICK_COUNT 16

struct DevInput {
        s32 Yaw[MAX_JOYSTICK_COUNT];
        s32 Roll[MAX_JOYSTICK_COUNT];
        s32 Pitch[MAX_JOYSTICK_COUNT];
        s32 AnalogueX[MAX_JOYSTICK_COUNT];
        s32 AnalogueY[MAX_JOYSTICK_COUNT];
        s32 AnalogueZ[MAX_JOYSTICK_COUNT];
        s32 AnalogueU[MAX_JOYSTICK_COUNT];
        s32 AnalogueV[MAX_JOYSTICK_COUNT];
        s32 AnalogueR[MAX_JOYSTICK_COUNT];
        s32 DigitalX[MAX_JOYSTICK_COUNT];
        s32 DigitalY[MAX_JOYSTICK_COUNT];
        s32 DigitalZ[MAX_JOYSTICK_COUNT];
        s32 DigitalU[MAX_JOYSTICK_COUNT];
        s32 DigitalV[MAX_JOYSTICK_COUNT];
        s32 DigitalR[MAX_JOYSTICK_COUNT];
        s32 MinXAxis[MAX_JOYSTICK_COUNT];
        s32 MinYAxis[MAX_JOYSTICK_COUNT];
        s32 MinZAxis[MAX_JOYSTICK_COUNT];
        s32 MinUAxis[MAX_JOYSTICK_COUNT];
        s32 MinVAxis[MAX_JOYSTICK_COUNT];
        s32 MinRAxis[MAX_JOYSTICK_COUNT];
        s32 MaxXAxis[MAX_JOYSTICK_COUNT];
        s32 MaxYAxis[MAX_JOYSTICK_COUNT];
        s32 MaxZAxis[MAX_JOYSTICK_COUNT];
        s32 MaxUAxis[MAX_JOYSTICK_COUNT];
        s32 MaxVAxis[MAX_JOYSTICK_COUNT];
        s32 MaxRAxis[MAX_JOYSTICK_COUNT];
        s32 XCentre[MAX_JOYSTICK_COUNT];
        s32 YCentre[MAX_JOYSTICK_COUNT];
        s32 ZCentre[MAX_JOYSTICK_COUNT];
        s32 UCentre[MAX_JOYSTICK_COUNT];
        s32 VCentre[MAX_JOYSTICK_COUNT];
        s32 RCentre[MAX_JOYSTICK_COUNT];
        s32 HatX[MAX_JOYSTICK_COUNT];
        s32 HatY[MAX_JOYSTICK_COUNT];
        s32 HatMax[MAX_JOYSTICK_COUNT];
        u32 Buttons[MAX_JOYSTICK_COUNT];
        s32 NumberOfButtons[MAX_JOYSTICK_COUNT];
        s32 ConfigType[MAX_JOYSTICK_COUNT];
        s32 MenuButtons[MAX_JOYSTICK_COUNT];
        s32 Type;
        s32 NumberOfDevices;
        s32 DeviceType[MAX_JOYSTICK_COUNT];
        unsigned char Init[MAX_JOYSTICK_COUNT];
};

#pragma pack()
/******************************************************************************/
extern struct DevInput joy;

TbResult JoySetInterrupt(short val);
TbResult JoyGetDeviceName(char *textbuf);
TbResult JoyUpdateInputs(struct DevInput *dinp);
TbResult JoyRefreshDevices(struct DevInput *dinp);
TbResult JoySetupDevice(struct DevInput *dinp, int jtype);

/** Joystick drivers initialization.
 */
TbResult JoyDriverInit(void);

/** Joystick drivers shutdown.
 */
TbResult JoyDriverShutdown(void);

const char *JoyGetButtonLabel(int button);


typedef enum
{
    CONTROLLER_BUTTON_INVALID = -1,
    CONTROLLER_BUTTON_A,
    CONTROLLER_BUTTON_B,
    CONTROLLER_BUTTON_X,
    CONTROLLER_BUTTON_Y,
    CONTROLLER_BUTTON_BACK,
    CONTROLLER_BUTTON_GUIDE,
    CONTROLLER_BUTTON_START,
    CONTROLLER_BUTTON_LEFTSTICK,
    CONTROLLER_BUTTON_RIGHTSTICK,
    CONTROLLER_BUTTON_LEFTSHOULDER,
    CONTROLLER_BUTTON_RIGHTSHOULDER,
    CONTROLLER_BUTTON_DPAD_UP,
    CONTROLLER_BUTTON_DPAD_DOWN,
    CONTROLLER_BUTTON_DPAD_LEFT,
    CONTROLLER_BUTTON_DPAD_RIGHT,
    CONTROLLER_BUTTON_MISC1,    /* Xbox Series X share button, PS5 microphone button, Nintendo Switch Pro capture button, Amazon Luna microphone button */
    CONTROLLER_BUTTON_PADDLE1,  // Xbox Elite paddle P1 (upper left, facing the back)
    CONTROLLER_BUTTON_PADDLE2,  // Xbox Elite paddle P3 (upper right, facing the back)
    CONTROLLER_BUTTON_PADDLE3,  // Xbox Elite paddle P2 (lower left, facing the back)
    CONTROLLER_BUTTON_PADDLE4,  // Xbox Elite paddle P4 (lower right, facing the back)
    CONTROLLER_BUTTON_TOUCHPAD, // PS4/PS5 touchpad button
    CONTROLLER_BUTTON_TRIGGER_LEFT,
    CONTROLLER_BUTTON_TRIGGER_RIGHT,
    CONTROLLER_BUTTON_RIGHT_THUMB_LEFT,
    CONTROLLER_BUTTON_RIGHT_THUMB_RIGHT,
    CONTROLLER_BUTTON_RIGHT_THUMB_UP,
    CONTROLLER_BUTTON_RIGHT_THUMB_DOWN,
    CONTROLLER_BUTTON_MAX,
} GameControllerButton;

/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif
