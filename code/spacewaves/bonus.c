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
#include "station.h"
#include "enemycraft.h"

#include "bonus.h"

// Array to hold all active bonuses in the game
bonus_t bonuses[MAX_BONUSES];

// Timer for the next bonus spawn
float nextbonustime = 5.0f;

// Function to initialize the bonus system
void bonus_init(){
    // Set all bonuses to be disabled at the start
    for(int b = 0; b < MAX_BONUSES; b++){
        bonuses[b].enabled = false;
    }
}

// Function to update the state of bonuses each frame
void bonus_update(){
    // Decrease the timer for the next bonus spawn
    nextbonustime -= DELTA_TIME;

    // Check if it's time to spawn a new bonus
    if(nextbonustime < 0){
        // Reset the timer with a random value between 2 and 6 seconds
        nextbonustime = frandr(2.0f, 6.0f);

        // Find the first available bonus slot
        int b = 0;
        while(b < MAX_BONUSES && bonuses[b].enabled) b++;
        
        // If there is an available slot, enable a new bonus
        if(b < MAX_BONUSES){
            bonuses[b].enabled = true; // Enable the bonus
            bonuses[b].speedx = frandr(-0.05, 0.05); // Random horizontal speed
            bonuses[b].speedy = frandr(-0.05, 0.05); // Random vertical speed
            // Set the polar position of the bonus with random angles
            bonuses[b].polarpos = (T3DVec3){{frandr(T3D_DEG_TO_RAD(-40), T3D_DEG_TO_RAD(40)), frandr(T3D_DEG_TO_RAD(-180), T3D_DEG_TO_RAD(180)), 60.0f}};
            bonuses[b].time = 25.0f; // Set the active duration of the bonus
            bonuses[b].type = randm(4); // Randomly select the type of bonus
        }
    }

    // Update the state of all bonuses
    for(int b = 0; b < MAX_BONUSES; b++){
        bonuses[b].time -= DELTA_TIME; // Decrease the remaining time
        // Update the position of the bonus based on its speed
        bonuses[b].polarpos.v[0] += bonuses[b].speedx * DELTA_TIME;
        bonuses[b].polarpos.v[1] += bonuses[b].speedy * DELTA_TIME;
        // Disable the bonus if its time has expired
        if(bonuses[b].time < 0)
            bonuses[b].enabled = false;       
    }
}

// Function to apply a specific bonus to a player or defense station
void bonus_apply(int bindex, PlyNum playernum, DefenseStation* station, int craft){
    // Check if the bonus index is valid
    if(bindex >= MAX_BONUSES || bindex < 0) return;

    // Get the bonus to apply
    bonus_t* bonus = &bonuses[bindex];

    // Apply the bonus based on its type
    switch(bonus->type){
        case BONUS_POINTS:
            // Increase the player's score
            gamestatus.playerscores[playernum] += 1500;
            bonus->enabled = false; // Disable the bonus after applying
            wav64_play(&sounds[snd_reload], SFX_CHANNEL_BONUS); // Play sound effect
            break;
        case BONUS_UPGRADE:
            // Upgrade the defense station or craft's shooting power
            if(station) station->arm.powerup = 10.0f;
            if(craft >= 0)   crafts[craft].arm.powerup = 10.0f;
            bonus->enabled = false; // Disable the bonus after applying
            wav64_play(&sounds[snd_pickup_shield], SFX_CHANNEL_BONUS); // Play sound effect
            break;
        case BONUS_SHIELD:
            // Grant a invicibility shield to the defense station or craft
            if(station) station->arm.shield = 10.0f;
            if(craft >= 0)   crafts[craft].arm.shield = 10.0f;
            bonus->enabled = false; // Disable the bonus after applying
            wav64_play(&sounds[snd_pickup_shield], SFX_CHANNEL_BONUS); // Play sound effect
            break;
        case BONUS_ROCKETS:
            // Increase the rocket count for the defense station or craft
            if(station) station->arm.rocketcount += 5;
            if(craft >= 0)   crafts[craft].arm.rocketcount += 3;
            bonus->enabled = false; // Disable the bonus after applying
            wav64_play(&sounds[snd_pickup_ammo], SFX_CHANNEL_BONUS); // Play sound effect
            break;
    }
}

// Function to draw the bonuses on the screen
void bonus_draw(){
    // Set rendering modes for drawing bonuses
    rdpq_set_mode_standard();
    rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
    rdpq_mode_alphacompare(5);
    
    T3DVec3 worldpos, viewpos; // Variables for world and view positions
    float xpos, ypos; // Variables for screen coordinates

    // Iterate through all bonuses to draw them
    for(int b = 0; b < MAX_BONUSES; b++){
        bonus_t* bonus = &bonuses[b];
        if(bonus->enabled){ // Only draw enabled bonuses
            // Convert polar coordinates to world position
            worldpos = gfx_worldpos_from_polar(bonus->polarpos.v[0], bonus->polarpos.v[1], bonus->polarpos.v[2]);
            // Calculate the view position from the world position
            t3d_viewport_calc_viewspace_pos(&viewport, &viewpos, &worldpos);
            xpos = viewpos.v[0]; ypos = viewpos.v[1] - 16; // Adjust y position for drawing

            // Check if the bonus is within the viewport and facing the player
            if(gfx_pos_within_viewport(xpos, ypos) && t3d_vec3_dot(&station.forward, &worldpos) > 0.0f){
                // Draw the bonus sprite with a scaling effect based on time
                rdpq_sprite_blit(sprites[spr_ui_bonus1 + bonus->type], xpos, ypos, &(rdpq_blitparms_t){.cx = 16, .scale_x = sin(bonus->time * 3)});
            }
        }
    }
}

// Function to clean up and close the bonus system (noting to deallocate)
void bonus_close(){

}