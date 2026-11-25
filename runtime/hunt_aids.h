// Damage numbers ([Hunting Aids] DamageNumbers, runtime/hle/huntaids.c + runtime/overlay.cpp): each hit on a large or
// small monster shows the health it took as a number over the monster, rising and fading in about a second. The game's
// side reads the game's memory once a frame (no hook of the game's code); the drawing is the overlay's, over the game's
// picture. A trimmed port: white numbers only (the styles, colours, elements and health bars of the original are left
// for later).
#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HUNT_NUMBERS 24             // (numbers kept at once: the oldest goes first)
#define HUNT_NUMBER_NS 1000000000ull // how long a number shows (it rises and fades meanwhile)

typedef struct {
    float x, y;   // where its hit is on the game's picture this frame: 0..1 from its left and top edges
    bool shown;   // in front of the camera (else not drawn this frame)
    int value;    // the damage
    uint64_t born_ns;  // when it was dealt (SDL_GetTicksNS)
} HuntNumber;

typedef struct {
    int nnum;
    HuntNumber num[HUNT_NUMBERS];
} HuntView;

// (the game's update thread, once a frame, after the game's update: overlay_frame) the numbers made and placed for this
// frame
void huntaids_frame(void);
// (game thread, the overlay's frame) what to draw now; false: nothing to draw. huntaids_any: only whether there is
bool huntaids_view(HuntView *out);
bool huntaids_any(void);

#ifdef __cplusplus
}
#endif
