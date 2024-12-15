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
#include "bonus.h"
#include "effects.h"
#include "enemycraft.h"

// Array to hold enemy spacecraft data
enemycraft_t crafts[3];

/**
 * @brief Initializes the enemy spacecrafts.
 * 
 * This function sets up the enemy spacecrafts by assigning player numbers,
 * colors, health points, and other initial parameters. It also allocates
 * memory for the transformation matrices used for rendering.
 * 
 * @param currentstation The station number of the current player.
 */
void crafts_init(PlyNum currentstation){
    PlyNum player = PLAYER_1; // Start with the first player
    for(int c = 0; c < NUM_CRAFTS; c++, player++){
        // Skip the current player in the enemy list
        if(player == currentstation) player++;
        
        // Determine if the craft is controlled by a bot by checking the amount of players available and setting all the crafts to bots after none are left
        crafts[c].bot = player >= core_get_playercount();
        crafts[c].currentplayer = player; // Assign the current player number
        crafts[c].currentplayerport = core_get_playercontroller(player); // Get the controller port
        crafts[c].color = gfx_get_playercolor(player); // Get the player's color
        crafts[c].maxhp = MAX_HEALTH; // Set maximum health points
        crafts[c].hp = crafts[c].maxhp; // Initialize current health points
        crafts[c].pitch = frandr(-0.5f, 0.5f); // Randomize pitch
        crafts[c].yaw = frandr(-0.5f, 0.5f); // Randomize yaw
        crafts[c].pitchoff = 0; // Initialize pitch to 0
        crafts[c].yawoff = 0; // Initialize yaw to 0
        crafts[c].distance = INITIAL_DISTANCE; // Set initial distance from the center of the scene (station)
        crafts[c].distanceoff = crafts[c].distance;
        crafts[c].matx = malloc_uncached(sizeof(T3DMat4FP)); // Allocate memory for transformation matrix
        crafts[c].arm.rocketcount = 0; // Initialize rocket count
        
        // Initialize projectiles (rockets and asteroids)
        for(int b = 0; b < MAX_PROJECTILES; b++){
            crafts[c].arm.rockets[b].matx = malloc_uncached(sizeof(T3DMat4FP)); // Allocate memory for rocket matrix
            crafts[c].arm.asteroids[b].matx = malloc_uncached(sizeof(T3DMat4FP)); // Allocate memory for asteroid matrix
            crafts[c].arm.rockets[b].enabled = false; // Disable rockets initially
            crafts[c].arm.asteroids[b].enabled = false; // Disable asteroids initially
        }
        
        crafts[c].arm.asteroidnexttime = CURRENT_TIME + ASTEROID_NEXT_TIME_OFFSET; // Set the next asteroid available time
        crafts[c].arm.powerup = 0; // Initialize power-up timer
        crafts[c].arm.shield = 0; // Initialize shield timer
        crafts[c].enabled = true; // Enable the craft
    }
}

// Bot AI in the game (output joystick state only)
void crafts_botlogic_getinput(enemycraft_t* craft, int index, joypad_inputs_t* outinput, joypad_buttons_t* outpressed, joypad_buttons_t* outheld) {
    // Get the current AI difficulty level
    AiDiff diff = core_get_aidifficulty();
    
    // Get the dimensions of the display
    int w, h, w2, h2;
    w = display_get_width();
    h = display_get_height();
    w2 = display_get_width() / 2; // Half width
    h2 = display_get_height() / 2; // Half height
    
    // Set a margin for the bots to avoid the crosshair
    float md_toavoid = 0.5f;

    // Initialize cursor position to the one of station's (and rocket launcher) and view positions
    T3DVec3 cursorpos = (T3DVec3){{0, 0, 0}};
    T3DVec3 viewpos, cursorviewpos = (T3DVec3){{0, 0, 0}};
    
    // If the difficulty is hard or higher, calculate the machinegun cursor position instead
    if (diff >= DIFF_HARD) {
        cursorpos = gfx_worldpos_from_polar(station.pitch, station.yaw, 1000);
        t3d_viewport_calc_viewspace_pos(&viewport, &cursorviewpos, &cursorpos);
        cursorviewpos.v[0] -= w2; // Center cursor position
        cursorviewpos.v[1] -= h2;
    }

    // Get the current position of the craft in polar coordinates
    T3DVec3 polarpos = (T3DVec3){{craft->pitchoff, craft->yawoff, craft->distanceoff}};
    T3DVec3 worldpos = gfx_worldpos_from_polar(craft->pitchoff, craft->yawoff, craft->distanceoff);
    t3d_viewport_calc_viewspace_pos(&viewport, &viewpos);
    
    // Calculate the position of the craft relative to the cursor
    float xpos = viewpos.v[0] - cursorviewpos.v[0];
    float ypos = viewpos.v[1] - cursorviewpos.v[1];

    // Adjust input based on AI difficulty
    switch (diff) {
        case DIFF_EASY: {
            md_toavoid = AI_SCREENEDGE_MARGIN_EASY; // Set a big margin from the edges for avoiding the crosshair for easy difficulty
            outheld->z = true; // Shoot asteroids
        } break;
        case DIFF_MEDIUM: {
            md_toavoid = AI_SCREENEDGE_MARGIN_MEDIUM; // Set a smaller margin to avoid the crosshair for medium difficulty
            outpressed->a = true; // Activate bonuses
            outpressed->b = true;
            outheld->z = true; // Shoot asteroids
        } break;
        case DIFF_HARD: {
            md_toavoid = AI_SCREENEDGE_MARGIN_HARD; // Set a small margin to avoid the crosshair for hard difficulty
            outpressed->a = true; // Activate bonuses
            outpressed->b = true;
            outheld->l = true; // Shoot rockets if any
            outheld->r = true;
            outheld->z = true; // Shoot asteroids
        } break;
    }

    // Calculate the borders for avoiding edges
    float borderx = w * md_toavoid;
    float bordery = h * md_toavoid;

    // Check if the craft is within the defined rectangle and is in the halfsphere of the station
    if (gfx_pos_within_rect(xpos, ypos, borderx, bordery, w - borderx, h - bordery) 
        && t3d_vec3_dot(&station.forward, &worldpos) > 0.0f) {
        
        // Adjust the stick input based on the position relative to the cursor (avoid the crosshair)
        outinput->stick_x = fclampr((xpos - w2) * 100, -STICK_MAX_VALUE, STICK_MAX_VALUE);
        outinput->stick_y = fclampr((h2 - ypos) * 100, -STICK_MAX_VALUE, STICK_MAX_VALUE);
        outpressed->c_up = true; // Press C up button if not in the rectangle (hard difficulty) to get close to the station
        outpressed->c_down = false; // Unpress C down button if not in the rectangle (hard difficulty)
    } else {
        // Reset stick input if not within the rectangle and noting else to do
        outinput->stick_x = 0;
        outinput->stick_y = 0;

        // Adjust vertical stick input based on pitch (gradually move towards the center pitch of the scene)
        if (fabs(craft->pitchoff) > 0.25)
            outinput->stick_y = fclampr(craft->pitchoff * 100, -STICK_MAX_VALUE, STICK_MAX_VALUE);

        // If the difficulty is medium or higher, seek bonuses
        if (diff >= DIFF_MEDIUM) {
            float mindist = INFINITY; 
            int bonusindex = -1;

            // Find the closest bonus
            for (int b = 0; b < MAX_BONUSES; b++) {
                if (bonuses[b].enabled) {
                    float dist = t3d_vec3_distance(&bonuses[b].polarpos, &polarpos);
                    if (dist < mindist) {
                        mindist = dist;
                        bonusindex = b; // Update closest bonus index
                    }
                }
            }

            // If a bonus is found, adjust the stick input towards it
            if (bonusindex >= 0) {
                float targpitch = 1000 * (bonuses[bonusindex].polarpos.v[0] - polarpos.v[0]);
                float targyaw = 1000 * (bonuses[bonusindex].polarpos.v[1] - polarpos.v[1]);
                outinput->stick_x = fclampr(targyaw, -STICK_MAX_VALUE, STICK_MAX_VALUE);
                outinput->stick_y = fclampr(-targpitch, -STICK_MAX_VALUE, STICK_MAX_VALUE);
            }
        }

        // If the difficulty is hard, adjust button presses
        if (diff >= DIFF_HARD) {
            outpressed->c_up = false; // Do not press C up button
            outpressed->c_down = true; // Press C down button
        }
    }
}

void crafts_update(){
    // Iterate through each spacecraft
    for(int c = 0; c < NUM_CRAFTS; c++){
        // Check if the spacecraft is alive
        if(crafts[c].enabled){
            // If the spacecraft's health is zero or below, disable it
            if(crafts[c].hp <= 0) {
                crafts[c].enabled = false; // Disable the spacecraft
                // Trigger visual and audio effects for destruction
                effects_add_exp3d(gfx_worldpos_from_polar(crafts[c].pitchoff, crafts[c].yawoff, crafts[c].distanceoff * 25), crafts[c].color);
                effects_add_rumble(crafts[c].currentplayerport, 1.25f);
                effects_add_shake(1.25f);
                effects_add_ambientlight(RGBA32(50,50,25,0));
            }
            // Initialize input structures
            joypad_inputs_t input = {0};
            joypad_buttons_t pressed = {0}, held = {0};
            // Get input from player or AI
            if(!crafts[c].bot){
                pressed = joypad_get_buttons_pressed(crafts[c].currentplayerport);
                input = joypad_get_inputs(crafts[c].currentplayerport);
                held = joypad_get_buttons_held(crafts[c].currentplayerport);
            } else {
                // Get AI input if the spacecraft is controlled by a bot
                crafts_botlogic_getinput(&crafts[c], c, &input, &pressed, &held);
            }
            // Determine speed based on powerup status
            float speed = crafts[c].arm.powerup ? 0.15f : 0.12f;
            // Update yaw and pitch based on input
            crafts[c].yaw     += T3D_DEG_TO_RAD(speed * DELTA_TIME * input.stick_x);
            crafts[c].pitch   -= T3D_DEG_TO_RAD(speed * DELTA_TIME * input.stick_y);
            // Clamp pitch to prevent excessive angles
            crafts[c].pitch = fclampr(crafts[c].pitch, T3D_DEG_TO_RAD(-60), T3D_DEG_TO_RAD(60));
            // Smoothly interpolate real pitch and yaw to the desired onces
            crafts[c].pitchoff = t3d_lerp(crafts[c].pitchoff, crafts[c].pitch, 0.1f);
            crafts[c].yawoff =   t3d_lerp(crafts[c].yawoff, crafts[c].yaw, 0.1f);
            // Calculate world position based on polar coordinates
            T3DVec3 worldpos = gfx_worldpos_from_polar(crafts[c].pitchoff, crafts[c].yawoff, crafts[c].distanceoff * 40);
            // Update transformation matrix for rendering
            t3d_mat4fp_from_srt_euler(crafts[c].matx, 
                (float[3]){1.0f, 1.0f, 1.0f}, // Scale
                (float[3]){0.0f, crafts[c].yawoff - T3D_DEG_TO_RAD(0.4f * input.stick_x), crafts[c].pitchoff + T3D_DEG_TO_RAD(0.4f * input.stick_y)}, // Rotation
                (float[3]){worldpos.v[0], worldpos.v[1], worldpos.v[2]}); // Translation

            // Adjust distance based on input
            if(held.c_down) crafts[c].distance -= 5.0 * DELTA_TIME; // Move closer
            if(held.c_up) crafts[c].distance += 5.0 * DELTA_TIME; // Move away
            // Clamp distance to a minimum and maximum value
            crafts[c].distance = fclampr(crafts[c].distance, 30, 90);
            // Smoothly interpolate distance offset
            crafts[c].distanceoff = t3d_lerp(crafts[c].distanceoff, crafts[c].distance, 0.2f);

            // Handle shield activation
            if((pressed.a && crafts[c].arm.shield == 10.0f)){
                crafts[c].arm.shield -= DELTA_TIME; // Decrease shield over time
                wav64_play(&sounds[snd_use_shield], SFX_CHANNEL_BONUS); // Play shield sound
                effects_add_ambientlight(RGBA32(0,0,100,0)); // Add ambient light effect
            }
            // Gradually decrease shield if it's active
            if(crafts[c].arm.shield < 10.0f) crafts[c].arm.shield -= DELTA_TIME;
            crafts[c].arm.shield = fclampr(crafts[c].arm.shield, 0.0f, 10.0f); // Clamp shield value

            // Handle powerup activation
            if((pressed.b && crafts[c].arm.powerup == 10.0f)){
                crafts[c].arm.powerup -= DELTA_TIME; // Decrease powerup over time
                wav64_play(&sounds[snd_use_powerup], SFX_CHANNEL_BONUS); // Play powerup sound
                effects_add_ambientlight(RGBA32(0,100,0,0)); // Add ambient light effect
            }
            // Gradually decrease powerup if it's active
            if(crafts[c].arm.powerup < 10.0f) crafts[c].arm.powerup -= DELTA_TIME;
            crafts[c].arm.powerup = fclampr(crafts[c].arm.powerup, 0.0f, 10.0f); // Clamp powerup value

            // Handle asteroid firing logic
            if(held.z && CURRENT_TIME >= crafts[c].arm.asteroidnexttime && !gamestatus.paused){
                int b = 0; 
                // Find the next available asteroid slot
                while(crafts[c].arm.asteroids[b].enabled && b < MAX_PROJECTILES - 1) b++;
                crafts[c].arm.asteroids[b].enabled = true; // Enable the asteroid
                crafts[c].arm.asteroids[b].polarpos = (T3DVec3){{crafts[c].pitchoff, crafts[c].yawoff, crafts[c].distanceoff}}; // Set position
                crafts[c].arm.asteroids[b].rotation = frandr(0, 360); // Random rotation
                crafts[c].arm.asteroids[b].xspeed = frandr(-0.025, 0.025); // Random x speed
                crafts[c].arm.asteroids[b].yspeed = frandr(-0.025, 0.025); // Random y speed
                // Adjust speed based on directional input
                if(held.d_up) crafts[c].arm.asteroids[b].yspeed += 0.03;
                if(held.d_down) crafts[c].arm.asteroids[b].yspeed -= 0.03;
                if(held.d_right) crafts[c].arm.asteroids[b].xspeed += 0.03;
                if(held.d_left) crafts[c].arm.asteroids[b].xspeed -= 0.03;
                crafts[c].arm.asteroids[b].hp = randr(5, 25); // Set random health for the asteroid
                crafts[c].arm.asteroidnexttime = CURRENT_TIME + 12.0f; // Set next firing time
                wav64_play(&sounds[snd_hit], SFX_CHANNEL_EFFECTS); // Play spawn sound
                effects_add_ambientlight(RGBA32(50,50,50,0)); // Add ambient light effect
            }

            // Handle rocket firing logic
            if((held.l || held.r) && CURRENT_TIME >= crafts[c].arm.rocketnexttime && crafts[c].arm.rocketcount > 0){
                int b = 0; 
                // Find the next available rocket slot
                while(crafts[c].arm.rockets[b].enabled && b < MAX_PROJECTILES - 1) b++;
                crafts[c].arm.rockets[b].enabled = true; // Enable the rocket
                crafts[c].arm.rockets[b].polarpos = (T3DVec3){{crafts[c].pitchoff, crafts[c].yawoff, crafts[c].distanceoff}}; // Set position
                crafts[c].arm.rocketnexttime = CURRENT_TIME + 1.0f; // Set next firing time
                crafts[c].arm.rockets[b].hp = 5; // Set health for the rocket
                crafts[c].arm.rocketcount--; // Decrease rocket count
                wav64_play(&sounds[snd_shoot_rocket], SFX_CHANNEL_ROCKET); // Play rocket sound
                effects_add_rumble(crafts[c].currentplayerport, 0.45f); // Add rumble effect
                effects_add_ambientlight(RGBA32(50,50,25,0)); // Add ambient light effect
            }

            for(int b = 0; b < MAX_BONUSES; b++){
                // Check if the bonus is enabled
                if(bonuses[b].enabled){
                    // Calculate the world position of the bonus using its polar coordinates
                    T3DVec3 ast_worldpos = gfx_worldpos_from_polar(
                        bonuses[b].polarpos.v[0],
                        bonuses[b].polarpos.v[1],
                        bonuses[b].polarpos.v[2]);
                    
                    // Calculate the world position of the spacecraft
                    T3DVec3 encraft_worldpos = gfx_worldpos_from_polar(
                        crafts[c].pitchoff,
                        crafts[c].yawoff,
                        bonuses[b].polarpos.v[2]);
                    
                    // Calculate the distance between the bonus and the spacecraft
                    float dist = t3d_vec3_distance(&ast_worldpos, &encraft_worldpos);
                    
                    // If the distance is less than 5.0, apply the bonus to the spacecraft
                    if(dist < 5.0f)
                        bonus_apply(b, crafts[c].currentplayer, NULL, c);
                }
            }
        }
    }
}

// Update projectiles (asteroids and rockets) for the current spacecraft
for(int b = 0; b < MAX_PROJECTILES; b++){
    // Update asteroids
    if(crafts[c].arm.asteroids[b].enabled){
        // Move the asteroid based on its speed
        crafts[c].arm.asteroids[b].polarpos.v[0] += DELTA_TIME * crafts[c].arm.asteroids[b].xspeed;
        crafts[c].arm.asteroids[b].polarpos.v[1] += DELTA_TIME * crafts[c].arm.asteroids[b].yspeed;
        crafts[c].arm.asteroids[b].polarpos.v[2] -= DELTA_TIME * 3.0f; // Move towards the station
        
        // Update the visual rotation of the asteroid
        crafts[c].arm.asteroids[b].rotation += DELTA_TIME;
        
        // Check if the asteroid's health is depleted
        if(crafts[c].arm.asteroids[b].hp <= 0) {
            crafts[c].arm.asteroids[b].enabled = false; // Disable the asteroid
            // Create an explosion effect at the asteroid's position
            effects_add_exp3d(gfx_worldpos_from_polar(
                crafts[c].arm.asteroids[b].polarpos.v[0],
                crafts[c].arm.asteroids[b].polarpos.v[1],
                crafts[c].arm.asteroids[b].polarpos.v[2] * 25), 
                RGBA32(255,255,255,255));
            effects_add_rumble(station.currentplayerport, 0.45f);
            effects_add_shake(0.75f);
            effects_add_ambientlight(RGBA32(50,50,25,0));
        }
        
        // Check if the asteroid has reached below a certain distance
        if(crafts[c].arm.asteroids[b].polarpos.v[2] < 2.0f){
            crafts[c].arm.asteroids[b].enabled = false; // Disable the asteroid
            // Create an explosion effect at the asteroid's position
            effects_add_exp3d(gfx_worldpos_from_polar(
                crafts[c].arm.asteroids[b].polarpos.v[0],
                crafts[c].arm.asteroids[b].polarpos.v[1],
                crafts[c].arm.asteroids[b].polarpos.v[2] * 25), 
                RGBA32(255,255,255,255));
            effects_add_rumble(station.currentplayerport, 0.75f);
            effects_add_shake(1.00f);
            effects_add_ambientlight(RGBA32(50,50,25,0));
            
            // Apply damage to the station if the shield is not active
            float damage = 50;
            if(!(station.arm.shield > 0.0f && station.arm.shield < 10.0f)){
                station.hp -= damage; // Reduce station health
                gamestatus.playerscores[crafts[c].currentplayer] += damage * 40; // Update player score
            }
        }
    }
    
    // Update rockets
    if(crafts[c].arm.rockets[b].enabled){
        // Move the rocket towards the station
        crafts[c].arm.rockets[b].polarpos.v[2] -= DELTA_TIME * 7.0f;
        
        // Check if the rocket's health is depleted
        if(crafts[c].arm.rockets[b].hp <= 0) {
            crafts[c].arm.rockets[b].enabled = false; // Disable the rocket
            // Create an explosion effect at the rocket's position
            effects_add_exp3d(gfx_worldpos_from_polar(
                crafts[c].arm.rockets[b].polarpos.v[0],
                crafts[c].arm.rockets[b].polarpos.v[1],
                crafts[c].arm.rockets[b].polarpos.v[2] * 25), 
                RGBA32(255,255,255,255));
            effects_add_rumble(station.currentplayerport, 0.45f);
            effects_add_ambientlight(RGBA32(25,25,5,0));
        }
        
        // Check if the rocket has reached below a certain distance
        if(crafts[c].arm.rockets[b].polarpos.v[2] < 2.0f){
            crafts[c].arm.rockets[b].enabled = false; // Disable the rocket
            // Create an explosion effect at the rocket's position
            effects_add_exp3d(gfx_worldpos_from_polar(
                crafts[c].arm.rockets[b].polarpos.v[0],
                crafts[c].arm.rockets[b].polarpos.v[1],
                crafts[c].arm.rockets[b].polarpos.v[2] * 25), 
                RGBA32(255,255,255,255));
            effects_add_rumble(station.currentplayerport, 0.65f);
            effects_add_shake(1.00f);
            effects_add_ambientlight(RGBA32(50,50,25,0));
            
            // Apply damage to the station if the shield is not active
            float damage = 65;
            if(!(station.arm.shield > 0.0f && station.arm.shield < 10.0f)){
                station.hp -= damage; // Reduce station health
                gamestatus.playerscores[crafts[c].currentplayer] += damage * 40; // Update player score
            }
        }
    }
}

void crafts_draw(){
    // Loop through all crafts
    for(int c = 0; c < NUM_CRAFTS; c++){
        // Check if the craft is enabled
        if(crafts[c].enabled){
            // Push the current transformation matrix for the craft
            t3d_matrix_push(crafts[c].matx);

            // Create an iterator for the enemy craft model
            T3DModelIter it = t3d_model_iter_create(models[ENEMYCRAFT], T3D_CHUNK_TYPE_OBJECT);
            
            // Set the environment color based on the craft's shield status
            if(crafts[c].arm.shield > 0.0f && crafts[c].arm.shield < 10.0f) 
                rdpq_set_env_color(RGBA32(0x80,0x80,0xB0,0xFF)); // Light blue for low shield
            else 
                rdpq_set_env_color(RGBA32(0x00,0x00,0x00,0xFF)); // Black for no shield
            
            // Iterate through the model objects
            while(t3d_model_iter_next(&it)){
                // Check if the object has a material
                if(it.object->material) {
                    // Set the color combiner and blend mode for the material
                    rdpq_mode_combiner(it.object->material->colorCombiner);
                    rdpq_mode_blender(it.object->material->blendMode);

                    // Disable fog effects
                    t3d_fog_set_enabled(false);
                    // Disable depth buffer for rendering
                    rdpq_mode_zbuf(false, false);
                    
                    // Get the texture associated with the material
                    T3DMaterialTexture *tex = &(it.object->material->textureA);
                    // Check if the texture path or reference is valid
                    if(tex->texPath || tex->texReference){
                        // Load the texture if it hasn't been loaded yet
                        if(tex->texPath && !tex->texture) 
                            tex->texture = sprite_load(tex->texPath);

                        // Set texture parameters
                        rdpq_texparms_t texParam = (rdpq_texparms_t){};
                        texParam.s.mirror = tex->s.mirror;
                        texParam.s.repeats = tex->s.clamp ? 1 : REPEAT_INFINITE;
                        texParam.s.scale_log = 1;

                        texParam.t.mirror = tex->t.mirror;
                        texParam.t.repeats = tex->t.clamp ? 1 : REPEAT_INFINITE;
                        texParam.t.scale_log = 1;

                        // Upload the texture to the graphics pipeline
                        rdpq_sprite_upload(TILE0, tex->texture, &texParam);
                    }
                }
                // Enable antialiasing for smoother edges
                rdpq_mode_antialias(AA_STANDARD);
                // Set the primary color for the craft
                rdpq_set_prim_color(crafts[c].color);
                // Synchronize the graphics pipeline to avoid crashes
                rdpq_sync_pipe(); 
                rdpq_sync_tile(); 
                // Draw the current object in the model
                t3d_model_draw_object(it.object, NULL);
                // Synchronize again after drawing
                rdpq_sync_pipe(); 
                rdpq_sync_tile(); 
            }
            // Pop the transformation matrix for the craft
            t3d_matrix_pop(1);
        }
    }

    // Set ambient light color for the rockets
    color_t amb = RGBA32(0xA0, 0xA0, 0xA0, 0xFF);
    t3d_light_set_ambient((uint8_t*)&amb); // Apply ambient light
    t3d_light_set_directional(0, (uint8_t*)&world.sun.color, &world.sun.direction); // Set directional light from the sun
    t3d_light_set_count(1); // Set the number of lights to 1

    // Retrieve the material and object for the rocket model
    T3DMaterial* mat = t3d_model_get_material(models[ROCKET], "f3d.rocket");
    T3DObject* obj = t3d_model_get_object_by_index(models[ROCKET], 0);
    t3d_model_draw_material(mat, NULL); // Draw the rocket material

    // Loop through all crafts to draw their projectiles
    for(int c = 0; c < NUM_CRAFTS; c++)
        for(int b = 0; b < MAX_PROJECTILES; b++){
            // Check if the rocket is enabled
            if(crafts[c].arm.rockets[b].enabled){
                // Calculate the world position of the rocket using polar coordinates
                T3DVec3 worldpos = gfx_worldpos_from_polar(
                    crafts[c].arm.rockets[b].polarpos.v[0], 
                    crafts[c].arm.rockets[b].polarpos.v[1], 
                    crafts[c].arm.rockets[b].polarpos.v[2] * 50.0f);
                
                T3DMat4 rocketmat;
                // Create a transformation matrix for the rocket
                t3d_mat4_from_srt_euler(&rocketmat,
                (float[3]){1.0f, 1.0f, 1.0f}, // Scale
                (float[3]){0.0f, crafts[c].arm.rockets[b].polarpos.v[1], crafts[c].arm.rockets[b].polarpos.v[0] + T3D_DEG_TO_RAD(180.0f)}, // Rotation
                (float[3]){worldpos.v[0], worldpos.v[1], worldpos.v[2]}); // Translation
                
                // Store the transformation matrix in the rocket's matrix
                t3d_mat4_to_fixed(crafts[c].arm.rockets[b].matx, &rocketmat);
                t3d_matrix_push(crafts[c].arm.rockets[b].matx); // Push the matrix onto the stack
                
                rdpq_mode_antialias(AA_STANDARD); // Enable antialiasing
                
                // Check if the display list for the rocket is not created
                if(!crafts[c].arm.rocketdl){
                    rspq_block_begin(); // Begin a new display list block
                    rdpq_sync_pipe(); // Synchronize the rendering pipeline
                    rdpq_sync_tile(); // Synchronize tile data
                    
                    // Draw the rocket object
                    t3d_model_draw_object(obj, NULL);
                    
                    rdpq_sync_pipe(); // Synchronize the rendering pipeline
                    rdpq_sync_tile(); // Synchronize tile data
                    
                    crafts[c].arm.rocketdl = rspq_block_end(); // End the display list block and store it
                } else {
                    rspq_block_run(crafts[c].arm.rocketdl); // Run the existing display list
                }
                t3d_matrix_pop(1); // Pop the matrix from the stack
                rdpq_sync_pipe(); // Synchronize the rendering pipeline
                rdpq_sync_tile(); // Synchronize tile data
            }
        }

    // Set ambient light color based on the world's sun ambient light
    t3d_light_set_ambient((uint8_t*)&world.sun.ambient);

    // Retrieve manually the material and object for the asteroid model
    // because something always goes wrong when drawing it from the fast3d settings
    mat = t3d_model_get_material(models[ASTEROID], "f3d.rock");
    obj = t3d_model_get_object_by_index(models[ASTEROID], 0);
    t3d_model_draw_material(mat, NULL); // Draw the asteroid material

    // Loop through all crafts to draw their asteroids
    for(int c = 0; c < NUM_CRAFTS; c++)
        for(int b = 0; b < MAX_PROJECTILES; b++){
            // Check if the asteroid is enabled
            if(crafts[c].arm.asteroids[b].enabled){
                // Calculate the world position of the asteroid using polar coordinates
                T3DVec3 worldpos = gfx_worldpos_from_polar(
                    crafts[c].arm.asteroids[b].polarpos.v[0], 
                    crafts[c].arm.asteroids[b].polarpos.v[1], 
                    crafts[c].arm.asteroids[b].polarpos.v[2] * 50.0f);
                
                T3DMat4 mat;
                // Create a transformation matrix for the asteroid
                t3d_mat4_from_srt_euler(&mat,
                (float[3]){1.0f, 1.0f, 1.0f}, // Scale
                (float[3]){0.0f, crafts[c].arm.asteroids[b].rotation, crafts[c].arm.asteroids[b].rotation}, // Rotation
                (float[3]){worldpos.v[0], worldpos.v[1], worldpos.v[2]}); // Translation
                
                // Store the transformation matrix in the asteroid's matrix
                t3d_mat4_to_fixed(crafts[c].arm.asteroids[b].matx, &mat);
                t3d_matrix_push(crafts[c].arm.asteroids[b].matx); // Push the matrix onto the stack
                
                rdpq_mode_antialias(AA_STANDARD); // Enable antialiasing specifically once again
                
                // Check if the display list for the asteroid is not created
                if(!crafts[c].arm.asteroiddl){
                    rspq_block_begin(); // Begin a new display list block
                    rdpq_sync_pipe(); // Synchronize the rendering pipeline
                    rdpq_sync_tile(); // Synchronize tile data
                    
                    // Draw the asteroid object
                    t3d_model_draw_object(obj, NULL);
                    
                    rdpq_sync_pipe(); // Synchronize the rendering pipeline
                    rdpq_sync_tile(); // Synchronize tile data
                    
                    crafts[c].arm.asteroiddl = rspq_block_end(); // End the display list block and store it
                } else {
                    rspq_block_run(crafts[c].arm.asteroiddl); // Run the existing display list
                }
                t3d_matrix_pop(1); // Pop the matrix from the stack
                rdpq_sync_pipe(); // Synchronize the rendering pipeline
                rdpq_sync_tile(); // Synchronize tile data
            }
        }
}


void crafts_close(){
    for(int c = 0; c < NUM_CRAFTS; c++){
        if(crafts[c].matx) free_uncached(crafts[c].matx);
        for(int b = 0; b < MAX_PROJECTILES; b++){
            if(crafts[c].arm.rockets[b].matx) free_uncached(crafts[c].arm.rockets[b].matx);
            if(crafts[c].arm.asteroids[b].matx) free_uncached(crafts[c].arm.asteroids[b].matx);
        }
    }

}