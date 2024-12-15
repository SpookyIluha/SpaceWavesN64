#include <unistd.h>
#include <time.h>
#include <libdragon.h>
#include <display.h>
#include <t3d/t3d.h>
#include <t3d/t3dmath.h>
#include <t3d/t3dmodel.h>
#include "../../core.h"
#include "../../minigame.h"
#include "world.h"
#include "gfx.h"
#include "station.h"
#include "enemycraft.h"
#include "spacewaves.h"
#include "scoreui.h"
#include "bonus.h"

/*********************************
             Globals
*********************************/

// You need this function defined somewhere in your project
// so that the minigame manager can work
const MinigameDef minigame_def = {
    .gamename = "SpaceWaves",
    .developername = "Spooky Iluha",
    .description = "A station-defense minigame set in various galaxies. Defend yourself from other player's spacecrafts.",
    .instructions = "Z, L/R to fire, A,B to activate bonuses. Hit to win points! Each player will get a round in a station."
};

// the main loop of the game
int main()
{
  // init all the systems
  main_init();

  T3DMat4 modelMat; // global transformation matrix for our scene, this is a "normal" float matrix
  t3d_mat4_identity(&modelMat);

  // Now allocate a fixed-point matrix, this is what t3d uses internally.
  // Note: this gets DMA'd to the RSP, so it needs to be uncached.
  // If you can't allocate uncached memory, remember to flush the cache after writing to it instead.
  float rotation = 0.0f; // additional rotation of the scene
  PlyNum playerturn = PLAYER_1; // by default player 1's turn is first
  int countdownseconds = 0; // float->integer countdown in seconds for the audio beeps
  
  while(true){
    mixer_try_play();
    joypad_poll();
    // ======== Update ======== //
    float modelScale = 0.1f; // scale the entire scene 10x for collision calculations
    float shakemax = (effects.screenshaketime * 1.5f); // shake amplitude, based on the current effect's shake timer
    if(shakemax <= 0.0f) shakemax = 0.0001f; // to midigate div by 0 error

    // to preserve colors better and to achieve 24bpp output instead of the usual 21 switch the dithering pattern each frame (temporal dithering)
    if(dither == DITHER_SQUARE_INVSQUARE)
      dither = DITHER_BAYER_BAYER;
    else dither = DITHER_SQUARE_INVSQUARE;

    // 75 FOV, range from 1 meter to 220 meters
    t3d_viewport_set_projection(&viewport, T3D_DEG_TO_RAD(75.0f), 1.0f, 220.0f);

    // slowly rotate model on start up screen and shake if appropriate
    t3d_mat4_from_srt_euler(&modelMat,
      (float[3]){modelScale, modelScale, modelScale},
      (float[3]){rotation * 0.2f, rotation, rotation},
      (float[3]){frandr(-shakemax,shakemax),frandr(-shakemax,shakemax),frandr(-shakemax,shakemax)});
    t3d_mat4_to_fixed(modelMatFP, &modelMat);

    // ======== Draw ======== //
    framebuffer = display_get();
    rdpq_attach(framebuffer, NULL);
    t3d_frame_start();
    // when playing update the entities each frame
    if(gamestatus.state == GAMESTATE_PLAY || gamestatus.state == GAMESTATE_COUNTDOWN){
        station_update();
        mixer_try_play();
        crafts_update();
        bonus_update();
        effects_update();
    }
    // apply main viewport's camera to t3d's pipeline
    station_apply_camera();

    // No depth buffer is ever used, the scene is depth ordered manually
    //t3d_screen_clear_color(RGBA32(100, 80, 80, 0xFF));
    //t3d_screen_clear_depth();
    t3d_matrix_push(modelMatFP);
    mixer_try_play();
    // draw all entities and ui in order
    world_draw();
    if(gamestatus.state == GAMESTATE_PLAY || gamestatus.state == GAMESTATE_PAUSED || gamestatus.state == GAMESTATE_COUNTDOWN){
      bonus_draw();
      crafts_draw();
      effects_draw();
      station_draw();
    }
    world_draw_lensflare();
    userinterface_draw();
    t3d_matrix_pop(1);
    
    timesys_update();
    // poll the buttons players pressed
    for(int c = 0; c < MAXPLAYERS; c++){
      joypad_buttons_t btn = joypad_get_buttons_pressed((joypad_port_t)c);
      // check if any player pressed the start button and pause/unpause the game
      if(btn.start && (gamestatus.state == GAMESTATE_PLAY || gamestatus.state == GAMESTATE_PAUSED)){
        gamestatus.paused = !gamestatus.paused;
        gamestatus.state = gamestatus.paused? GAMESTATE_PAUSED : GAMESTATE_PLAY;
        wav64_play(&sounds[snd_button_click2], SFX_CHANNEL_EFFECTS);
      }
      // check if any player pressed the start button and start the round
      if(btn.start && gamestatus.state == GAMESTATE_GETREADY){
        gamestatus.paused = true;
        gamestatus.state = GAMESTATE_COUNTDOWN;
        gamestatus.statetime = 5.0f;
        currentmusicindex = rand() % MUSIC_COUNT;
        if(!xmplayeropen)
          xm64player_open(&xmplayer, music_path[currentmusicindex]);
        xm64player_play(&xmplayer, SFX_CHANNEL_MUSIC);
        xm64player_set_loop(&xmplayer, true);
        wav64_play(&sounds[snd_button_click1], SFX_CHANNEL_EFFECTS);
        xmplayeropen = true;
      }
    }
    // when countdown reaches 0 start the actual game
    if(gamestatus.state == GAMESTATE_COUNTDOWN && gamestatus.statetime <= 0.0f){
      gamestatus.state = GAMESTATE_PLAY;
      gamestatus.paused = false;
      gamestatus.statetime = 180.0f;
      wav64_play(&sounds[snd_button_click3], SFX_CHANNEL_BONUS);
    }
    // while countdown play a beep each second
    if(gamestatus.state == GAMESTATE_COUNTDOWN){
      int newcountseconds = (int)gamestatus.statetime;
      if (newcountseconds != countdownseconds){
          wav64_play(&sounds[snd_button_click3], SFX_CHANNEL_BONUS);
          countdownseconds = newcountseconds;
      }
    }
    // slowly rotate the scene while in the round transition
    if(gamestatus.state == GAMESTATE_GETREADY || gamestatus.state == GAMESTATE_TRANSITION)
      rotation += DELTA_TIME_REAL * 0.07;
    else rotation = 0.0f;

    // constantly check the game rules to see if victory is achiever or the time is up
    if(gamestatus.state == GAMESTATE_PLAY){
      bool craftsalive = false;
      for(int i = 0; i < 3; i++) if(crafts[i].enabled) craftsalive = true;
      // one team is dead, stop the game's effects and count the score
      if((gamestatus.statetime <= 0.0f || !craftsalive || station.hp <= 0.0f)){
        uint32_t count = core_get_playercount();
        playerturn++;
        effects.screenshaketime = 0;
        for(int i = 0; i < MAXPLAYERS; i++) effects.rumbletime[i] = 0;
        effects_update();
        // if there are still players turns left do the transition to the next round
        if(playerturn < count){
            gamestatus.state = GAMESTATE_TRANSITION;
            gamestatus.paused = false;
            gamestatus.statetime = 10.0f;
        } else { // if this was the last player's turn count the scores and set the winner (or tie)
            int maxscore = 0;
            int secondmaxscore = 0;
            for(int pl = 0; pl < MAXPLAYERS; pl++)
            if(gamestatus.playerscores[pl] > maxscore) {
                secondmaxscore = maxscore;
                maxscore = gamestatus.playerscores[pl];
                gamestatus.winner = pl;
            }
            // set the winner and finish the game
            if(maxscore == secondmaxscore) gamestatus.winner = -1;
            gamestatus.state = GAMESTATE_FINISHED;
            gamestatus.paused = false;
            gamestatus.statetime = 10.0f;
        }
      }
    }
    // reinit all the entities when transitioning between rounds (new galaxy and players)
    if(gamestatus.state == GAMESTATE_TRANSITION && gamestatus.statetime <= 0.0f){
      gamestatus.state = GAMESTATE_GETREADY;
      gamestatus.paused = false;
      if(xmplayeropen) {
        xm64player_close(&xmplayer);
        xmplayeropen = false;
      }
      rspq_wait();
      world_reinit();
      station_close();
      station_init((PlyNum)playerturn);
      crafts_close();
      crafts_init((PlyNum)playerturn);
      bonus_close();
      bonus_init();
    }
    // when the game is finished after the transition, end the minigame entirely
    if(gamestatus.state == GAMESTATE_FINISHED && gamestatus.statetime <= 0.0f){
      if(xmplayeropen) {
        xm64player_close(&xmplayer);
        xmplayeropen = false;
      }
      rspq_wait();
      core_set_winner(gamestatus.winner);
      goto end;
    }

    rdpq_detach_show();
    mixer_try_play();
  }

  end:
    rdpq_detach_show();
    minigame_end();
    return 0;
}