#include <fpp-pch.h>

#include "FPPBreakout.h"
#include <array>
#include <random>

#include "overlays/PixelOverlay.h"
#include "overlays/PixelOverlayModel.h"
#include "overlays/PixelOverlayEffects.h"
#include <curl/curl.h>
#include <iostream>

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp){
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

FPPBreakout::FPPBreakout(Json::Value &config) : FPPArcadeGame(config) {
    std::srand(time(NULL));
}
FPPBreakout::~FPPBreakout() {
}


class Block {
public:
    int r = 255;
    int g = 255;
    int b = 255;

    float y = 0;
    float x = 0;
    float width = 0;
    float height = 0;
    
    float left() const { return x; }
    float top() const { return y; }
    float right() const { return x + width - 0.1; }
    float bottom() const { return y + height - 0.1; }

    void draw(PixelOverlayModel *m) {
        for (int xp = 0; xp < width; xp++) {
            for (int yp = 0; yp < height; yp++) {
                m->setOverlayPixelValue(xp + x, yp + y, r, g, b);
            }
        }
    }
    
    bool intersects(const Block &mB) const {
        return right() >= mB.left() && left() <= mB.right() &&
               bottom() >= mB.top() && top() <= mB.bottom();
    }
};

class Ball : public Block {
public:
    
    
    float directionX = 0.25;
    float directionY = 0.75;
    float speed = 1;
};


static std::array<uint32_t, 7> COLORS = {
    0xFF0000,
    0x00FF00,
    0x0000FF,
    0xFF00FF,
    0xFFFF00,
    0x00FFFF,
    0xFFFFFF,
};

class BreakoutEffect : public FPPArcadeGameEffect {
public:
    BreakoutEffect(PixelOverlayModel *m) : FPPArcadeGameEffect(m) {
        int w = m->getWidth();
        int h = m->getHeight();
        paddle.width = w / 8;
        paddle.x = (w - paddle.width) / 2;
        paddle.height = h / 64;
        if (paddle.height < 1) {
            paddle.height = 1;
        }
        paddle.y = h - 1 - paddle.height;
        
        
        int blockW = w / 20;
        if (blockW < 2) {
            blockW = 2;
        }
        int blockH = h / 32;
        if (blockH < 1) {
            blockH = 1;
        }
        
        int numBlockW = w / (blockW+1);
        int curY = blockH*2;
        int xOff = (w - numBlockW * (blockW+1)) / 2;
        for (int y = 0; y < COLORS.size(); y++, curY += (blockH+1) ) {
            for (int x = 0; x < numBlockW; x++) {
                Block b;
                b.x = xOff + x * (blockW+1);
                b.height = blockH;
                b.width = blockW;
                b.y = curY;
                b.r = (COLORS[y] >> 16) & 0xFF;
                b.g = (COLORS[y] >> 8) & 0xFF;
                b.b = COLORS[y] & 0xFF;
                blocks.push_back(b);
            }
        }
        
        ball.x = w / 2;
        ball.y = h * 2 / 3;
        ball.height = paddle.height;
        ball.width = paddle.height;
        ball.speed *= (float)paddle.height;
        CopyToModel();
    }
    ~BreakoutEffect() {
    }
    
    const std::string &name() const override {
        static std::string NAME = "Breakout";
        return NAME;
    }

    void moveBall() {
        ball.x += ball.directionX * ball.speed;
        ball.y += ball.directionY * ball.speed;
        
        if (ball.y < 0) {
            ball.directionY = std::fabs(ball.directionY);
            ball.y = 0;
        }
        if (ball.x < 0) {
            ball.directionX = std::fabs(ball.directionX);
            ball.x = 0;
        }
        if (ball.x >= model->getWidth()) {
            ball.directionX = -std::fabs(ball.directionX);
            ball.x = model->getWidth() - 1;
        }
        auto it = blocks.begin();
        while (it != blocks.end()) {
            if (it->intersects(ball)) {
                if (ball.right() <= (it->left() + 0.35)) {
                    ball.directionX = std::fabs(ball.directionX);
                } else if (ball.left() >= (it->right() - 0.35)) {
                    ball.directionX = std::fabs(ball.directionX);
                } else if (ball.bottom() <= (it->top()+0.5)) {
                    ball.directionY = -std::fabs(ball.directionY);
                } else {
                    ball.directionY = std::fabs(ball.directionY);
                }

                blocks.erase(it);
                return;
            }
            it++;
        }
    }

    void CopyToModel() {
        model->clearOverlayBuffer();
        for (auto &b : blocks) {
            b.draw(model);
        }
        paddle.draw(model);
        ball.draw(model);
        model->flushOverlayBuffer();
    }
    
    virtual int32_t update() override {
        if (!GameOn) {
            model->clearOverlayBuffer();
            model->flushOverlayBuffer();
            
            if (WaitingUntilOutput) {
                model->setState(PixelOverlayState(PixelOverlayState::PixelState::Disabled));
                return 0;
            }
            WaitingUntilOutput = true;
            return -1;
        }
        paddle.x += direction * paddle.height;
        if (paddle.x < 0) {
            paddle.x = 0;
        } else if ((paddle.x + paddle.width) >= model->getWidth()) {
            paddle.x = model->getWidth() - paddle.width;
        }
        moveBall();
        
        if (ball.intersects(paddle)) {
            float t = ((ball.x - paddle.x) / paddle.width) - 0.5f;
            ball.directionY = -std::fabs(ball.directionY);
            ball.directionX = t;
        }
        
        // make sure that length of dir stays at 1
        vec2_norm(ball.directionX, ball.directionY);

        CopyToModel();
        if (ball.y >= model->getHeight()) {
            //end game
            GameOn = false;
            float scl = paddle.height;
            outputString("GAME", (model->getWidth()-(8 * scl))/ 2 / scl, (model->getHeight()/2-(6 * scl)) / scl, 255, 255, 255, scl);
            outputString("OVER", (model->getWidth()-(8 * scl))/ 2 / scl, model->getHeight()/2 / scl, 255, 255, 255, scl);
            model->flushOverlayBuffer();

            // send curl notifying us game is over
            CURL * curl;
            CURLcode res;
            std::string readBuffer;

            curl = curl_easy_init();
            if(curl) {
                curl_easy_setopt(curl, CURLOPT_URL, "https://api.megatr.ee/api/games/breakout/callback?win=False");
                //curl_easy_setopt(curl, CURLOPT_URL, "http://10.10.2.5:8000/api/games/breakout/callback?win=False");
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
                res = curl_easy_perform(curl);
                curl_easy_cleanup(curl);
                std::cout << readBuffer << std::endl;
            }

            return 2000;
        }
        if (blocks.empty()) {
            GameOn = false;
            float scl = paddle.height;
            outputString("YOU", (model->getWidth()-(6 * scl))/ 2 / scl, (model->getHeight()/2-(6 * scl)) / scl, 255, 255, 255, scl);
            outputString("WIN", (model->getWidth()-(6 * scl))/ 2 / scl, model->getHeight()/2 / scl, 255, 255, 255, scl);
            model->flushOverlayBuffer();

            // send curl notifying us game is over
            CURL * curl;
            CURLcode res;
            std::string readBuffer;

            curl = curl_easy_init();
            if(curl) {
                curl_easy_setopt(curl, CURLOPT_URL, "https://api.megatr.ee/api/games/breakout/callback?win=True");
                //curl_easy_setopt(curl, CURLOPT_URL, "http://10.10.2.50:8000/api/games/breakout/callback?win=True");
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
                res = curl_easy_perform(curl);
                curl_easy_cleanup(curl);

                std::cout << readBuffer << std::endl;
            }

            return 2000;
        }
        
        return 50;
    }
    void vec2_norm(float& x, float &y) {
        // sets a vectors length to 1 (which means that x + y == 1)
        float length = std::sqrt((x * x) + (y * y));
        if (length != 0.0f) {
            length = 1.0f / length;
            x *= length;
            y *= length;
        }
    }
    void button(const std::string &button) {
        if (button == "Left - Pressed") {
            direction = -1;
        } else if (button == "Left - Released") {
            direction = 0;
        } else if (button == "Right - Pressed") {
            direction = 1;
        } else if (button == "Right - Released") {
            direction = 0;
        }
    }
    
    Ball ball;
    Block paddle;
    std::list<Block> blocks;
    
    int direction = 0;
    
    bool GameOn = true;
    bool WaitingUntilOutput = false;

};

const std::string &FPPBreakout::getName() {
    static const std::string name = "Breakout";
    return name;
}

void FPPBreakout::button(const std::string &button) {
    PixelOverlayModel *m = PixelOverlayManager::INSTANCE.getModel(modelName);
    if (m != nullptr) {
        BreakoutEffect *effect = dynamic_cast<BreakoutEffect*>(m->getRunningEffect());
        if (!effect) {
            if (findOption("overlay", "Overwrite") == "Transparent") {
                m->setState(PixelOverlayState(PixelOverlayState::PixelState::TransparentRGB));
            } else {
                m->setState(PixelOverlayState(PixelOverlayState::PixelState::Enabled));
            }
            effect = new BreakoutEffect(m);
            m->setRunningEffect(effect, 50);
        } else {
            effect->button(button);
        }
    }
}
