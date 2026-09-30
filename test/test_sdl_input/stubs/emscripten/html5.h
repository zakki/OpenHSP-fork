#pragma once
using EM_BOOL = int;
struct EmscriptenDeviceMotionEvent {
    double accelerationIncludingGravityX, accelerationIncludingGravityY, accelerationIncludingGravityZ;
};
struct EmscriptenDeviceOrientationEvent { double alpha, beta, gamma; };
struct EmscriptenMouseEvent {};
void emscripten_set_mousedown_callback(...);
void emscripten_set_deviceorientation_callback(...);
void emscripten_set_devicemotion_callback(...);
