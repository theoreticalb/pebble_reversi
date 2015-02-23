#include <pebble.h>
#include <stdlib.h>
#include "util.h"
#include "game.h"
#include "ai.h"


//Returns raw heuristic score of the passed board.  -100 to 100.
static int board_evaluator(char* board)
{
  // heuristic:  Add value for sides pieces?
  // heuristic:  Add value for having more options, reduce value for fewer? 
  const int CORNER_BONUS = 100;
  int loc_black_score = 0;
  int loc_white_score = 0;

  get_board_score(board, &loc_black_score, &loc_white_score);
  int top_right = get_board_index(0,7);
  int bottom_left =  get_board_index(7,0);
  int bottom_right =  get_board_index(7,7);

  // heuristic:  Add value for corners
  if(board[0] == BLACK)
  {
    loc_black_score += CORNER_BONUS;
  }
  if(board[top_right] == BLACK)
  {
    loc_black_score += CORNER_BONUS;
  }
  if(board[bottom_left] == BLACK)
  {
    loc_black_score += CORNER_BONUS;
  }
  if(board[bottom_right] == BLACK)
  {
    loc_black_score += CORNER_BONUS;
  }
  if(board[0] == WHITE)
  {
    loc_white_score += CORNER_BONUS;
  }
  if(board[top_right] == WHITE)
  {
    loc_white_score += CORNER_BONUS;
  }
  if(board[bottom_left] == WHITE)
  {
    loc_white_score += CORNER_BONUS;
  }
  if(board[bottom_right] == WHITE)
  {
    loc_white_score += CORNER_BONUS;
  }
  
  return loc_black_score - loc_white_score;
}
//Returns min or max value of the passed board based on current_player
int min_max_evaluator(char* board, int cur_depth, int current_player, int alpha, int beta, int *selection_index)
{
  //APP_LOG(APP_LOG_LEVEL_INFO, "Player %d Checking board with alpha: %d and beta: %d at depth %d", current_player, alpha, beta, cur_depth);
  const bool RANDOMIZE_EQUIVALENT_MOVES = true;
  int black_score = 0;
  int white_score = 0;
  int selectables = set_board_selectables_and_score(board, &black_score, &white_score, current_player);
  if(selectables==0)
  {
    //APP_LOG(APP_LOG_LEVEL_INFO, "Player %d passes!  No selectables", current_player);

    //No choices. It's a skip or a game over.
    selectables = set_board_selectables_and_score(board, &black_score, &white_score, toggle_player(current_player));
    if(selectables==0)
    {
      // Game over! return 100 for a black win, -100 for a white win, 0 for a tie.
      if(black_score > white_score)
      {
        return 1000;
      }
      else if(white_score > black_score)
      {
        return -1000;
      }
      else
      {
        return 0;
      }
    }
    else
    {
      // The current board position is a Skip. 
      if(cur_depth == 0)
      {
        return board_evaluator(board);
      }
      else
      {
        return min_max_evaluator(board, cur_depth-1, toggle_player(current_player), alpha, beta, selection_index);
      }
    }
  }
  int return_score = 0;
  // For our purposes, -infinity and infinity.
  if(get_player_char(current_player) == BLACK)
  {
    return_score = -10000;
  }
  else
  {
    return_score = 10000;
  }
  int return_index = 0;
  int cur_selection_index = 0;
  for(int i = 0; i < BOARD_WIDTH; i++)
  {
    for(int j = 0; j < BOARD_HEIGHT; j++)
    {
      if(board[get_board_index(i,j)] == SELECTABLE)
      {
        char new_board[8*8];
        memcpy(new_board, board, sizeof(char[BOARD_WIDTH*BOARD_HEIGHT]));
        commit_selection(new_board, i, j, current_player);

        if(cur_depth == 0)
        {
          //APP_LOG(APP_LOG_LEVEL_INFO, "Player %d Checking branch with alpha: %d and beta: %d at depth %d", current_player, alpha, beta, cur_depth);

          // if cur depth = 0, evaluate all available board positions via the board_evaluator
            // if black, return highest value.
            // if white, return lowest value.
          int new_score = board_evaluator(new_board);
          
          if(get_player_char(current_player) == BLACK)
          {
            if(new_score > return_score)
            {
              return_score = new_score;
              return_index = get_board_index(i,j);
            }
            if(new_score == return_score && RANDOMIZE_EQUIVALENT_MOVES && flip_coin())
            {
              return_score = new_score;
              return_index = get_board_index(i,j);              
            }
            #if ALPHA_BETA
            if(beta <= return_score)
            {
              //Prune: best case this tree is as bad as options we've already discovered.
              //  This will be used to actually play the move.
               *selection_index = return_index;
              //APP_LOG(APP_LOG_LEVEL_DEBUG, "Player %d Pruning branch with score: %d and beta: %d at depth %d", current_player, return_score, beta, cur_depth);
              return return_score;
            }
            #endif
          }
          else
          {
            if(new_score < return_score)
            {
              return_score = new_score;
              return_index = get_board_index(i,j);
            }
            if(new_score == return_score && RANDOMIZE_EQUIVALENT_MOVES && flip_coin())
            {
              return_score = new_score;
              return_index = get_board_index(i,j);              
            }
            #if ALPHA_BETA
            if(return_score <= alpha)
            {
              //Prune: best case this tree is as bad as options we've already discovered.
              //  This will be used to actually play the move.
               *selection_index = return_index;
              //APP_LOG(APP_LOG_LEVEL_DEBUG, "Player %d Pruning branch with score: %d and alpha: %d at depth %d", current_player, return_score, alpha, cur_depth);
               return return_score;
            }
            #endif
          }
        }
        else
        {

          // else, recursively call min_max evaluator on all board positions, with cur_depth lowered, and current_player toggled
            // if black, return highest value.
            // if white, return lowest value.
          if(get_player_char(current_player) == BLACK)
          {
            //APP_LOG(APP_LOG_LEVEL_INFO, "Player %d is about to call min_max.", current_player);
            int new_score = min_max_evaluator(new_board, cur_depth-1, toggle_player(current_player), return_score, beta, selection_index);            
            //APP_LOG(APP_LOG_LEVEL_INFO, "Player %d called min_max and got back:%d", current_player, new_score);
            if(new_score > return_score)
            {
              return_score = new_score;
              return_index = get_board_index(i,j);
            }
            if(new_score == return_score && RANDOMIZE_EQUIVALENT_MOVES && flip_coin())
            {
              return_score = new_score;
              return_index = get_board_index(i,j);              
            }
            if(beta <= return_score)
            {
              //Prune: best case this tree is as bad as options we've already discovered.
              //  This will be used to actually play the move.
              // *selection_index = return_index;
              //APP_LOG(APP_LOG_LEVEL_DEBUG, "Player %d Pruning branch with score: %d and beta: %d at depth %d", current_player, return_score, beta, cur_depth);
              //return return_score;
            }
          }
          else
          {
            //APP_LOG(APP_LOG_LEVEL_INFO, "Player %d is about to call min_max.", current_player);
            int new_score = min_max_evaluator(new_board, cur_depth-1, toggle_player(current_player), alpha, return_score, selection_index);      
            //APP_LOG(APP_LOG_LEVEL_INFO, "Player %d called min_max and got back:%d", current_player, new_score);
            if(new_score < return_score)
            {
              return_score = new_score;
              return_index = get_board_index(i,j);
            }
            if(new_score == return_score && RANDOMIZE_EQUIVALENT_MOVES && flip_coin())
            {
              return_score = new_score;
              return_index = get_board_index(i,j);              
            }
            if(return_score <= alpha)
            {
              //Prune: best case this tree is as bad as options we've already discovered.
              //  This will be used to actually play the move.
              // *selection_index = return_index;
              //APP_LOG(APP_LOG_LEVEL_DEBUG, "Player %d Pruning branch with score: %d and alpha: %d at depth %d", current_player, return_score, alpha, cur_depth);
              //return return_score;
            }
          }
        }
        cur_selection_index++;
      }
    }
  }
  //FOR ALL RETURNS: set the selection index to the index of the move corresponding to the returned option.
  //  This will be used to actually play the move.
  *selection_index = return_index;
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "Player %d Returning score: %d and index: %d", current_player, return_score, return_index);
  return return_score;
}
