#ifndef SPACEWAVES_BONUS_H
#define SPACEWAVES_BONUS_H

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
#include "gamestatus.h"

// Define the maximum number of bonuses that can exist at one time
#define MAX_BONUSES 10

// Enum to represent different types of bonuses available in the game
typedef enum bonustype_s{
    BONUS_ROCKETS,   // Bonus that provides additional rockets
    BONUS_UPGRADE,   // Bonus that upgrades the player temporarily
    BONUS_SHIELD,    // Bonus that grants a temporary shield
    BONUS_POINTS      // Bonus that gives extra points
} bonustype_t;

// Structure to represent a bonus in the game
typedef struct bonus_s{
    bool enabled;          // Flag to indicate if the bonus is alive
    bonustype_t type;     // Type of the bonus
    T3DVec3 polarpos;     // Position of the bonus in polar coordinates
    float time;           // Duration for which the bonus is alive
    float speedx, speedy; // Speed of the bonus in the x and y directions
} bonus_t;

// Array to hold all active bonuses in the game
extern bonus_t bonuses[MAX_BONUSES];

// Function to initialize the bonus system
extern void bonus_init();

// Function to update the state of bonuses each frame
extern void bonus_update();

// Function to apply a specific bonus to a player or defense station
extern void bonus_apply(int bindex, PlyNum playernum, DefenseStation* station, int craft);

// Function to draw the bonuses on the screen
extern void bonus_draw();

// Function to clean up and close the bonus system
extern void bonus_close();

#endif