#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

// Hook PlayLayer directly so gameplay input is intercepted at the layer
// that receives actual player button presses.
class $modify(EarlyClickBuffer, PlayLayer) {
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
        return player && player->m_isOnGround;
    }

    void replayPress(Fields::PendingJump& pending) {
        m_fields->replayingInput = true;
        PlayLayer::handleButton(true, pending.button, pending.isPlayer1);
        m_fields->replayingInput = false;

        if (pending.released) {
            pending.releaseNextTick = true;
        } else {
            pending = Fields::PendingJump{};
        }
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        if (m_fields->replayingInput || button != 1) {
            PlayLayer::handleButton(down, button, isPlayer1);
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

        PlayLayer::handleButton(down, button, isPlayer1);
    }

    void update(float dt) {
        PlayLayer::update(dt);
        ++m_fields->physicsTick;

        for (int i = 0; i < 2; ++i) {
            auto& pending = m_fields->pending[i];
            if (!pending.active) continue;

            if (pending.releaseNextTick) {
                m_fields->replayingInput = true;
                PlayLayer::handleButton(false, pending.button, pending.isPlayer1);
                m_fields->replayingInput = false;
                pending = Fields::PendingJump{};
                continue;
            }

            auto* player = pending.isPlayer1 ? m_player1 : m_player2;
            bool becamePassable = isGrounded(player);
            bool expired = m_fields->physicsTick >= pending.expiresAtTick;

            if (becamePassable || expired || !supportsGroundJumpBuffer(player)) {
                replayPress(pending);
            }
        }
    }
};
