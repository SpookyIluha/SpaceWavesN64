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
#include "gfx.h"
#include "effects.h"

// Global variable to hold all effect data
effectdata_t effects;

/**
 * @brief Initializes the effects system.
 * 
 * This function sets up the initial state of the effects, disabling all 3D and 2D effects,
 * allocating memory for 3D effect matrices, and resetting rumble and shake times.
 */
void effects_init(){
    // Loop through all possible effects and initialize them
    for(int i = 0; i < MAX_EFFECTS; i++){
        effects.exp3d[i].enabled = false; // Disable 3D effect
        effects.exp3d[i].matx = malloc_uncached(sizeof(T3DMat4FP)); // Allocate memory for 3D effect matrix

        effects.exp2d[i].enabled = false; // Disable 2D effect
    }
    effects.screenshaketime = 0.0f; // Reset screen shake time
    // Reset rumble times for all players
    for(int i = 0; i < MAXPLAYERS; i++)
        effects.rumbletime[i] = 0;
}

/**
 * @brief Updates the state of all effects.
 * 
 * This function decreases the time remaining for active effects and disables them if their time has expired.
 * It also updates rumble and screen shake states.
 */
void effects_update(){
    // Update 3D effects
    for(int i = 0; i < MAX_EFFECTS; i++){
        if(effects.exp3d[i].enabled){
            effects.exp3d[i].time -= DELTA_TIME; // Decrease time for active 3D effect
            if(effects.exp3d[i].time <= 0) effects.exp3d[i].enabled = false; // Disable if time is up
        }
        if(effects.exp2d[i].enabled){
            effects.exp2d[i].time -= DELTA_TIME; // Decrease time for active 2D effect
            if(effects.exp2d[i].time <= 0) effects.exp2d[i].enabled = false; // Disable if time is up
        }
    }
    // Update rumble effects for each player
    for(int i = 0; i < MAXPLAYERS; i++){
        if(effects.rumbletime[i] > 0){
            effects.rumbletime[i] -= DELTA_TIME; // Decrease rumble time
            joypad_set_rumble_active(i, true); // Activate rumble
        } else joypad_set_rumble_active(i, false); // Deactivate rumble
    }
    // Update screen shake time
    if(effects.screenshaketime > 0){
        effects.screenshaketime -= DELTA_TIME; // Decrease shake time
    } else effects.screenshaketime = 0.0f; // Reset shake time if expired

    // Gradually reduce ambient light values
    for(int i = 0; i < 4; i++){
        if(effects.ambientlight.r > 3) effects.ambientlight.r -= 3;
        if(effects.ambientlight.g > 3) effects.ambientlight.g -= 3;
        if(effects.ambientlight.b > 3) effects.ambientlight.b -= 3;
        if(effects.ambientlight.a > 3) effects.ambientlight.a -= 3;
    }
    // Update the world's ambient light with the current effect's ambient light
    world.sun.ambient = effects.ambientlight;
}

void effects_add_exp3d(T3DVec3 pos, color_t color){
    // Find the first available effect slot for a 3D explosion
    int i = 0; 
    while(effects.exp3d[i].enabled && i < MAX_EFFECTS - 1) i++;

    // Enable the effect and set its properties
    effects.exp3d[i].enabled = true; // Mark the effect as enabled
    effects.exp3d[i].position = pos; // Set the position of the explosion
    effects.exp3d[i].color = color;   // Set the color of the explosion
    effects.exp3d[i].time = 2.0f;      // Set the duration of the explosion effect
    wav64_play(&sounds[snd_explosion_blast], SFX_CHANNEL_EXPLOSION); // Play explosion sound
}

void effects_add_exp2d(T3DVec3 pos, color_t color){
    // Find the first available effect slot for a 2D explosion
    int i = 0; 
    while(effects.exp2d[i].enabled && i < MAX_EFFECTS - 1) i++;

    // Enable the effect and set its properties
    effects.exp2d[i].enabled = true; // Mark the effect as enabled
    effects.exp2d[i].position = pos; // Set the position of the explosion
    effects.exp2d[i].color = color;   // Set the color of the explosion
    effects.exp2d[i].color.a = 255;   // Set the alpha value for full opacity
    effects.exp2d[i].time = 1.6f;      // Set the duration of the explosion effect
}

void effects_add_rumble(joypad_port_t port, float time){
    // Add rumble effect time to the specified joypad port
    effects.rumbletime[port] += time; // Accumulate rumble time for the specified port
}

void effects_add_shake(float time){
    // Add screen shake effect time
    effects.screenshaketime += time; // Accumulate shake time for the screen
}

void effects_add_ambientlight(color_t light){
    // Adjust the ambient light settings
    effects.ambientlight.r += light.r; // Increase red component of ambient light
    effects.ambientlight.g += light.g; // Increase green component of ambient light
    effects.ambientlight.b += light.b; // Increase blue component of ambient light
    effects.ambientlight.a += light.a; // Increase alpha component of ambient light
}

void effects_draw(){
    // Set up rendering modes for drawing effects
    rdpq_set_mode_standard(); // Set standard rendering mode
    rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY); // Set blending mode
    rdpq_mode_combiner(RDPQ_COMBINER_TEX_FLAT); // Set combiner mode
    rdpq_mode_dithering(dither); // Set dithering mode
    rdpq_mode_filter(FILTER_BILINEAR); // Set texture filtering mode
    rdpq_mode_zbuf(false, false); // Disable z-buffering
    rdpq_mode_persp(true); // Enable perspective projection

    // Get material and object for 3D effects
    T3DMaterial* mat =  t3d_model_get_material(models[EXP3D], "f3d.exp3d.rgba32");
    T3DObject* obj = t3d_model_get_object_by_index(models[EXP3D], 0);
    t3d_model_draw_material(mat, NULL); // Draw the material for 3D effects

    // Draw all enabled 3D effects
    for(int i = 0; i < MAX_EFFECTS; i++)
        if(effects.exp3d[i].enabled){
            float scale = 2.0 - effects.exp3d[i].time; // Calculate scale based on remaining time
            effects.exp3d[i].color.a = effects.exp3d[i].time * (255 / 2); // Adjust alpha based on time
            rdpq_set_env_color(effects.exp3d[i].color); // Set environment color for the effect
            t3d_mat4fp_from_srt_euler(effects.exp3d[i].matx,
            (float[3]){scale,scale,scale}, // Scale for the effect
            (float[3]){0,0,0}, // No rotation
            effects.exp3d[i].position.v); // Position of the effect
            t3d_matrix_push(effects.exp3d[i].matx); // Push the transformation matrix onto the stack
            rdpq_sync_pipe(); // Synchronize the rendering pipeline
            rdpq_sync_tile(); // Synchronize tile rendering
            t3d_model_draw_object(obj, NULL); // Draw the 3D object for the effect
            rdpq_sync_pipe(); // Synchronize the rendering pipeline
            rdpq_sync_tile(); // Synchronize tile rendering
            t3d_matrix_pop(1); // Pop the transformation matrix from the stack
        }
    
    // Set up rendering modes for drawing 2D effects
    rdpq_set_mode_standard(); // Set standard rendering mode
    rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY); // Set blending mode
    rdpq_mode_combiner(RDPQ_COMBINER_TEX_FLAT); // Set combiner mode
    rdpq_mode_dithering(dither); // Set dithering mode
    rdpq_mode_filter(FILTER_BILINEAR); // Set texture filtering mode
    rdpq_mode_zbuf(false, false); // Disable z-buffering

    // Draw all enabled 2D effects
    for(int i = 0; i < MAX_EFFECTS; i++)
        if(effects.exp2d[i].enabled){
            T3DVec3 viewpos; // Variable to hold the viewport position
            t3d_viewport_calc_viewspace_pos(&viewport, &viewpos, &(effects.exp2d[i].position)); // Calculate view space position
            rdpq_set_prim_color(effects.exp2d[i].color); // Set the primary color for the effect
            float xpos = viewpos.v[0]; // Get x position
            float ypos = viewpos.v[1]; // Get y position
            int index = 16 - (effects.exp2d[i].time * 10); // Calculate sprite index based on time
            if(index >= 15) index = 15; // Clamp index to maximum
            if(index <= 0) index = 0; // Clamp index to minimum

            // Check if the position is within the viewport before drawing
            if(gfx_pos_within_viewport(xpos, ypos))
                rdpq_sprite_blit(sprites[spr_exp], xpos, ypos, &(rdpq_blitparms_t){.cx = 16, .cy = 16, .s0 = (index % 4)*32, (index / 4)*32, .width = 32, .height = 32}); // Draw the sprite
        }
}

void effects_close(){
    // Disable all effects and free resources
    for(int i = 0; i < MAX_EFFECTS; i++){
        effects.exp3d[i].enabled = false; // Disable 3D effect
        if(effects.exp3d[i].matx) free_uncached(effects.exp3d[i].matx); // Free allocated matrix memory

        effects.exp2d[i].enabled = false; // Disable 2D effect
    }
    // Disable rumble for all joypad ports
    for(int i = 0; i < MAXPLAYERS; i++)
        joypad_set_rumble_active(i, false); // Turn off rumble for each player
}