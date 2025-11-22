/* 
 * Iron Goron Mod - Final Working Version
 */

#include "modding.h"
#include "global.h"
#include "z64player.h"
#include "overlays/actors/ovl_Obj_Taru/z_obj_taru.h"
#include "overlays/actors/ovl_En_Wood02/z_en_wood02.h"
#include "overlays/effects/ovl_Effect_Ss_G_Splash/z_eff_ss_g_splash.h"

// External declarations
extern void func_80834104(PlayState* play, Player* this);
extern s32 func_8082DA90(PlayState* play);
extern void PlayerAnimation_Change(PlayState* play, SkelAnime* skelAnime, PlayerAnimationHeader* anim, f32 playSpeed, f32 startFrame, f32 endFrame, u8 mode, f32 morphFrames);

// External water speed globals
extern f32 sWaterSpeedFactor;
extern f32 sInvWaterSpeedFactor;

#define ANIMMODE_ONCE 2
#define PLAYER_STATE1_IN_WATER 0x00000001
#define PLAYER_STATE1_SWIMMING 0x08000000
#define PLAYER_BOOTS_ZORA_UNDERWATER 3

// Static variables
static Player* sGoronSpeedPlayer = NULL;
static PlayState* sGoronSpeedPlay = NULL;

// Patch: func_8082FD0C - Allow transformation masks on C buttons underwater
RECOMP_PATCH s32 func_8082FD0C(Player* this, s32 item) {
    if (item >= ITEM_MASK_DEKU && item <= ITEM_MASK_FIERCE_DEITY) {
        return true;
    }
    
    if (this->stateFlags1 & (PLAYER_STATE1_IN_WATER | PLAYER_STATE1_SWIMMING)) {
        return false;
    }
    
    return true;
}

// Hook: Player_UpdateCommon ENTRY
RECOMP_HOOK("Player_UpdateCommon") void Player_UpdateCommon_Entry(Player* this, PlayState* play, Input* input) {
    sGoronSpeedPlayer = this;
    sGoronSpeedPlay = play;
    
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    
    // Water speed fix
    if (this->transformation == PLAYER_FORM_GORON && this->actor.depthInWater > 0.0f) {
        sWaterSpeedFactor = 1.0f;
        sInvWaterSpeedFactor = 1.0f;
    }
    
    // Enable C buttons for transformation masks underwater
    if (this->stateFlags1 & PLAYER_STATE1_SWIMMING) {
        for (s32 i = EQUIP_SLOT_C_LEFT; i <= EQUIP_SLOT_C_RIGHT; i++) {
            ItemId item = GET_CUR_FORM_BTN_ITEM(i);
            if (item >= ITEM_MASK_DEKU && item <= ITEM_MASK_FIERCE_DEITY) {
                gSaveContext.buttonStatus[i] = BTN_ENABLED;
                
                if (i == EQUIP_SLOT_C_LEFT) {
                    interfaceCtx->cLeftAlpha = 255;
                } else if (i == EQUIP_SLOT_C_DOWN) {
                    interfaceCtx->cDownAlpha = 255;
                } else if (i == EQUIP_SLOT_C_RIGHT) {
                    interfaceCtx->cRightAlpha = 255;
                }
            }
        }
    }
    
    // Goron underwater
    if (this->transformation == PLAYER_FORM_GORON && this->actor.depthInWater > 0.0f) {
        this->currentBoots = PLAYER_BOOTS_ZORA_UNDERWATER;
        
        gSaveContext.buttonStatus[EQUIP_SLOT_B] = BTN_ENABLED;
        gSaveContext.buttonStatus[EQUIP_SLOT_C_LEFT] = BTN_ENABLED;
        gSaveContext.buttonStatus[EQUIP_SLOT_C_DOWN] = BTN_ENABLED;
        gSaveContext.buttonStatus[EQUIP_SLOT_C_RIGHT] = BTN_ENABLED;
        
        this->underwaterTimer = 0;
        this->stateFlags1 &= ~PLAYER_STATE1_SWIMMING;
    }
}

// Hook: Player_UpdateCommon RETURN
RECOMP_HOOK_RETURN("Player_UpdateCommon") void Player_UpdateCommon_Return(void) {
    if (sGoronSpeedPlayer != NULL && sGoronSpeedPlay != NULL) {
        if (sGoronSpeedPlayer->transformation == PLAYER_FORM_GORON && sGoronSpeedPlayer->actor.depthInWater > 0.0f) {
            sWaterSpeedFactor = 1.0f;
            sInvWaterSpeedFactor = 1.0f;
        }
        
        sGoronSpeedPlayer = NULL;
        sGoronSpeedPlay = NULL;
    }
}

// Hook: Player_Init
RECOMP_HOOK("Player_Init") void Player_Init_Goron_Hook(Actor* thisx, PlayState* play) {
    Player* this = (Player*)thisx;
    
    if (this->transformation == PLAYER_FORM_GORON) {
        this->stateFlags1 &= ~PLAYER_STATE1_SWIMMING;
        this->underwaterTimer = 0;
        if (this->actor.depthInWater > 0.0f) {
            this->currentBoots = PLAYER_BOOTS_ZORA_UNDERWATER;
        }
    }
}

// Hook: Register melee weapon collision
RECOMP_HOOK("Player_UpdateCommon") void Player_UpdateCommon_RegisterPunch(Player* this, PlayState* play, Input* input) {
    if (this->meleeWeaponState >= PLAYER_MELEE_WEAPON_STATE_1) {
        if ((this->transformation == PLAYER_FORM_GORON && this->actor.depthInWater > 0.0f) ||
            this->actor.depthInWater == 0.0f) {
            
            CollisionCheck_SetAT(play, &play->colChkCtx, &this->meleeWeaponQuads[0].base);
            CollisionCheck_SetAT(play, &play->colChkCtx, &this->meleeWeaponQuads[1].base);
        }
    }
}

// Hook: Break nearby objects when Goron punches
RECOMP_HOOK("Player_UpdateCommon") void Player_UpdateCommon_BreakObjects(Player* this, PlayState* play, Input* input) {
    static u8 punchTriggered = 0;
    
    if (this->transformation != PLAYER_FORM_GORON || 
        this->meleeWeaponState < PLAYER_MELEE_WEAPON_STATE_1) {
        punchTriggered = 0;
        return;
    }
    
    if (punchTriggered) {
        return;
    }
    punchTriggered = 1;
    
    Actor* actor = play->actorCtx.actorLists[ACTORCAT_BG].first;
    while (actor != NULL) {
        f32 distSq = Math3D_Vec3fDistSq(&this->actor.world.pos, &actor->world.pos);
        
        if (actor->id == ACTOR_OBJ_TARU) {
            ObjTaru* taru = (ObjTaru*)actor;
            if (OBJ_TARU_GET_80(&taru->dyna.actor) && 
                distSq < SQ(150.0f) && 
                this->actor.depthInWater > 0.0f) {
                taru->dyna.actor.home.rot.z = 1;
            }
        } else if (actor->id == ACTOR_OBJ_YASI && distSq < SQ(150.0f)) {
            actor->home.rot.z = 1;
        } else if (actor->id == ACTOR_OBJ_TREE && distSq < SQ(150.0f)) {
            actor->home.rot.y = 1;
        }
        
        actor = actor->next;
    }
    
    actor = play->actorCtx.actorLists[ACTORCAT_PROP].first;
    while (actor != NULL) {
        f32 distSq = Math3D_Vec3fDistSq(&this->actor.world.pos, &actor->world.pos);
        
        if (actor->id == ACTOR_OBJ_YASI && distSq < SQ(50.0f)) {
            actor->home.rot.z = 1;
        } else if ((actor->id == ACTOR_EN_WOOD02 || 
                    actor->id == ACTOR_EN_SNOWWD || 
                    actor->id == ACTOR_OBJ_TREE) && 
                   distSq < SQ(50.0f)) {
            actor->home.rot.y = 1;
        }
        
        actor = actor->next;
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

// Patch: Skip water splash effects for Goron underwater
RECOMP_PATCH s32 func_80837730(PlayState* play, Player* this, f32 arg2, s32 scale) {
    if (this->transformation == PLAYER_FORM_GORON && this->actor.depthInWater > 55.0f) {
        return false;
    }
    
    f32 sp3C = fabsf(arg2);
    if (sp3C > 2.0f) {
        WaterBox* waterBox;
        f32 sp34;
        Vec3f pos;
        
        Math_Vec3f_Copy(&pos, &this->bodyPartsPos[PLAYER_BODYPART_WAIST]);
        pos.y += 20.0f;
        if (WaterBox_GetSurface1(play, &play->colCtx, pos.x, pos.z, &pos.y, &waterBox)) {
            sp34 = pos.y - this->bodyPartsPos[PLAYER_BODYPART_LEFT_FOOT].y;
            if ((sp34 > -2.0f) && (sp34 < 100.0f)) {
                EffectSsGSplash_Spawn(play, &pos, NULL, NULL,
                                      (sp3C <= 10.0f) ? EFFSSGSPLASH_TYPE_0 : EFFSSGSPLASH_TYPE_1, scale);
                return true;
            }
        }
    }
    return false;
}

// Patch: Fix Deku mask handling
RECOMP_PATCH void func_808345C8(void) {
    if (INV_CONTENT(ITEM_MASK_DEKU) == ITEM_MASK_DEKU && 
        gSaveContext.save.saveInfo.playerData.health == 0) {
        gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
        gSaveContext.save.equippedMask = PLAYER_MASK_NONE;
    }
}