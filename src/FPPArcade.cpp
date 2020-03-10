#include <unistd.h>
#include <ifaddrs.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <list>
#include <vector>
#include <jsoncpp/json/json.h>
#include <cmath>

#include "FPPArcade.h"

#include "commands/Commands.h"
#include "common.h"
#include "settings.h"
#include "Plugin.h"
#include "log.h"

#include "overlays/PixelOverlayModel.h"

#include "FPPTetris.h"
#include "FPPPong.h"
#include "FPPSnake.h"


std::vector<std::string> BUTTONS({
    "Up - Pressed", "Up - Released",
    "Down - Pressed", "Down - Released",
    "Left - Pressed", "Left - Released",
    "Right - Pressed", "Right - Released",
    "Fire - Pressed", "Fire - Released",
});

class FPPArcadePlugin;
class FPPArcadeCommand : public Command {
public:
    FPPArcadeCommand(FPPArcadePlugin *p) : Command("FPP Arcade Button"), plugin(p) {
        args.push_back(CommandArg("Button", "string", "Button").setContentList(BUTTONS));
        args.push_back(CommandArg("Target", "string", "Target").setContentListUrl("api/models?simple=true", true));
    }
    
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
    FPPArcadePlugin *plugin;
};

FPPArcadeGame::FPPArcadeGame(Json::Value &c) : modelName(c["model"].asString()), config(c) {
}

std::string FPPArcadeGame::findOption(const std::string &s, const std::string &def) {
    if (config.isMember("options")) {
        for (int x = 0; x < config["options"].size(); x++) {
            if (config["options"][x]["name"].asString() == s) {
                return config["options"][x]["value"].asString();
            }
        }
    }
    return def;
}


static const std::map<uint8_t, std::vector<uint8_t>> LETTERS = {
    {'G', {1, 1, 1, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 1, 1}},
    {'A', {1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1}},
    {'M', {1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1}},
    {'E', {1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1}},
    {'O', {1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1}},
    {'V', {1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 0}},
    {'R', {1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1}},

    {'0', {1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1}},
    {'1', {0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0}},
    {'2', {1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1}},
    {'3', {1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1}},
    {'4', {1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0, 1}},
    {'5', {1, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 1}},
    {'6', {1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1}},
    {'7', {1, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1}},
    {'8', {1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1}},
    {'9', {1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0, 1}},

    {':', {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0}}
};

class Letter {
public:
    Letter(int l) {
        set(l);
    }
    
    uint8_t get(int x, int y) {
        return data[y * 3 + x];
    }
    void set(int x, int y, uint8_t d) {
        data[y * 3 + x] = d;
    }
private:
    void set(int l) {
        auto it = LETTERS.find(l);
        if (it != LETTERS.end()) {
            data = it->second;
        }
    }
    
    std::vector<uint8_t> data;
};
FPPArcadeGameEffect::FPPArcadeGameEffect(PixelOverlayModel *m) : RunningEffect(m), scale(1), offsetX(0), offsetY(0) {
}
FPPArcadeGameEffect::~FPPArcadeGameEffect() {
}
void FPPArcadeGameEffect::outputPixel(int x, int y, int r, int g, int b, int scl) {
    if (scl == -1) {
        scl = scale;
    }
    x *= scl;
    x += offsetX;
    y *= scl;
    y += offsetY;
    for (int nx = 0; nx < scl; nx++) {
        for (int ny = 0; ny < scl; ny++) {
            model->setOverlayPixelValue(x + nx, y + ny, r, g, b);
        }
    }
}
void FPPArcadeGameEffect::outputLetter(int x, int y, char l, int r, int g, int b, int scl) {
    Letter letter(l);
    for (int nx = 0; nx < 3; nx++) {
        for (int ny = 0; ny < 5; ny++) {
            if (letter.get(nx, ny)) {
                outputPixel(x + nx, y + ny, r, g, b, scl);
            }
        }
    }
}
void FPPArcadeGameEffect::outputString(const std::string &s, int x, int y, int r, int g, int b, int scl) {
    for (auto ch : s) {
        outputLetter(x, y, ch, r, g, b, scl);
        x += 4;
    }
}



class FPPArcadePlugin : public FPPPlugin {
public:
    
    FPPArcadePlugin() : FPPPlugin("fpp-arcade") {
        LogInfo(VB_PLUGIN, "Initializing Arcade Plugin\n");
        
        if (FileExists("/home/fpp/media/config/plugin.fpp-arcade.json")) {
            Json::Value root;
            if (LoadJsonFromFile("/home/fpp/media/config/plugin.fpp-arcade.json", root)
                && root.isMember("games")) {
                for (int x = 0; x < root["games"].size(); x++) {
                    if (root["games"][x]["enabled"].asBool()) {
                        std::string model = root["games"][x]["model"].asString();
                        if (games[model] == nullptr) {
                            games[model] = createGame(root["games"][x]);
                        }
                    }
                }
            }
        }
    }
    virtual ~FPPArcadePlugin() {
        for (auto & a : games) {
            if (a.second) {
                delete a.second;
            }
        }
            
    }
    
    FPPArcadeGame *createGame(Json::Value &config) {
        std::string game = config["game"].asString();
        if (game == "Tetris") {
            return new FPPTetris(config);
        }
        if (game == "Pong") {
            return new FPPPong(config);
        }
        if (game == "Snake") {
            return new FPPSnake(config);
        }
        return nullptr;
    }
    

    virtual std::unique_ptr<Command::Result> runCommand(const std::vector<std::string> &args) {
        const std::string button = args[0];
        const std::string model = args.size() > 1 ? args[1] : "";
        if (model != "") {
            if (games[model]) {
                games[model]->button(button);
            }
        } else {
            for (auto &a : games) {
                if (a.second) {
                    a.second->button(button);
                }
            }
        }
        return std::make_unique<Command::Result>("FPP Arcade Button Processed");
    }

    virtual void addControlCallbacks(std::map<int, std::function<bool(int)>> &callbacks) {
        CommandManager::INSTANCE.addCommand(new FPPArcadeCommand(this));
    }
    
    std::map<std::string, FPPArcadeGame*> games;
};


std::unique_ptr<Command::Result> FPPArcadeCommand::run(const std::vector<std::string> &args) {
    return plugin->runCommand(args);
}


extern "C" {
    FPPPlugin *createPlugin() {
        return new FPPArcadePlugin();
    }
}
