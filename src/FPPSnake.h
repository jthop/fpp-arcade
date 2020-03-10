#ifndef __FPPARCADE_SNAKE_
#define __FPPARCADE_SNAKE_

#include "FPPArcade.h"

class FPPSnake : public FPPArcadeGame {
public:
    FPPSnake(Json::Value &config);
    virtual ~FPPSnake();
    
    virtual void button(const std::string &button) override;
};


#endif
