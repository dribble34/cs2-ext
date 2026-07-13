#pragma once

// ============================================================
//  SMOKE COLOR (write feature)
//  Recolors deployed smoke grenades to Config::colSmoke by writing
//  C_SmokeGrenadeProjectile::m_vSmokeColor. Ported from
//  deadlocked-rust (entity/smoke.rs color()). Self-throttled and
//  self-gated (no-op unless Config::bSmokeColor). Offline / -insecure
//  only — a local user-space visual write.
// ============================================================
namespace Smoke {

    // Scans the entity list for smoke projectiles and writes the chosen
    // color. Call once per tick from the main loop; it rate-limits itself.
    void Tick();

}
