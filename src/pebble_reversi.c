#include <pebble.h>
#include <stdio.h>
#include <stdlib.h>
#include "util.h"
#include "ai.h"
#include "game.h"

#ifdef PBL_SDK_3
//Status bar support for SDK 3
static StatusBarLayer *s_status_bar;
static const int TOP_BAR_OFFSET = 16;
#else
static const int TOP_BAR_OFFSET = 0;
#endif

//Settings Windows
static Window *settings_window;
static SimpleMenuLayer* settings_menu_layer;
static SimpleMenuSection settings_menu_section_array[1];
static SimpleMenuItem settings_menu_item_array [5];

//Debug Variables
static const bool SPECIAL_SCREENSHOT_MODE = false;

//AI Window
static Window *ai_settings_window;
static SimpleMenuLayer* ai_settings_menu_layer;
static SimpleMenuSection ai_settings_menu_section_array[1];
static SimpleMenuItem ai_settings_menu_item_array [4];

//Player Count Window
static Window *pc_settings_window;
static SimpleMenuLayer* pc_settings_menu_layer;
static SimpleMenuSection pc_settings_menu_section_array[1];
static SimpleMenuItem pc_settings_menu_item_array [3];

//Game Window
static Window *window;
static TextLayer *text_layer;
static TextLayer *black_score_layer;
static TextLayer *white_score_layer;
static Layer *s_canvas_layer;

//Animation state stuff
#define ANIM_FRAME_SPEED 50
static char g_old_board[BOARD_WIDTH*BOARD_HEIGHT];
static char g_anim_board[BOARD_WIDTH*BOARD_HEIGHT];
static int anim_state = 0;
static int anim_center_x = 0;
static int anim_center_y = 0;
static bool anim_frames = false;
static int anim_frame = 0;
static GBitmap *flip_white[4];
static int flip_frame_count = 4;
static bool play_animations = true;

static AppTimer *anim_timer;
static AppTimer *ai_timer;

//Game State: Stuff to serialize
static char g_board[BOARD_WIDTH*BOARD_HEIGHT];
static char g_current_game_state = GAME_OVER;
static int g_player_count = 1;
static int g_current_player = 0; //Black is 0, White is 1.
static int ai_strength = 1;
static bool g_grid_display = false;

//Stuff that'll get reconstructed once the game is restored.
static int g_selected_square = 0; //An index into g_selectable_array
static int g_selectable_array[BOARD_WIDTH*BOARD_HEIGHT];
static int g_selectable_count = 0; //Number of selctable elements in g_selectable_array

//Current Board Score
static int g_white_score = 0;
static int g_black_score = 0;
static char *g_black_score_string = "00"; //Different because otherwise the compiler sets the two strings to the same address, amazing.
static char *g_white_score_string = "02";

//AI Strength.  
//0= Random selection from available moves
static bool ai_thinking = false;
//static int ai_boards_in_memory = 0; // Safeguard against OOMing.


//Because I'm too lazy to make a header for this file.
static void animate_move();
static void make_ai_move();
static void update_score_display();
static void set_players_turn_display();
static void set_ai_thinking_display();
static void restore_game_state();
static void set_settings_menu_grid_item();


static int get_current_selectable_index()
{
  return g_selectable_array[g_selected_square];
}

static void inc_selectable_index()
{
  if((g_selected_square + 1) >= g_selectable_count)
  {
    g_selected_square = 0;
  }
  else
  {
    g_selected_square++;
  }
}

static void dec_selectable_index()
{
  if((g_selected_square - 1) < 0)
  {
    g_selected_square = max((g_selectable_count-1), 0);//In case this is a decrement when there are no selectables, which should never happen.
  }
  else
  {
    g_selected_square--;
  }
}

static void clear_selectable_array()
{
  for(int i = 0; i < BOARD_WIDTH*BOARD_HEIGHT; i++)
  {
    //clear all 
    g_selectable_array[i] = -1;
  }
  g_selectable_count = 0;
  g_selected_square = 0;
}

static void append_selectable_index(int index)
{
  g_selectable_array[g_selectable_count] = index;
  g_selectable_count++;
}

static void generate_selectables_array()
{
  clear_selectable_array();
  for(int i = 0; i < BOARD_WIDTH*BOARD_HEIGHT; i++)
  {
    //clear all 
    if(g_board[i] == SELECTABLE)
    {
      append_selectable_index(i);
    }
  }
}

static void set_board_to_new()
{
  for(int i = 0; i < BOARD_WIDTH*BOARD_HEIGHT; i++)
  {
    //clear all 
    g_board[i] = EMPTY;
  }
  g_board[3 + (3*BOARD_WIDTH)] = WHITE;
  g_board[3 + (4*BOARD_WIDTH)] = BLACK;
  g_board[4 + (3*BOARD_WIDTH)] = BLACK;
  g_board[4 + (4*BOARD_WIDTH)] = WHITE;
}

static void set_initial_player()
{
  g_current_player = 0;
}

static void reset_game()
{
  set_initial_player();
  set_board_to_new();
  set_board_selectables_and_score(g_board, &g_black_score, &g_white_score, g_current_player);
  generate_selectables_array();
  g_selected_square = 0;
  if(g_player_count > 0)
  {
    g_current_game_state = BLACK_PLAYER_SELECTING;
    set_players_turn_display();
  }
  else
  {
    g_current_game_state = AI_THINKING;
    set_ai_thinking_display();
    make_ai_move();
  }
  update_score_display();
}


static int get_play_frame_from_anim_frame(int cur_frame){
  if(g_current_player == 1)
  {
    return cur_frame;
  }
  else
  {
    return flip_frame_count-(cur_frame+1);
  }
}

static void write_board_to_layer(Layer *this_layer, GContext *ctx)
{
  const int TOP_OFFSET = 22 + TOP_BAR_OFFSET;
  const int LEFT_OFFSET = 8;
  const int CIRCLE_SIZE = 16;
  graphics_context_set_fill_color(ctx, GColorBlack);
  char *active_board;
  bool animating = false;
  if(g_current_game_state == ANIMATION_PLAYING)
  {
    graphics_context_set_compositing_mode(ctx, GCompOpAnd);
    active_board = g_anim_board;
    animating = true;
  }
  else
  {
    active_board = g_board;
  }
  //draw grid
  if(g_grid_display)
  {
    //Render the board first, behind the play area
    int board_total_width = BOARD_WIDTH * CIRCLE_SIZE;
    for(int i = 0; i <= BOARD_WIDTH; i++)
    {
      int cur_x = (i * CIRCLE_SIZE) + LEFT_OFFSET;
      GPoint origin = GPoint(cur_x, TOP_OFFSET);
      GPoint dest = GPoint(cur_x, TOP_OFFSET+board_total_width);
      graphics_draw_line(ctx, origin, dest);
    }
    for(int i = 0; i <= BOARD_WIDTH; i++)
    {
      int cur_y = (i * CIRCLE_SIZE) + TOP_OFFSET;
      GPoint origin = GPoint(LEFT_OFFSET, cur_y);
      GPoint dest = GPoint(LEFT_OFFSET+board_total_width, cur_y);
      graphics_draw_line(ctx, origin, dest);
    }
  }
  for(int i = 0; i < BOARD_WIDTH; i++)
  {
    for(int j = 0; j < BOARD_HEIGHT; j++)
    {
      int index = i + (j*BOARD_WIDTH);
      if(get_board_value(index, active_board) == WHITE)
      {
        //graphics_draw_circle(ctx, GPoint((i*CIRCLE_SIZE)+CIRCLE_SIZE/2 + LEFT_OFFSET -1, (j*CIRCLE_SIZE)+CIRCLE_SIZE/2 + TOP_OFFSET -1), CIRCLE_SIZE/2);
        #ifdef PBL_COLOR
          graphics_context_set_fill_color(ctx, GColorWhite);
          graphics_fill_rect(ctx, GRect(i*CIRCLE_SIZE + LEFT_OFFSET, j*CIRCLE_SIZE + TOP_OFFSET, CIRCLE_SIZE, CIRCLE_SIZE), 8, GCornersAll);
          graphics_context_set_fill_color(ctx, GColorBlack);
        #else
          graphics_draw_round_rect(ctx, GRect(i*CIRCLE_SIZE + LEFT_OFFSET, j*CIRCLE_SIZE + TOP_OFFSET, CIRCLE_SIZE, CIRCLE_SIZE), 8);
        #endif
      }
      else if(get_board_value(index, active_board) == BLACK)
      {
        graphics_fill_rect(ctx, GRect(i*CIRCLE_SIZE + LEFT_OFFSET, j*CIRCLE_SIZE + TOP_OFFSET, CIRCLE_SIZE, CIRCLE_SIZE), 8, GCornersAll);
      }
      else if(get_board_value(index, active_board) == ANIMATING && animating)
      {
        #ifdef PBL_COLOR
        graphics_context_set_compositing_mode(ctx, GCompOpSet);
        #else
        graphics_context_set_compositing_mode(ctx, GCompOpAnd);
        #endif
        graphics_draw_bitmap_in_rect(ctx, flip_white[get_play_frame_from_anim_frame(anim_frame)], (GRect) { .origin = { i*CIRCLE_SIZE + LEFT_OFFSET, j*CIRCLE_SIZE + TOP_OFFSET }, .size = {17,17} });
        graphics_context_set_compositing_mode(ctx, GCompOpAnd);
      }
      else if(get_board_value(index, active_board) == SELECTABLE && !animating)//Don't render selectables while animating.
      {
        if(index == get_current_selectable_index())
        {
          if(get_player_char(g_current_player) == WHITE)
          {
            //graphics_draw_circle(ctx, GPoint((i*CIRCLE_SIZE)+CIRCLE_SIZE/2 +LEFT_OFFSET -1, (j*CIRCLE_SIZE)+CIRCLE_SIZE/2 + TOP_OFFSET -1), CIRCLE_SIZE/2);
            graphics_draw_round_rect(ctx, GRect(i*CIRCLE_SIZE + LEFT_OFFSET, j*CIRCLE_SIZE + TOP_OFFSET, CIRCLE_SIZE, CIRCLE_SIZE), 8);
            graphics_fill_rect(ctx, GRect(i*CIRCLE_SIZE+6 +LEFT_OFFSET, j*CIRCLE_SIZE+6 + TOP_OFFSET, 4, 4), 0, GCornersAll);
          }
          else
          {
            graphics_fill_rect(ctx, GRect(i*CIRCLE_SIZE + LEFT_OFFSET, j*CIRCLE_SIZE + TOP_OFFSET, CIRCLE_SIZE, CIRCLE_SIZE), 7, GCornersAll);
            graphics_context_set_fill_color(ctx, GColorWhite);
            graphics_fill_rect(ctx, GRect(i*CIRCLE_SIZE+6 + LEFT_OFFSET, j*CIRCLE_SIZE+6 + TOP_OFFSET, 4, 4), 0, GCornersAll);
            graphics_context_set_fill_color(ctx, GColorBlack);
          }
        }
        else
        {
          graphics_fill_rect(ctx, GRect(i*CIRCLE_SIZE+6 + LEFT_OFFSET, j*CIRCLE_SIZE+6 + TOP_OFFSET, 4, 4), 0, GCornersAll);
        }
      }
      else if(get_board_value(index, active_board) == EMPTY)
      {
        //graphics_fill_rect(ctx, GRect(i*18+9, j*18+9, 2, 2), 0, GCornersAll);
        int top_right = get_board_index(0,7);
        int bottom_left =  get_board_index(7,0);
        int bottom_right =  get_board_index(7,7);

        if(index == 0 || index == top_right || index == bottom_left || index == bottom_right)
        {
          //Draw the corner indicators
          graphics_fill_rect(ctx, GRect(i*CIRCLE_SIZE+7 + LEFT_OFFSET, j*CIRCLE_SIZE+7 + TOP_OFFSET, 2, 2), 0, GCornerNone);
        }
      }
    }
  }
}

static void reset_text_color() {
  text_layer_set_text_color(text_layer, GColorBlack);
  text_layer_set_background_color(text_layer, GColorClear);
}

static void update_score_display()
{
  snprintf(g_black_score_string,3, "%d", g_black_score);
  snprintf(g_white_score_string,3, "%d", g_white_score);
  text_layer_set_text(black_score_layer, g_black_score_string);
  text_layer_set_text(white_score_layer, g_white_score_string);
  layer_mark_dirty(text_layer_get_layer(black_score_layer));
  layer_mark_dirty(text_layer_get_layer(white_score_layer));
}
static void set_text_box(char *top_string)
{
  reset_text_color();
  text_layer_set_text(text_layer, top_string);
  layer_mark_dirty(text_layer_get_layer(text_layer));
}

static void set_game_over_display()
{
  if(g_player_count == 0 || g_player_count == 2)
  {
    if(g_black_score > g_white_score)
    {
      set_text_box(BLACK_WINS);
    }
    else if (g_black_score < g_white_score)
    {
      set_text_box(WHITE_WINS);
    }
    else
    {
      set_text_box(TIE_GAME);
    }
  }
  else
  {
    if(g_black_score > g_white_score)
    {
      set_text_box(PLAYER_WIN);
    }
    else if (g_black_score < g_white_score)
    {
      set_text_box(AI_WIN);
    }
    else
    {
      set_text_box(TIE_GAME);
    }
  }
}
static void set_must_skip_display()
{
  set_text_box(MUST_PASS);
}
static void set_ai_thinking_display()
{
  set_text_box(AI_TURN_THINKING);
}
static void set_players_turn_display()
{
  if(g_player_count == 0 || g_player_count == 2)
  {
    if(g_current_player == 0)
    {
      set_text_box(BLACK_TURN);
    }
    else
    {
      set_text_box(WHITE_TURN);
    }
  }
  else
  {
    if(g_current_player == 0)
    {
      set_text_box(PLAYER_TURN);
    }
    else
    {
      set_text_box(AI_TURN);
    }

  }
}

static void advance_state()
{
  if(g_current_game_state == WHITE_PLAYER_SELECTING)
  {
    //The white player has moved.
    //Advance to animation.
    g_current_game_state = ANIMATION_PLAYING;
    animate_move();
  }
  else if(g_current_game_state == BLACK_PLAYER_SELECTING)
  {
    //The white player has moved.
    //Advance to animation.
    g_current_game_state = ANIMATION_PLAYING;
    animate_move();
  }
  else if(g_current_game_state == ANIMATION_PLAYING)
  {
    //Should be switched to the new player.  See if they have any moves available.
    if(g_selectable_count > 0)
    {
      // If yes, check to see if the current player is an AI
        //If they are, switch to AI_THINKING.
        //If they aren't, switch to X_PLAYER_SELECTING based on the player.
      if(g_player_count == 0)
      {
        set_ai_thinking_display();
        g_current_game_state = AI_THINKING;
        make_ai_move();
      }
      else if(g_player_count == 1 && g_current_player == 1)
      {
        set_ai_thinking_display();
        g_current_game_state = AI_THINKING;
        make_ai_move();
      }
      else
      {
        if(g_current_player == 0)
        {
          set_players_turn_display();
          g_current_game_state = BLACK_PLAYER_SELECTING;
        }
        else
        { 
          set_players_turn_display();
          g_current_game_state = WHITE_PLAYER_SELECTING;
        }
      }
    }
    else
    {
      //If no, test to see if the old player has moves available.
      int new_black_score = 0;
      int new_white_score = 0;
      int not_current_player = toggle_player(g_current_player);
      set_board_selectables_and_score(g_board, &new_black_score, &new_white_score, not_current_player);
      generate_selectables_array();
      if(g_selectable_count == 0)
      {
        //If neither player has moves available, switch to GAME_OVER
        set_game_over_display();
        g_current_game_state = GAME_OVER;
      }
      else
      {
        //If just the current player has to skip, change BACK to the new player and switch to PLAYER MUST SKIP.
        set_board_selectables_and_score(g_board, &new_black_score, &new_white_score, g_current_player);
        generate_selectables_array();
        set_must_skip_display();
        g_current_game_state = PLAYER_MUST_SKIP;
      }
    }
  }
  else if(g_current_game_state == PLAYER_MUST_SKIP)
  {
    // The player has to acknowledge the skip even if it's AI.
    // We've already tested to make sure only one character needs to skip (not both) when we assigned this state, so
    // this player MUST be able to play.
    // We should have already switched to the new player and generated their moves.
    // Check to see if the current player is an AI
        // If they are, switch to AI_THINKING.
        // If they aren't, switch to X_PLAYER_SELECTING based on the player.      
    if(g_player_count == 0)
    {
      set_ai_thinking_display();
      g_current_game_state = AI_THINKING;
      make_ai_move();
    }
    else if(g_player_count == 1 && g_current_player == 1)
    {
      set_ai_thinking_display();
      g_current_game_state = AI_THINKING;
      make_ai_move();
    }
    else
    {
      if(g_current_player == 0)
      {
        set_players_turn_display();
        g_current_game_state = BLACK_PLAYER_SELECTING;
      }
      else
      { 
        set_players_turn_display();
        g_current_game_state = WHITE_PLAYER_SELECTING;
      }
    }
  }
  else if(g_current_game_state == AI_THINKING)
  {
    //AI has selected.  Switch to ANIMATION PLAYING.
    g_current_game_state = ANIMATION_PLAYING;
    animate_move();
  }
  else if(g_current_game_state == GAME_OVER)
  {
    // Player has to acknowledge GAME_OVER.
    // Once they do, restart the game.
    // reset_game sets state to the appropriate player_selecting.
    reset_game();
  }
}

static bool animate_board(char *original_board, char *new_board, char * anim_board, int step)
{
  bool dirty_bit = false;
  //copy the original board to the anim board
  //memcpy(g_anim_board, g_old_board, sizeof(char[BOARD_WIDTH*BOARD_HEIGHT]));
  for(int i=0; i<BOARD_WIDTH; i++)
  {
    for(int j=0; j<BOARD_HEIGHT; j++)
    {
      if(abs(i-anim_center_x) < step && abs(j-anim_center_y) < step)
      {
        //for all positions within STEP positions of anim, replace the orig value with the new val in anim.
        if(g_anim_board[get_board_index(i,j)] == ANIMATING)
        {
          g_anim_board[get_board_index(i,j)] = g_board[get_board_index(i,j)];
          dirty_bit = true;
        }
        else if(g_board[get_board_index(i,j)] == WHITE || g_board[get_board_index(i,j)] == BLACK)
        {
          if(g_anim_board[get_board_index(i,j)] != g_board[get_board_index(i,j)])
          {
            g_anim_board[get_board_index(i,j)] = ANIMATING;
            dirty_bit = true;
          }
        }
      }
    }
  }
  return dirty_bit;
}


static void update_animation()
{
  if(!anim_frames || (anim_frames && (++anim_frame >= flip_frame_count)))
  {
    anim_frames = false;
    anim_state++;
    //Generate the anim board for the current (original) state
    bool dirty_bit = animate_board(g_old_board, g_board, g_anim_board, anim_state);
    if(anim_state > 7 || !dirty_bit)
    {
      //all done!
      advance_state();
      layer_mark_dirty(s_canvas_layer);
    }
    else
    {
      anim_frames = true;
      anim_frame = 0;
      //schedule another round.
      layer_mark_dirty(s_canvas_layer);
      if(!SPECIAL_SCREENSHOT_MODE)
      {
        anim_timer = app_timer_register(ANIM_FRAME_SPEED, update_animation, NULL);
      }
    }
  }
  else
  {
    //Anim_frame already incremented
    layer_mark_dirty(s_canvas_layer);
    if(!SPECIAL_SCREENSHOT_MODE)
    {
      anim_timer = app_timer_register(ANIM_FRAME_SPEED, update_animation, NULL);
    }
  }
}

static void animate_move()
{
  if(!play_animations)
  {
    advance_state();
    return;
  }
  anim_state = 0;
  anim_frames = false;
  // Manage the animation of a move.
  // Set the animation board to the original board
  memcpy(g_anim_board, g_old_board, sizeof(char[BOARD_WIDTH*BOARD_HEIGHT]));
  layer_mark_dirty(s_canvas_layer);
  //Set the update timer.
  if(!SPECIAL_SCREENSHOT_MODE)
  {
    anim_timer = app_timer_register(ANIM_FRAME_SPEED, update_animation, NULL);
  }
}

static int get_depth_by_ai_strength(int strength)
{
  switch(strength)
  {
    case 0:
      return 1;
      break;
    case 1:
      return 2;
      break;
    case 2 :
      return 3;
      break;
    case 3:
      return 4;
      break;
    default:
      return 2;
  }
}

static void async_ai_move()
{
  if(g_current_game_state == AI_THINKING)
  {
    memcpy(g_old_board, g_board, sizeof(char[BOARD_WIDTH*BOARD_HEIGHT]));
    int local_x = 0;
    int local_y = 0;
    int index_to_select;
    int empty_squares = (BOARD_WIDTH*BOARD_HEIGHT) - (g_white_score + g_black_score);
    int depth = get_depth_by_ai_strength(ai_strength);
    if(empty_squares <= END_GAME_DEPTH_OVERRIDE)
    {
      depth = END_GAME_DEPTH_OVERRIDE;
    }
    min_max_evaluator(g_board, depth, g_current_player, ALPHA_MIN, BETA_MAX, &index_to_select);
    //reverse_index(g_selectable_array[rand()%g_selectable_count], &local_x, &local_y);          

    reverse_index(index_to_select, &local_x, &local_y);
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "Player %d selected index %d,%d (option %d) with score: %d",g_current_player, local_x,local_y, index_to_select, new_score);
    for(int i = 0; i < g_selectable_count; i++)
    {
      int t_x = 0;
      int t_y = 0;
      reverse_index(g_selectable_array[i], &t_x, &t_y);
      //APP_LOG(APP_LOG_LEVEL_DEBUG, "Player %d other option %d,%d (option %d)",g_current_player, t_x,t_y, i);
    }
    commit_selection(g_board, local_x, local_y, g_current_player);
    anim_center_x = local_x;
    anim_center_y = local_y;
    g_current_player = toggle_player(g_current_player);
    set_board_selectables_and_score(g_board, &g_black_score, &g_white_score, g_current_player);
    generate_selectables_array();
    update_score_display();
    advance_state();
  }
  //layer_mark_dirty(s_canvas_layer);

  ai_thinking = false;
}

//This should be asynchronous, but let's see how ambitious we get with it.
static void make_ai_move()
{
  ai_thinking = true;
  srand(time(NULL));
  ai_timer = app_timer_register(30, async_ai_move, NULL);
}


static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(text_layer, "");
  if(g_current_game_state == WHITE_PLAYER_SELECTING || g_current_game_state == BLACK_PLAYER_SELECTING)
  {
    memcpy(g_old_board, g_board, sizeof(char[BOARD_WIDTH*BOARD_HEIGHT]));
    int local_x = 0;
    int local_y = 0;
    reverse_index(get_current_selectable_index(), &local_x, &local_y);
    commit_selection(g_board, local_x, local_y, g_current_player);
    anim_center_x = local_x;
    anim_center_y = local_y;
    g_current_player = toggle_player(g_current_player);
    set_board_selectables_and_score(g_board, &g_black_score, &g_white_score, g_current_player);
    generate_selectables_array();
    update_score_display();
    advance_state();
  }
  else if(g_current_game_state == PLAYER_MUST_SKIP || g_current_game_state == GAME_OVER)
  {
    g_current_player = toggle_player(g_current_player);
    set_board_selectables_and_score(g_board, &g_black_score, &g_white_score, g_current_player);
    generate_selectables_array();
    advance_state();
  }
  else if(SPECIAL_SCREENSHOT_MODE && g_current_game_state == ANIMATION_PLAYING)
  {
    update_animation();
  }
  //layer_mark_dirty(s_canvas_layer);
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  dec_selectable_index();
  layer_mark_dirty(s_canvas_layer);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  inc_selectable_index();
  layer_mark_dirty(s_canvas_layer);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

static void init_anim_frames()
{
  flip_white[0] = gbitmap_create_with_resource(RESOURCE_ID_FLIP_1);
  flip_white[1] = gbitmap_create_with_resource(RESOURCE_ID_FLIP_2);
  flip_white[2] = gbitmap_create_with_resource(RESOURCE_ID_FLIP_3);
  flip_white[3] = gbitmap_create_with_resource(RESOURCE_ID_FLIP_4);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  const int TOP_OFFSET = TOP_BAR_OFFSET;
  init_anim_frames();
  //Create Life Layer
  s_canvas_layer = layer_create(GRect(0, 0, bounds.size.w, bounds.size.h));
  layer_add_child(window_layer, s_canvas_layer);

  // Create Text Layer
  text_layer = text_layer_create((GRect) { .origin = { bounds.size.w/6, TOP_OFFSET }, .size = { (bounds.size.w * 2)/3, 20 } });
  text_layer_set_text_color(text_layer, GColorBlack);
  text_layer_set_background_color(text_layer, GColorClear);
  text_layer_set_text(text_layer, "Tiny Reversi");
  text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(text_layer));

  // Create Black Score Layer
  black_score_layer = text_layer_create((GRect) { .origin = { 0, TOP_OFFSET }, .size = { bounds.size.w/6, 20 } });
  text_layer_set_text_color(black_score_layer, GColorWhite);
  text_layer_set_background_color(black_score_layer, GColorBlack);
  text_layer_set_text(black_score_layer, "00");
  text_layer_set_text_alignment(black_score_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(black_score_layer));

  // Create White Score Layer
  white_score_layer = text_layer_create((GRect) { .origin = { (bounds.size.w * 5)/6, TOP_OFFSET }, .size = { bounds.size.w/6, 20 } });
  text_layer_set_text_color(white_score_layer, GColorBlack);
  text_layer_set_background_color(white_score_layer, GColorWhite);
  text_layer_set_text(white_score_layer, "00");
  text_layer_set_text_alignment(white_score_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(white_score_layer));

  //Update scores
  //reset_game();
  update_score_display();

  // Set the update_proc
  layer_set_update_proc(s_canvas_layer, write_board_to_layer);

#ifdef PBL_SDK_3
  // Set up the status bar last to ensure it is on top of other Layers
  s_status_bar = status_bar_layer_create();
  layer_add_child(window_layer, status_bar_layer_get_layer(s_status_bar));
#endif
}

static void window_unload(Window *window) {
  text_layer_destroy(text_layer);
  layer_destroy(s_canvas_layer);
}

static void settings_click_config_provider(void *context) {
  //window_single_click_subscribe(BUTTON_ID_SELECT, settings_select_click_handler);
}

static void settings_new_game()
{
  reset_game();
  //Return to game.
  const bool animated = true;
  window_stack_push(window, animated);
}
static void settings_resume_game()
{
  //Return to game.
  const bool animated = true;
  window_stack_push(window, animated);
}
static void settings_ai_options()
{
  const bool animated = true;
  window_stack_push(ai_settings_window, animated);
}
static void settings_player_count()
{
  const bool animated = true;
  window_stack_push(pc_settings_window, animated);
}

static void set_settings_menu_ai_item()
{
  if(ai_strength == 0)
  {
    settings_menu_item_array[2] = (SimpleMenuItem){.callback = settings_ai_options, .icon=NULL,.subtitle=SETTINGS_AI_SUB_1, .title=SETTINGS_AI};
  }
  else if(ai_strength == 1)
  {
    settings_menu_item_array[2] = (SimpleMenuItem){.callback = settings_ai_options, .icon=NULL,.subtitle=SETTINGS_AI_SUB_2, .title=SETTINGS_AI};
  }
  else //ai_strength == 2
  {
    settings_menu_item_array[2] = (SimpleMenuItem){.callback = settings_ai_options, .icon=NULL,.subtitle=SETTINGS_AI_SUB_3, .title=SETTINGS_AI};
  }
}
static void set_settings_menu_pc_item()
{
  if(g_player_count == 0)
  {
    settings_menu_item_array[3] = (SimpleMenuItem){.callback = settings_player_count, .icon=NULL,.subtitle=SETTINGS_PLAYER_COUNT_SUB_0,.title=SETTINGS_PLAYER_COUNT};
  }
  else if(g_player_count == 1)
  {
    settings_menu_item_array[3] = (SimpleMenuItem){.callback = settings_player_count, .icon=NULL,.subtitle=SETTINGS_PLAYER_COUNT_SUB_1,.title=SETTINGS_PLAYER_COUNT};  
  }
  else //ai_strength == 2
  {
    settings_menu_item_array[3] = (SimpleMenuItem){.callback = settings_player_count, .icon=NULL,.subtitle=SETTINGS_PLAYER_COUNT_SUB_2,.title=SETTINGS_PLAYER_COUNT};
  }
}
static void settings_toggle_grid()
{
  g_grid_display = !(g_grid_display);
  set_settings_menu_grid_item();
  //Return to game.
  const bool animated = true;
  window_stack_push(window, animated);
}
static void set_settings_menu_grid_item()
{
  if(g_grid_display == true)
  {
    settings_menu_item_array[4] = (SimpleMenuItem){.callback = settings_toggle_grid, .icon=NULL,.subtitle=SETTINGS_GRID_DISPLAY_SUB_TRUE,.title=SETTINGS_GRID_DISPLAY};
  }
  else
  {
    settings_menu_item_array[4] = (SimpleMenuItem){.callback = settings_toggle_grid, .icon=NULL,.subtitle=SETTINGS_GRID_DISPLAY_SUB_FALSE,.title=SETTINGS_GRID_DISPLAY};  
  }
}

static void settings_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  settings_menu_item_array[0] = (SimpleMenuItem){.callback = settings_resume_game, .icon=NULL,.subtitle=NULL,.title = SETTINGS_RESUME_GAME};
  settings_menu_item_array[1] = (SimpleMenuItem){.callback = settings_new_game, .icon=NULL,.subtitle=NULL,.title=SETTINGS_NEW_GAME};
  set_settings_menu_ai_item();
  set_settings_menu_pc_item();
  set_settings_menu_grid_item();
  settings_menu_section_array[0] = (SimpleMenuSection){.items=settings_menu_item_array,.num_items=5,.title=SETTINGS_TITLE};
  settings_menu_layer = simple_menu_layer_create((GRect) { .origin = { 0, 0 }, .size = { bounds.size.w, bounds.size.h } },
    window,
    settings_menu_section_array,
    1,
    NULL);
  layer_add_child(window_layer, simple_menu_layer_get_layer(settings_menu_layer));

  // Set the update_proc
  //layer_set_update_proc(s_canvas_layer, write_board_to_layer);
}
static void settings_window_unload(Window *window) {
  layer_destroy(simple_menu_layer_get_layer(settings_menu_layer));
}

static void ai_settings_set(int new_ai_strength)
{
  ai_strength = new_ai_strength;
  //Return to game
  window_stack_pop(false);
  window_stack_push(window, true);
  set_settings_menu_ai_item();
}

static void ai_settings_easy_button()
{
  ai_settings_set(0);
}

static void ai_settings_normal_button()
{
  ai_settings_set(1);
}

static void ai_settings_hard_button()
{
  ai_settings_set(2);
}

static void ai_settings_brutal_button()
{
  ai_settings_set(3);
}

static void ai_settings_click_config_provider(void *context) {

}

static void ai_settings_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  ai_settings_menu_item_array[0] = (SimpleMenuItem){.callback = ai_settings_easy_button, .icon=NULL,.subtitle=NULL,.title = AI_SETTINGS_EASY};
  ai_settings_menu_item_array[1] = (SimpleMenuItem){.callback = ai_settings_normal_button, .icon=NULL,.subtitle=NULL,.title=AI_SETTINGS_NORMAL};
  ai_settings_menu_item_array[2] = (SimpleMenuItem){.callback = ai_settings_hard_button, .icon=NULL,.subtitle=NULL, .title=AI_SETTINGS_HARD};
  ai_settings_menu_item_array[3] = (SimpleMenuItem){.callback = ai_settings_brutal_button, .icon=NULL,.subtitle=NULL, .title=AI_SETTINGS_BRUTAL};
  #ifdef PBL_PLATFORM_BASALT
  ai_settings_menu_section_array[0] = (SimpleMenuSection){.items=ai_settings_menu_item_array,.num_items=4,.title=AI_SETTINGS_TITLE};
  #else
  ai_settings_menu_section_array[0] = (SimpleMenuSection){.items=ai_settings_menu_item_array,.num_items=3,.title=AI_SETTINGS_TITLE};
  #endif
  ai_settings_menu_layer = simple_menu_layer_create((GRect) { .origin = { 0, 0 }, .size = { bounds.size.w, bounds.size.h } },
    window,
    ai_settings_menu_section_array,
    1,
    NULL);
  layer_add_child(window_layer, simple_menu_layer_get_layer(ai_settings_menu_layer));
  //set to initial choice
  simple_menu_layer_set_selected_index(ai_settings_menu_layer,ai_strength,false);

  // Set the update_proc
  //layer_set_update_proc(s_canvas_layer, write_board_to_layer);
}

static void ai_settings_window_unload(Window *window) {
  layer_destroy(simple_menu_layer_get_layer(ai_settings_menu_layer));
}


static void pc_settings_set(int new_player_count)
{
  if(g_player_count != new_player_count)
  {
    g_player_count = new_player_count;
    if(g_player_count == 0 && (g_current_game_state == BLACK_PLAYER_SELECTING || g_current_game_state == WHITE_PLAYER_SELECTING))
    {
      g_current_game_state = AI_THINKING;
    }
    else if(g_player_count == 1 && g_current_game_state == WHITE_PLAYER_SELECTING)
    {
      g_current_game_state = AI_THINKING;
    }
    restore_game_state();
  }
  //Return to game
  window_stack_pop(false);
  window_stack_push(window, true);
  set_settings_menu_pc_item();
}

static void pc_settings_0p_button()
{
  pc_settings_set(0);
}

static void pc_settings_1p_button()
{
  pc_settings_set(1);
}

static void pc_settings_2p_button()
{
  pc_settings_set(2);
}

static void pc_settings_click_config_provider(void *context) {

}

static void pc_settings_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  pc_settings_menu_item_array[0] = (SimpleMenuItem){.callback = pc_settings_0p_button, .icon=NULL,.subtitle=NULL,.title = PC_SETTINGS_0};
  pc_settings_menu_item_array[1] = (SimpleMenuItem){.callback = pc_settings_1p_button, .icon=NULL,.subtitle=NULL,.title= PC_SETTINGS_1};
  pc_settings_menu_item_array[2] = (SimpleMenuItem){.callback = pc_settings_2p_button, .icon=NULL,.subtitle=NULL, .title= PC_SETTINGS_2};
  pc_settings_menu_section_array[0] = (SimpleMenuSection){.items=pc_settings_menu_item_array,.num_items=3,.title=PC_SETTINGS_TITLE};
  pc_settings_menu_layer = simple_menu_layer_create((GRect) { .origin = { 0, 0 }, .size = { bounds.size.w, bounds.size.h } },
    window,
    pc_settings_menu_section_array,
    1,
    NULL);
  layer_add_child(window_layer, simple_menu_layer_get_layer(pc_settings_menu_layer));
  //set to initial choice
  simple_menu_layer_set_selected_index(pc_settings_menu_layer,g_player_count,false);

  // Set the update_proc
  //layer_set_update_proc(s_canvas_layer, write_board_to_layer);
}
static void pc_settings_window_unload(Window *window) {
  layer_destroy(simple_menu_layer_get_layer(pc_settings_menu_layer));
}

static void serialize_game_state()
{
  persist_write_data(BOARD_KEY, g_board, BOARD_WIDTH * BOARD_HEIGHT);
  persist_write_data(CURRENT_GAME_STATE_KEY, &g_current_game_state, 1);
  persist_write_int(PLAYER_COUNT_KEY, g_player_count);
  persist_write_int(CURRENT_PLAYER_KEY, g_current_player);
  persist_write_int(AI_STRENGTH_KEY, ai_strength);
  persist_write_bool(SHOW_GRID_KEY, g_grid_display);
}

static bool validate_deserialization()
{
  for(int i = 0; i < BOARD_WIDTH * BOARD_HEIGHT; i++)
  {
    if(g_board[i] != BLACK && g_board[i] != WHITE && g_board[i] != EMPTY && g_board[i] != SELECTABLE)
    {
      return false;
    }
  }
  if (g_current_game_state != WHITE_PLAYER_SELECTING &&
      g_current_game_state != BLACK_PLAYER_SELECTING && 
      g_current_game_state != ANIMATION_PLAYING &&
      g_current_game_state != PLAYER_MUST_SKIP && 
      g_current_game_state != AI_THINKING && 
      g_current_game_state != GAME_OVER)
  {
    return false;
  }
  if(g_player_count < 0 || g_player_count > 2)
  {
    return false;
  }
  if(g_current_player < 0 || g_current_player > 1)
  {
    return false;
  }
  if(ai_strength < 0 || ai_strength > 3)
  {
    return false;
  }
  return true;
}
static void restore_game_state()
{
  //Generate the scores and selectable positions
  set_board_selectables_and_score(g_board, &g_black_score, &g_white_score, g_current_player);
  generate_selectables_array();
  g_selected_square = 0;
  //Update the score UI
  update_score_display();
  //Check game state.  Update banner UI
  if(g_current_game_state == BLACK_PLAYER_SELECTING || g_current_game_state == WHITE_PLAYER_SELECTING)
  {
    //if B/W: just update banner.
    set_players_turn_display();
  }
  else if(g_current_game_state == ANIMATION_PLAYING)
  {
    //if animation playing: skip it.  Just call advance state() (?)
    advance_state();
  }
  else if(g_current_game_state == PLAYER_MUST_SKIP)
  {
    //update banner
    set_must_skip_display();
  }
  else if(g_current_game_state == AI_THINKING)
  {
    //if AI: call make_ai_move
    set_ai_thinking_display();
    make_ai_move();

  }
  else if(g_current_game_state == GAME_OVER)
  {
    //if game_over: update banner
    set_game_over_display();
  }

  //Reset Settings Options
  set_settings_menu_ai_item();
  set_settings_menu_pc_item();

}

static void deserialize_game_state()
{
  if (persist_exists(BOARD_KEY) && persist_exists(CURRENT_GAME_STATE_KEY) && persist_exists(PLAYER_COUNT_KEY) && persist_exists(CURRENT_PLAYER_KEY) && persist_exists(AI_STRENGTH_KEY))
  {
    //Restore serializable values, if present
    //Check bounds for appropriateness.
    persist_read_data(BOARD_KEY, g_board, BOARD_WIDTH * BOARD_HEIGHT);
    persist_read_data(CURRENT_GAME_STATE_KEY, &g_current_game_state, 1);
    g_player_count = persist_read_int(PLAYER_COUNT_KEY);
    g_current_player = persist_read_int(CURRENT_PLAYER_KEY);
    ai_strength = persist_read_int(AI_STRENGTH_KEY);
    if(persist_exists(SHOW_GRID_KEY))
    {
      g_grid_display = persist_read_bool(SHOW_GRID_KEY);
    }
    else
    {
      g_grid_display = false; //default
    }
    if(!validate_deserialization())
    {
      reset_game();
    }
    restore_game_state();
  }
  else
  {
    //If state cannot be restored or does not exist
    reset_game();
  }
}

static void init(void) {
  const bool animated = true;

  //ai settings window
  ai_settings_window = window_create();
  window_set_click_config_provider(ai_settings_window, ai_settings_click_config_provider);
  window_set_window_handlers(ai_settings_window, (WindowHandlers) {
    .load = ai_settings_window_load,
    .unload = ai_settings_window_unload,
  });

  //pc settings window
  pc_settings_window = window_create();
  window_set_click_config_provider(pc_settings_window, pc_settings_click_config_provider);
  window_set_window_handlers(pc_settings_window, (WindowHandlers) {
    .load = pc_settings_window_load,
    .unload = pc_settings_window_unload,
  });

  //settings window
  settings_window = window_create();
  window_set_click_config_provider(settings_window, settings_click_config_provider);
  window_set_window_handlers(settings_window, (WindowHandlers) {
    .load = settings_window_load,
    .unload = settings_window_unload,
  });
  window_stack_push(settings_window, animated);

  //game window
  window = window_create();
  #ifdef PBL_COLOR
    window_set_background_color(window, (GColor8){ .argb = 0b11001100 });
  #endif
  window_set_click_config_provider(window, click_config_provider);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(window, animated);

  deserialize_game_state();
}

static void deinit(void) {
  serialize_game_state();
  window_destroy(window);
  window_destroy(settings_window);
  window_destroy(ai_settings_window);
}

int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

  app_event_loop();
  deinit();
}
