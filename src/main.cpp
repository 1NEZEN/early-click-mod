#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

// Buffers airborne jump presses for ground-action modes. The configured
// duration is a maximum window, not a fixed delay: replay as soon as the
// player becomes grounded, otherwise pass the input through when the window
// expires.
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

        return !player->m_isShip &&
               !player->m_isBird &&
               !player->m_isDart &&
               !player->m_isSwing;
    }

    static bool isGrounded(PlayerObject* player) {
        // GD exposes two ground-state flags; either can indicate the
        // first frame on which a buffered ground action can be accepted.
        return player && (player->m_isOnGround || player->m_isOnGround2);
    }

    void replayPress(Fields::PendingJump& pending) {
        m_fields->replayingInput = true;
        GJBaseGameLayer::handleButton(true, pending.button, pending.isPlayer1);
        m_fields->replayingInput = false;

        if (pending.released) {
            // Keep the queued tap paired with its release.
            pending.releaseNextTick = true;
        } else {
            pending = Fields::PendingJump{};
        }
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        if (m_fields->replayingInput || button != 1) {
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }

        auto& pending = m_fields->pending[isPlayer1 ? 0 : 1];
        auto* player = isPlayer1 ? m_player1 : m_player2;

        if (!down && pending.active) {
            pending.released = true;
            return;
        }

        if (down && supportsGroundJumpBuffer(player) &&
            !isGrounded(player) && !pending.active) {
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

            if (pending.releaseNextTick) {
                m_fields->replayingInput = true;
                GJBaseGameLayer::handleButton(false, pending.button, pending.isPlayer1);
                m_fields->replayingInput = false;
                pending = Fields::PendingJump{};
                continue;
            }

            auto* player = pending.isPlayer1 ? m_player1 : m_player2;
            bool becamePassable = isGrounded(player);
            bool expired = m_fields->physicsTick >= pending.expiresAtTick;

            // Release on the first valid grounded tick, not after the whole
            // selected window. Expiry is only the fallback if it never lands.
            if (becamePassable || expired || !supportsGroundJumpBuffer(player)) {
                replayPress(pending);
            }
        }
    }
};
