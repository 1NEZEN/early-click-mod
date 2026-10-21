#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

// Buffers jump presses only while the selected player is airborne.
// If the player lands within the configured physics-tick window, the
// press is replayed on landing. If the window expires first, the press
// is passed through anyway, so an actually-too-early click is not erased.
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

    void handleButton(bool down, int button, bool isPlayer1) {
        if (m_fields->replayingInput || button != 1) {
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }

        auto& pending = m_fields->pending[isPlayer1 ? 0 : 1];
        auto* player = isPlayer1 ? m_player1 : m_player2;

        // Keep a release paired with a press that is currently buffered.
        if (!down && pending.active) {
            pending.released = true;
            return;
        }

        // Only buffer a fresh jump press when the player is airborne.
        // All other inputs are handled immediately as normal.
        if (down && player && !player->m_isOnGround && !pending.active) {
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

            // Once the press is replayed, deliver its release on the
            // following physics tick if the user already lifted.
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

            if (landed || expired) {
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
