#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <deque>

using namespace geode::prelude;

// Delays button transitions by a configurable number of game-update ticks.
class $modify(EarlyClickBuffer, GJBaseGameLayer) {
    struct Fields {
        struct BufferedInput {
            int dueTick;
            bool down;
            int button;
            bool isPlayer1;
        };

        std::deque<BufferedInput> bufferedInputs;
        int inputTick = 0;
        bool replayingInput = false;
    };

    void handleButton(bool down, int button, bool isPlayer1) {
        if (m_fields->replayingInput) {
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }

        int bufferTicks = Mod::get()->getSettingValue<int>("buffer-ticks");
        if (bufferTicks <= 0) {
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }

        m_fields->bufferedInputs.push_back({
            m_fields->inputTick + bufferTicks,
            down,
            button,
            isPlayer1
        });
    }

    void update(float dt) {
        ++m_fields->inputTick;

        while (!m_fields->bufferedInputs.empty() &&
               m_fields->bufferedInputs.front().dueTick <= m_fields->inputTick) {
            auto input = m_fields->bufferedInputs.front();
            m_fields->bufferedInputs.pop_front();

            m_fields->replayingInput = true;
            GJBaseGameLayer::handleButton(
                input.down,
                input.button,
                input.isPlayer1
            );
            m_fields->replayingInput = false;
        }

        GJBaseGameLayer::update(dt);
    }
};
