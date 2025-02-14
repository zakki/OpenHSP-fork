#ifndef GAME_H_
#define GAME_H_

#include "Rectangle.h"
#include "Vector4.h"

namespace gameplay
{

class Properties;

/**
 * Defines the base class your game will extend for game initialization, logic and platform delegates.
 *
 * This represents a running cross-platform game application and provides an abstraction
 * to most typical platform functionality and events.
 *
 * @see http://gameplay3d.github.io/GamePlay/docs/file-formats.html#wiki-Game_Config
 */
class Game
{
    friend class Platform;
    friend class ShutdownListener;

public:

    /**
     * The game states.
     */
    enum State
    {
        UNINITIALIZED,
        RUNNING,
        PAUSED
    };

    /**
     * Flags used when clearing the active frame buffer targets.
     */
    enum ClearFlags
    {
        CLEAR_COLOR = GL_COLOR_BUFFER_BIT,
        CLEAR_DEPTH = GL_DEPTH_BUFFER_BIT,
        CLEAR_STENCIL = GL_STENCIL_BUFFER_BIT,
        CLEAR_COLOR_DEPTH = CLEAR_COLOR | CLEAR_DEPTH,
        CLEAR_COLOR_STENCIL = CLEAR_COLOR | CLEAR_STENCIL,
        CLEAR_DEPTH_STENCIL = CLEAR_DEPTH | CLEAR_STENCIL,
        CLEAR_COLOR_DEPTH_STENCIL = CLEAR_COLOR | CLEAR_DEPTH | CLEAR_STENCIL
    };

    /**
     * Constructor.
     */
    Game();

    /**
     * Destructor.
     */
    virtual ~Game();

    /**
     * Gets the single instance of the game.
     *
     * @return The single instance of the game.
     */
    static Game* getInstance();

    /**
     * Gets whether vertical sync is enabled for the game display.
     *
     * @return true if vsync is enabled; false if not.
     */
    static bool isVsync();

    /**
     * Sets whether vertical sync is enabled for the game display.
     *
     * @param enable true if vsync is enabled; false if not.
     */
    static void setVsync(bool enable);

    /**
     * Gets the total absolute running time (in milliseconds) since Game::run().
     *
     * @return The total absolute running time (in milliseconds).
     */
    static double getAbsoluteTime();

    /**
     * Gets the total game time (in milliseconds). This is the total accumulated game time (unpaused).
     *
     * You would typically use things in your game that you want to stop when the game is paused.
     * This includes things such as game physics and animation.
     *
     * @return The total game time (in milliseconds).
     */
    static double getGameTime();

    /**
     * Gets the game state.
     *
     * @return The current game state.
     */
    inline State getState() const;

    /**
     * Determines if the game has been initialized.
     *
     * @return true if the game initialization has completed, false otherwise.
     */
    inline bool isInitialized() const;

    /**
     * Returns the game configuration object.
     *
     * This method returns a Properties object containing the contents
     * of the game.config file.
     *
     * @return The game configuration Properties object.
     */
    Properties* getConfig() const;

    /**
     * Called to initialize the game, and begin running the game.
     *
     * @return Zero for normal termination, or non-zero if an error occurred.
     */
    int run();

    /**
     * Exits the game.
     */
    void exit();

	/**
	 * Platform frame delegate.
	 *
	 * This is called every frame from the platform.
	 * This in turn calls back on the user implemented game methods: update() then render()
	 */
	void frame();

	/**
     * Gets the current frame rate.
     *
     * @return The current frame rate.
     */
    inline unsigned int getFrameRate() const;

    /**
     * Gets the game window width.
     *
     * @return The game window width.
     */
    inline unsigned int getWidth() const;

    /**
     * Gets the game window height.
     *
     * @return The game window height.
     */
    inline unsigned int getHeight() const;

    /**
     * Gets the aspect ratio of the window. (width / height)
     *
     * @return The aspect ratio of the window.
     */
    inline float getAspectRatio() const;

    /**
     * Gets the game current viewport.
     *
     * The default viewport is Rectangle(0, 0, Game::getWidth(), Game::getHeight()).
     */
    inline const Rectangle& getViewport() const;

    /**
     * Sets the game current viewport.
     *
     * The x, y, width and height of the viewport must all be positive.
     *
     * viewport The custom viewport to be set on the game.
     */
    void setViewport(const Rectangle& viewport);

    /**
     * Clears the specified resource buffers to the specified clear values.
     *
     * @param flags The flags indicating which buffers to be cleared.
     * @param clearColor The color value to clear to when the flags includes the color buffer.
     * @param clearDepth The depth value to clear to when the flags includes the color buffer.
     * @param clearStencil The stencil value to clear to when the flags includes the color buffer.
     */
    void clear(ClearFlags flags, const Vector4& clearColor, float clearDepth, int clearStencil);

    /**
     * Clears the specified resource buffers to the specified clear values.
     *
     * @param flags The flags indicating which buffers to be cleared.
     * @param red The red channel.
     * @param green The green channel.
     * @param blue The blue channel.
     * @param alpha The alpha channel.
     * @param clearDepth The depth value to clear to when the flags includes the color buffer.
     * @param clearStencil The stencil value to clear to when the flags includes the color buffer.
     */
    void clear(ClearFlags flags, float red, float green, float blue, float alpha, float clearDepth, int clearStencil);

    /**
     * Called when the game window has been resized.
     *
     * This method is called once the game window is created with its initial size
     * and then again any time the game window changes size.
     *
     * @param width The new game window width.
     * @param height The new game window height.
     */
    virtual void resizeEvent(unsigned int width, unsigned int height);

    /**
     * Sets whether multi-sampling is to be enabled/disabled. Default is disabled.
     *
     * @param enabled true sets multi-sampling to be enabled, false to be disabled.
     */
    inline void setMultiSampling(bool enabled);

    /**
     * Is multi-sampling enabled.
     *
     * @return true if multi-sampling is enabled.
     */
    inline bool isMultiSampling() const;

    /**
     * Whether this game is allowed to exit programmatically.
     *
     * @return true if a programmatic exit is allowed.
     */
    inline bool canExit() const;

protected:

    /**
     * Initialize callback that is called just before the first frame when the game starts.
     */
    virtual void initialize();

    /**
     * Finalize callback that is called when the game on exits.
     */
    virtual void finalize();

    /**
     * Render callback for handling rendering routines.
     *
     * Called just after update, once per frame when game is running.
     * Ideal for all rendering code.
     *
     * @param elapsedTime The elapsed game time.
     */
    virtual void render(float elapsedTime);

private:

    /**
     * Constructor.
     *
     * @param copy The game to copy.
     */
    Game(const Game& copy);

    /**
     * Starts the game.
     */
    bool startup();

    /**
     * Shuts down the game.
     */
    void shutdown();

    /**
     * Loads the game configuration.
     */
    void loadConfig();

    void resizeEventInternal(unsigned int width, unsigned int height);

    bool _initialized;                          // If game has initialized yet.
    State _state;                               // The game state.
    double _elapsedTime;                        // The frame elapsed time
    double _frameLastFPS;                       // The last time the frame count was updated.
    unsigned int _frameCount;                   // The current frame count.
    unsigned int _frameRate;                    // The current frame rate.
    unsigned int _width;                        // The game's display width.
    unsigned int _height;                       // The game's display height.
    Rectangle _viewport;                        // the games's current viewport.
    Vector4 _clearColor;                        // The clear color value last used for clearing the color buffer.
    float _clearDepth;                          // The clear depth value last used for clearing the depth buffer.
    int _clearStencil;                          // The clear stencil value last used for clearing the stencil buffer.
    Properties* _properties;                    // Game configuration properties object.

    // Note: Do not add STL object member variables on the stack; this will cause false memory leaks to be reported.
};

}

#include "Game.inl"

#endif
