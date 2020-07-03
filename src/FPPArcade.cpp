#include <unistd.h>
#include <ifaddrs.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/joystick.h>
#include <arpa/inet.h>
#include <cstring>
#include <list>
#include <vector>
#include <jsoncpp/json/json.h>
#include <httpserver.hpp>
#include <cmath>
#include <fcntl.h>

#include "FPPArcade.h"

#include "commands/Commands.h"
#include "common.h"
#include "settings.h"
#include "Plugin.h"
#include "log.h"

#include "overlays/PixelOverlayModel.h"
#include "overlays/PixelOverlay.h"

#include "FPPTetris.h"
#include "FPPPong.h"
#include "FPPSnake.h"
#include "FPPBreakout.h"


std::vector<std::string> BUTTONS({
    "Up - Pressed", "Up - Released",
    "Down - Pressed", "Down - Released",
    "Left - Pressed", "Left - Released",
    "Right - Pressed", "Right - Released",
    "Up/Left - Pressed", "Up/Left - Released",
    "Up/Right - Pressed", "Up/Right - Released",
    "Down/Left - Pressed", "Down/Left - Released",
    "Down/Right - Pressed", "Down/Right - Released",
    "Fire - Pressed", "Fire - Released",
    "Select - Pressed", "Select - Released",
    "Start - Pressed", "Start - Released",
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

bool FPPArcadeGame::isRunning() {
    PixelOverlayModel *m = PixelOverlayManager::INSTANCE.getModel(modelName);
    if (m != nullptr) {
        FPPArcadeGameEffect *effect = dynamic_cast<FPPArcadeGameEffect*>(m->getRunningEffect());
        if (effect) {
            return true;
        }
    }
    return false;
}

class ClearRunningEffect : public RunningEffect {
public:
    ClearRunningEffect(PixelOverlayModel *m) : RunningEffect(m) {}
    
    const std::string &name() const override {
        static std::string NAME = "Clear";
        return NAME;
    }
    
    virtual int32_t update() {
        if (calledOnce) {
            model->setState(PixelOverlayState(PixelOverlayState::PixelState::Disabled));
            return 0;
        }
        model->clearOverlayBuffer();
        model->flushOverlayBuffer();
        calledOnce = true;
        return -1;
    }
    bool calledOnce = false;
};

void FPPArcadeGame::stop() {
    PixelOverlayModel *m = PixelOverlayManager::INSTANCE.getModel(modelName);
    if (m != nullptr) {
        m->setRunningEffect(new ClearRunningEffect(m), 10);
    }
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

    {'Y', {1, 0, 1, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0}},
    {'U', {1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1}},
    {'W', {1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1}},
    {'I', {0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0}},
    {'N', {1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1}},

    
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



class FPPArcadePlugin : public FPPPlugin , public httpserver::http_resource {
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
                        games[model].push_back(createGame(root["games"][x]));
                    }
                }
            }
        }
        
        if (FileExists("/home/fpp/media/config/joysticks.json")) {
            Json::Value root;
            if (LoadJsonFromFile("/home/fpp/media/config/joysticks.json", root)) {
                for (int x = 0; x < root.size(); x++) {
                    if (root[x]["enabled"].asBool()) {
                        std::string controller = root[x]["controller"].asString();
                        int button =  root[x]["button"].asInt();
                        std::string ev = controller + ":" + std::to_string(button);
                        events[ev + ":1"] = root[x]["pressed"];
                        events[ev + ":0"] = root[x]["released"];
                    }
                }
            }
        }
    }
    virtual ~FPPArcadePlugin() {
        for (auto & a : games) {
            for (auto &g : a.second) {
                delete g;
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
        if (game == "Breakout") {
            return new FPPBreakout(config);
        }
        return nullptr;
    }
    

    void handleButton(std::list<FPPArcadeGame *> &games, const std::string &button, const std::vector<std::string> &args) {
        if (button == "Start - Pressed" || button == "Select - Pressed") {
            if (games.front()->isRunning()) {
                games.front()->stop();
            }
        }
        if (button == "Start - Released" || button == "Select - Released") {
            return;
        }
        if (button == "Select - Pressed") {
            FPPArcadeGame *g = games.front();
            games.pop_front();
            games.push_back(g);
        } else {
            games.front()->button(button);
        }
    }
    virtual std::unique_ptr<Command::Result> runCommand(const std::vector<std::string> &args) {
        const std::string button = args[0];
        const std::string model = args.size() > 1 ? args[1] : "";
        if (model != "") {
            if (!games[model].empty()) {
                handleButton(games[model], button, args);
            }
        } else {
            for (auto &a : games) {
                if (!a.second.empty()) {
                    handleButton(a.second, button, args);
                }
            }
        }
        return std::make_unique<Command::Result>("FPP Arcade Button Processed");
    }
    
    
    
    virtual const std::shared_ptr<httpserver::http_response> render_GET(const httpserver::http_request &req) override {
        int plen = req.get_path_pieces().size();
        if (plen == 2) {
            if (req.get_path_pieces()[1] == "controllers") {
                std::string s = "[";
                for (auto &j : joysticks) {
                    if (s.size() > 2) {
                        s += ",\n";
                    }
                    s += "  {\n";
                    s += "    \"name\": \"";
                    s += j.name;
                    s += "\",\n    \"buttons\": ";
                    s += std::to_string(j.numButtons);
                    s += ",\n    \"axis\": ";
                    s += std::to_string(j.numAxis);
                    s += "\n  }";
                }
                s += "\n]";
                return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(s, 200, "application/json"));
            } else  if (req.get_path_pieces()[1] == "events") {
                std::string v;
                for (auto &a : lastEvents) {
                    v += a + "\n";
                }
                return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(v, 200));
            }
        }
        return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("Not found", 404));
    }
    void registerApis(httpserver::webserver *m_ws) override {
        m_ws->register_resource("/arcade", this, true);
    }

    virtual void addControlCallbacks(std::map<int, std::function<bool(int)>> &callbacks) {
        CommandManager::INSTANCE.addCommand(new FPPArcadeCommand(this));
        
        for (int x = 0; x < 10; x++) {
            std::string js = "/dev/input/js" + std::to_string(x);
            if (FileExists(js)) {
                int i = open(js.c_str(), O_RDONLY | O_NONBLOCK);
                joysticks.push_back(Joystick(i));
            }
        }
        
        for (auto & a : joysticks) {
            callbacks[a.file] = [&a, this] (int f) {
                struct js_event ev;
                while (read(f, &ev, sizeof(ev)) > 0) {
                    if (!(ev.type & JS_EVENT_INIT)) {
                        std::string s = a.name;
                        s += " - ";
                        s += "button: " + std::to_string(ev.number);
                        s += ", value: " + std::to_string(ev.value);
                        s += ", type: " + std::to_string(ev.type);
                        lastEvents.push_back(s);
                        while (lastEvents.size() > 20) {
                            lastEvents.pop_front();
                        }
                        
                        std::string evnt = a.name + ":" + std::to_string(ev.number) + ":" + std::to_string(ev.value);
                        processEvent(evnt);
                    }
                }
                return false;
            };
        }
        
    }
    
    void processEvent(const std::string &ev) {
        const auto &f = events.find(ev);
        if (f != events.end()) {
            if (f->second["command"] != "") {
                CommandManager::INSTANCE.run(f->second);
            }
        }
    }
    
    std::map<std::string, std::list<FPPArcadeGame*>> games;
    
    class Joystick {
    public:
        Joystick(int f) : file(f) {
            char buf[256] = {0};
            ioctl(file, JSIOCGNAME(sizeof(buf)), buf);
            name = buf;
            TrimWhiteSpace(name);

            char tmp;
            ioctl(file, JSIOCGAXES, &tmp);
            numAxis = tmp;
            ioctl(file, JSIOCGBUTTONS, &tmp);
            numButtons = tmp;
        }
        Joystick(Joystick &&j) : file(j.file), name(j.name), numButtons(j.numButtons), numAxis(j.numAxis) {
            j.file = -1;
        }
        ~Joystick() {
            if (file != -1) {
                close(file);
            }
        }
        std::string name;
        int numButtons = 0;
        int numAxis = 0;
        int file;
    };
    
    std::list<Joystick> joysticks;
    std::list<std::string> lastEvents;
    std::map<std::string, Json::Value> events;
};


std::unique_ptr<Command::Result> FPPArcadeCommand::run(const std::vector<std::string> &args) {
    return plugin->runCommand(args);
}


extern "C" {
    FPPPlugin *createPlugin() {
        return new FPPArcadePlugin();
    }
}
