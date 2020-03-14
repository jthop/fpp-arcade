#ifndef __FPPARCADE_PONG_
#define __FPPARCADE_PONG_

#include "FPPArcade.h"

class FPPPong : public FPPArcadeGame {
public:
    FPPPong(Json::Value &config);
    virtual ~FPPPong();
    
    virtual const std::string &getName() override;
    
    virtual void button(const std::string &button) override;
};


#endif
