#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

// Buffer jump input in game modes where a discrete ground action is expected.
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

    void replayRelease(Fields::PendingJump& pending) {
        m_fields->replayingInput = true;
        GJBaseGameLayer::handleButton(false, pending.button, pending.isPlayer1);
        m_fields->replayingInput = false;
        pending = Fields::PendingJump{};
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
        ++m_fields->physicsTick;

        // Resolve anything already grounded, expired, or no longer eligible
        // before the original update so the input can affect this update.
        for (int i = 0; i < 2; ++i) {
            auto& pending = m_fields->pending[i];
            if (!pending.active || pending.releaseNextTick) continue;

            auto* player = pending.isPlayer1 ? m_player1 : m_player2;
            bool grounded = isGrounded(player);
            bool expired = m_fields->physicsTick >= pending.expiresAtTick;

            if (grounded || expired || !supportsGroundJumpBuffer(player)) {
                replayPress(pending);
            }
        }

        // Only releases queued before this update are sent here. If the
        // player lands during the original update, its buffered press is
        // replayed below and gets one update before its paired release.
        bool releaseWasQueued[2] = {
            m_fields->pending[0].active && m_fields->pending[0].releaseNextTick,
            m_fields->pending[1].active && m_fields->pending[1].releaseNextTick
        };

        GJBaseGameLayer::update(dt);

        // Landing can happen inside the original physics update. Check again
        // afterward so we don't wait until the full buffer duration expires.
        for (int i = 0; i < 2; ++i) {
            auto& pending = m_fields->pending[i];
            if (!pending.active || pending.releaseNextTick) continue;

            auto* player = pending.isPlayer1 ? m_player1 : m_player2;
            if (isGrounded(player)) {
                replayPress(pending);
            }
        }

        for (int i = 0; i < 2; ++i) {
            auto& pending = m_fields->pending[i];
            if (releaseWasQueued[i] && pending.active && pending.releaseNextTick) {
                replayRelease(pending);
            }
        }
    }
};
