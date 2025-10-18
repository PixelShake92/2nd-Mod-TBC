/* 
 * Iron Goron Mod V1.0
 * Allows Goron to walk underwater without drowning
 * 
 * Features:
 * - Goron can walk underwater freely

 * Note: A drowning timer will appear underwater but perhaps a toggle in later versions. 
 */

#include "modding.h"
#include "global.h"
#include "z64player.h"

// External declarations
extern void func_80834104(PlayState* play, Player* this);
extern s32 func_8082DA90(PlayState* play);
extern void PlayerAnimation_Change(PlayState* play, SkelAnime* skelAnime, PlayerAnimationHeader* anim, f32 playSpeed, f32 startFrame, f32 endFrame, u8 mode, f32 morphFrames);
extern void Player_Action_24(Player* this, PlayState* play);

#define ANIMMODE_ONCE 2
#define PLAYER_STATE1_IN_WATER 0x00000001


static bool sNeedsButtonCleanup = false;

// Hook: Clean up button state when Goron exits water
RECOMP_HOOK("Player_Action_24") void Player_Action_24_Goron_Hook(Player* this, PlayState* play) {
    if (this->transformation == PLAYER_FORM_GORON) {
        if (this->actor.depthInWater > 0.0f) {
            // In water - mark that we'll need cleanup when we exit
            sNeedsButtonCleanup = true;
        } else if (sNeedsButtonCleanup && (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
            // Out of water and on ground - clear timer state
            for (int i = 0; i < TIMER_ID_MAX; i++) {
                gSaveContext.timerStates[i] = 0;
                gSaveContext.timerCurTimes[i] = 0;
            }
            
            // Clear action variables that might lock input
            this->av1.actionVar1 = 0;
            this->av2.actionVar2 = 0;
            
            // Clear underwater/drowning flags
            this->stateFlags1 &= ~(PLAYER_STATE1_IN_WATER | PLAYER_STATE1_20000000 | PLAYER_STATE1_80000000);
            
            sNeedsButtonCleanup = false;
        }
    }
}

// Patch: Prevent drowning death for Goron underwater
RECOMP_PATCH void func_80834140(PlayState* play, Player* this, PlayerAnimationHeader* anim) {
    // Block drowning for Goron underwater (but allow damage deaths)
    if (this->transformation == PLAYER_FORM_GORON && 
        this->actor.depthInWater > 20.0f &&
        this->actor.colChkInfo.health > 0) {
        return;
    }
    
    // ORIGINAL for all other cases 
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

// Patch: Fix Deku mask handling to only reset on actual death
RECOMP_PATCH void func_808345C8(void) {
    // Only reset form/mask if player is actually dead (health = 0)
    // This prevents button lock when using Iron Goron and then leaving the water
    if (INV_CONTENT(ITEM_MASK_DEKU) == ITEM_MASK_DEKU && 
        gSaveContext.save.saveInfo.playerData.health == 0) {
        gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
        gSaveContext.save.equippedMask = PLAYER_MASK_NONE;
    }
}