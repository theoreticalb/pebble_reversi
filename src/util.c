
#include <pebble.h>
#include "util.h"

//Some utility functions

bool flip_coin()
{
  return ( (rand() % 2) == 1);
}