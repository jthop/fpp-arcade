#ifndef __FPPARCADE_BREAKOUT_
#define __FPPARCADE_BREAKOUT_

#include "FPPArcade.h"

class FPPBreakout : public FPPArcadeGame {
public:
    FPPBreakout(Json::Value &config);
    virtual ~FPPBreakout();
    
    virtual const std::string &getName() override;
    
    virtual void button(const std::string &button) override;
};


#endif
