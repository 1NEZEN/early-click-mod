#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

// Buffers airborne jump presses only for ground-jump modes. Continuous/flight
// modes (Ship, UFO, Wave, Swing) must receive taps immediately or their
// controls would be delayed until landing.
class $modify(EarlyClickBuffer, GJBaseGameLayer) {
    struct Fields {
        struct PendingJump {
            bool active = false;
            bool released = false;
            bool releaseNextTick = false;
            int expiresAtTick = 0;
            int button = 1;
            bool isPlayer1 = true;
        };

        PendingJump pending[2];
        int physicsTick = 0;
        bool replayingInput = false;
    };

    static bool supportsGroundJumpBuffer(PlayerObject* player) {
        if (!player) return false;

        // Cube is the default mode. Ball, Robot, and Spider also have
        // ground-triggered actions. Do not delay continuous flight-mode taps.
        return !player->m_isShip &&
               !player->m_isBird &&
               !player->m_isDart &&
               !player->m_isSwing;
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        if (m_fields->replayingInput || button != 1) {
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }

        auto& pending = m_fields->pending[isPlayer1 ? 0 : 1];
        auto* player = isPlayer1 ? m_player1 : m_player2;

        // Pair a release with its buffered press.
        if (!down && pending.active) {
            pending.released = true;
            return;
        }

        // Buffer only modes where a landing can trigger a ground action.
        // All other modes receive their input immediately.
        if (down && supportsGroundJumpBuffer(player) &&
            !player->m_isOnGround && !pending.active) {
            int bufferTicks = Mod::get()->getSettingValue<int>("buffer-ticks");
            if (bufferTicks > 0) {
                pending.active = true;
                pending.released = false;
                pending.releaseNextTick = false;
                pending.expiresAtTick = m_fields->physicsTick + bufferTicks;
                pending.button = button;
                pending.isPlayer1 = isPlayer1;
                return;
            }
        }

        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void update(float dt) {
        GJBaseGameLayer::update(dt);
        ++m_fields->physicsTick;

        for (int i = 0; i < 2; ++i) {
            auto& pending = m_fields->pending[i];
            if (!pending.active) continue;

            // If the user already released, pair that release after replaying
            // the press so a buffered tap does not become a held input.
            if (pending.releaseNextTick) {
                m_fields->replayingInput = true;
                GJBaseGameLayer::handleButton(false, pending.button, pending.isPlayer1);
                m_fields->replayingInput = false;
                pending = Fields::PendingJump{};
                continue;
            }

            auto* player = pending.isPlayer1 ? m_player1 : m_player2;
            bool landed = player && player->m_isOnGround;
            bool expired = m_fields->physicsTick >= pending.expiresAtTick;

            // If the window expires first, still pass the click through;
            // the mod never silently discards an early click.
            if (landed || expired || !supportsGroundJumpBuffer(player)) {
                m_fields->replayingInput = true;
                GJBaseGameLayer::handleButton(true, pending.button, pending.isPlayer1);
                m_fields->replayingInput = false;

                if (pending.released) {
                    pending.releaseNextTick = true;
                } else {
                    pending = Fields::PendingJump{};
                }
            }
        }
    }
};
