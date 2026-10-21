#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

// Input is handled by GJBaseGameLayer in Geometry Dash 2.2081, so hook
// that class directly. The buffer duration is a maximum window.
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
        // Buffer ground-action modes, but don't delay continuous flight input.
        return !player->m_isShip &&
               !player->m_isBird &&
               !player->m_isDart &&
               !player->m_isSwing;
    }

    static bool isGrounded(PlayerObject* player) {
        return player && (player->m_isOnGround || player->m_isOnGround2);
    }

    void replayPress(Fields::PendingJump& pending) {
        m_fields->replayingInput = true;
        GJBaseGameLayer::handleButton(true, pending.button, pending.isPlayer1);
        m_fields->replayingInput = false;

        if (pending.released) {
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

        // Preserve the release of a click that is currently buffered.
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
        // Check before the game's update so the buffered press can be seen
        // by this tick's physics, rather than waiting an extra tick.
        ++m_fields->physicsTick;

        for (int i = 0; i < 2; ++i) {
            auto& pending = m_fields->pending[i];
            if (!pending.active || pending.releaseNextTick) continue;

            auto* player = pending.isPlayer1 ? m_player1 : m_player2;
            bool becamePassable = isGrounded(player);
            bool expired = m_fields->physicsTick >= pending.expiresAtTick;

            if (becamePassable || expired || !supportsGroundJumpBuffer(player)) {
                replayPress(pending);
            }
        }

        GJBaseGameLayer::update(dt);

        // Pair the queued release after one physics update of the press.
        for (int i = 0; i < 2; ++i) {
            auto& pending = m_fields->pending[i];
            if (!pending.active || !pending.releaseNextTick) continue;

            m_fields->replayingInput = true;
            GJBaseGameLayer::handleButton(false, pending.button, pending.isPlayer1);
            m_fields->replayingInput = false;
            pending = Fields::PendingJump{};
        }
    }
};
