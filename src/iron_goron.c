/* 
 * Iron Goron Mod - Final Version
 * Allows Goron full underwater functionality
 */

#include "modding.h"
#include "global.h"
#include "z64player.h"

// External declarations
extern void func_80834104(PlayState* play, Player* this);
extern s32 func_8082DA90(PlayState* play);
extern void PlayerAnimation_Change(PlayState* play, SkelAnime* skelAnime, PlayerAnimationHeader* anim, f32 playSpeed, f32 startFrame, f32 endFrame, u8 mode, f32 morphFrames);
extern s32 Player_GetEnvironmentalHazard(PlayState* play);
extern void func_80836A5C(Player* this, PlayState* play);

#define ANIMMODE_ONCE 2
#define PLAYER_STATE1_IN_WATER 0x00000001
#define PLAYER_STATE1_SWIMMING 0x08000000

// Hook: Enable buttons for Goron underwater
RECOMP_HOOK("Player_UpdateCommon") void Player_UpdateCommon_Goron_Hook(Player* this, PlayState* play) {
    // ONLY affect Goron - early exit for other forms
    if (this->transformation != PLAYER_FORM_GORON) {
        return;
    }
    
    // Check if Goron is underwater
    if ((Player_GetEnvironmentalHazard(play) >= PLAYER_ENV_HAZARD_UNDERWATER_FLOOR) &&
        (Player_GetEnvironmentalHazard(play) <= PLAYER_ENV_HAZARD_UNDERWATER_FREE)) {
        
        // Force enable all buttons for Goron underwater (functional state)
        gSaveContext.buttonStatus[EQUIP_SLOT_B] = BTN_ENABLED;
        gSaveContext.buttonStatus[EQUIP_SLOT_C_LEFT] = BTN_ENABLED;
        gSaveContext.buttonStatus[EQUIP_SLOT_C_DOWN] = BTN_ENABLED;
        gSaveContext.buttonStatus[EQUIP_SLOT_C_RIGHT] = BTN_ENABLED;
        
        // Clear swimming state flag to prevent slowdown
        this->stateFlags1 &= ~PLAYER_STATE1_SWIMMING;
        
        // Boost speed when player is moving
        if ((play->state.input[0].rel.stick_x || play->state.input[0].rel.stick_y) && 
            this->actor.speed < 2.0f) {
            this->actor.speed = 2.5f;
        }
    }
}

// Hook: Player_Action_54 - intercept swimming dive action for Goron
RECOMP_HOOK("Player_Action_54") void Player_Action_54_Goron_Hook(Player* this, PlayState* play) {
    // If Goron underwater, clear swimming state and allow normal movement
    if (this->transformation == PLAYER_FORM_GORON && this->actor.depthInWater > 0.0f) {
        // Clear swimming state
        this->stateFlags1 &= ~PLAYER_STATE1_SWIMMING;
        // Call cleanup function to restore normal state
        func_80836A5C(this, play);
    }
}

// Hook: Player_Action_55 - intercept swimming action for Goron  
RECOMP_HOOK("Player_Action_55") void Player_Action_55_Goron_Hook(Player* this, PlayState* play) {
    // If Goron underwater, clear swimming state and allow normal movement
    if (this->transformation == PLAYER_FORM_GORON && this->actor.depthInWater > 0.0f) {
        // Clear swimming state
        this->stateFlags1 &= ~PLAYER_STATE1_SWIMMING;
        // Call cleanup function to restore normal state
        func_80836A5C(this, play);
    }
}

// Patch: Prevent drowning death for Goron
RECOMP_PATCH void func_80834140(PlayState* play, Player* this, PlayerAnimationHeader* anim) {
    if (this->transformation == PLAYER_FORM_GORON && 
        this->actor.depthInWater > 20.0f &&
        this->actor.colChkInfo.health > 0) {
        return;
    }
    
    if (!(this->stateFlags1 & PLAYER_STATE1_DEAD)) {
        func_80834104(play, this);
        if (func_8082DA90(play)) {
            this->av2.actionVar2 = -30;
        }
        this->stateFlags1 |= PLAYER_STATE1_DEAD;
        PlayerAnimation_Change(play, &this->skelAnime, anim, 1.0f, 0.0f, 84.0f, ANIMMODE_ONCE, -6.0f);
        this->av1.actionVar1 = 1;
        this->speedXZ = 0.0f;
    }
}

// Patch: Fix Deku mask handling
RECOMP_PATCH void func_808345C8(void) {
    if (INV_CONTENT(ITEM_MASK_DEKU) == ITEM_MASK_DEKU && 
        gSaveContext.save.saveInfo.playerData.health == 0) {
        gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
        gSaveContext.save.equippedMask = PLAYER_MASK_NONE;
    }
}