#include "stdafx.h"
#include "CharacterMovement.h"
#include "WallCollision.h"
#include "PlatformCollision.h"
#include <iostream>
#include <SDL.h>
#include "SceneManager.h"
#include "Object.h"
#include "LadderCollision.h"
#include "TeleportCollision.h"

const float CharacterMovement::JUMP_FORCE = 0.9f;
const float CharacterMovement::GRAVITY = 2.5f;
const float CharacterMovement::GROUND_Y = 0.0f;
const float CharacterMovement::MOVE_SPEED = 0.25f;
const float CharacterMovement::CLIMB_SPEED = 0.25f;
const float CharacterMovement::CLIMB_DOWN_SPEED = 0.375f;
const float CharacterMovement::DIE_KNOCKBACK_SPEED = 0.8f;
const float CharacterMovement::DIE_SLOWMO_DURATION = 0.6f;
const float CharacterMovement::DIE_SLOWMO_SCALE = 0.15f;

const PlayerInputConfig CharacterMovement::PLAYER1_INPUT('A', 'D', 'W', 'S', ' ', 'J', 'K', 'L', 0, 'A', 'S', 'S', 'D');
const PlayerInputConfig CharacterMovement::PLAYER2_INPUT(0x25, 0x27, 0x26, 0x28, '0', 'a', 'b', 'c', 0, 0x25, 0x28, 0x27, 0x28);

CharacterMovement::CharacterMovement() 
    : m_posX(0.0f), m_posY(0.0f), m_facingLeft(false), m_state(CharState::Idle),
      m_isJumping(false), m_jumpVelocity(0.0f), m_jumpStartY(0.0f), m_wasJumping(false),
      m_isSitting(false), m_isDying(false), m_isDead(false), m_dieTimer(0.0f), 
      m_knockdownTimer(0.0f), m_knockdownComplete(false), m_attackerFacingLeft(false), m_inputConfig(PLAYER1_INPUT),
      m_characterWidth(0.1f), m_characterHeight(0.2f), m_isOnPlatform(false), m_currentPlatformY(0.0f),
      m_wallCollision(std::make_unique<WallCollision>()),
      m_platformCollision(std::make_unique<PlatformCollision>()),
      m_ladderCollision(std::make_unique<LadderCollision>()),
      m_teleportCollision(std::make_unique<TeleportCollision>()) {
}

CharacterMovement::CharacterMovement(const PlayerInputConfig& inputConfig)
    : m_posX(0.0f), m_posY(0.0f), m_facingLeft(false), m_state(CharState::Idle),
      m_isJumping(false), m_jumpVelocity(0.0f), m_jumpStartY(0.0f), m_wasJumping(false),
      m_isSitting(false), m_isDying(false), m_isDead(false), m_dieTimer(0.0f), 
      m_knockdownTimer(0.0f), m_knockdownComplete(false), m_attackerFacingLeft(false), m_inputConfig(inputConfig),
      m_characterWidth(0.1f), m_characterHeight(0.2f), m_isOnPlatform(false), m_currentPlatformY(0.0f),
      m_wallCollision(std::make_unique<WallCollision>()),
      m_platformCollision(std::make_unique<PlatformCollision>()),
      m_ladderCollision(std::make_unique<LadderCollision>()),
      m_teleportCollision(std::make_unique<TeleportCollision>()) {
}

CharacterMovement::~CharacterMovement() {
}

void CharacterMovement::Initialize(float startX, float startY, float groundY) {
    m_posX = startX;
    m_posY = startY;
    m_groundY = groundY;
    m_state = CharState::Idle;
    m_facingLeft = false;
    m_isJumping = false;
    m_wasJumping = false;
    m_isSitting = false;
    m_isDying = false;
    m_isDead = false;
    m_dieTimer = 0.0f;
    m_knockdownTimer = 0.0f;
    m_knockdownComplete = false;
    m_attackerFacingLeft = false;
    m_isRolling = false;
    m_isOnPlatform = false;
    m_currentPlatformY = groundY;
    m_highestYInAir = startY;
    m_hardLandingRequested = false;
    m_hasPendingFallDamage = false;
    m_pendingFallDamage = 0.0f;
}

void CharacterMovement::Update(float deltaTime, const bool* keyStates) {
    if (!keyStates) {
        return;
    }
    
    m_wasJumping = m_isJumping;
    
    if (m_isDying) {
        HandleDie(deltaTime);
    } else if (m_noClipNoGravity) {
        UpdateNoClip(deltaTime, keyStates);
    } else {
        HandleMovement(deltaTime, keyStates);
        HandleJump(deltaTime, keyStates);
        
        if (m_wasJumping && !m_isJumping) {
            HandleLanding(keyStates);
        }
    }
}

void CharacterMovement::HandleMovement(float deltaTime, const bool* keyStates) {
    if (!keyStates) {
        return;
    }
    
    if (m_inputLocked) {
        if (!m_isJumping) {
            m_state = CharState::Idle;
        }
        return;
    }

    bool rollHeldLeft = (keyStates[m_inputConfig.rollLeftKey1] && keyStates[m_inputConfig.rollLeftKey2]);
    bool rollHeldRight = (keyStates[m_inputConfig.rollRightKey1] && keyStates[m_inputConfig.rollRightKey2]);
    bool anyRollHeld = rollHeldLeft || rollHeldRight;

    if (anyRollHeld && !m_isJumping) {
        m_rollCadenceActive = true;
        m_rollPhaseTimer += deltaTime;
        float phaseDuration = m_rollPhaseIsRoll ? ROLL_PHASE_DURATION : WALK_PHASE_DURATION;
        if (m_rollPhaseTimer >= phaseDuration) {
            m_rollPhaseTimer -= phaseDuration;
            m_rollPhaseIsRoll = !m_rollPhaseIsRoll;
        }
    } else if (!anyRollHeld) {
        m_rollCadenceActive = false;
        m_rollPhaseIsRoll = true;
        m_rollPhaseTimer = 0.0f;
    }

    m_isRolling = anyRollHeld && m_rollCadenceActive && m_rollPhaseIsRoll;

    if (!m_allowRun) {
        m_isRunningLeft = false;
        m_isRunningRight = false;
    }
    
    bool isOtherAction = keyStates[m_inputConfig.jumpKey];
    
    float currentTime = SDL_GetTicks() / 1000.0f;

    if (keyStates[m_inputConfig.moveLeftKey] && !m_prevLeftKey) {
        if (currentTime - m_lastLeftPressTime < DOUBLE_TAP_THRESHOLD) {
            m_isRunningLeft = m_allowRun;
        }
        m_lastLeftPressTime = currentTime;
    }
    if (!keyStates[m_inputConfig.moveLeftKey]) {
        m_isRunningLeft = false;
    }
    m_prevLeftKey = keyStates[m_inputConfig.moveLeftKey];

    if (keyStates[m_inputConfig.moveRightKey] && !m_prevRightKey) {
        if (currentTime - m_lastRightPressTime < DOUBLE_TAP_THRESHOLD) {
            m_isRunningRight = m_allowRun;
        }
        m_lastRightPressTime = currentTime;
    }
    if (!keyStates[m_inputConfig.moveRightKey]) {
        m_isRunningRight = false;
    }
    m_prevRightKey = keyStates[m_inputConfig.moveRightKey];

    if (!m_isJumping) {
        bool leftKeyHeld = keyStates[m_inputConfig.moveLeftKey];
        bool rightKeyHeld = keyStates[m_inputConfig.moveRightKey];
        bool useInvert = m_invertHorizontal && (leftKeyHeld || rightKeyHeld);
        int moveLeftKey = useInvert ? m_inputConfig.moveRightKey : m_inputConfig.moveLeftKey;
        int moveRightKey = useInvert ? m_inputConfig.moveLeftKey : m_inputConfig.moveRightKey;

        if (m_invertHorizontal && !leftKeyHeld && !rightKeyHeld) {
            m_invertHorizontal = false;
        }
        m_isSitting = keyStates[m_inputConfig.sitKey] && !m_isRolling;
        if (m_isRolling && rollHeldLeft) {
            m_state = CharState::MoveLeft;
            m_facingLeft = true;
            m_posX -= MOVE_SPEED * (1.5f * m_moveSpeedMultiplier) * deltaTime;
        } else if (m_isRolling && rollHeldRight) {
            m_state = CharState::MoveRight;
            m_facingLeft = false;
            m_posX += MOVE_SPEED * (1.5f * m_moveSpeedMultiplier) * deltaTime;
        } else if (!isOtherAction) {
            if (keyStates[moveLeftKey]) {
                m_facingLeft = true;
                m_state = CharState::MoveLeft;
                float walkMul = 0.8f * m_moveSpeedMultiplier;
                float runMul  = (1.5f * m_moveSpeedMultiplier) * m_runSpeedMultiplier;
                if (m_isRunningLeft) { m_posX -= MOVE_SPEED * runMul  * deltaTime; }
                else                 { m_posX -= MOVE_SPEED * walkMul * deltaTime; }
                if (!m_isOnPlatform && m_posY <= m_groundY + 0.05f) {
                    m_isJumping = false;
                    m_posY = m_groundY;
                }
            } else if (keyStates[moveRightKey]) {
                m_facingLeft = false;
                m_state = CharState::MoveRight;
                float walkMul = 0.8f * m_moveSpeedMultiplier;
                float runMul  = (1.5f * m_moveSpeedMultiplier) * m_runSpeedMultiplier;
                if (m_isRunningRight) { m_posX += MOVE_SPEED * runMul  * deltaTime; }
                else                  { m_posX += MOVE_SPEED * walkMul * deltaTime; }
                if (!m_isOnPlatform && m_posY <= m_groundY + 0.05f) {
                    m_isJumping = false;
                    m_posY = m_groundY;
                }
            } else if (keyStates[m_inputConfig.sitKey]) {
                m_state = CharState::Idle;
            } else {
                m_state = CharState::Idle;
            }
        }
    }
}

void CharacterMovement::HandleJump(float deltaTime, const bool* keyStates) {
    if (!keyStates) {
        return;
    }
    if (m_inputLocked) {
        return;
    }
    
    if (keyStates[m_inputConfig.jumpKey]) {
        if (!m_isJumping && (m_posY <= m_groundY + 0.01f || m_isOnPlatform)) {
            m_isSitting = false;
            m_isJumping = true;
            m_jumpVelocity = JUMP_FORCE * m_jumpForceMultiplier;
            m_jumpStartY = m_posY;
            m_highestYInAir = m_posY;
            m_isOnPlatform = false;
            m_justStartedUpwardJump = true;
        }
    }
    

    
    if (m_isJumping) {
        if (!m_allowRun) {
            m_isRunningLeft = false;
            m_isRunningRight = false;
        }
        m_jumpVelocity -= GRAVITY * deltaTime;
        m_posY += m_jumpVelocity * deltaTime;
        if (m_posY > m_highestYInAir) m_highestYInAir = m_posY;
        
        bool isMovingLeft = !m_inputLocked && keyStates[m_inputConfig.moveLeftKey];
        bool isMovingRight = !m_inputLocked && keyStates[m_inputConfig.moveRightKey];
        
        if (isMovingLeft) {
            m_facingLeft = true;
            m_state = CharState::MoveLeft;
            float walkMul = 0.8f * m_moveSpeedMultiplier;
            float runMul  = (1.5f * m_moveSpeedMultiplier) * m_runSpeedMultiplier;
            if (m_isRunningLeft) { m_posX -= MOVE_SPEED * runMul  * deltaTime; }
            else                 { m_posX -= MOVE_SPEED * walkMul * deltaTime; }
        } else if (isMovingRight) {
            m_facingLeft = false;
            m_state = CharState::MoveRight;
            float walkMul = 0.8f * m_moveSpeedMultiplier;
            float runMul  = (1.5f * m_moveSpeedMultiplier) * m_runSpeedMultiplier;
            if (m_isRunningRight) { m_posX += MOVE_SPEED * runMul  * deltaTime; }
            else                  { m_posX += MOVE_SPEED * walkMul * deltaTime; }
        }
        
        float newY = m_posY;
        if (CheckPlatformCollision(newY)) {
            m_posY = newY;
            m_isJumping = false;
            m_jumpVelocity = 0.0f;
            m_isOnPlatform = true;
            m_currentPlatformY = newY;
            float dropDistance = m_highestYInAir - m_posY;
            if (dropDistance >= HARD_LANDING_DROP_THRESHOLD) {
                m_hardLandingRequested = true;
                QueueFallDamageFromDrop(dropDistance);
            }
        }
        else if (m_posY <= m_groundY) {
            m_posY = m_groundY;
            m_isJumping = false;
            m_jumpVelocity = 0.0f;
            m_isOnPlatform = false;
            m_currentPlatformY = m_groundY;
            float dropDistance = m_highestYInAir - m_posY;
            if (dropDistance >= HARD_LANDING_DROP_THRESHOLD) {
                m_hardLandingRequested = true;
                QueueFallDamageFromDrop(dropDistance);
            }
        }
    }
    
    if (!m_isJumping && m_isOnPlatform) {
        float newY = m_posY;
        if (!CheckPlatformCollision(newY)) {
            m_isOnPlatform = false;
            m_isJumping = true;
            m_jumpVelocity = 0.0f;
            m_currentMovingPlatformId = -1;
            m_highestYInAir = m_posY;
        }
    }

    if (!m_isJumping && !m_isDying && !m_isDead && (m_posY <= m_groundY + 0.0001f)) {
        if (!keyStates[m_inputConfig.moveLeftKey] && !keyStates[m_inputConfig.moveRightKey]) {
            m_state = CharState::Idle;
        }
    }
}

void CharacterMovement::QueueFallDamageFromDrop(float dropDistance) {
    float clamped = dropDistance;
    if (clamped < FALL_DAMAGE_MIN_DROP) clamped = FALL_DAMAGE_MIN_DROP;
    if (clamped > FALL_DAMAGE_MAX_DROP) clamped = FALL_DAMAGE_MAX_DROP;
    float t = (clamped - FALL_DAMAGE_MIN_DROP) / (FALL_DAMAGE_MAX_DROP - FALL_DAMAGE_MIN_DROP);
    float dmg = FALL_DAMAGE_MIN + t * (FALL_DAMAGE_MAX - FALL_DAMAGE_MIN);
    m_pendingFallDamage = dmg;
    m_hasPendingFallDamage = true;
}

void CharacterMovement::SetPosition(float x, float y) {
    m_posX = x;
    m_posY = y;
}

void CharacterMovement::TriggerDie(bool attackerFacingLeft) {
    if (!m_isDying && !m_isDead) {
        m_isDying = true;
        m_isDead = false;
        m_dieTimer = 0.0f;
        m_knockdownTimer = 0.0f;
        m_knockdownComplete = false;
        m_attackerFacingLeft = attackerFacingLeft;
        m_state = CharState::Die;
        m_dieBaseY = m_isOnPlatform ? m_currentPlatformY : m_posY;
        m_dieLanded = false;
        m_dieVerticalVelocity = 0.0f;
    }
}

void CharacterMovement::HandleDie(float deltaTime) {
    m_dieTimer += deltaTime;
    
    if (m_dieTimer >= 1.5f) {
        m_isDying = false;
        m_isDead = true;
    }
}

void CharacterMovement::ResetDieState() {
    m_isDying = false;
    m_isDead = false;
    m_dieTimer = 0.0f;
    m_knockdownTimer = 0.0f;
    m_knockdownComplete = false;
    m_dieLanded = false;
    m_dieVerticalVelocity = 0.0f;
    m_state = CharState::Idle;
}

void CharacterMovement::HandleLanding(const bool* keyStates) {
    if (!keyStates) {
        return;
    }
    
    if (keyStates[m_inputConfig.moveLeftKey]) {
        m_state = CharState::MoveLeft;
        m_facingLeft = true;
    } else if (keyStates[m_inputConfig.moveRightKey]) {
        m_state = CharState::MoveRight;
        m_facingLeft = false;
    } else {
        m_state = CharState::Idle;
    }
} 

void CharacterMovement::AddPlatform(float x, float y, float width, float height) {
    if (m_platformCollision) {
        m_platformCollision->AddPlatform(x, y, width, height);
    }
}

void CharacterMovement::ClearPlatforms() {
    if (m_platformCollision) {
        m_platformCollision->ClearPlatforms();
    }
}

void CharacterMovement::AddMovingPlatformById(int objectId) {
    if (m_platformCollision) {
        m_platformCollision->AddMovingPlatform(objectId);
    }
}

void CharacterMovement::ClearMovingPlatforms() {
    if (m_platformCollision) {
        m_platformCollision->ClearMovingPlatforms();
    }
}

void CharacterMovement::SetCharacterSize(float width, float height) {
    m_characterWidth = width;
    m_characterHeight = height;
}

bool CharacterMovement::CheckPlatformCollision(float& newY) {
    if (!m_platformCollision) return false;
    bool collided = m_platformCollision->CheckPlatformCollision(newY, m_posX, m_posY, m_jumpVelocity, m_characterWidth, m_characterHeight);
    if (collided) {
        m_currentMovingPlatformId = m_platformCollision->GetLastCollidedMovingPlatformId();
    }
    return collided;
}

bool CharacterMovement::CheckPlatformCollisionWithHurtbox(float& newY, float hurtboxWidth, float hurtboxHeight, float hurtboxOffsetX, float hurtboxOffsetY) {
    if (!m_platformCollision) return false;
    bool collided = m_platformCollision->CheckPlatformCollisionWithHurtbox(newY, m_posX, m_posY, m_jumpVelocity, hurtboxWidth, hurtboxHeight, hurtboxOffsetX, hurtboxOffsetY);
    if (collided) {
        m_currentMovingPlatformId = m_platformCollision->GetLastCollidedMovingPlatformId();
    }
    return collided;
}

void CharacterMovement::UpdateWithHurtbox(float deltaTime, const bool* keyStates, float hurtboxWidth, float hurtboxHeight, float hurtboxOffsetX, float hurtboxOffsetY, bool allowPlatformFallThrough) {
    if (!keyStates) return;
    
    if (m_isDying || m_isDead) {
        HandleDie(deltaTime);
        return;
    }
    if (m_noClipNoGravity) {
        UpdateNoClip(deltaTime, keyStates);
        return;
    }
    Vector3 currentPos(m_posX, m_posY, 0.0f);
    
    bool handledLadder = HandleLadderWithHurtbox(deltaTime, keyStates, hurtboxWidth, hurtboxHeight, hurtboxOffsetX, hurtboxOffsetY);
    if (!handledLadder) {
        HandleMovement(deltaTime, keyStates);
        HandleJumpWithHurtbox(deltaTime, keyStates, hurtboxWidth, hurtboxHeight, hurtboxOffsetX, hurtboxOffsetY, allowPlatformFallThrough);
    }
    HandleLandingWithHurtbox(keyStates, hurtboxWidth, hurtboxHeight, hurtboxOffsetX, hurtboxOffsetY);
    
    if (m_teleportCollision) {
        const float TELEPORT_LOCK_DURATION = 0.25f;
        if (m_teleportLockTimer > 0.0f) {
            m_teleportLockTimer -= deltaTime;
            if (m_teleportLockTimer < 0.0f) m_teleportLockTimer = 0.0f;
        }

        const bool movingRight = keyStates[m_inputConfig.moveRightKey];
        Vector3 newPos(m_posX, m_posY, 0.0f);
        int fromId = -1, toId = -1;
        if (m_teleportLockTimer <= 0.0f &&
            m_teleportCollision->DetectEnterFromLeft(currentPos, newPos,
                                                    hurtboxWidth, hurtboxHeight,
                                                    hurtboxOffsetX, hurtboxOffsetY,
                                                    movingRight, fromId, toId)) {
            Vector3 exitPos;
            if (m_teleportCollision->ComputeExitPosition(toId, m_posY,
                                                         hurtboxWidth, hurtboxHeight,
                                                         hurtboxOffsetX, hurtboxOffsetY,
                                                         exitPos)) {
                m_posX = exitPos.x;
                m_posY = exitPos.y;
                m_jumpVelocity = 0.0f;
                m_highestYInAir = m_posY;
                m_hardLandingRequested = false;
                m_hasPendingFallDamage = false;
                m_teleportLockTimer = TELEPORT_LOCK_DURATION;
                m_lastTeleportFromId = fromId;
                if (movingRight) {
                    m_invertHorizontal = true;
                }
                if (!m_isJumping) {
                    m_state = CharState::Idle;
                }
            }
        }
    }

    if (m_wallCollision) {
        Vector3 newPos(m_posX, m_posY, 0.0f);
        Vector3 resolvedPos = m_wallCollision->ResolveWallCollision(currentPos, newPos,
                                                                   hurtboxWidth, hurtboxHeight,
                                                                   hurtboxOffsetX, hurtboxOffsetY);
        
        if (resolvedPos.x != newPos.x || resolvedPos.y != newPos.y) {
            m_posX = resolvedPos.x;
            m_posY = resolvedPos.y;

            if (resolvedPos.y != newPos.y && resolvedPos.y > newPos.y) {
                m_isJumping = false;
                m_jumpVelocity = 0.0f;
                float dropDistance = m_highestYInAir - m_posY;
                if (dropDistance >= HARD_LANDING_DROP_THRESHOLD) {
                    m_hardLandingRequested = true;
                    QueueFallDamageFromDrop(dropDistance);
                }
                if (!keyStates[m_inputConfig.moveLeftKey] && !keyStates[m_inputConfig.moveRightKey]) {
                    m_state = CharState::Idle;
                }
            }

            if (resolvedPos.x != newPos.x) {
                if (!keyStates[m_inputConfig.moveLeftKey] && !keyStates[m_inputConfig.moveRightKey]) {
                    m_state = CharState::Idle;
                }
            }
        }
    }
}

void CharacterMovement::UpdateNoClip(float deltaTime, const bool* keyStates) {
    if (!keyStates) return;
    const PlayerInputConfig& ic = GetInputConfig();
    float speed = MOVE_SPEED * 2.0f * m_moveSpeedMultiplier;
    if (keyStates[ic.moveLeftKey])  { m_posX -= speed * deltaTime; m_facingLeft = true;  m_state = CharState::MoveLeft; }
    if (keyStates[ic.moveRightKey]) { m_posX += speed * deltaTime; m_facingLeft = false; m_state = CharState::MoveRight; }
    if (keyStates[ic.jumpKey])      { m_posY += speed * deltaTime; }
    if (keyStates[ic.sitKey])       { m_posY -= speed * deltaTime; }
    m_isJumping = false;
    m_isOnPlatform = false;
}
bool CharacterMovement::HandleLadderWithHurtbox(float deltaTime, const bool* keyStates, float hurtboxWidth, float hurtboxHeight, float hurtboxOffsetX, float hurtboxOffsetY) {
    if (m_inputLocked) {
        return false;
    }
    if (!m_ladderCollision) return false;
    if (!m_ladderEnabled) {
        m_isOnLadder = false;
        return false;
    }

    if (!m_isOnLadder) {
        float cx, top, bottom;
        bool overlapped = m_ladderCollision->CheckLadderOverlapWithHurtbox(
            m_posX, m_posY,
            hurtboxWidth, hurtboxHeight,
            hurtboxOffsetX, hurtboxOffsetY,
            cx, top, bottom
        );
        if (overlapped) {
            float hurtboxBottom = (m_posY + hurtboxOffsetY) - hurtboxHeight * 0.5f;
            bool requireDoubleTap = (hurtboxBottom <= bottom + 0.001f) && !m_isJumping;

            const PlayerInputConfig& input = GetInputConfig();
            bool upKey = keyStates[input.jumpKey];
            bool downKey = keyStates[input.sitKey];

            bool canEnter = false;
            float now = SDL_GetTicks() / 1000.0f;

            if (requireDoubleTap) {
                if (m_allowLadderDoubleTap) {
                    if (upKey && !m_prevUpKey) {
                        if (now - m_lastUpTapTimeForLadder < DOUBLE_TAP_THRESHOLD) {
                            m_upTapCountForLadder++;
                        } else {
                            m_upTapCountForLadder = 1;
                        }
                        m_lastUpTapTimeForLadder = now;
                    }
                    if (downKey && !m_prevDownKeyForLadder) {
                        if (now - m_lastDownTapTimeForLadder < DOUBLE_TAP_THRESHOLD) {
                            m_downTapCountForLadder++;
                        } else {
                            m_downTapCountForLadder = 1;
                        }
                        m_lastDownTapTimeForLadder = now;
                    }
                    canEnter = (m_upTapCountForLadder >= 2) || (m_downTapCountForLadder >= 2);
                } else {
                    canEnter = false;
                }
            } else {
                canEnter = (upKey && !m_prevUpKey) || (downKey && !m_prevDownKeyForLadder);
            }

            if (canEnter) {
                m_isOnLadder = true;
                m_ladderCenterX = cx;
                m_ladderTop = top;
                m_ladderBottom = bottom;
                m_isJumping = false;
                m_jumpVelocity = 0.0f;
                m_isOnPlatform = false;
                m_upTapCountForLadder = 0;
                m_downTapCountForLadder = 0;
            }

            m_prevUpKey = upKey;
            m_prevDownKeyForLadder = downKey;
        }
    }

    if (!m_isOnLadder) {
        return false;
    }

    const PlayerInputConfig& input = GetInputConfig();
    bool upHeld = keyStates[input.jumpKey];
    bool downHeld = keyStates[input.sitKey];
    bool leftHeld = keyStates[input.moveLeftKey];
    bool rightHeld = keyStates[input.moveRightKey];

    if (!leftHeld && !rightHeld) {
        m_posX = m_ladderCenterX;
    }
    if (leftHeld) {
        m_facingLeft = true;
        m_state = CharState::MoveLeft;
        m_posX -= MOVE_SPEED * 0.8f * deltaTime;
    } else if (rightHeld) {
        m_facingLeft = false;
        m_state = CharState::MoveRight;
        m_posX += MOVE_SPEED * 0.8f * deltaTime;
    }

    if (upHeld) {
        m_posY += CLIMB_SPEED * deltaTime;
        if (m_posY > m_ladderTop) m_posY = m_ladderTop;
    } else if (downHeld) {
        m_posY -= CLIMB_DOWN_SPEED * deltaTime;
        if (m_posY < m_ladderBottom) m_posY = m_ladderBottom;
    }

    if (downHeld) {
        const float hurtboxBottom = (m_posY + hurtboxOffsetY) - hurtboxHeight * 0.5f;
        const float eps = 0.0005f;
        if (hurtboxBottom <= m_ladderBottom + eps) {
            m_isOnLadder = false;
            m_isJumping = true;
            m_jumpVelocity = 0.0f;
            return false;
        }
    }

    const float LADDER_EDGE_EPS = 0.002f;
    bool atTop = (m_posY >= m_ladderTop - LADDER_EDGE_EPS);
    if (atTop) {
        if (!upHeld && m_prevUpKey) {
        }
        if (!m_prevUpKey && upHeld) {
            m_isOnLadder = false;
            m_isJumping = true;
            m_jumpVelocity = JUMP_FORCE;
            m_prevUpKey = upHeld;
            return false;
        }
    }

    float cx, top, bottom;
    bool stillOverlap = m_ladderCollision->CheckLadderOverlapWithHurtbox(
        m_posX, m_posY, hurtboxWidth, hurtboxHeight, hurtboxOffsetX, hurtboxOffsetY, cx, top, bottom);
    if (!stillOverlap) {
        m_isOnLadder = false;
        if (m_posY > m_groundY + 0.01f) {
            m_isJumping = true;
            m_jumpVelocity = 0.0f;
        }
        return false;
    }

    m_state = CharState::Idle;
    m_prevUpKey = upHeld;
    return true;
}


void CharacterMovement::HandleJumpWithHurtbox(float deltaTime, const bool* keyStates, float hurtboxWidth, float hurtboxHeight, float hurtboxOffsetX, float hurtboxOffsetY, bool allowPlatformFallThrough) {
    const PlayerInputConfig& inputConfig = GetInputConfig();
    
    bool onGround = (m_posY <= m_groundY + 0.01f);
    bool onWallSupport = false;
    
    if (m_wallCollision) {
        Vector3 testPos(m_posX, m_posY - 0.01f, 0.0f);
        onWallSupport = m_wallCollision->CheckWallCollision(testPos, hurtboxWidth, hurtboxHeight, 
                                                          hurtboxOffsetX, hurtboxOffsetY);
    }
    
    float currentTime = SDL_GetTicks() / 1000.0f;
    int downKey = inputConfig.sitKey;
    bool isDownPressed = keyStates[downKey];
    
    if (isDownPressed && !m_prevDownKey) {
        if (currentTime - m_lastDownPressTime < DOUBLE_TAP_THRESHOLD) {
            if (m_isOnPlatform && !m_isJumping && allowPlatformFallThrough) {
                m_isOnPlatform = false;
                m_isJumping = true;
                m_jumpVelocity = 0.0f;
                m_isFallingThroughPlatform = true;
                m_fallThroughTimer = 0.0f;
            }
        }
        m_lastDownPressTime = currentTime;
    }
    m_prevDownKey = isDownPressed;
    
    if (m_isFallingThroughPlatform) {
        float newY = m_posY;
        if (!CheckPlatformCollisionWithHurtbox(newY, hurtboxWidth, hurtboxHeight, hurtboxOffsetX, hurtboxOffsetY)) {
            m_isFallingThroughPlatform = false;
        }
    }
    
    if (!m_isJumping && (onGround || m_isOnPlatform || onWallSupport)) {
        if (!m_inputLocked && keyStates[inputConfig.jumpKey]) {
            m_isJumping = true;
            m_jumpVelocity = JUMP_FORCE;
            m_jumpStartY = m_posY;
            m_wasJumping = false;
            m_state = CharState::Idle;
            m_isOnPlatform = false;
            m_justStartedUpwardJump = true;
            m_highestYInAir = m_posY;
        }
    }
    
    if (m_isJumping) {
        m_jumpVelocity -= GRAVITY * deltaTime;
        m_posY += m_jumpVelocity * deltaTime;
        if (m_posY > m_highestYInAir) m_highestYInAir = m_posY;
        
        if (!m_inputLocked && keyStates[inputConfig.moveLeftKey]) {
            m_posX -= MOVE_SPEED * 1.5f * deltaTime;
            m_facingLeft = true;
            m_state = CharState::MoveLeft;
        }
        
        if (!m_inputLocked && keyStates[inputConfig.moveRightKey]) {
            m_posX += MOVE_SPEED * 1.5f * deltaTime;
            m_facingLeft = false;
            m_state = CharState::MoveRight;
        }
        
        float newY = m_posY;
        bool landedOnPlatform = !m_isFallingThroughPlatform && CheckPlatformCollisionWithHurtbox(newY, hurtboxWidth, hurtboxHeight, hurtboxOffsetX, hurtboxOffsetY);
        if (landedOnPlatform) {
            m_posY = newY;
            m_isJumping = false;
            m_jumpVelocity = 0.0f;
            m_wasJumping = true;
            m_isOnPlatform = true;
            m_currentPlatformY = newY;
            float dropDistance = m_highestYInAir - m_posY;
            if (dropDistance >= HARD_LANDING_DROP_THRESHOLD) {
                m_hardLandingRequested = true;
                QueueFallDamageFromDrop(dropDistance);
            }
            
            if (!keyStates[inputConfig.moveLeftKey] && !keyStates[inputConfig.moveRightKey]) {
                m_state = CharState::Idle;
            }
        } else if (!landedOnPlatform && m_posY <= m_groundY) {
            m_posY = m_groundY;
            m_isJumping = false;
            m_jumpVelocity = 0.0f;
            m_wasJumping = true;
            m_isOnPlatform = false;
            m_currentPlatformY = m_groundY;
            m_isFallingThroughPlatform = false;
            float dropDistance = m_highestYInAir - m_posY;
            if (dropDistance >= HARD_LANDING_DROP_THRESHOLD) {
                m_hardLandingRequested = true;
                QueueFallDamageFromDrop(dropDistance);
            }
            
            if (!keyStates[inputConfig.moveLeftKey] && !keyStates[inputConfig.moveRightKey]) {
                m_state = CharState::Idle;
            }
        }
    }
}

void CharacterMovement::HandleLandingWithHurtbox(const bool* keyStates, float hurtboxWidth, float hurtboxHeight, float hurtboxOffsetX, float hurtboxOffsetY) {
    if (!m_isJumping) {
        float newY = m_posY;
        bool onPlatform = !m_isFallingThroughPlatform && CheckPlatformCollisionWithHurtbox(newY, hurtboxWidth, hurtboxHeight, hurtboxOffsetX, hurtboxOffsetY);
        if (!onPlatform && !m_isFallingThroughPlatform && m_currentMovingPlatformId != -1) {
            Object* platformObj = SceneManager::GetInstance()->GetObject(m_currentMovingPlatformId);
            if (platformObj) {
                const Vector3& pPos = platformObj->GetPosition();
                const Vector3& pScale = platformObj->GetScale();
                float platformLeft = pPos.x - pScale.x * 0.5f;
                float platformRight = pPos.x + pScale.x * 0.5f;
                float platformTop = pPos.y + pScale.y * 0.5f;

                float hurtboxCenterX = m_posX + hurtboxOffsetX;
                float hurtboxLeft = hurtboxCenterX - hurtboxWidth * 0.5f;
                float hurtboxRight = hurtboxCenterX + hurtboxWidth * 0.5f;
                float hurtboxBottom = m_posY + hurtboxOffsetY - hurtboxHeight * 0.5f;

                bool horizontalOverlap = (hurtboxRight > platformLeft && hurtboxLeft < platformRight);
                const float stickMargin = 0.05f;
                bool closeToTop = (hurtboxBottom >= platformTop - stickMargin && hurtboxBottom <= platformTop + stickMargin);
                if (horizontalOverlap && closeToTop) {
                    newY = platformTop - hurtboxOffsetY + hurtboxHeight * 0.5f;
                    onPlatform = true;
                }
            }
        }
        bool onWall = false;
        if (m_wallCollision) {
            Vector3 testPos(m_posX, m_posY - 0.01f, 0.0f);
            onWall = m_wallCollision->CheckWallCollision(testPos, hurtboxWidth, hurtboxHeight, 
                                                        hurtboxOffsetX, hurtboxOffsetY);
        }
        if (!onPlatform && !onWall) {
            m_isOnPlatform = false;
            if (m_posY > m_groundY + 0.01f) {
                m_isJumping = true;
                m_jumpVelocity = 0.0f;
                m_highestYInAir = m_posY;
            }
        } else if (onPlatform) {
            m_posY = newY;
            m_isOnPlatform = true;
            if (m_currentMovingPlatformId != -1) {
                Object* platformObj = SceneManager::GetInstance()->GetObject(m_currentMovingPlatformId);
                if (platformObj) {
                }
            }
        }
    }
}

void CharacterMovement::InitializeWallCollision() {
    if (m_wallCollision) {
        m_wallCollision->LoadWallsFromScene();
    }
} 

void CharacterMovement::InitializeLadderCollision() {
    if (m_ladderCollision) {
        m_ladderCollision->LoadLaddersFromScene();
    }
}

void CharacterMovement::InitializeTeleportCollision() {
    if (m_teleportCollision) {
        m_teleportCollision->LoadTeleportsFromScene();
        m_teleportLockTimer = 0.0f;
        m_lastTeleportFromId = -1;
    }
}