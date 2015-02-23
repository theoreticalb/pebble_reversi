#include <pebble.h>
#include <stdlib.h>
#include "util.h"
#include "game.h"

int get_board_index(int x, int y)
{
  return (x + (y*BOARD_WIDTH));
}

char get_player_char(int in_player)
{
  return (in_player == 0) ? BLACK : WHITE;
}

char get_board_value(int index, char *board)
{
  return board[index];
}
// Helper to get the x/y associated with an index, because I'm an idiot who didn't want to use a 2-dim array
void reverse_index(int in_index, int *x, int *y)
{
  *x = in_index % BOARD_WIDTH;
  *y = (in_index - *x)/BOARD_HEIGHT;
}

void get_board_score(char *board, int *black_score, int *white_score)
{
  int black_pieces = 0;
  int white_pieces = 0;
  for(int i = 0; i < BOARD_WIDTH; i++)
  {
    for(int j= 0; j < BOARD_HEIGHT; j++)
    {
      if(board[get_board_index(i,j)] == BLACK)
      {
        black_pieces += 1;
      }
      else if(board[get_board_index(i,j)] == WHITE)
      {
        white_pieces += 1;
      }
    }
  }
  *black_score = black_pieces;
  *white_score = white_pieces;
}


bool select_vector(char* board, int vector_x, int vector_y, int pos_x, int pos_y, int current_player, bool color_line_found, bool mark_if_found)
{
  char current_player_color = get_player_char(current_player);
  char other_player_color = (get_player_char(current_player) == BLACK) ? WHITE : BLACK;
  if (pos_x < 0 || pos_x >= BOARD_WIDTH || pos_y < 0 || pos_y >= BOARD_HEIGHT) //Current location out of bounds.  We reached the end of the board.
  {
    return false;
  }
  if(!color_line_found)
  {
    if(board[get_board_index(pos_x,pos_y)] == other_player_color) //Color line found!  Flip bit and see if it has a terminator
    {
      if(select_vector(board, vector_x, vector_y, pos_x + vector_x, pos_y+ vector_y, current_player, true, mark_if_found))
      {
        if(mark_if_found)
        {
          board[get_board_index(pos_x,pos_y)] = current_player_color;
        }
        return true;
      }
      else
      {
        return false;
      }
    }
    else // We haven't found a color line yet, and this isn't an opposite color tile, so we're done.  Return false.
    {
      return false;
    }
  }
  else// Color line found is true.  If this matches the other_player color, keep going.  If it matches the player color 
  {
    if(board[get_board_index(pos_x,pos_y)] == current_player_color)
    {
      return true;
    }
    else if(board[get_board_index(pos_x,pos_y)] == other_player_color)
    {
      if(select_vector(board, vector_x, vector_y, pos_x + vector_x, pos_y+ vector_y, current_player, color_line_found, mark_if_found))
      {
        if(mark_if_found)
        {
          board[get_board_index(pos_x,pos_y)] = current_player_color;
        }
        return true;
      }
      return false;
    }
    else //it's empty, or "selectable," which is also empty.
    {
      return false;
    }
  }
}

void select_position(char* board, int i, int j, int current_player)
{
  for(int x = -1; x <= 1; x++)
  {
    for(int y = -1; y <= 1; y++)
    {
      if(y == 0 && x == 0)//skip the cell itself.
      {
        continue;
      }
      if (x+i < 0 || x+i >= BOARD_WIDTH || y+j < 0 || y+j >= BOARD_HEIGHT)
      {
        continue;
      }
      select_vector(board,x,y,i+x,j+y,current_player,false,true);
    }
  }
}

void commit_selection(char *board, int i, int j, int current_player)
{
  // Add the current character to the board
  char new_piece = get_player_char(current_player);
  int new_idx = get_board_index(i,j);
  board[new_idx] = new_piece;
  // Run select_position
  select_position(board, i, j, current_player);

}

bool is_position_selectable(char* board, int i, int j, int current_player)
{
  // Check the eight neighboring squares.  
  // If any of them are the opposite of the current_player color,
  // Continue to check neighbors along that vector until you
  // A) Hit a square that is the current_player color (return true)
  // B) Hit a square that is empty (return false)
  // C) Hit the edge of the board (return false)
  for(int x = -1; x <= 1; x++)
  {
    for(int y = -1; y <= 1; y++)
    {
      if(y == 0 && x == 0)//skip the cell itself.
      {
        continue;
      }
      if (x+i < 0 || x+i >= BOARD_WIDTH || y+j < 0 || y+j >= BOARD_HEIGHT)
      {
        continue;
      }
      if(select_vector(board,x,y,i+x,j+y,current_player,false,false))
      {
        return true;
      }
    }
  }
  return false;
}

//Returns the number of selectable locations found;
int set_board_selectables_and_score(char *board, int *black_score, int *white_score, int current_player)
{
  int black_pieces = 0;
  int white_pieces = 0;
  int selectable_positions = 0;
  for(int i = 0; i < BOARD_WIDTH; i++)
  {
    for(int j= 0; j < BOARD_HEIGHT; j++)
    {
      if(board[get_board_index(i,j)] == BLACK)
      {
        black_pieces += 1;
      }
      else if(board[get_board_index(i,j)] == WHITE)
      {
        white_pieces += 1;
      }
      else// if already selectable, or empty
      {
        if(is_position_selectable(board, i, j, current_player))
        {
          board[get_board_index(i,j)] = SELECTABLE;
          selectable_positions++;
        }
        else
        {
          board[get_board_index(i,j)] = EMPTY;
        }
      }
    }
  }
  *black_score = black_pieces;
  *white_score = white_pieces;
  return selectable_positions;
}

int toggle_player(int in_player)
{
  return (in_player == 0) ? 1 : 0;
}
