#pragma once

/**
 * ============================================================================
 * Vape V4.21 Combat Suite for Minecraft 1.21+ (Fabric/Yarn)
 * Fully Converted to Modern C++20 Header-Only Architecture
 *
 * Ordered from Simplest to Most Complex:
 * ============================================================================
 */

// Level 1: Core Mathematics, Vectors & Primitives
#include "math/Vector3.hpp"
#include "math/RotationUtil.hpp"
#include "utility/Timer.hpp"
#include "utility/Sprint.hpp"
#include "utility/NoItemRelease.hpp"

// Level 1.5: Calculation Mechanics & Utility Sources (Raytracing, Cooldown, Item Scoring & Comparators)
#include "utils/RayTraceUtil.hpp"
#include "utils/ItemStackScoreUtil.hpp"
#include "utils/AttackCooldownUtil.hpp"
#include "utils/EntityComparators.hpp"

// Level 2: Movement & Timing Mechanics
#include "movement/WTap.hpp"
#include "movement/JumpReset.hpp"
#include "movement/Velocity.hpp"
#include "movement/KnockbackDelay.hpp"

// Level 3: Click Engines & Basic Combat Triggers
#include "clicker/AutoClickerTimingState.hpp"
#include "clicker/ClickEngine.hpp"
#include "clicker/LeftClicker.hpp"
#include "clicker/RightClicker.hpp"
#include "combat/Triggerbot.hpp"
#include "combat/Reach.hpp"

// Level 4: Tactics & Dynamic Weapon Switching
#include "combat/ShieldBreaker.hpp"
#include "combat/AutoMace.hpp"
#include "combat/HitSwap.hpp"
#include "combat/HitSelect.hpp"
#include "combat/BlockHit.hpp"

// Level 5: State Trackers & Damage Prediction
#include "tracker/AttackStrengthTracker.hpp"
#include "tracker/AttackPacketTimingTracker.hpp"

// Level 6: Advanced Rotations, Aimbot & Aura
#include "rotation/RotationManager.hpp"
#include "aimassist/AimAssistRotation.hpp"
#include "aimassist/AimAssistTargeting.hpp"
#include "aimassist/AimAssist.hpp"
#include "combat/BowAimbot.hpp"
#include "combat/HitFlick.hpp"
#include "combat/SilentAura.hpp"
#include "combat/CrystalAura.hpp"
