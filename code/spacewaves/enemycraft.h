#ifndef ENEMYCRAFT_H
#define ENEMYCRAFT_H

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

#define NUM_CRAFTS 3 // Max is 4 - one in a station
#define MAX_PROJECTILES 16 ///< Maximum number of projectiles per craft.

#define MAX_HEALTH 100 // Maximum health points for crafts
#define INITIAL_DISTANCE 90 // Initial distance from the center of the scene (station)
#define ASTEROID_NEXT_TIME_OFFSET 0.5f // Time offset for the next asteroid availability

#define AI_SCREENEDGE_MARGIN_EASY 0.45f
#define AI_SCREENEDGE_MARGIN_MEDIUM 0.3f
#define AI_SCREENEDGE_MARGIN_HARD 0.2f
#define STICK_MAX_VALUE 68

/**
 * @struct enemycraft_s
 * @brief Structure representing an enemy spacecraft used by players and bots.
 */
typedef struct enemycraft_s {
    PlyNum currentplayer; ///< The player currently controlling this craft.
    joypad_port_t currentplayerport; ///< The joypad port associated with the current player.
    color_t color; ///< Color of the spacecraft.

    T3DVec4 rotq; ///< Quaternion representing the rotation of the craft.
    T3DMat4FP* matx; ///< Pointer to the transformation matrix of the craft.

    float yaw; ///< Desired yaw rotation of the craft.
    float pitch; ///< Desired pitch rotation of the craft.
    float yawoff; ///< Current for yaw rotation.
    float pitchoff; ///< Current for pitch rotation.
    float distance; ///< Desired distance from the center point.
    float distanceoff; ///< Current distance from the center point.

    float hp; ///< Current hit points of the craft.
    float maxhp; ///< Maximum hit points of the craft.

    struct {
        float asteroidnexttime; ///< Time until the next asteroid is available.
        struct {
            bool enabled; ///< Indicates if the asteroid is active.
            T3DVec3 polarpos; ///< Polar coordinates of the asteroid's position.
            float xspeed; ///< Speed of the asteroid in the x direction.
            float yspeed; ///< Speed of the asteroid in the y direction.
            float rotation; ///< Rotation of the asteroid.
            T3DMat4FP* matx; ///< Pointer to the transformation matrix of the asteroid.
            float hp; ///< Hit points of the asteroid.
        } asteroids[MAX_PROJECTILES]; ///< Array of asteroids associated with the craft.
        
        struct {
            bool enabled; ///< Indicates if the rocket is active.
            T3DVec3 polarpos; ///< Polar coordinates of the rocket's position.
            T3DMat4FP* matx; ///< Pointer to the transformation matrix of the rocket.
            float hp; ///< Hit points of the rocket.
        } rockets[MAX_PROJECTILES]; ///< Array of rockets associated with the craft.
        
        float rocketnexttime; ///< Time until the next rocket is fired.
        int rocketcount; ///< Count of rockets currently fired.
        rspq_block_t* rocketdl; ///< Display list for rendering rockets.
        rspq_block_t* asteroiddl; ///< Display list for rendering asteroids.

        float powerup; ///< Current power-up seconds.
        float shield; ///< Current shield seconds.
    } arm;

    bool enabled; ///< Indicates if the craft is alive.
    bool bot; ///< Indicates if the craft is controlled by a bot.

} enemycraft_t;

extern enemycraft_t crafts[NUM_CRAFTS]; ///< Array of scene's crafts.

/**
 * @brief Updates the state of all crafts.
 */
void crafts_update();

/**
 * @brief Initializes the crafts for the specified player in a station.
 * @param currentstation The player in a station to avoid initializing crafts to.
 */
void crafts_init(PlyNum currentstation);

/**
 * @brief Draws the enemy crafts on the screen.
 */
void crafts_draw();

/**
 * @brief Closes and cleans up resources used by the enemy crafts.
 */
void crafts_close();

#endif