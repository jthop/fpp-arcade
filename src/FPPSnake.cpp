#include <fpp-pch.h>

#include "FPPSnake.h"
#include <array>
#include <random>
#include <curl/curl.h>
#include <iostream>

#include "overlays/PixelOverlay.h"
#include "overlays/PixelOverlayModel.h"
#include "overlays/PixelOverlayEffects.h"

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp){
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}


FPPSnake::FPPSnake(Json::Value &config) : FPPArcadeGame(config) {
    std::srand(time(NULL));
}
FPPSnake::~FPPSnake() {
}

class SnakeEffect : public FPPArcadeGameEffect {
public:
    SnakeEffect(int sc, PixelOverlayModel *m) : FPPArcadeGameEffect(m) {
        m->getSize(cols, rows);
        scale = sc;
        cols /= sc;
        rows /= sc;
        
        offsetX = 0;
        offsetY = 0;
        
        direction = 0;
        snake.push_back(std::pair<int, int>(cols / 2, rows / 2));
        snake.push_back(std::pair<int, int>(cols / 2 + 1, rows / 2));
        snake.push_back(std::pair<int, int>(cols / 2 + 2, rows / 2));
        
        addFood();
        addFood();
        addFood();
    }
    ~SnakeEffect() {
    }
    
    void addFood() {
        bool OK = false;
        int x, y;
        while (!OK) {
            OK = true;
            x = rand() % (cols - 2) + 1;
            y = rand() % (rows - 2) + 1;
            for (auto &a : food) {
                if (a.first == x && a.second == y) {
                    OK = false;
                }
            }
            for (auto &a : snake) {
                if (a.first == x && a.second == y) {
                    OK = false;
                }
            }
        }
        food.push_back({x, y});
    }
    const std::string &name() const override {
        static std::string NAME = "Snake";
        return NAME;
    }

    
    void CopyToModel() {
        model->clearOverlayBuffer();
        for (auto &a : food) {
            outputPixel(a.first, a.second, 0, 255, 0);
        }
        int size = snake.size();
        int count = 0;
        for (auto &a : snake) {
            if (count == 0) {
                outputPixel(a.first, a.second, 0, 0, 255);
            } else {
                int c = size - count;
                c *= 200;
                c /= size;
                c += 50;
                outputPixel(a.first, a.second, c, 0, 0);
            }
            count++;
        }
        for (int r = 0; r < rows; r++) {
            outputPixel(0, r, 128, 128, 128);
            outputPixel(cols-1, r, 128, 128, 128);
        }
        for (int c = 0; c < cols; c++) {
            outputPixel(c, 0, 128, 128, 128);
            outputPixel(c, rows-1, 128, 128, 128);
        }
    }
    
    void moveSnake() {
        int x = snake.front().first;
        int y = snake.front().second;
        switch (direction) {
            case 0:
                --x;
                break;
            case 1:
                --y;
                break;
            case 2:
                ++x;
                break;
            case 3:
                ++y;
                break;
        }


        bool foundFood = false;
        for (auto &a : food) {
            if (a.first == x && a.second == y) {
                foundFood = true;
            }
        }
        if (!foundFood) {
            snake.pop_back();
            for (auto &a : snake) {
                if (a.first == x && a.second == y) {
                    GameOn = false;
                }
            }
        } else {
            food.remove({x, y});
            addFood();
        }
        snake.push_front({x, y});
        if (x == 0 || y == 0 || x == (cols-1) || y == (rows-1)) {
            //hit a wall
            GameOn = false;
        }
        
    }
    
    virtual int32_t update() override {
        if (!GameOn) {
            if (WaitingUntilOutput) {
                model->setState(PixelOverlayState(PixelOverlayState::PixelState::Disabled));
                return 0;
            }
            model->clearOverlayBuffer();
            model->flushOverlayBuffer();
            WaitingUntilOutput = true;
            return -1;
        }
        moveSnake();
        CopyToModel();
        if (!GameOn) {
            GameOn = false;
            outputString("GAME", cols/ 2 - 8, rows/2-9);
            outputString("OVER", cols/ 2 - 8, rows/2-3);
            char buf[25];
            sprintf(buf, "%d", (uint32_t)snake.size());
            outputString(buf, (cols)/ 2 - 4, rows/2+3);
            model->flushOverlayBuffer();

            // send curl notifying us game is over
            CURL * curl;
            CURLcode res;
            std::string readBuffer;

            curl = curl_easy_init();
            if(curl) {
                //char url[71] = "https://api.megatr.ee/api/games/cb?g=snake&s=";
                char url[71] = "http://10.10.2.5:8000/api/games/cb?g=snake&s=";
                strcat(url, buf);
                curl_easy_setopt(curl, CURLOPT_URL, url);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
                res = curl_easy_perform(curl);
                curl_easy_cleanup(curl);

                std::cout << readBuffer << std::endl;
            }


            return 2000;
        }
        model->flushOverlayBuffer();
        return timer;
    }
    
    
    void button(const std::string &button) {
        if (button == "Left - Pressed") {
            direction = 0;
        } else if (button == "Right - Pressed") {
            direction = 2;
        } else if (button == "Up - Pressed") {
            direction = 1;
        } else if (button == "Down - Pressed") {
            direction = 3;
        }
    }
    
    
    std::list<std::pair<int, int>> food;
    std::list<std::pair<int, int>> snake;
    int direction = 0;

    int rows = 20;
    int cols = 20;
    
    bool GameOn = true;
    bool WaitingUntilOutput = false;
    long long timer = 100;
};
const std::string &FPPSnake::getName() {
    static const std::string name = "Snake";
    return name;
}


void FPPSnake::button(const std::string &button) {
    PixelOverlayModel *m = PixelOverlayManager::INSTANCE.getModel(modelName);
    if (m != nullptr) {
        SnakeEffect *effect = dynamic_cast<SnakeEffect*>(m->getRunningEffect());
        if (!effect) {
            if (findOption("overlay", "Overwrite") == "Transparent") {
                m->setState(PixelOverlayState(PixelOverlayState::PixelState::TransparentRGB));
            } else {
                m->setState(PixelOverlayState(PixelOverlayState::PixelState::Enabled));
            }
            int pixelScaling = std::stoi(findOption("Pixel Scaling", "1"));
            effect = new SnakeEffect(pixelScaling, m);
            m->setRunningEffect(effect, 50);
        } else {
            effect->button(button);
        }
    }
}
