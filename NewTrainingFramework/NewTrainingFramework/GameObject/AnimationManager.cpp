#include "stdafx.h"
#include "AnimationManager.h"
#include <iostream>

AnimationManager::AnimationManager() 
    : m_spriteWidth(0), m_spriteHeight(0), m_currentAnimation(-1), 
      m_currentFrame(0), m_timer(0.0f), m_isLooping(false), m_isPlaying(false) {
}

AnimationManager::~AnimationManager() {
}

void AnimationManager::Initialize(int spriteWidth, int spriteHeight, const std::vector<AnimationData>& animations) {
    m_spriteWidth = spriteWidth;
    m_spriteHeight = spriteHeight;
    m_animations = animations;
    
    for (auto& anim : m_animations) {
        if (anim.numFrames > 1) {
            anim.frameTime = (float)anim.duration / (anim.numFrames - 1) / 1000.0f;
        } else {
            anim.frameTime = 0.1f;
        }
    }
}

void AnimationManager::Play(int animationIndex, bool loop) {
    if (animationIndex < 0 || animationIndex >= static_cast<int>(m_animations.size())) {
        return;
    }
    
    m_currentAnimation = animationIndex;
    m_currentFrame = 0;
    m_timer = 0.0f;
    m_isLooping = loop;
    m_isPlaying = true;
    
}

void AnimationManager::Stop() {
    m_isPlaying = false;
    m_currentFrame = 0;
    m_timer = 0.0f;
}

void AnimationManager::Pause() {
    m_isPlaying = false;
}

void AnimationManager::Resume() {
    m_isPlaying = true;
}

void AnimationManager::Update(float deltaTime) {
    if (!m_isPlaying || m_currentAnimation < 0 || m_currentAnimation >= static_cast<int>(m_animations.size())) {
        return;
    }
    
    const AnimationData& currentAnim = m_animations[m_currentAnimation];
    
    if (currentAnim.numFrames <= 1) {
        // Single frame animation - auto-finish after duration
        m_timer += deltaTime;
        if (m_timer >= (float)currentAnim.duration / 1000.0f) {
            m_isPlaying = false;
        }
        return;
    }
    
    m_timer += deltaTime;
    
    if (m_timer >= currentAnim.frameTime) {
        m_timer -= currentAnim.frameTime;
        m_currentFrame++;
        
        if (m_currentFrame >= currentAnim.numFrames) {
            if (m_isLooping) {
                m_currentFrame = 0;
            } else {
                m_currentFrame = currentAnim.numFrames - 1;
                m_isPlaying = false;
            }
        }
    }
}

void AnimationManager::GetUV(float& u0, float& v0, float& u1, float& v1) const {
    if (m_currentAnimation < 0 || m_currentAnimation >= static_cast<int>(m_animations.size())) {
        u0 = 0.0f; v0 = 0.0f; u1 = 1.0f; v1 = 1.0f;
        return;
    }
    
    const AnimationData& currentAnim = m_animations[m_currentAnimation];
    int frameIndex = currentAnim.startFrame + m_currentFrame;
    
    int frameX = frameIndex % m_spriteWidth;
    int frameY = frameIndex / m_spriteWidth;
    
    float du = 1.0f / m_spriteWidth;
    float dv = 1.0f / m_spriteHeight;
    
    u0 = frameX * du;
    v0 = (m_spriteHeight - 1 - frameY) * dv;
    u1 = u0 + du;
    v1 = v0 + dv;
}

const AnimationData* AnimationManager::GetAnimation(int index) const {
    if (index >= 0 && index < static_cast<int>(m_animations.size())) {
        return &m_animations[index];
    }
    return nullptr;
} 