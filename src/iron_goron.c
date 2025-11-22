/* 
 * Iron Goron Mod - With Transformations, Without Weird B
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
extern PlayerItemAction Player_ItemToItemAction(Player* this, ItemId item);
extern PlayerMeleeWeapon Player_MeleeWeaponFromIA(PlayerItemAction itemAction);
extern PlayerExplosive Player_ExplosiveFromIA(Player* this, PlayerItemAction itemAction);
extern void func_808318C0(PlayState* play);
extern s32 func_80831814(Player* this, PlayState* play, s32 arg2);
extern void func_8083A658(PlayState* play, Player* this);
extern void Player_DestroyHookshot(Player* this);
extern void Player_DetachHeldActor(PlayState* play, Player* this);
extern void Player_InitItemActionWithAnim(PlayState* play, Player* this, PlayerItemAction itemAction);
extern void func_8082E1F0(Player* this, u16 sfxId);

// External water speed globals
extern f32 sWaterSpeedFactor;
extern f32 sInvWaterSpeedFactor;

// Structs and arrays
typedef struct {
    ItemId itemId;
} PlayerExplosiveInfo;

extern PlayerExplosiveInfo sPlayerExplosiveInfo[];
extern s8 sPlayerItemChangeTypes[][21];

// Global vars
extern u8 sPlayerUseHeldItem;
extern u8 sPlayerHeldItemButtonIsHeldDown;

#define ANIMMODE_ONCE 2
#define PLAYER_STATE1_IN_WATER 0x00000001
#define PLAYER_STATE1_SWIMMING 0x08000000
#define PLAYER_BOOTS_ZORA_UNDERWATER 3
#define PLAYER_IA_MINUS1 -1
#define PLAYER_ITEM_CHG_0 0
#define PLAYER_UNKAA5_2 2
#define PLAYER_UNKAA5_5 5

// Static variables for water speed fix
static Player* sGoronSpeedPlayer = NULL;
static PlayState* sGoronSpeedPlay = NULL;

// Patch: func_8082FD0C - allow Goron mask on C buttons in water
RECOMP_PATCH s32 func_8082FD0C(Player* this, s32 item) {
    if (item == ITEM_MASK_GORON) {
        return true;
    }
    
    if (this->stateFlags1 & (PLAYER_STATE1_IN_WATER | PLAYER_STATE1_SWIMMING)) {
        return false;
    }
    
    return true;
}

// Patch: Player_UseItem - Allow transformation masks underwater (ONLY THIS PATCH, NOT func_8083B930)
RECOMP_PATCH void Player_UseItem(PlayState* play, Player* this, ItemId item) {
    PlayerItemAction itemAction = Player_ItemToItemAction(this, item);

    if ((((this->heldItemAction == this->itemAction) &&
          (!(this->stateFlags1 & PLAYER_STATE1_400000) ||
           (Player_MeleeWeaponFromIA(itemAction) != PLAYER_MELEEWEAPON_NONE) || (itemAction == PLAYER_IA_NONE))) ||
         ((this->itemAction <= PLAYER_IA_MINUS1) &&
          ((Player_MeleeWeaponFromIA(itemAction) != PLAYER_MELEEWEAPON_NONE) || (itemAction == PLAYER_IA_NONE)))) &&
        ((itemAction == PLAYER_IA_NONE) || !(this->stateFlags1 & PLAYER_STATE1_8000000) ||
         (itemAction >= PLAYER_IA_MASK_TRANSFORMATION_MIN && itemAction <= PLAYER_IA_MASK_MAX) ||
         ((this->currentBoots >= PLAYER_BOOTS_ZORA_UNDERWATER) && (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)))) {
        s32 var_v1 = ((itemAction >= PLAYER_IA_MASK_MIN) && (itemAction <= PLAYER_IA_MASK_MAX) &&
                      ((this->transformation != PLAYER_FORM_HUMAN) || (itemAction >= PLAYER_IA_MASK_GIANT)));
        CollisionPoly* sp5C;
        s32 sp58;
        f32 sp54;
        PlayerExplosive explosiveType;

        if (var_v1 || (CHECK_FLAG_ALL(this->actor.flags, ACTOR_FLAG_TALK) && (itemAction != PLAYER_IA_NONE)) ||
            (itemAction == PLAYER_IA_OCARINA) ||
            ((itemAction > PLAYER_IA_BOTTLE_MIN) && itemAction < PLAYER_IA_MASK_MIN) ||
            ((itemAction == PLAYER_IA_PICTOGRAPH_BOX) && (this->talkActor != NULL) &&
             (this->exchangeItemAction > PLAYER_IA_NONE))) {
            if (var_v1) {
                PlayerTransformation playerForm = (itemAction < PLAYER_IA_MASK_FIERCE_DEITY)
                                                      ? PLAYER_FORM_HUMAN
                                                      : itemAction - PLAYER_IA_MASK_FIERCE_DEITY;

                if (((this->currentMask != PLAYER_MASK_GIANT) && (itemAction == PLAYER_IA_MASK_GIANT) &&
                     ((gSaveContext.magicState != MAGIC_STATE_IDLE) ||
                      (gSaveContext.save.saveInfo.playerData.magic == 0))) ||
                    (!(this->stateFlags1 & PLAYER_STATE1_8000000) &&
                     BgCheck_EntityCheckCeiling(&play->colCtx, &sp54, &this->actor.world.pos,
                                                this->ageProperties->ceilingCheckHeight, &sp5C, &sp58,
                                                &this->actor))) {
                    Audio_PlaySfx(NA_SE_SY_ERROR);
                    return;
                }
            }
            if ((itemAction == PLAYER_IA_MAGIC_BEANS) && (AMMO(ITEM_MAGIC_BEANS) == 0)) {
                Audio_PlaySfx(NA_SE_SY_ERROR);
            } else {
                this->itemAction = itemAction;
                this->unk_AA5 = PLAYER_UNKAA5_5;
            }
        } else if (((itemAction == PLAYER_IA_DEKU_STICK) && (AMMO(ITEM_DEKU_STICK) == 0)) ||
                   (((play->unk_1887D != 0) || (play->unk_1887E != 0)) &&
                    (play->actorCtx.actorLists[ACTORCAT_EXPLOSIVES].length >= 5)) ||
                   ((play->unk_1887D == 0) && (play->unk_1887E == 0) &&
                    ((explosiveType = Player_ExplosiveFromIA(this, itemAction)) > PLAYER_EXPLOSIVE_NONE) &&
                    ((AMMO(sPlayerExplosiveInfo[explosiveType].itemId) == 0) ||
                     (play->actorCtx.actorLists[ACTORCAT_EXPLOSIVES].length >= 3)))) {
            Audio_PlaySfx(NA_SE_SY_ERROR);
        } else if (itemAction == PLAYER_IA_LENS_OF_TRUTH) {
            func_808318C0(play);
        } else if (itemAction == PLAYER_IA_PICTOGRAPH_BOX) {
            if (!func_80831814(this, play, PLAYER_UNKAA5_2)) {
                Audio_PlaySfx(NA_SE_SY_ERROR);
            }
        } else if ((itemAction == PLAYER_IA_DEKU_NUT) &&
                   ((this->transformation != PLAYER_FORM_DEKU) || (this->heldItemButton != 0))) {
            if (AMMO(ITEM_DEKU_NUT) != 0) {
                func_8083A658(play, this);
            } else {
                Audio_PlaySfx(NA_SE_SY_ERROR);
            }
        } else if ((this->transformation == PLAYER_FORM_HUMAN) && (itemAction >= PLAYER_IA_MASK_MIN) &&
                   (itemAction < PLAYER_IA_MASK_GIANT)) {
            PlayerMask maskId = GET_MASK_FROM_IA(itemAction);

            this->prevMask = this->currentMask;
            if (maskId == this->currentMask) {
                this->currentMask = PLAYER_MASK_NONE;
                func_8082E1F0(this, NA_SE_PL_TAKE_OUT_SHIELD);
            } else {
                this->currentMask = maskId;
                func_8082E1F0(this, NA_SE_PL_CHANGE_ARMS);
            }
            gSaveContext.save.equippedMask = this->currentMask;
        } else if ((itemAction != this->heldItemAction) ||
                   ((this->heldActor == NULL) && (Player_ExplosiveFromIA(this, itemAction) > PLAYER_EXPLOSIVE_NONE))) {
            u8 nextAnimType;

            this->nextModelGroup = Player_ActionToModelGroup(this, itemAction);
            nextAnimType = gPlayerModelTypes[this->nextModelGroup].modelAnimType;
            var_v1 = ((this->transformation != PLAYER_FORM_GORON) || (itemAction == PLAYER_IA_POWDER_KEG));

            if (var_v1 && (this->heldItemAction >= 0) && (item != this->heldItemId) &&
                (sPlayerItemChangeTypes[gPlayerModelTypes[this->modelGroup].modelAnimType][nextAnimType] !=
                 PLAYER_ITEM_CHG_0)) {
                this->heldItemId = item;
                this->stateFlags3 |= PLAYER_STATE3_START_CHANGING_HELD_ITEM;
            } else {
                Player_DestroyHookshot(this);
                Player_DetachHeldActor(play, this);
                Player_InitItemActionWithAnim(play, this, itemAction);
                if (!var_v1) {
                    sPlayerUseHeldItem = true;
                    sPlayerHeldItemButtonIsHeldDown = true;
                }
            }
        } else {
            sPlayerUseHeldItem = true;
            sPlayerHeldItemButtonIsHeldDown = true;
        }
    }
}

// Hook: Player_UpdateCommon ENTRY - Like old code but with water speed fix
RECOMP_HOOK("Player_UpdateCommon") void Player_UpdateCommon_Entry(Player* this, PlayState* play, Input* input) {
    sGoronSpeedPlayer = this;
    sGoronSpeedPlay = play;
    
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    
    // Water speed fix
    if (this->transformation == PLAYER_FORM_GORON && this->actor.depthInWater > 0.0f) {
        sWaterSpeedFactor = 1.0f;
        sInvWaterSpeedFactor = 1.0f;
    }
    
    // Enable C buttons for ALL forms underwater
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
    
    // Goron underwater - just like old code
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

// Hook: Player_UpdateCommon RETURN - Water speed fix
RECOMP_HOOK_RETURN("Player_UpdateCommon") void Player_UpdateCommon_Return(void) {
    if (sGoronSpeedPlayer != NULL && sGoronSpeedPlay != NULL) {
        // Water speed fix
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