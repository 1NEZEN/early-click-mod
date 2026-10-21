#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <deque>

using namespace geode::prelude;

// This mod delays button transitions by a configurable number of game-update
// ticks. Presses and releases are queued together so their order is preserved.
class $modify(EarlyClickBuffer, GJBaseGameLayer) {
    struct Fields {
        struct BufferedInput {
            int dueTick;
            bool down;
            int button;
            bool isPlayer1;
        };

        std::deque<BufferedInput> m_fields->m_bufferedInputs;
        int m_fields->m_inputTick = 0;
        bool m_fields->m_replayingInput = false;
    };

    void handleButton(bool down, int button, bool isPlayer1) {
        // Replayed inputs must reach the original handler instead of being
        // queued again.
        if (m_fields->m_replayingInput) {
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }

        int bufferTicks = Mod::get()->getSettingValue<int>("buffer-ticks");
        if (bufferTicks <= 0) {
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }

        m_fields->m_bufferedInputs.push_back({
            m_fields->m_inputTick + bufferTicks,
            down,
            button,
            isPlayer1
        });
    }

    void update(float dt) {
        ++m_fields->m_inputTick;

        while (!m_fields->m_bufferedInputs.empty() &&
               m_fields->m_bufferedInputs.front().dueTick <= m_fields->m_inputTick) {
            auto input = m_fields->m_bufferedInputs.front();
            m_fields->m_bufferedInputs.pop_front();

            m_fields->m_replayingInput = true;
            GJBaseGameLayer::handleButton(
                input.down,
                input.button,
                input.isPlayer1
            );
            m_fields->m_replayingInput = false;
        }

        GJBaseGameLayer::update(dt);
    }
};
