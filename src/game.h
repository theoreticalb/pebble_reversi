#ifndef GAME_H
#define GAME_H

int get_board_index(int x, int y);
char get_board_value(int index, char *board);
void reverse_index(int in_index, int *x, int *y);
char get_player_char(int in_player);
void get_board_score(char *board, int *black_score, int *white_score);
void commit_selection(char *board, int i, int j, int current_player);
int set_board_selectables_and_score(char *board, int *black_score, int *white_score, int current_player);
int toggle_player(int in_player);

#endif