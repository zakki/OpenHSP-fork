#include "Game.h"
#include "Platform.h"

namespace gameplay
{

inline Game::State Game::getState() const
{
    return _state;
}

inline bool Game::isInitialized() const
{
    return _initialized;
}

inline unsigned int Game::getFrameRate() const
{
    return _frameRate;
}

inline unsigned int Game::getWidth() const
{
    return _width;
}

inline unsigned int Game::getHeight() const
{
    return _height;
}

inline float Game::getAspectRatio() const
{
    return (float)_width / (float)_height;
}

inline const Rectangle& Game::getViewport() const
{
    return _viewport;
}

inline void Game::setMultiSampling(bool enabled)
{
    Platform::setMultiSampling(enabled);
}

inline bool Game::isMultiSampling() const
{
    return Platform::isMultiSampling();
}

inline bool Game::canExit() const
{
    return Platform::canExit();
}

}
