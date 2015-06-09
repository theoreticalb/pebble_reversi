#ifndef UTIL_H
#define UTIL_H

//Board items.  Done as a bunch of chars because if I use enums they'll be ints and the AI will run out of memory before it runs out of processing power. 
#define BLACK 'B' 
#define WHITE 'W' 
#define EMPTY 'E'
#define SELECTABLE 'S'
#define ANIMATING 'A'

//States  
#define WHITE_PLAYER_SELECTING 'W'
#define BLACK_PLAYER_SELECTING 'B'
#define ANIMATION_PLAYING 'A'
#define PLAYER_MUST_SKIP 'P'
#define GAME_OVER 'G'
#define AI_THINKING 'T'

//Serialization Keys
#define BOARD_KEY 0
#define CURRENT_GAME_STATE_KEY 3
#define PLAYER_COUNT_KEY 1
#define CURRENT_PLAYER_KEY 2
#define AI_STRENGTH_KEY 4
#define SHOW_GRID_KEY 5

//Board constants
#define BOARD_WIDTH 8
#define BOARD_HEIGHT 8

// Safeguard against OOMing.  200 * 64 = 13kb.
#define MAX_AI_BOARDS 200

// Note, alpha-beta pruning deactivated at the moment because it didn't seem to be playing optimally
// and the Pebble logging code was making debuggging it challenging.
#define ALPHA_BETA false
#define ALPHA_MIN -1001 //One worse than white winning
#define BETA_MAX 1001 // One greater than black winning

//When we get to the endgame, use exhaustive search.
#define END_GAME_DEPTH_OVERRIDE 7 // Exhaustively search for a win past this number of empty squares

//Strings
	//Settings Window
	#define SETTINGS_NEW_GAME "New Game"
	#define SETTINGS_RESUME_GAME "Resume Game"
	#define SETTINGS_AI "Difficulty"
	#define SETTINGS_AI_SUB_1 "Current: Easy"
	#define SETTINGS_AI_SUB_2 "Current: Normal"
	#define SETTINGS_AI_SUB_3 "Current: Hard"
	#define SETTINGS_PLAYER_COUNT "Number of Players"
	#define SETTINGS_PLAYER_COUNT_SUB_0 "Current: AI vs. AI"
	#define SETTINGS_PLAYER_COUNT_SUB_1 "Current: 1 Player"
	#define SETTINGS_PLAYER_COUNT_SUB_2 "Current: 2 Players"
	#define SETTINGS_TITLE "Tiny Reversi"
	#define SETTINGS_GRID_DISPLAY "Toggle Board Grid"
	#define SETTINGS_GRID_DISPLAY_SUB_TRUE "Current: Show Grid"
	#define SETTINGS_GRID_DISPLAY_SUB_FALSE "Current: No Grid"

	//AI Window
	#define AI_SETTINGS_EASY "Easy AI"
	#define AI_SETTINGS_NORMAL "Normal AI"
	#define AI_SETTINGS_HARD "Hard AI"
	#define AI_SETTINGS_BRUTAL "Brutal AI"
	#define AI_SETTINGS_TITLE "AI Difficulty"


	//Player Count Window
	#define PC_SETTINGS_0 "AI vs. AI"
	#define PC_SETTINGS_1 "Player vs. AI"
	#define PC_SETTINGS_2 "Player vs. Player"
	#define PC_SETTINGS_TITLE "Number of Players"

	//In-game
	#define BLACK_WINS "Black wins!"
	#define WHITE_WINS "White wins!"
	#define TIE_GAME "A tie!"
	#define PLAYER_WIN "You win!"
	#define AI_WIN "Pebble wins!"
	
	#define MUST_PASS "Must pass!"
	#define AI_TURN_THINKING "Thinking..."
	#define BLACK_TURN "Black's turn"
	#define WHITE_TURN "White's turn"
	#define PLAYER_TURN "Black's turn"
	#define AI_TURN "Pebble's turn"

//Man, these are useful.

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })
#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

//Utility functions

bool flip_coin();

#endif
