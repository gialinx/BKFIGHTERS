#include "stdafx.h"
#include "CharacterAnimation.h"
#include "CharacterMovement.h"
#include "CharacterMovement.h" // ensure PlayerInputConfig is visible
#include "CharacterCombat.h"
#include "AnimationManager.h"
#include "SceneManager.h"
#include "../GameManager/ResourceManager.h"
#include "../GameManager/SoundManager.h"
#include "Object.h"
#include <SDL.h>
#include <iostream>
#include <cmath>

static inline float ClampFloat(float v, float mn, float mx) {
    return v < mn ? mn : (v > mx ? mx : v);
}

CharacterAnimation::CharacterAnimation() 
    : m_lastAnimation(-1), m_objectId(1000),
      m_climbHoldTimer(0.0f), m_lastClimbDir(0),
      m_prevClimbUpPressed(false), m_prevClimbDownPressed(false),
      m_downPressStartTime(-1.0f) {
}

CharacterAnimation::~CharacterAnimation() {
}

void CharacterAnimation::Initialize(std::shared_ptr<AnimationManager> animManager, int objectId) {
    m_animManager = animManager;
    m_objectId = objectId;
    
    m_characterObject = std::make_unique<Object>(objectId);
    
    Object* originalObj = SceneManager::GetInstance()->GetObject(objectId);
    if (originalObj) {
        m_characterObject->SetModel(originalObj->GetModelId());
        const std::vector<int>& textureIds = originalObj->GetTextureIds();
        if (!textureIds.empty()) {
            m_characterObject->SetTexture(textureIds[0], 0);
        }
        m_characterObject->SetShader(originalObj->GetShaderId());
        m_characterObject->SetScale(originalObj->GetScale());
    }
    
    if (m_animManager) {
        m_animManager->Play(0, true);
    }

    int headTexId = (m_objectId == 1000) ? 8 : 9;
    const TextureData* headTex = ResourceManager::GetInstance()->GetTextureData(headTexId);
    if (headTex && headTex->spriteWidth > 0 && headTex->spriteHeight > 0) {
        m_topAnimManager = std::make_shared<AnimationManager>();
        std::vector<AnimationData> topAnims;
        topAnims.reserve(headTex->animations.size());
        for (const auto& a : headTex->animations) {
            topAnims.push_back({a.startFrame, a.numFrames, a.duration, 0.0f});
        }
        m_topAnimManager->Initialize(headTex->spriteWidth, headTex->spriteHeight, topAnims);
        m_topAnimManager->Play(0, true);

        m_topObject = std::make_unique<Object>(objectId + 10000); // unique id for overlay
        if (originalObj) {
            m_topObject->SetModel(originalObj->GetModelId());
            m_topObject->SetShader(originalObj->GetShaderId());
            m_topObject->SetScale(originalObj->GetScale());
        }
        m_topObject->SetTexture(headTexId, 0);
    }
}

void CharacterAnimation::Update(float deltaTime, CharacterMovement* movement, CharacterCombat* combat) {
    if (m_animManager) {
        m_animManager->Update(deltaTime);
        UpdateAnimationState(movement, combat);
    }
    if (movement) {
        m_lastFacingLeft = movement->IsFacingLeft();
    }
    if (m_topAnimManager) {
        m_topAnimManager->Update(deltaTime);
    }
    if (m_orcFireActive && m_orcFireAnim) {
        m_orcFireAnim->Update(deltaTime);
        if (!m_orcFireAnim->IsPlaying()) {
            m_orcFireActive = false;
            if (m_orcFireObject) {
                m_orcFireObject->SetVisible(false);
            }
        }
    }
    if (m_orcAppearActive && m_orcAppearAnim) {
        m_orcAppearAnim->Update(deltaTime);
        if (!m_orcAppearAnim->IsPlaying()) {
            m_orcAppearActive = false;
            if (m_orcAppearObject) {
                m_orcAppearObject->SetVisible(false);
            }
        }
    }
    if (m_werewolfAppearActive && m_werewolfAppearAnim) {
        m_werewolfAppearAnim->Update(deltaTime);
        if (!m_werewolfAppearAnim->IsPlaying()) {
            m_werewolfAppearActive = false;
            if (m_werewolfAppearObject) {
                m_werewolfAppearObject->SetVisible(false);
            }
        }
    }
    if (m_batAppearActive && m_batAppearAnim) {
        m_batAppearAnim->Update(deltaTime);
        if (!m_batAppearAnim->IsPlaying()) {
            m_batAppearActive = false;
            if (m_batAppearObject) {
                m_batAppearObject->SetVisible(false);
            }
        }
    }
    if (m_kitsuneAppearActive && m_kitsuneAppearAnim) {
        m_kitsuneAppearAnim->Update(deltaTime);
        if (!m_kitsuneAppearAnim->IsPlaying()) {
            m_kitsuneAppearActive = false;
            if (m_kitsuneAppearObject) {
                m_kitsuneAppearObject->SetVisible(false);
            }
        }
    }

    if (m_batWindActive && m_batWindAnim) {
        m_batWindAnim->Update(deltaTime);
        if (m_batWindObject) {
            const Vector3& p = m_batWindObject->GetPosition();
            float dx = m_batWindSpeed * m_batWindFaceSign * deltaTime;
            m_batWindObject->SetPosition(p.x + dx, p.y, p.z);
        }
        if (!m_batWindAnim->IsPlaying()) {
            m_batWindActive = false;
            if (m_batWindObject) {
                m_batWindObject->SetVisible(false);
            }
        }
    }

    if (m_batSlashCooldownTimer > 0.0f) {
        m_batSlashCooldownTimer -= deltaTime;
        if (m_batSlashCooldownTimer < 0.0f) m_batSlashCooldownTimer = 0.0f;
    }
    
    if (m_isBatDemon) {
        static float wingFlapTimer = 0.0f;
        wingFlapTimer += deltaTime;
        if (wingFlapTimer >= 2.0f) {
            SoundManager::Instance().PlaySFXByID(32, 1);
            wingFlapTimer = 0.0f;
        }
    } else {
        static float wingFlapTimer = 0.0f;
        wingFlapTimer = 0.0f;
    }

    if (m_kitsuneEnergyOrbCooldownTimer > 0.0f) {
        m_kitsuneEnergyOrbCooldownTimer -= deltaTime;
        if (m_kitsuneEnergyOrbCooldownTimer < 0.0f) m_kitsuneEnergyOrbCooldownTimer = 0.0f;
    }

    if (m_orcActionCooldownTimer > 0.0f) {
        m_orcActionCooldownTimer -= deltaTime;
        if (m_orcActionCooldownTimer < 0.0f) m_orcActionCooldownTimer = 0.0f;
    }

    if (m_isWerewolf && movement) {
        if (movement->IsJumping()) {
            m_werewolfAirTimer += deltaTime;
        } else {
            m_werewolfAirTimer = 0.0f;
        }
        if (m_werewolfPounceActive) {
            float dir = movement->IsFacingLeft() ? -1.0f : 1.0f;
            Vector3 pos = movement->GetPosition();
            movement->SetPosition(pos.x + dir * m_werewolfPounceSpeed * deltaTime, pos.y);
            if (combat && m_werewolfPounceHitWindowTimer > 0.0f) {
                float w = 0.15f;
                float h = 0.2f;
                float ox = (dir < 0.0f) ? -0.12f : 0.12f;
                float oy = -0.1f;
                combat->ShowHitbox(w, h, ox, oy);
            }
        }
        if (m_werewolfComboCooldownTimer > 0.0f) {
            m_werewolfComboCooldownTimer -= deltaTime;
            if (m_werewolfComboCooldownTimer < 0.0f) m_werewolfComboCooldownTimer = 0.0f;
        }
        if (m_werewolfPounceCooldownTimer > 0.0f) {
            m_werewolfPounceCooldownTimer -= deltaTime;
            if (m_werewolfPounceCooldownTimer < 0.0f) m_werewolfPounceCooldownTimer = 0.0f;
        }
        if (m_werewolfComboHitWindowTimer > 0.0f) {
            m_werewolfComboHitWindowTimer -= deltaTime;
            if (m_werewolfComboHitWindowTimer < 0.0f) m_werewolfComboHitWindowTimer = 0.0f;
        }
    }

    // Handle turn timing
    if (m_gunMode && movement && m_isTurning) {
        m_turnTimer += deltaTime;
        if (m_turnTimer >= TURN_DURATION) {
            m_isTurning = false;
            m_prevFacingLeft = m_turnTargetLeft;
            movement->SetFacingLeft(m_turnTargetLeft);
            PlayTopAnimation(1, true);
        }
    }
    
    if (m_recoilActive) {
        m_recoilTimer += deltaTime;
        if (m_recoilTimer >= RECOIL_DURATION) {
            m_recoilActive = false;
            if (m_gunTopAnimReload >= 0) {
                m_reloadActive = true;
                m_reloadTimer = 0.0f;
            }
            m_recoilOffsetX = 0.0f;
            m_recoilOffsetY = 0.0f;
        } else {
            float progress = m_recoilTimer / RECOIL_DURATION;
            float easingFactor = 1.0f - powf(1.0f - progress, 3.0f);
            float currentStrength = (1.0f - easingFactor);
            
            float aimRad = m_lastShotAimDeg * 3.14159265f / 180.0f;
            float angleWorld = m_recoilFaceSign * aimRad;
            float muzzleX = cosf(angleWorld);
            float muzzleY = sinf(angleWorld);
            m_recoilOffsetX = muzzleX * RECOIL_STRENGTH * currentStrength;
            m_recoilOffsetY = muzzleY * RECOIL_STRENGTH * currentStrength;
        }
    }

    if (m_reloadActive) {
        m_reloadTimer += deltaTime;
        if (m_reloadTimer >= RELOAD_DURATION) {
            m_reloadActive = false;
            m_reloadTimer = 0.0f;
        }
    }
}

void CharacterAnimation::StartHardLanding(CharacterMovement* movement) {
    if (m_hardLandingActive) return;
    unsigned int nowMs = SDL_GetTicks();
    if (m_blockHardLandingUntilMs != 0 && nowMs < m_blockHardLandingUntilMs) {
        return;
    }
    m_hardLandingActive = true;
    m_hardLandingPhase = 0;
    if (movement) {
        m_restoreInputAfterHardLanding = !movement->IsInputLocked();
        movement->SetInputLocked(true);
    } else {
        m_restoreInputAfterHardLanding = false;
    }
    m_gunMode = false;
    m_grenadeMode = false;
    m_isTurning = false;
    m_gunEntering = false;
    m_reloadActive = false;
    m_recoilActive = false;
    PlayAnimation(15, false);
}

void CharacterAnimation::Draw(Camera* camera, CharacterMovement* movement) {
    if (m_characterObject && m_animManager && m_characterObject->GetModelId() >= 0 && m_characterObject->GetModelPtr()) {
        float u0, v0, u1, v1;
        m_animManager->GetUV(u0, v0, u1, v1);
        
        if (movement && movement->IsFacingLeft()) {
            float temp = u0;
            u0 = u1;
            u1 = temp;
        }
        
        m_characterObject->SetCustomUV(u0, v0, u1, v1);
        Vector3 position = movement ? movement->GetPosition() : Vector3(0, 0, 0);
        if (m_isWerewolf) {
            m_characterObject->SetPosition(position.x, position.y + m_werewolfBodyOffsetY, position.z);
        } else {
            m_characterObject->SetPosition(position);
        }
        
        if (camera) {
            m_characterObject->Draw(camera->GetViewMatrix(), camera->GetProjectionMatrix());
        }
    }

    if (!m_isBatDemon && !m_isKitsune && !m_isOrc && (m_gunMode || m_recoilActive) && m_topObject && m_topAnimManager && m_topObject->GetModelId() >= 0 && m_topObject->GetModelPtr()) {
        float u0, v0, u1, v1;
        m_topAnimManager->GetUV(u0, v0, u1, v1);
        
        bool shouldFlipUV = m_gunMode ? 
                           (movement && movement->IsFacingLeft()) : 
                           (m_recoilFaceSign < 0.0f);
        if (shouldFlipUV) {
            std::swap(u0, u1);
        }
        m_topObject->SetCustomUV(u0, v0, u1, v1);
        Vector3 position = movement ? movement->GetPosition() : Vector3(0, 0, 0);
        
        bool isLeftFacing = m_gunMode ? 
                           (movement && movement->IsFacingLeft()) : 
                           (m_recoilFaceSign < 0.0f);
        float offsetX = isLeftFacing ? -m_topOffsetX : m_topOffsetX;
        
        float finalOffsetX = offsetX + m_recoilOffsetX;
        float finalOffsetY = m_topOffsetY + m_recoilOffsetY;
        float bodyY = m_isWerewolf ? (position.y + m_werewolfBodyOffsetY) : position.y;
        m_topObject->SetPosition(position.x + finalOffsetX, bodyY + finalOffsetY, position.z);
        
        float faceSign = m_gunMode ? 
                        ((movement && movement->IsFacingLeft()) ? -1.0f : 1.0f) : 
                        m_recoilFaceSign;
        float aimAngle = m_gunMode ? m_aimAngleDeg : m_lastShotAimDeg;
        m_topObject->SetRotation(0.0f, 0.0f, faceSign * aimAngle * 3.14159265f / 180.0f);
        if (camera) {
            m_topObject->Draw(camera->GetViewMatrix(), camera->GetProjectionMatrix());
        }
    }

    if (!m_isBatDemon && !m_isKitsune && !m_isOrc && m_grenadeMode && m_topObject && m_topAnimManager && m_topObject->GetModelId() >= 0 && m_topObject->GetModelPtr()) {
        float u0, v0, u1, v1;
        m_topAnimManager->GetUV(u0, v0, u1, v1);
        if (movement && movement->IsFacingLeft()) {
            std::swap(u0, u1);
        }
        m_topObject->SetCustomUV(u0, v0, u1, v1);
        Vector3 position = movement ? movement->GetPosition() : Vector3(0, 0, 0);
        float offsetX = (movement && movement->IsFacingLeft()) ? -m_topOffsetX : m_topOffsetX;
        float bodyY2 = m_isWerewolf ? (position.y + m_werewolfBodyOffsetY) : position.y;
        m_topObject->SetPosition(position.x + offsetX, bodyY2 + m_topOffsetY, position.z);
        float faceSign = (movement && movement->IsFacingLeft()) ? -1.0f : 1.0f;
        m_topObject->SetRotation(0.0f, 0.0f, faceSign * m_aimAngleDeg * 3.14159265f / 180.0f);
        if (camera) {
            m_topObject->Draw(camera->GetViewMatrix(), camera->GetProjectionMatrix());
        }
    }

    if (m_orcFireActive && m_orcFireObject && m_orcFireObject->GetModelId() >= 0 && m_orcFireObject->GetModelPtr()) {
        float u0, v0, u1, v1;
        m_orcFireAnim->GetUV(u0, v0, u1, v1);
        m_orcFireObject->SetCustomUV(u0, v0, u1, v1);
        if (camera) {
            m_orcFireObject->Draw(camera->GetViewMatrix(), camera->GetProjectionMatrix());
        }
    }
    if (m_orcAppearActive && m_orcAppearObject && m_orcAppearObject->GetModelId() >= 0 && m_orcAppearObject->GetModelPtr()) {
        float u0, v0, u1, v1;
        m_orcAppearAnim->GetUV(u0, v0, u1, v1);
        m_orcAppearObject->SetCustomUV(u0, v0, u1, v1);
        if (camera) {
            m_orcAppearObject->Draw(camera->GetViewMatrix(), camera->GetProjectionMatrix());
        }
    }
    if (m_werewolfAppearActive && m_werewolfAppearObject && m_werewolfAppearObject->GetModelId() >= 0 && m_werewolfAppearObject->GetModelPtr()) {
        float u0, v0, u1, v1;
        m_werewolfAppearAnim->GetUV(u0, v0, u1, v1);
        m_werewolfAppearObject->SetCustomUV(u0, v0, u1, v1);
        if (camera) {
            m_werewolfAppearObject->Draw(camera->GetViewMatrix(), camera->GetProjectionMatrix());
        }
    }
    if (m_batAppearActive && m_batAppearObject && m_batAppearObject->GetModelId() >= 0 && m_batAppearObject->GetModelPtr()) {
        float u0, v0, u1, v1;
        m_batAppearAnim->GetUV(u0, v0, u1, v1);
        m_batAppearObject->SetCustomUV(u0, v0, u1, v1);
        if (camera) {
            m_batAppearObject->Draw(camera->GetViewMatrix(), camera->GetProjectionMatrix());
        }
    }
    if (m_kitsuneAppearActive && m_kitsuneAppearObject && m_kitsuneAppearObject->GetModelId() >= 0 && m_kitsuneAppearObject->GetModelPtr()) {
        float u0, v0, u1, v1;
        m_kitsuneAppearAnim->GetUV(u0, v0, u1, v1);
        m_kitsuneAppearObject->SetCustomUV(u0, v0, u1, v1);
        if (camera) {
            m_kitsuneAppearObject->Draw(camera->GetViewMatrix(), camera->GetProjectionMatrix());
        }
    }

    if (m_batWindActive && m_batWindObject && m_batWindObject->GetModelId() >= 0 && m_batWindObject->GetModelPtr()) {
        float u0, v0, u1, v1;
        m_batWindAnim->GetUV(u0, v0, u1, v1);
        if (m_batWindFaceSign < 0.0f) {
            std::swap(u0, u1);
        }
        m_batWindObject->SetCustomUV(u0, v0, u1, v1);
        if (camera) {
            m_batWindObject->Draw(camera->GetViewMatrix(), camera->GetProjectionMatrix());
        }
    }
}

Vector3 CharacterAnimation::GetTopWorldPosition(CharacterMovement* movement) const {
    Vector3 base = movement ? movement->GetPosition() : Vector3(0, 0, 0);
    float offsetX = (movement && movement->IsFacingLeft()) ? -m_topOffsetX : m_topOffsetX;
    return Vector3(base.x + offsetX, base.y + m_topOffsetY, base.z);
}

void CharacterAnimation::UpdateAnimationState(CharacterMovement* movement, CharacterCombat* combat) {
    if (!movement) return;

    // BatDemon mode
    if (m_isBatDemon) {
        if (m_characterObject) {
            const std::vector<int>& texIds = m_characterObject->GetTextureIds();
            int currentTex = texIds.empty() ? -1 : texIds[0];
            if (currentTex != 61) {
                m_characterObject->SetTexture(61, 0);
                if (auto texData = ResourceManager::GetInstance()->GetTextureData(61)) {
                    if (!m_animManager) {
                        m_animManager = std::make_shared<AnimationManager>();
                    }
                    std::vector<AnimationData> anims;
                    anims.reserve(texData->animations.size());
                    for (const auto& a : texData->animations) {
                        anims.push_back({a.startFrame, a.numFrames, a.duration, 0.0f});
                    }
                    m_animManager->Initialize(texData->spriteWidth, texData->spriteHeight, anims);
                    m_lastAnimation = -1;
                }
            }
        }
        if (m_animManager) {
            if (m_batSlashActive) {
                int cur = GetCurrentAnimation();
                if (cur != 1) {
                    m_animManager->Play(1, false);
                    m_lastAnimation = 1;
                }
                if (!m_animManager->IsPlaying()) {
                    m_batSlashActive = false;
                    m_batSlashCooldownTimer = BAT_SLASH_COOLDOWN;
                }
            } else {
                int cur = GetCurrentAnimation();
                if (cur != 0) {
                    m_animManager->Play(0, true);
                    m_lastAnimation = 0;
                } else {
                    m_animManager->Resume();
                }
            }
        }
        return;
    }

    // Werewolf mode
    if (m_isWerewolf) {
        if (m_characterObject) {
            const std::vector<int>& texIds = m_characterObject->GetTextureIds();
            int currentTex = texIds.empty() ? -1 : texIds[0];
            if (currentTex != 60) {
                m_characterObject->SetTexture(60, 0);
                if (auto texData = ResourceManager::GetInstance()->GetTextureData(60)) {
                    if (!m_animManager) {
                        m_animManager = std::make_shared<AnimationManager>();
                    }
                    std::vector<AnimationData> anims;
                    anims.reserve(texData->animations.size());
                    for (const auto& a : texData->animations) {
                        anims.push_back({a.startFrame, a.numFrames, a.duration, 0.0f});
                    }
                    m_animManager->Initialize(texData->spriteWidth, texData->spriteHeight, anims);
                    m_lastAnimation = -1;
                }
            }
        }
        // Drive werewolf anims by movement state
        if (movement && m_animManager) {
            bool physJumping = movement->IsJumping();
            if (!physJumping) { m_werewolfAirTimer = 0.0f; }
            bool considerJumping = (m_werewolfAirTimer >= WEREWOLF_AIR_DEBOUNCE);

            // Pounce overrides combo and movement
            if (m_werewolfPounceActive) {
                int cur = GetCurrentAnimation();
                if (cur != 3) {
                    m_animManager->Play(3, false);
                    m_lastAnimation = 3;
                }
                if (!m_animManager->IsPlaying()) {
                    m_werewolfPounceActive = false;
                    m_werewolfPounceCooldownTimer = m_werewolfPounceCooldown;
                }
                return;
            }
            if (m_werewolfComboActive) {
                int cur = GetCurrentAnimation();
                if (cur != 1) {
                    m_animManager->Play(1, false);
                    m_lastAnimation = 1;
                }
                if (!m_animManager->IsPlaying()) {
                    m_werewolfComboActive = false;
                    m_werewolfComboCooldownTimer = m_werewolfComboCooldown;
                }
                return;
            }

            int desired = 0;
            bool loop = true;
            if (considerJumping) {
                desired = 5; loop = false;
            } else {
                CharState st = movement->GetState();
                bool isMoving = (st == CharState::MoveLeft || st == CharState::MoveRight);
                bool isRunning = movement->IsRunningLeft() || movement->IsRunningRight();
                if (isMoving && isRunning) { desired = 2; loop = true; }
                else if (isMoving) { desired = 4; loop = true; }
                else { desired = 0; loop = true; }
            }
            int cur = GetCurrentAnimation();
            if (cur != desired || (desired == 5 && !m_animManager->IsPlaying())) {
                m_animManager->Play(desired, loop);
                m_lastAnimation = desired;
            } else {
                m_animManager->Resume();
            }
        }
        return;
    }

    // Kitsune mode
    if (m_isKitsune) {
        if (m_characterObject) {
            const std::vector<int>& texIds = m_characterObject->GetTextureIds();
            int currentTex = texIds.empty() ? -1 : texIds[0];
            if (currentTex != 62) {
                m_characterObject->SetTexture(62, 0);
                if (auto texData = ResourceManager::GetInstance()->GetTextureData(62)) {
                    if (!m_animManager) {
                        m_animManager = std::make_shared<AnimationManager>();
                    }
                    std::vector<AnimationData> anims;
                    anims.reserve(texData->animations.size());
                    for (const auto& a : texData->animations) {
                        anims.push_back({a.startFrame, a.numFrames, a.duration, 0.0f});
                    }
                    m_animManager->Initialize(texData->spriteWidth, texData->spriteHeight, anims);
                    m_lastAnimation = -1;
                }
            }
        }
        if (m_animManager) {
            if (m_kitsuneEnergyOrbActive) {
                int cur = GetCurrentAnimation();
                if (cur != 3) {
                    m_animManager->Play(3, false);
                    m_lastAnimation = 3;
                    m_kitsuneEnergyOrbAnimationComplete = false;
                }
                if (!m_animManager->IsPlaying()) {
                    m_kitsuneEnergyOrbActive = false;
                    m_kitsuneEnergyOrbCooldownTimer = KITSUNE_ENERGY_ORB_COOLDOWN;
                    m_kitsuneEnergyOrbAnimationComplete = true;
                }
            } else {
                int desired = 0;
                if (movement) {
                    CharState st = movement->GetState();
                    bool isMoving = (st == CharState::MoveLeft || st == CharState::MoveRight);
                    bool isRunning = movement->IsRunningLeft() || movement->IsRunningRight();
                    if (isMoving) { desired = isRunning ? 2 : 1; }
                }
                int cur = GetCurrentAnimation();
                if (cur != desired) {
                    m_animManager->Play(desired, true);
                    m_lastAnimation = desired;
                } else {
                    m_animManager->Resume();
                }
            }
        }
        return;
    }

    // Orc mode
    if (m_isOrc) {
        if (m_characterObject) {
            const std::vector<int>& texIds = m_characterObject->GetTextureIds();
            int currentTex = texIds.empty() ? -1 : texIds[0];
            if (currentTex != 63) {
                m_characterObject->SetTexture(63, 0);
                if (auto texData = ResourceManager::GetInstance()->GetTextureData(63)) {
                    if (!m_animManager) {
                        m_animManager = std::make_shared<AnimationManager>();
                    }
                    std::vector<AnimationData> anims;
                    anims.reserve(texData->animations.size());
                    for (const auto& a : texData->animations) {
                        anims.push_back({a.startFrame, a.numFrames, a.duration, 0.0f});
                    }
                    m_animManager->Initialize(texData->spriteWidth, texData->spriteHeight, anims);
                    m_lastAnimation = -1;
                }
            }
        }
        if (m_animManager) {
            if (m_orcMeteorStrikeActive) {
                int cur = GetCurrentAnimation();
                if (cur != 2) {
                    m_animManager->Play(2, false);
                    m_lastAnimation = 2;
                }
                if (!m_animManager->IsPlaying()) {
                    m_orcMeteorStrikeActive = false;
                    if (movement) {
                        movement->SetInputLocked(false);
                    }
                } else {
                    if (movement) {
                        movement->SetInputLocked(true);
                    }
                }
                return;
            } else if (m_orcFlameBurstActive) {
                int cur = GetCurrentAnimation();
                if (cur != 3) {
                    m_animManager->Play(3, false);
                    m_lastAnimation = 3;
                }
                if (!m_animManager->IsPlaying()) {
                    m_orcFlameBurstActive = false;
                    if (movement) {
                        movement->SetInputLocked(false);
                    }
                    m_animManager->Play(0, true);
                } else {
                    if (movement) {
                        movement->SetInputLocked(true);
                    }
                }
                return;
            }
            
            int cur = GetCurrentAnimation();
            int desired = 0;
            if (movement && !m_orcMeteorStrikeActive && !m_orcFlameBurstActive) {
                CharState st = movement->GetState();
                bool isMoving = (st == CharState::MoveLeft || st == CharState::MoveRight);
                if (isMoving) { desired = 1; } // Walk
            }
            if (cur != desired && !m_orcMeteorStrikeActive && !m_orcFlameBurstActive) {
                m_animManager->Play(desired, true);
                m_lastAnimation = desired;
            } else if (!m_orcMeteorStrikeActive && !m_orcFlameBurstActive) {
                m_animManager->Resume();
            }
        }
        return;
    }

    if (!m_hardLandingActive && movement->ConsumeHardLandingRequested()) {
        StartHardLanding(movement);
        return;
    }

    if (m_hardLandingActive) {
        if (movement && !movement->IsInputLocked()) {
            movement->SetInputLocked(true);
        }
        int cur = GetCurrentAnimation();
        if (m_hardLandingPhase == 0) {
            if (cur != 15) {
                PlayAnimation(15, false);
            } else if (!m_animManager->IsPlaying()) {
                m_hardLandingPhase = 1;
                PlayAnimation(14, false);
            }
        } else {
            if (cur != 14) {
                PlayAnimation(14, false);
            } else if (!m_animManager->IsPlaying()) {
                m_hardLandingActive = false;
                m_hardLandingPhase = 0;
                if (m_restoreInputAfterHardLanding) {
                    movement->SetInputLocked(false);
                }
                PlayAnimation(0, true);
                unsigned int doneMs = SDL_GetTicks();
                m_blockHardLandingUntilMs = doneMs + 120; // ~0.12s debounce
            }
        }
        return;
    }

    if (!combat) {
        return;
    }

    if (m_grenadeMode) {
        if (m_isKitsune) { m_grenadeMode = false; return; }
        if (m_isOrc) { m_grenadeMode = false; return; }
        PlayAnimation(31, true);
        PlayTopAnimation(7, true);
        return;
    }

    if (m_gunMode) {
        if (m_isKitsune) { m_gunMode = false; return; }
        if (m_isWerewolf) {
            return;
        }
        if (m_isBatDemon) {
            m_gunMode = false;
            m_isTurning = false;
            m_gunEntering = false;
            // fall back to BatDemon render path
            return;
        }
        if (m_isOrc) { m_gunMode = false; return; }
        return;
    }
    
    if (movement->IsSitting()) {
        if (combat->IsKicking() || combat->IsInCombo() || combat->IsInAxeCombo()) {
            combat->CancelAllCombos();
        }
        return;
    }
    
    if (movement->IsDying()) {
        return;
    }
    
    if (combat->IsInCombo() && !m_animManager->IsPlaying()) {
        if (combat->IsComboCompleted()) {
            combat->CancelAllCombos();
            m_animManager->Play(0, true);
        } else if (combat->GetComboTimer() <= 0.0f) {
            combat->CancelAllCombos();
            m_animManager->Play(0, true);
        } else {
            m_animManager->Play(0, true);
        }
    }
    
    if (combat->IsInAxeCombo() && !m_animManager->IsPlaying()) {
        if (combat->IsAxeComboCompleted()) {
            combat->CancelAllCombos();
            m_animManager->Play(0, true);
        } else if (combat->GetAxeComboTimer() <= 0.0f) {
            combat->CancelAllCombos();
            m_animManager->Play(0, true);
        } else {
            m_animManager->Play(0, true);
        }
    }
    
    if (combat->IsKicking()) {
        int cur = m_animManager->GetCurrentAnimation();
        bool isKickAnim = (cur == 17 || cur == 19);
        if (!m_animManager->IsPlaying() || !movement->IsJumping() || !isKickAnim) {
            combat->CancelAllCombos();
            m_animManager->Play(0, true);
        }
    }
    
    if (!m_animManager->IsPlaying() && 
        !combat->IsInCombo() && 
        !combat->IsInAxeCombo() && 
        !combat->IsKicking() && 
        !movement->IsJumping() && 
        !movement->IsSitting() && 
        !combat->IsHit() &&
        !movement->IsDying()) {
        m_animManager->Play(0, true);
    }
}

void CharacterAnimation::HandleMovementAnimations(const bool* keyStates, CharacterMovement* movement, CharacterCombat* combat) {
    if (!keyStates || !movement || !combat) {
        return;
    }
    if (m_hardLandingActive) {
        return;
    }
    if (m_isKitsune) {
        return;
    }
    
    if (m_isOrc && (m_orcMeteorStrikeActive || m_orcFlameBurstActive)) {
        return;
    }
    
    const PlayerInputConfig& inputConfig = movement->GetInputConfig();
    bool isShiftPressed = keyStates[16];

    if (m_isBatDemon) { return; }

    if (m_isWerewolf) { return; }

    // Grenade mode: allow aiming like gun mode
    if (m_grenadeMode) {
        if (m_isWerewolf) {
            m_grenadeMode = false;
        } else {
        if (m_isBatDemon) {
            m_grenadeMode = false;
            return;
        }
        // Allow turning; when turn happens, reset aim like gun mode
        bool currentLeft = movement->IsFacingLeft();
        bool wantLeft = keyStates[inputConfig.moveLeftKey];
        bool wantRight = keyStates[inputConfig.moveRightKey];
        bool turned = false;
        if (wantLeft && !currentLeft) {
            movement->SetFacingLeft(true);
            turned = true;
        } else if (wantRight && currentLeft) {
            movement->SetFacingLeft(false);
            turned = true;
        }
        if (turned) {
            m_aimAngleDeg = 0.0f;
            m_aimHoldTimerUp = m_aimHoldTimerDown = 0.0f;
            m_aimSincePressUp = m_aimSincePressDown = 0.0f;
            m_prevAimUp = m_prevAimDown = false;
        }

        HandleGunAim(keyStates, inputConfig);
        PlayAnimation(31, true);
        PlayTopAnimation(7, true);
        return;
        }
    }

    if (m_gunMode) {
        if (m_syncFacingOnEnter) {
            m_prevFacingLeft = movement->IsFacingLeft();
            m_syncFacingOnEnter = false;
        }
        bool facingLeft = m_prevFacingLeft;
        const PlayerInputConfig& ic = movement->GetInputConfig();
        bool wantLeft = keyStates[ic.moveLeftKey];
        bool wantRight = keyStates[ic.moveRightKey];
        if (!m_isTurning) {
            if (wantLeft && !facingLeft) {
                m_turnTargetLeft = true;
                m_isTurning = true;
                m_turnTimer = 0.0f;
                m_aimAngleDeg = 0.0f;
                m_aimHoldTimerUp = m_aimHoldTimerDown = 0.0f;
                m_aimSincePressUp = m_aimSincePressDown = 0.0f;
                m_prevAimUp = m_prevAimDown = false;
                PlayTopAnimation(0, false);
            } else if (wantRight && facingLeft) {
                m_turnTargetLeft = false;
                m_isTurning = true;
                m_turnTimer = 0.0f;
                m_aimAngleDeg = 0.0f;
                m_aimHoldTimerUp = m_aimHoldTimerDown = 0.0f;
                m_aimSincePressUp = m_aimSincePressDown = 0.0f;
                m_prevAimUp = m_prevAimDown = false;
                PlayTopAnimation(0, false);
            }
        }

        if (m_isTurning || m_gunEntering) {
            if (m_gunEntering) {
                PlayTopAnimation(m_gunTopAnimReverse, false);
            }
            PlayAnimation(29, true);
            if (m_gunEntering) {
                unsigned int nowMs = SDL_GetTicks();
                float elapsed = (nowMs - m_gunEnterStartMs) / 1000.0f;
                if (elapsed >= GUN_ENTER_DURATION) {
                    m_gunEntering = false;
                }
            }
            return;
        }

        HandleGunAim(keyStates, movement->GetInputConfig());

        PlayAnimation(29, true);
        if (m_reloadActive && m_gunTopAnimReload >= 0) {
            PlayTopAnimation(m_gunTopAnimReload, false);
        } else {
            PlayTopAnimation(m_gunTopAnimHold, true);
        }
        return;
    }
    
    if (movement->IsDying()) {
        PlayAnimation(15, false);
        return;
    }
    
    if (!combat->IsInCombo() && !combat->IsInAxeCombo() && !combat->IsKicking() && !combat->IsHit()) {
        if (movement->IsOnLadder()) {
            if (GetCurrentAnimation() != 6) {
                PlayAnimation(6, true);
            }

            const PlayerInputConfig& input = movement->GetInputConfig();
            const bool upHeld = keyStates[input.jumpKey];
            const bool downHeld = keyStates[input.sitKey];
            const bool upPressed = upHeld;
            const bool downPressed = downHeld;
            const bool upJustPressed = upPressed && !m_prevClimbUpPressed;
            const bool downJustPressed = downPressed && !m_prevClimbDownPressed;

            if (upJustPressed && !upHeld) {
                const AnimationData* anim = m_animManager->GetAnimation(6);
                if (anim) {
                    int frame = m_animManager->GetCurrentFrame();
                    frame = (frame + 1) % anim->numFrames;
                    m_animManager->SetCurrentFrame(frame);
                }
            }
            if (downJustPressed) {
                const AnimationData* anim = m_animManager->GetAnimation(6);
                if (anim) {
                    int frame = m_animManager->GetCurrentFrame();
                    frame = (frame - 1);
                    if (frame < 0) frame = anim->numFrames - 1;
                    m_animManager->SetCurrentFrame(frame);
                }
            }

            float now = SDL_GetTicks() / 1000.0f;
            if (downJustPressed) {
                m_downPressStartTime = now;
            }
            bool isDownHeldLong = false;
            if (downHeld && m_downPressStartTime >= 0.0f) {
                isDownHeldLong = (now - m_downPressStartTime) > CLIMB_DOWN_HOLD_THRESHOLD;
            }
            if (!downHeld) {
                m_downPressStartTime = -1.0f;
            }

            bool leftHeld = keyStates[input.moveLeftKey];
            bool rightHeld = keyStates[input.moveRightKey];
            if (upHeld || leftHeld || rightHeld) {
                m_animManager->SetPlaying(true);
                m_lastClimbDir = 1;
            } else if (isDownHeldLong) {
                const AnimationData* anim = m_animManager->GetAnimation(6);
                if (anim) {
                    m_animManager->Pause();
                    m_animManager->SetCurrentFrame(0);
                }
                m_lastClimbDir = -1;
            } else if (!upHeld && !downHeld) {
                m_animManager->Pause();
                m_lastClimbDir = 0;
            }

            m_prevClimbUpPressed = upPressed;
            m_prevClimbDownPressed = downPressed;
            return;
        }
        
        if (movement->IsJumping()) {
            if (combat->IsKicking() && GetCurrentAnimation() == 17) {
            } else {
                PlayAnimation(16, false);
            }
        } else if (movement->IsRolling()) {
            PlayAnimation(4, true);
        } else if (keyStates[inputConfig.moveRightKey]) {
            if (isShiftPressed) {
                PlayAnimation(2, true);
            } else {
                PlayAnimation(1, true);
            }
        } else if (keyStates[inputConfig.moveLeftKey]) {
            if (isShiftPressed) {
                PlayAnimation(2, true);
            } else {
                PlayAnimation(1, true);
            }
        } else if (keyStates[inputConfig.sitKey] || movement->IsSitting()) {
            PlayAnimation(3, true);
        } else {
            PlayAnimation(0, true);
        }
    }
}

void CharacterAnimation::PlayAnimation(int animIndex, bool loop) {
    if (m_hardLandingActive) {
        int required = (m_hardLandingPhase == 0) ? 15 : 14;
        if (animIndex != required) {
            return;
        }
        if (m_animManager) {
            int cur = m_animManager->GetCurrentAnimation();
            if (cur == required && m_animManager->IsPlaying()) {
                return;
            }
        }
    }
    if (m_animManager) {
        bool allowReplay = (animIndex == 19 || animIndex == 17) ||
                          (animIndex >= 10 && animIndex <= 12) ||
                          (animIndex >= 20 && animIndex <= 22) ||
                          (animIndex == 8 || animIndex == 9) ||
                          (animIndex == 3);

        if (m_lastAnimation != animIndex || allowReplay) {
            m_animManager->Play(animIndex, loop);
            m_lastAnimation = animIndex;
        }
    }
}

void CharacterAnimation::PlayTopAnimation(int animIndex, bool loop) {
    if (m_hardLandingActive) {
        return;
    }
    if (m_topAnimManager) {
        if (m_lastTopAnimation != animIndex) {
            m_topAnimManager->Play(animIndex, loop);
            m_lastTopAnimation = animIndex;
        }
    }
}

int CharacterAnimation::GetCurrentAnimation() const {
    return m_animManager ? m_animManager->GetCurrentAnimation() : -1;
}

bool CharacterAnimation::IsAnimationPlaying() const {
    return m_animManager ? m_animManager->IsPlaying() : false;
}

bool CharacterAnimation::IsFacingLeft(CharacterMovement* movement) const {
    return movement ? movement->IsFacingLeft() : false;
} 

void CharacterAnimation::SetGunMode(bool enabled) {
    if (m_isBatDemon && enabled) {
        return;
    }
    if (enabled && !m_gunMode) {
        unsigned int nowMsEnter = SDL_GetTicks();
        bool isSticky = (m_lastShotTickMs != 0 && (nowMsEnter - m_lastShotTickMs) <= STICKY_AIM_WINDOW_MS);
        if (isSticky) {
            m_aimAngleDeg = m_lastShotAimDeg;
        } else {
            m_aimAngleDeg = 0.0f;
        }
        m_prevAimUp = m_prevAimDown = false;
        m_aimHoldTimerUp = m_aimHoldTimerDown = 0.0f;
        m_aimSincePressUp = m_aimSincePressDown = 0.0f;
        m_gunEntering = !isSticky;
        m_gunEnterStartMs = nowMsEnter;
        m_syncFacingOnEnter = true;
        m_aimHoldBlockUntilMs = m_gunEnterStartMs + (unsigned int)(AIM_HOLD_INITIAL_DELAY * 1000.0f);
        m_lastAimTickMs = m_gunEnterStartMs;
    }
    if (enabled) {
        // Gun mode and grenade mode are mutually exclusive
        m_grenadeMode = false;
    }
    m_gunMode = enabled;
}

void CharacterAnimation::SetGrenadeMode(bool enabled) {
    if (m_isBatDemon && enabled) {
        return;
    }
    if (enabled && !m_grenadeMode) {
        // Initialize aim like entering gun mode
        unsigned int nowMsEnter = SDL_GetTicks();
        bool isSticky = (m_lastShotTickMs != 0 && (nowMsEnter - m_lastShotTickMs) <= STICKY_AIM_WINDOW_MS);
        if (!isSticky) {
            m_aimAngleDeg = 0.0f;
        }
        m_prevAimUp = m_prevAimDown = false;
        m_aimHoldTimerUp = m_aimHoldTimerDown = 0.0f;
        m_aimSincePressUp = m_aimSincePressDown = 0.0f;
        m_aimHoldBlockUntilMs = nowMsEnter + (unsigned int)(AIM_HOLD_INITIAL_DELAY * 1000.0f);
        m_lastAimTickMs = nowMsEnter;
    }
    if (enabled) {
        // Grenade mode and gun mode are mutually exclusive
        m_gunMode = false;
        m_isTurning = false;
        m_gunEntering = false;
    }
    m_grenadeMode = enabled;
}

void CharacterAnimation::SetBatDemonMode(bool enabled) {
    m_isBatDemon = enabled;
    if (enabled) { m_isKitsune = false; }
    if (enabled) {
        SoundManager::Instance().PlaySFXByID(29, 0);
        
        SoundManager::Instance().PlaySFXByID(32, 1);
        
        // Disable overlays
        m_gunMode = false;
        m_grenadeMode = false;
        // Switch texture and start Fly
        if (m_characterObject) {
            m_characterObject->SetTexture(61, 0);
        }
        if (auto texData = ResourceManager::GetInstance()->GetTextureData(61)) {
            if (!m_animManager) {
                m_animManager = std::make_shared<AnimationManager>();
            }
            std::vector<AnimationData> anims;
            anims.reserve(texData->animations.size());
            for (const auto& a : texData->animations) {
                anims.push_back({a.startFrame, a.numFrames, a.duration, 0.0f});
            }
            m_animManager->Initialize(texData->spriteWidth, texData->spriteHeight, anims);
            // Animation 0: Fly
            m_animManager->Play(0, true);
            m_lastAnimation = 0;
        }
        m_batSlashActive = false;
        m_batSlashCooldownTimer = 0.0f;
        m_orcMeteorStrikeActive = false;
        if (m_characterObject) {
            const Vector3& pos = m_characterObject->GetPosition();
            TriggerBatAppearEffectAt(pos.x, pos.y);
        }
    } else {
        if (m_isWerewolf) {
            if (m_characterObject) {
                m_characterObject->SetTexture(60, 0);
            }
            if (auto texData = ResourceManager::GetInstance()->GetTextureData(60)) {
                if (!m_animManager) {
                    m_animManager = std::make_shared<AnimationManager>();
                }
                std::vector<AnimationData> anims;
                anims.reserve(texData->animations.size());
                for (const auto& a : texData->animations) {
                    anims.push_back({a.startFrame, a.numFrames, a.duration, 0.0f});
                }
                m_animManager->Initialize(texData->spriteWidth, texData->spriteHeight, anims);
                m_animManager->Play(0, true);
                m_lastAnimation = 0;
            }
        } else {
            int bodyTexId = (m_objectId == 1000) ? 10 : 11;
            if (m_characterObject) {
                m_characterObject->SetTexture(bodyTexId, 0);
            }
            if (auto texData = ResourceManager::GetInstance()->GetTextureData(bodyTexId)) {
                if (!m_animManager) {
                    m_animManager = std::make_shared<AnimationManager>();
                }
                std::vector<AnimationData> anims;
                anims.reserve(texData->animations.size());
                for (const auto& a : texData->animations) {
                    anims.push_back({a.startFrame, a.numFrames, a.duration, 0.0f});
                }
                m_animManager->Initialize(texData->spriteWidth, texData->spriteHeight, anims);
                m_animManager->Play(0, true);
                m_lastAnimation = 0;
            }
        }
        m_batSlashCooldownTimer = 0.0f;
        m_orcMeteorStrikeActive = false;
    }
}

void CharacterAnimation::TriggerBatAppearEffectAt(float x, float y) {
    if (!m_batAppearAnim) {
        m_batAppearAnim = std::make_shared<AnimationManager>();
    }
    if (!m_batAppearObject) {
        m_batAppearObject = std::make_unique<Object>(m_objectId + 23000);
        if (Object* originalObj = SceneManager::GetInstance()->GetObject(m_objectId)) {
            m_batAppearObject->SetModel(originalObj->GetModelId());
            m_batAppearObject->SetShader(originalObj->GetShaderId());
            m_batAppearObject->SetScale(originalObj->GetScale());
        }
    }
    if (auto texData = ResourceManager::GetInstance()->GetTextureData(72)) { // BatDemon_Appear.tga
        std::vector<AnimationData> anims;
        anims.reserve(texData->animations.size());
        for (const auto& a : texData->animations) {
            anims.push_back({a.startFrame, a.numFrames, a.duration, 0.0f});
        }
        m_batAppearAnim->Initialize(texData->spriteWidth, texData->spriteHeight, anims);
        m_batAppearAnim->Play(0, false);
        m_batAppearActive = true;
        if (m_batAppearObject) {
            m_batAppearObject->SetTexture(72, 0);
            m_batAppearObject->SetVisible(true);
            m_batAppearObject->SetPosition(x, y + BAT_APPEAR_Y_OFFSET, 0.0f);
            m_batAppearObject->SetScale(1.5f, 1.5f, 0.0f);
        }
    }
}

void CharacterAnimation::TriggerBatDemonSlash() {
    if (!m_isBatDemon) return;
    if (m_batSlashActive) return;
    if (m_batSlashCooldownTimer > 0.0f) return;
    
    SoundManager::Instance().PlaySFXByID(30, 0);
    SoundManager::Instance().PlayMusicByID(34, 0);
    
    if (m_animManager) {
        m_animManager->Play(1, false);
        m_lastAnimation = 1;
        m_batSlashActive = true;
    }

    if (!m_batWindAnim) {
        m_batWindAnim = std::make_shared<AnimationManager>();
    }
    if (!m_batWindObject) {
        m_batWindObject = std::make_unique<Object>(m_objectId + 25000);
        if (Object* originalObj = SceneManager::GetInstance()->GetObject(m_objectId)) {
            m_batWindObject->SetModel(originalObj->GetModelId());
            m_batWindObject->SetShader(originalObj->GetShaderId());
            m_batWindObject->SetScale(originalObj->GetScale());
        }
    }
    if (auto texData = ResourceManager::GetInstance()->GetTextureData(68)) {
        std::vector<AnimationData> anims;
        anims.reserve(texData->animations.size());
        for (const auto& a : texData->animations) {
            anims.push_back({a.startFrame, a.numFrames, a.duration, 0.0f});
        }
        m_batWindAnim->Initialize(texData->spriteWidth, texData->spriteHeight, anims);
        m_batWindAnim->Play(0, false);
        m_batWindActive = true;
        m_batWindHasDealtDamage = false;
        if (m_batWindObject && m_characterObject) {
            m_batWindObject->SetTexture(68, 0);
            m_batWindObject->SetVisible(true);
            const Vector3& pos = m_characterObject->GetPosition();
            float face = m_lastFacingLeft ? -1.0f : 1.0f;
            m_batWindFaceSign = face;
            m_batWindObject->SetPosition(pos.x + 0.24f * face, pos.y, pos.z);
            m_batWindObject->SetScale(0.7f, 0.7f, 0.0f);
        }
    }
}

void CharacterAnimation::GetBatWindAabb(float& left, float& right, float& bottom, float& top) const {
    if (m_batWindObject && m_batWindActive) {
        const Vector3& pos = m_batWindObject->GetPosition();
        const Vector3& sc  = m_batWindObject->GetScale();
        float halfW = fabsf(sc.x) * 0.5f * BAT_WIND_HITBOX_SCALE_X;
        float halfH = fabsf(sc.y) * 0.5f * BAT_WIND_HITBOX_SCALE_Y;
        left = pos.x - halfW;
        right = pos.x + halfW;
        bottom = pos.y - halfH;
        top = pos.y + halfH;
    } else {
        left = right = bottom = top = 0.0f;
    }
}

void CharacterAnimation::GetCurrentFrameUV(float& u0, float& v0, float& u1, float& v1) const {
    if (m_animManager) {
        m_animManager->GetUV(u0, v0, u1, v1);
    } else {
        u0 = 0.0f; v0 = 0.0f; u1 = 1.0f; v1 = 1.0f;
    }
}

void CharacterAnimation::GetTopFrameUV(float& u0, float& v0, float& u1, float& v1) const {
    if (m_topAnimManager) {
        m_topAnimManager->GetUV(u0, v0, u1, v1);
    } else {
        u0 = 0.0f; v0 = 0.0f; u1 = 1.0f; v1 = 1.0f;
    }
}

void CharacterAnimation::StartTurn(bool toLeft, bool initialLeft) {
    m_isTurning = true;
    m_turnTargetLeft = toLeft;
    m_turnTimer = 0.0f;
    m_turnInitialLeft = initialLeft;
    PlayTopAnimation(0, false);
}

void CharacterAnimation::HandleGunAim(const bool* keyStates, const PlayerInputConfig& inputConfig) {
    if (!keyStates) return;
    const int aimUpKey = inputConfig.jumpKey;
    const int aimDownKey = inputConfig.sitKey;

    unsigned int nowMs = SDL_GetTicks();
    float dt = 0.0f;
    if (m_lastAimTickMs == 0) {
        dt = 0.0f;
    } else {
        unsigned int diff = nowMs - m_lastAimTickMs;
        dt = diff / 1000.0f;
        if (dt > 0.05f) dt = 0.05f;
        if (dt < 0.0f) dt = 0.0f;
    }
    m_lastAimTickMs = nowMs;

    bool upHeld = keyStates[aimUpKey];
    bool downHeld = keyStates[aimDownKey];
    bool upJust = upHeld && !this->m_prevAimUp;
    bool downJust = downHeld && !this->m_prevAimDown;

    if (upJust && nowMs >= m_aimHoldBlockUntilMs) {
        this->m_aimAngleDeg = ClampFloat(this->m_aimAngleDeg + 6.0f, -90.0f, 90.0f);
        m_aimSincePressUp = 0.0f;
    }
    if (downJust && nowMs >= m_aimHoldBlockUntilMs) {
        this->m_aimAngleDeg = ClampFloat(this->m_aimAngleDeg - 6.0f, -90.0f, 90.0f);
        m_aimSincePressDown = 0.0f;
    }

    if (upJust && downJust) {
        m_aimSincePressUp = m_aimSincePressDown = 0.0f;
        m_aimHoldTimerUp = m_aimHoldTimerDown = 0.0f;
    }

    if (upHeld && nowMs >= m_aimHoldBlockUntilMs) {
        m_aimSincePressUp += dt;
        if (m_aimSincePressUp >= AIM_HOLD_INITIAL_DELAY) {
            m_aimHoldTimerUp += dt;
            while (m_aimHoldTimerUp >= AIM_HOLD_STEP_INTERVAL) {
                m_aimHoldTimerUp -= AIM_HOLD_STEP_INTERVAL;
                this->m_aimAngleDeg = ClampFloat(this->m_aimAngleDeg + 6.0f, -90.0f, 90.0f);
            }
        }
    } else {
        m_aimHoldTimerUp = 0.0f;
        m_aimSincePressUp = 0.0f;
    }

    if (downHeld && nowMs >= m_aimHoldBlockUntilMs) {
        m_aimSincePressDown += dt;
        if (m_aimSincePressDown >= AIM_HOLD_INITIAL_DELAY) {
            m_aimHoldTimerDown += dt;
            while (m_aimHoldTimerDown >= AIM_HOLD_STEP_INTERVAL) {
                m_aimHoldTimerDown -= AIM_HOLD_STEP_INTERVAL;
                this->m_aimAngleDeg = ClampFloat(this->m_aimAngleDeg - 6.0f, -90.0f, 90.0f);
            }
        }
    } else {
        m_aimHoldTimerDown = 0.0f;
        m_aimSincePressDown = 0.0f;
    }

    this->m_prevAimUp = upHeld;
    this->m_prevAimDown = downHeld;
}

void CharacterAnimation::OnGunShotFired(CharacterMovement* movement) {
    m_lastShotAimDeg = m_aimAngleDeg;
    m_lastShotTickMs = SDL_GetTicks();
    
    m_recoilActive = true;
    m_recoilTimer = 0.0f;
    
    m_recoilFaceSign = (movement && movement->IsFacingLeft()) ? -1.0f : 1.0f;
    
    float aimRad = m_aimAngleDeg * 3.14159265f / 180.0f;
    float angleWorld = m_recoilFaceSign * aimRad;
    
    float muzzleX = cosf(angleWorld);
    float muzzleY = sinf(angleWorld);
    
    float k = RECOIL_STRENGTH * m_recoilStrengthMul;
    m_recoilOffsetX = muzzleX * k;
    m_recoilOffsetY = muzzleY * k;
}

void CharacterAnimation::SetGunByTextureId(int texId) {
    m_gunTopAnimReverse = 0;
    m_gunTopAnimHold    = 1;
    m_gunTopAnimReload  = -1;
    m_recoilStrengthMul = 1.0f;
    m_reloadActive = false;
    m_reloadTimer = 0.0f;
    m_recoilActive = false;
    m_recoilTimer = 0.0f;

    switch (texId) {
        case 41: // M4A1
            m_gunTopAnimReverse = 2;
            m_gunTopAnimHold    = 3;
            break;
        case 42: // Shotgun
            m_gunTopAnimReverse = 4;
            m_gunTopAnimHold    = 5;
            m_gunTopAnimReload  = 6;
            break;
        case 43: // Bazoka
            m_gunTopAnimReverse = 8;
            m_gunTopAnimHold    = 9;
            break;
        
        case 45: // Deagle
            m_gunTopAnimReverse = 12;
            m_gunTopAnimHold    = 13;
            break;
        case 46: // Sniper
            m_gunTopAnimReverse = 14;
            m_gunTopAnimHold    = 15;
            break;
        case 47: // Uzi
            m_gunTopAnimReverse = 16;
            m_gunTopAnimHold    = 17;
            m_recoilStrengthMul  = 1.8f;
            break;
        case 40: // Pistol (default)
        default:
            m_gunTopAnimReverse = 0;
            m_gunTopAnimHold    = 1;
            m_gunTopAnimReload  = -1;
            m_recoilStrengthMul  = 1.0f;
            break;
    }
}

void CharacterAnimation::SetWerewolfMode(bool enabled) {
    m_isWerewolf = enabled;
    if (enabled) { m_isKitsune = false; }
    m_werewolfComboActive = false;
    m_werewolfPounceActive = false;
    m_orcMeteorStrikeActive = false;
    m_werewolfComboCooldownTimer = 0.0f;
    m_werewolfPounceCooldownTimer = 0.0f;
    if (enabled) {
        SoundManager::Instance().PlaySFXByID(29, 0);
        
        m_gunMode = false;
        m_grenadeMode = false;
        if (m_characterObject) {
            m_characterObject->SetTexture(60, 0);
        }
        if (auto texData = ResourceManager::GetInstance()->GetTextureData(60)) {
            if (!m_animManager) {
                m_animManager = std::make_shared<AnimationManager>();
            }
            std::vector<AnimationData> anims;
            anims.reserve(texData->animations.size());
            for (const auto& a : texData->animations) {
                anims.push_back({a.startFrame, a.numFrames, a.duration, 0.0f});
            }
            m_animManager->Initialize(texData->spriteWidth, texData->spriteHeight, anims);
            m_animManager->Play(0, true);
            m_lastAnimation = 0;
        }
        if (m_characterObject) {
            const Vector3& pos = m_characterObject->GetPosition();
            TriggerWerewolfAppearEffectAt(pos.x, pos.y);
        }
        
        SoundManager::Instance().PlaySFXByID(31, 1);
    } else {
        m_werewolfComboCooldownTimer = 0.0f;
        m_werewolfPounceCooldownTimer = 0.0f;
        
        // Restore original player body texture and animations
        int bodyTexId = (m_objectId == 1000) ? 10 : 11;
        if (m_characterObject) {
            m_characterObject->SetTexture(bodyTexId, 0);
        }
        if (auto texData = ResourceManager::GetInstance()->GetTextureData(bodyTexId)) {
            if (!m_animManager) {
                m_animManager = std::make_shared<AnimationManager>();
            }
            std::vector<AnimationData> anims;
            anims.reserve(texData->animations.size());
            for (const auto& a : texData->animations) {
                anims.push_back({a.startFrame, a.numFrames, a.duration, 0.0f});
            }
            m_animManager->Initialize(texData->spriteWidth, texData->spriteHeight, anims);
            m_animManager->Play(0, true);
            m_lastAnimation = 0;
        }
    }
}

void CharacterAnimation::TriggerWerewolfAppearEffectAt(float x, float y) {
    if (!m_werewolfAppearAnim) {
        m_werewolfAppearAnim = std::make_shared<AnimationManager>();
    }
    if (!m_werewolfAppearObject) {
        m_werewolfAppearObject = std::make_unique<Object>(m_objectId + 22000);
        if (Object* originalObj = SceneManager::GetInstance()->GetObject(m_objectId)) {
            m_werewolfAppearObject->SetModel(originalObj->GetModelId());
            m_werewolfAppearObject->SetShader(originalObj->GetShaderId());
            m_werewolfAppearObject->SetScale(originalObj->GetScale());
        }
    }
    if (auto texData = ResourceManager::GetInstance()->GetTextureData(71)) { // Werewolf_Appear.tga
        std::vector<AnimationData> anims;
        anims.reserve(texData->animations.size());
        for (const auto& a : texData->animations) {
            anims.push_back({a.startFrame, a.numFrames, a.duration, 0.0f});
        }
        m_werewolfAppearAnim->Initialize(texData->spriteWidth, texData->spriteHeight, anims);
        m_werewolfAppearAnim->Play(0, false);
        m_werewolfAppearActive = true;
        if (m_werewolfAppearObject) {
            m_werewolfAppearObject->SetTexture(71, 0);
            m_werewolfAppearObject->SetVisible(true);
            m_werewolfAppearObject->SetPosition(x, y + WEREWOLF_APPEAR_Y_OFFSET, 0.0f);
            m_werewolfAppearObject->SetScale(0.8f, 0.8f, 0.0f);
        }
    }
}

void CharacterAnimation::SetKitsuneMode(bool enabled) {
    m_isKitsune = enabled;
    if (enabled) {
        m_gunMode = false;
        m_grenadeMode = false;
        m_isBatDemon = false;
        m_orcMeteorStrikeActive = false;
        m_kitsuneEnergyOrbCooldownTimer = 0.0f;
        if (m_characterObject) {
            m_characterObject->SetTexture(62, 0);
        }
        if (auto texData = ResourceManager::GetInstance()->GetTextureData(62)) {
            if (!m_animManager) {
                m_animManager = std::make_shared<AnimationManager>();
            }
            std::vector<AnimationData> anims;
            anims.reserve(texData->animations.size());
            for (const auto& a : texData->animations) {
                anims.push_back({a.startFrame, a.numFrames, a.duration, 0.0f});
            }
            m_animManager->Initialize(texData->spriteWidth, texData->spriteHeight, anims);
            m_animManager->Play(0, true);
            m_lastAnimation = 0;
        }
        if (m_characterObject) {
            const Vector3& pos = m_characterObject->GetPosition();
            TriggerKitsuneAppearEffectAt(pos.x, pos.y);
        }
    } else {
        m_kitsuneEnergyOrbCooldownTimer = 0.0f;
        if (m_isWerewolf) {
            if (m_characterObject) {
                m_characterObject->SetTexture(60, 0);
            }
            if (auto texData = ResourceManager::GetInstance()->GetTextureData(60)) {
                if (!m_animManager) {
                    m_animManager = std::make_shared<AnimationManager>();
                }
                std::vector<AnimationData> anims;
                anims.reserve(texData->animations.size());
                for (const auto& a : texData->animations) {
                    anims.push_back({a.startFrame, a.numFrames, a.duration, 0.0f});
                }
                m_animManager->Initialize(texData->spriteWidth, texData->spriteHeight, anims);
                m_animManager->Play(0, true);
                m_lastAnimation = 0;
            }
        } else if (m_isBatDemon) {
            if (m_characterObject) {
                m_characterObject->SetTexture(61, 0);
            }
            if (auto texData = ResourceManager::GetInstance()->GetTextureData(61)) {
                if (!m_animManager) {
                    m_animManager = std::make_shared<AnimationManager>();
                }
                std::vector<AnimationData> anims;
                anims.reserve(texData->animations.size());
                for (const auto& a : texData->animations) {
                    anims.push_back({a.startFrame, a.numFrames, a.duration, 0.0f});
                }
                m_animManager->Initialize(texData->spriteWidth, texData->spriteHeight, anims);
                m_animManager->Play(0, true);
                m_lastAnimation = 0;
            }
        } else {
            int bodyTexId = (m_objectId == 1000) ? 10 : 11;
            if (m_characterObject) {
                m_characterObject->SetTexture(bodyTexId, 0);
            }
            if (auto texData = ResourceManager::GetInstance()->GetTextureData(bodyTexId)) {
                if (!m_animManager) {
                    m_animManager = std::make_shared<AnimationManager>();
                }
                std::vector<AnimationData> anims;
                anims.reserve(texData->animations.size());
                for (const auto& a : texData->animations) {
                    anims.push_back({a.startFrame, a.numFrames, a.duration, 0.0f});
                }
                m_animManager->Initialize(texData->spriteWidth, texData->spriteHeight, anims);
                m_animManager->Play(0, true);
                m_lastAnimation = 0;
            }
        }
    }
}

void CharacterAnimation::TriggerKitsuneAppearEffectAt(float x, float y) {
    if (!m_kitsuneAppearAnim) {
        m_kitsuneAppearAnim = std::make_shared<AnimationManager>();
    }
    if (!m_kitsuneAppearObject) {
        m_kitsuneAppearObject = std::make_unique<Object>(m_objectId + 24000);
        if (Object* originalObj = SceneManager::GetInstance()->GetObject(m_objectId)) {
            m_kitsuneAppearObject->SetModel(originalObj->GetModelId());
            m_kitsuneAppearObject->SetShader(originalObj->GetShaderId());
            m_kitsuneAppearObject->SetScale(originalObj->GetScale());
        }
    }
    if (auto texData = ResourceManager::GetInstance()->GetTextureData(73)) { // Kitsune_Appear.tga (ID 73)
        std::vector<AnimationData> anims;
        anims.reserve(texData->animations.size());
        for (const auto& a : texData->animations) {
            anims.push_back({a.startFrame, a.numFrames, a.duration, 0.0f});
        }
        m_kitsuneAppearAnim->Initialize(texData->spriteWidth, texData->spriteHeight, anims);
        m_kitsuneAppearAnim->Play(0, false);
        m_kitsuneAppearActive = true;
        if (m_kitsuneAppearObject) {
            m_kitsuneAppearObject->SetTexture(73, 0);
            m_kitsuneAppearObject->SetVisible(true);
            m_kitsuneAppearObject->SetPosition(x, y + KITSUNE_APPEAR_Y_OFFSET, 0.0f);
            m_kitsuneAppearObject->SetScale(0.8f, 0.8f, 0.0f);
        }
    }
}

void CharacterAnimation::SetOrcMode(bool enabled) {
    m_isOrc = enabled;
    if (enabled) {
        m_gunMode = false;
        m_grenadeMode = false;
        m_isBatDemon = false;
        m_isWerewolf = false;
        m_isKitsune = false;
        m_orcMeteorStrikeActive = false;
        m_orcFlameBurstActive = false;
        m_topAnimManager.reset();
        m_topObject.reset();
        m_orcFireActive = false;
        m_orcFireAnim.reset();
        m_orcFireObject.reset();
        m_orcAppearActive = false;
        m_orcAppearAnim.reset();
        m_orcAppearObject.reset();
        m_orcActionCooldownTimer = 0.0f;
        if (m_characterObject) {
            m_characterObject->SetTexture(63, 0);
        }
        if (auto texData = ResourceManager::GetInstance()->GetTextureData(63)) {
            if (!m_animManager) {
                m_animManager = std::make_shared<AnimationManager>();
            }
            std::vector<AnimationData> anims;
            anims.reserve(texData->animations.size());
            for (const auto& a : texData->animations) {
                anims.push_back({a.startFrame, a.numFrames, a.duration, 0.0f});
            }
            m_animManager->Initialize(texData->spriteWidth, texData->spriteHeight, anims);
            m_animManager->Play(0, true); // Idle
            m_lastAnimation = 0;
        }
    } else {
        m_orcMeteorStrikeActive = false;
        m_orcFlameBurstActive = false;
        m_orcFireActive = false;
        m_orcFireAnim.reset();
        m_orcFireObject.reset();
        m_orcAppearActive = false;
        m_orcAppearAnim.reset();
        m_orcAppearObject.reset();
        m_orcActionCooldownTimer = 0.0f;
        if (m_isWerewolf) {
            if (m_characterObject) {
                m_characterObject->SetTexture(60, 0);
            }
            if (auto texData = ResourceManager::GetInstance()->GetTextureData(60)) {
                if (!m_animManager) {
                    m_animManager = std::make_shared<AnimationManager>();
                }
                std::vector<AnimationData> anims;
                anims.reserve(texData->animations.size());
                for (const auto& a : texData->animations) {
                    anims.push_back({a.startFrame, a.numFrames, a.duration, 0.0f});
                }
                m_animManager->Initialize(texData->spriteWidth, texData->spriteHeight, anims);
                m_animManager->Play(0, true);
                m_lastAnimation = 0;
            }
        } else if (m_isBatDemon) {
            if (m_characterObject) {
                m_characterObject->SetTexture(61, 0);
            }
            if (auto texData = ResourceManager::GetInstance()->GetTextureData(61)) {
                if (!m_animManager) {
                    m_animManager = std::make_shared<AnimationManager>();
                }
                std::vector<AnimationData> anims;
                anims.reserve(texData->animations.size());
                for (const auto& a : texData->animations) {
                    anims.push_back({a.startFrame, a.numFrames, a.duration, 0.0f});
                }
                m_animManager->Initialize(texData->spriteWidth, texData->spriteHeight, anims);
                m_animManager->Play(0, true);
                m_lastAnimation = 0;
            }
        } else if (m_isKitsune) {
            if (m_characterObject) {
                m_characterObject->SetTexture(62, 0);
            }
            if (auto texData = ResourceManager::GetInstance()->GetTextureData(62)) {
                if (!m_animManager) {
                    m_animManager = std::make_shared<AnimationManager>();
                }
                std::vector<AnimationData> anims;
                anims.reserve(texData->animations.size());
                for (const auto& a : texData->animations) {
                    anims.push_back({a.startFrame, a.numFrames, a.duration, 0.0f});
                }
                m_animManager->Initialize(texData->spriteWidth, texData->spriteHeight, anims);
                m_animManager->Play(0, true);
                m_lastAnimation = 0;
            }
        } else {
            int bodyTexId = (m_objectId == 1000) ? 10 : 11;
            if (m_characterObject) {
                m_characterObject->SetTexture(bodyTexId, 0);
            }
            if (auto texData = ResourceManager::GetInstance()->GetTextureData(bodyTexId)) {
                if (!m_animManager) {
                    m_animManager = std::make_shared<AnimationManager>();
                }
                std::vector<AnimationData> anims;
                anims.reserve(texData->animations.size());
                for (const auto& a : texData->animations) {
                    anims.push_back({a.startFrame, a.numFrames, a.duration, 0.0f});
                }
                m_animManager->Initialize(texData->spriteWidth, texData->spriteHeight, anims);
                m_animManager->Play(0, true);
                m_lastAnimation = 0;
            }
            int headTexId = (m_objectId == 1000) ? 8 : 9;
            const TextureData* headTex = ResourceManager::GetInstance()->GetTextureData(headTexId);
            if (headTex && headTex->spriteWidth > 0 && headTex->spriteHeight > 0) {
                if (!m_topAnimManager) {
                    m_topAnimManager = std::make_shared<AnimationManager>();
                    std::vector<AnimationData> topAnims;
                    topAnims.reserve(headTex->animations.size());
                    for (const auto& a : headTex->animations) {
                        topAnims.push_back({a.startFrame, a.numFrames, a.duration, 0.0f});
                    }
                    m_topAnimManager->Initialize(headTex->spriteWidth, headTex->spriteHeight, topAnims);
                    m_topAnimManager->Play(0, true);
                }
                if (!m_topObject) {
                    m_topObject = std::make_unique<Object>(m_objectId + 10000);
                    if (m_characterObject) {
                        m_topObject->SetModel(m_characterObject->GetModelId());
                        m_topObject->SetShader(m_characterObject->GetShaderId());
                        m_topObject->SetScale(m_characterObject->GetScale());
                    }
                }
                m_topObject->SetTexture(headTexId, 0);
            }
        }
    }
}

void CharacterAnimation::TriggerWerewolfCombo() {
    if (!m_isWerewolf) return;
    if (m_werewolfComboCooldownTimer > 0.0f || m_werewolfComboActive) return;
    
    SoundManager::Instance().PlaySFXByID(30, 0);
    
    m_werewolfComboActive = true;
    m_werewolfComboHitWindowTimer = m_werewolfComboHitWindow;
    if (m_animManager) {
        m_animManager->Play(1, false);
        m_lastAnimation = 1;
    }
}

void CharacterAnimation::TriggerWerewolfPounce() {
    if (!m_isWerewolf) return;
    if (m_werewolfPounceCooldownTimer > 0.0f || m_werewolfPounceActive) return;
    
    SoundManager::Instance().PlaySFXByID(30, 0);
    
    m_werewolfComboActive = false;
    m_werewolfPounceActive = true;
    m_werewolfPounceHitWindowTimer = m_werewolfPounceHitWindow;
    if (m_animManager) {
        m_animManager->Play(3, false);
        m_lastAnimation = 3;
    }
}

void CharacterAnimation::TriggerKitsuneEnergyOrb() {
    if (!m_isKitsune) return;
    if (m_kitsuneEnergyOrbActive) return;
    if (m_kitsuneEnergyOrbCooldownTimer > 0.0f) return;
    if (m_animManager) {
        m_animManager->Play(3, false);
        m_lastAnimation = 3;
        m_kitsuneEnergyOrbActive = true;
        m_kitsuneEnergyOrbAnimationComplete = false;
    }
}

void CharacterAnimation::TriggerOrcMeteorStrike() {
    if (!m_isOrc) return;
    if (m_orcMeteorStrikeActive) return;
    if (m_orcActionCooldownTimer > 0.0f) return;
    if (m_animManager) {
        m_animManager->Play(2, false);
        m_lastAnimation = 2;
        m_orcMeteorStrikeActive = true;
        m_orcActionCooldownTimer = ORC_ACTION_COOLDOWN;
    }
}

void CharacterAnimation::TriggerOrcFlameBurst() {
    if (!m_isOrc) return;
    if (m_orcMeteorStrikeActive || m_orcFlameBurstActive) return;
    if (m_orcActionCooldownTimer > 0.0f) return;
    
    SoundManager::Instance().PlaySFXByID(28, 0);
    
    if (m_animManager) {
        m_animManager->Play(3, false);
        m_lastAnimation = 3;
        m_orcFlameBurstActive = true;
        m_orcActionCooldownTimer = ORC_ACTION_COOLDOWN;
    }

    if (!m_orcFireAnim) {
        m_orcFireAnim = std::make_shared<AnimationManager>();
    }
    if (!m_orcFireObject) {
        m_orcFireObject = std::make_unique<Object>(m_objectId + 20000);
        if (Object* originalObj = SceneManager::GetInstance()->GetObject(m_objectId)) {
            m_orcFireObject->SetModel(originalObj->GetModelId());
            m_orcFireObject->SetShader(originalObj->GetShaderId());
            m_orcFireObject->SetScale(originalObj->GetScale());
        }
    }
    if (auto texData = ResourceManager::GetInstance()->GetTextureData(67)) {
        std::vector<AnimationData> anims;
        anims.reserve(texData->animations.size());
        for (const auto& a : texData->animations) {
            anims.push_back({a.startFrame, a.numFrames, a.duration, 0.0f});
        }
        m_orcFireAnim->Initialize(texData->spriteWidth, texData->spriteHeight, anims);
        m_orcFireAnim->Play(0, false);
        m_orcFireActive = true;
        if (m_orcFireObject) {
            m_orcFireObject->SetTexture(67, 0);
            m_orcFireObject->SetVisible(true);
            m_orcFireObject->SetPosition(-0.15f, -1.175f, 0.0f);
            m_orcFireObject->SetScale(7.3f, 0.8f, 0.0f);
        }
    }
}

void CharacterAnimation::GetOrcFireAabb(float& left, float& right, float& bottom, float& top) const {
    if (m_orcFireObject) {
        const Vector3& pos = m_orcFireObject->GetPosition();
        const Vector3& sc  = m_orcFireObject->GetScale();
        float halfW = fabsf(sc.x) * 0.5f;
        float halfH = fabsf(sc.y) * 0.5f;
        left = pos.x - halfW;
        right = pos.x + halfW;
        bottom = pos.y - halfH;
        top = pos.y + halfH;
    } else {
        left = right = bottom = top = 0.0f;
    }
}

void CharacterAnimation::TriggerOrcAppearEffectAt(float x, float y) {
    if (!m_orcAppearAnim) {
        m_orcAppearAnim = std::make_shared<AnimationManager>();
    }
    if (!m_orcAppearObject) {
        m_orcAppearObject = std::make_unique<Object>(m_objectId + 21000);
        if (Object* originalObj = SceneManager::GetInstance()->GetObject(m_objectId)) {
            m_orcAppearObject->SetModel(originalObj->GetModelId());
            m_orcAppearObject->SetShader(originalObj->GetShaderId());
            m_orcAppearObject->SetScale(originalObj->GetScale());
        }
    }
    if (auto texData = ResourceManager::GetInstance()->GetTextureData(74)) {
        std::vector<AnimationData> anims;
        anims.reserve(texData->animations.size());
        for (const auto& a : texData->animations) {
            anims.push_back({a.startFrame, a.numFrames, a.duration, 0.0f});
        }
        m_orcAppearAnim->Initialize(texData->spriteWidth, texData->spriteHeight, anims);
        m_orcAppearAnim->Play(0, false);
        m_orcAppearActive = true;
        if (m_orcAppearObject) {
            m_orcAppearObject->SetTexture(74, 0);
            m_orcAppearObject->SetVisible(true);
            m_orcAppearObject->SetPosition(x, y + ORC_APPEAR_Y_OFFSET, 0.0f);
            m_orcAppearObject->SetScale(0.8f, 0.8f, 0.0f);
        }
    }
}