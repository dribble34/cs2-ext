#include "esp.h"
#include "config.h"
#include "memory.h"
#include "offsets.h"
#include "overlay.h"

// ============================================================
//  ESP Skeleton Drawing
//  Bone lines using the corrected bone-matrix chain (see offsets.h).
//  Bone positions are read via Memory::Read (RPM) so a bad/relocated
//  bone-array pointer fails the read (returns 0) instead of faulting.
//  Bone pairs mirror deadlocked-rust's Bones::CONNECTIONS.
// ============================================================

static inline Vector3 ReadBone(uintptr_t boneArr, int idx) {
    return Memory::Read<Vector3>(boneArr + idx * Offsets::BONE_STRIDE);
}

static inline bool BoneIsZero(const Vector3& v) {
    return v.x == 0.f && v.y == 0.f && v.z == 0.f;
}

void ESP::DrawSkeleton(uintptr_t boneArr, const float* viewMatrix,
                       float screenW, float screenH, bool isEnemy) {
    if (!Config::bSkeleton) return;
    if (!Memory::IsValidPtr(boneArr)) return;

    Draw::Color colSkel = isEnemy ? Config::colEnemySkeleton : Config::colTeamSkeleton;
    float t = Config::fSkeletonThickness;

    using namespace Offsets;
    constexpr int k_Pairs[][2] = {
        { Bones::Head,      Bones::Neck      },
        { Bones::Neck,      Bones::Spine4    },
        { Bones::Spine4,    Bones::Spine3    },
        { Bones::Spine3,    Bones::Spine2    },
        { Bones::Spine2,    Bones::Spine1    },
        { Bones::Spine1,    Bones::Pelvis    },
        { Bones::Neck,      Bones::ShoulderL },
        { Bones::ShoulderL, Bones::ElbowL    },
        { Bones::ElbowL,    Bones::HandL     },
        { Bones::Neck,      Bones::ShoulderR },
        { Bones::ShoulderR, Bones::ElbowR    },
        { Bones::ElbowR,    Bones::HandR     },
        { Bones::Pelvis,    Bones::HipL      },
        { Bones::HipL,      Bones::KneeL     },
        { Bones::KneeL,     Bones::FootL     },
        { Bones::Pelvis,    Bones::HipR      },
        { Bones::HipR,      Bones::KneeR     },
        { Bones::KneeR,     Bones::FootR     }
    };

    for (const auto& pair : k_Pairs) {
        Vector3 wA = ReadBone(boneArr, pair[0]);
        Vector3 wB = ReadBone(boneArr, pair[1]);
        if (BoneIsZero(wA) || BoneIsZero(wB)) continue;

        Vector2 sA, sB;
        if (!Math::WorldToScreen(wA, sA, viewMatrix, screenW, screenH)) continue;
        if (!Math::WorldToScreen(wB, sB, viewMatrix, screenW, screenH)) continue;

        Draw::AddLine(sA.x, sA.y, sB.x, sB.y, Draw::MakeColor(0, 0, 0, 255), t + 1.5f);
        Draw::AddLine(sA.x, sA.y, sB.x, sB.y, colSkel, t);
    }
}
