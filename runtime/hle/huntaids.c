// Damage numbers (hunt_aids.h): each frame, each monster's health (large and small) against the one before; a drop is a
// hit's damage, shown as a number at the body's middle, projected onto the game's picture with the camera the game drew
// this frame with. A trimmed port of gu-native's runtime/hle/huntaids.c (white numbers, no styles/elements/hit steps).
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "../config.h"
#include "rt.h"
#include "hunt_aids.h"

#define SLOTS          10
#define G_SENEMY       0x1a51fe0u  // sEnemy *: +0x1c the ten monsters
#define G_SCAMERA      0x2326504u  // sCamera * (sFestaCamera)
#define G_GAMECONTROL  0x1a8ca48u  // sGameControl *: +0x4c the town (0xff in a quest), +0x30 our player slot
#define G_SPLAYER      0x1a85b58u  // sPlayer *: +0x1c the four players
#define G_SQUEST       0x1a8ad90u  // sQuest *: +0xa4 the quest being played
#define CAM_VIEW       0xe0u
#define CAM_PROJ       0x120u
#define EN_INFO        0x1428u
#define EN_ZONE        0x1054u
#define EN_STATE       0x73e0u
#define EN_SMALL       0xb5f7u
#define EN_TAKES_PART  0xb5f9u
#define ST_PLACE       0x40u
#define ST_HP          0x370u
#define ST_HP_MAX      0x374u
#define ST_HIT         0x4f0u

static bool sane(uint32_t a) { return a >= 0x08000000u && a < 0xF0000000u && !(a & 3); }

// what a slot followed last frame
static struct {
    uint32_t en;   // the monster this slot followed last frame (0: none)
    int hp;        // its health then
    float hit[3];  // its +0x4f0 when it last made a number
} g_slot[SLOTS];

typedef struct {
    float p[3];        // the world place
    int value;
    uint64_t born;
} Num;
static Num g_num[HUNT_NUMBERS];
static int g_nnum;
static HuntView g_view;
static bool g_view_any, g_touched;

static bool in_quest(void) {
    uint32_t gc = LD32(G_GAMECONTROL), sq = LD32(G_SQUEST);
    return sane(gc) && LD8(gc + 0x4c) == 0xff && sane(sq) && sane(LD32(sq + 0xa4));
}

// the zone of the map the local hunter is in; -1 none
static int hunter_zone(void) {
    uint32_t gc = LD32(G_GAMECONTROL), sp = LD32(G_SPLAYER);
    uint32_t slot = sane(gc) ? LD32(gc + 0x30) : 9, pl = sane(sp) && slot < 4 ? LD32(sp + 0x1c + 4u * slot) : 0;
    return sane(pl) ? LD8(pl + EN_ZONE) : -1;
}

// the body's middle: 100 units above the feet (the joint read of the original is left out)
static void body_middle(uint32_t en, uint32_t info, float out[3]) {
    (void)en;
    out[0] = LDF(info + ST_PLACE), out[1] = LDF(info + ST_PLACE + 4) + 100.0f, out[2] = LDF(info + ST_PLACE + 8);
}

// p (world) on the game's picture this frame: x, y 0..1 from its left and top; false behind the camera
static bool project(const float v[16], const float pr[16], const float p[3], float *x, float *y) {
    float e[4], c[4];
    for (int j = 0; j < 4; j++) e[j] = p[0] * v[j] + p[1] * v[4 + j] + p[2] * v[8 + j] + v[12 + j];
    for (int j = 0; j < 4; j++) c[j] = e[0] * pr[j] + e[1] * pr[4 + j] + e[2] * pr[8 + j] + e[3] * pr[12 + j];
    if (!(c[3] > 1.0f)) return false;
    *x = (c[0] / c[3] + 1.0f) * 0.5f, *y = (1.0f - c[1] / c[3]) * 0.5f;
    return isfinite(*x) && isfinite(*y);
}

static void add_number(int value, const float p[3], uint64_t now) {
    if (g_nnum == HUNT_NUMBERS)
        memmove(g_num, g_num + 1, sizeof g_num[0] * (HUNT_NUMBERS - 1)), g_nnum--;
    Num *n = &g_num[g_nnum++];
    n->p[0] = p[0], n->p[1] = p[1], n->p[2] = p[2], n->value = value, n->born = now;
}

static void forget(void) {
    memset(g_slot, 0, sizeof g_slot);
    g_nnum = 0;
    g_touched = false;
}

static void step(uint64_t now) {
    bool numbers = config_live_damage_numbers();
    g_view_any = false;
    if (!numbers) {
        if (g_touched) forget();
        return;
    }
    if (!in_quest()) {
        if (g_touched) forget();
        return;
    }
    g_touched = true;
    int zone = hunter_zone();
    uint32_t sen = LD32(G_SENEMY);
    for (int k = 0; sane(sen) && k < SLOTS; k++) {
        uint32_t en = LD32(sen + 0x1c + 4u * (uint32_t)k), info = sane(en) ? LD32(en + EN_INFO) : 0;
        if (!sane(info)) {
            g_slot[k].en = 0;
            continue;
        }
        if (g_slot[k].en != en) {  // (another monster in this slot: nothing of the one before)
            g_slot[k].en = en, g_slot[k].hp = (int32_t)LD32(info + ST_HP);
            memcpy(g_slot[k].hit, G2H(info + ST_HIT), sizeof g_slot[k].hit);
        }
        int hp = (int32_t)LD32(info + ST_HP), most = (int32_t)LD32(info + ST_HP_MAX);
        int drop = hp < g_slot[k].hp && g_slot[k].hp - hp <= most ? g_slot[k].hp - hp : 0;
        if (drop && zone >= 0 && LD8(en + EN_ZONE) == zone) {
            float hit[3], at[3];
            memcpy(hit, G2H(info + ST_HIT), sizeof hit);
            body_middle(en, info, at);
            bool fresh = memcmp(hit, g_slot[k].hit, sizeof hit) && isfinite(hit[0]) && isfinite(hit[1]) &&
                         isfinite(hit[2]) && fabsf(hit[0] - at[0]) + fabsf(hit[1] - at[1]) + fabsf(hit[2] - at[2]) < 2500.0f;
            if (fresh) memcpy(at, hit, sizeof at), memcpy(g_slot[k].hit, hit, sizeof hit);
            add_number(drop, at, now);
        }
        g_slot[k].hp = hp;
    }
    for (int i = 0; i < g_nnum;)  // (a number goes a second after it was dealt)
        if (now >= g_num[i].born + HUNT_NUMBER_NS)
            memmove(g_num + i, g_num + i + 1, sizeof g_num[0] * (size_t)(--g_nnum - i));
        else
            i++;
    uint32_t cam = LD32(G_SCAMERA);
    if (g_nnum && sane(cam)) {
        float view[16], proj[16];
        memcpy(view, G2H(cam + CAM_VIEW), sizeof view);
        memcpy(proj, G2H(cam + CAM_PROJ), sizeof proj);
        for (int i = 0; i < g_nnum; i++) {
            HuntNumber *n = &g_view.num[g_view.nnum++];
            n->shown = project(view, proj, g_num[i].p, &n->x, &n->y);
            n->value = g_num[i].value, n->born_ns = g_num[i].born;
        }
    }
    g_view_any = g_view.nnum != 0;
}

void huntaids_frame(void) {
    g_view.nnum = 0;
    step((uint64_t)plat_now_ns());
}

bool huntaids_view(HuntView *out) {
    if (!g_view_any) return false;
    *out = g_view;
    return true;
}

bool huntaids_any(void) { return g_view_any; }
