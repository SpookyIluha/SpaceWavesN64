#ifndef EFFECTS_H
#define EFFECTS_H

#include <libdragon.h>
#include <time.h>
#include <unistd.h>
#include <display.h>
#include <t3d/t3d.h>
#include <t3d/t3dmath.h>
#include <t3d/t3dmodel.h>
#include "../../core.h"
#include "../../minigame.h"
#include "world.h"

#define MAX_EFFECTS 10

/**
 * @struct effectdata_s
 * @brief Structure to hold visual effects data.
 */
typedef struct effectdata_s {
    struct {
        bool enabled;          /**< Indicates if the 3D effect is enabled */
        T3DVec3 position;     /**< Position of the 3D effect */
        float time;           /**< Duration of the effect */
        color_t color;        /**< Color of the effect */
        T3DMat4FP* matx;     /**< Pointer to the transformation matrix */
    } exp3d[MAX_EFFECTS];    /**< Array of 3D effects */

    struct {
        bool enabled;          /**< Indicates if the 2D effect is enabled */
        T3DVec3 position;     /**< Position of the 2D effect */
        float time;           /**< Duration of the effect */
        color_t color;        /**< Color of the effect */
    } exp2d[MAX_EFFECTS];    /**< Array of 2D effects */

    float rumbletime[MAXPLAYERS]; /**< Rumble effect duration for each player */
    float screenshaketime;         /**< Duration of the screen shake effect */
    color_t ambientlight;          /**< Ambient light color */
} effectdata_t;

extern effectdata_t effects; /**< Global effects data structure */

/**
 * @brief Initializes the effects system.
 */
void effects_init();

/**
 * @brief Updates the effects system, managing active effects.
 */
void effects_update();

/**
 * @brief Draws the active effects on the screen.
 */
void effects_draw();

/**
 * @brief Closes the effects system and cleans up resources.
 */
void effects_close();

/**
 * @brief Adds a 3D explosion effect at a specified position with a given color.
 * @param pos Position of the explosion effect.
 * @param color Color of the explosion effect.
 */
void effects_add_exp3d(T3DVec3 pos, color_t color);

/**
 * @brief Adds a 2D explosion effect at a specified position with a given color.
 * @param pos Position of the explosion effect.
 * @param color Color of the explosion effect.
 */
void effects_add_exp2d(T3DVec3 pos, color_t color);

/**
 * @brief Adds a rumble effect to a specified joypad port for a given duration.
 * @param port The joypad port to apply the rumble effect.
 * @param time Duration of the rumble effect.
 */
void effects_add_rumble(joypad_port_t port, float time);

/**
 * @brief Adds a screen shake effect for a specified duration.
 * @param time Duration of the screen shake effect.
 */
void effects_add_shake(float time);

/**
 * @brief Adds ambient light with a specified color.
 * @param light Color of the ambient light.
 */
void effects_add_ambientlight(color_t light);

#endif