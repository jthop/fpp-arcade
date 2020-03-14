#ifndef __FPPARCADE_TETRIS_
#define __FPPARCADE_TETRIS_

#include "FPPArcade.h"

class FPPTetris : public FPPArcadeGame {
public:
    FPPTetris(Json::Value &config);
    virtual ~FPPTetris();
    
    virtual const std::string &getName() override;
    
    virtual void button(const std::string &button) override;
};


#endif
