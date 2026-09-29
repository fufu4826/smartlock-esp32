#pragma once
#include <stdint.h>
class TFT_eSPI {
 public:
  bool down=false;
  bool getTouch(uint16_t* x,uint16_t* y,uint16_t){*x=100;*y=100;return down;}
};
