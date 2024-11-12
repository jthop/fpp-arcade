#include <fpp-pch.h>


#ifdef PLATFORM_OSX
#define USE_SDL_CONTROLLERS
#else
//#define USE_SDL_CONTROLLERS
#endif

#include <unistd.h>
#include <ifaddrs.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#ifndef USE_SDL_CONTROLLERS
#include <linux/joystick.h>
#else
extern "C" {
#include <SDL2/SDL.h>
}
#endif
#include <arpa/inet.h>
#include <cstring>
#include <list>
#include <vector>
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

#include <curl/curl.h>
#include <iostream>

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp){
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}


static std::vector<std::string> BUTTONS({
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

static std::vector<std::string> AXIS({
    "Up -> Down", "Left -> Right", "Down -> Up", "Right -> Left"
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

class FPPArcadeAxisCommand : public Command {
public:
    FPPArcadeAxisCommand(FPPArcadePlugin *p) : Command("FPP Arcade Axis"), plugin(p) {
        args.push_back(CommandArg("Axis", "string", "Axis").setContentList(AXIS));
        args.push_back(CommandArg("Target", "string", "Target").setContentListUrl("api/models?simple=true", true));
        args.push_back(CommandArg("Value", "int", "Value", true).setDefaultValue("0").setAdjustable().setRange(-32767, 32767));
    }
    
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
    FPPArcadePlugin *plugin;
};

class FPPArcadeSelectGameCommand : public Command {
public:
    FPPArcadeSelectGameCommand(FPPArcadePlugin *p) : Command("FPP Arcade Select Game"), plugin(p) {
        args.push_back(CommandArg("Game", "int", "Game", true).setDefaultValue("1").setAdjustable().setRange(1, 100));
        args.push_back(CommandArg("Target", "string", "Target").setContentListUrl("api/models?simple=true", true));
    }
    
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
    FPPArcadePlugin *plugin;
};

FPPArcadeGame::FPPArcadeGame(Json::Value &c) : modelName(c["model"].asString()), config(c), idx(0) {
    lastValues[0] = 0; lastValues[1] = 0;
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
    
    virtual int32_t update() override {
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
//default behavior will map the axis directions to button presses
void FPPArcadeGame::axis(const std::string &axis, int value) {
    std::string btn = "";
    if (axis == AXIS[2]) { // DOWN->UP
        if (value == 0 && lastValues[0] < 0) {
            btn = "Down - Released";
        } else if (value == 0 && lastValues[0] > 0) {
            btn = "Up - Released";
        } else if (value != 0) {
            btn = value > 0 ? "Up - Pressed" : "Down - Pressed";
        }
        lastValues[0] = value;
    } else if (axis == AXIS[1]) { // left -> right
        if (value == 0 && lastValues[1] < 0) {
            btn = "Left - Released";
        } else if (value == 0 && lastValues[1] > 0) {
            btn = "Right - Released";
        } else if (value != 0) {
            btn = value > 0 ? "Right - Pressed" : "Left - Pressed";
        }
        lastValues[1] = value;
    } else if (axis == AXIS[0]) { // up -> down
        if (value == 0 && lastValues[0] < 0) {
            btn = "Up - Released";
        } else if (value == 0 && lastValues[0] > 0) {
            btn = "Down - Released";
        } else if (value != 0) {
            btn = value < 0 ? "Up - Pressed" : "Down - Pressed";
        }
        lastValues[0] = value;
    } else if (axis == AXIS[3]) { // right -> left
        if (value == 0 && lastValues[1] < 0) {
            btn = "Right - Released";
        } else if (value == 0 && lastValues[1] > 0) {
            btn = "Left - Released";
        } else if (value != 0) {
            btn = value < 0 ? "Right - Pressed" : "Left - Pressed";
        }
        lastValues[1] = value;
    }
    if (btn != "") {
        button(btn);
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
    if (config.isMember(s)) {
        return config[s].asString();
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

        CURL * curl;
        CURLcode res;
        std::string readBuffer;

        curl = curl_easy_init();
        if(curl) {
            curl_easy_setopt(curl, CURLOPT_URL, "https://api.megatr.ee/api/games/init");
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
            res = curl_easy_perform(curl);
            curl_easy_cleanup(curl);

            std::cout << readBuffer << std::endl;
        }
        
        int idx = 0;
        if (FileExists(FPP_DIR_CONFIG("/plugin.fpp-arcade.json"))) {
            Json::Value root;
            if (LoadJsonFromFile(FPP_DIR_CONFIG("/plugin.fpp-arcade.json"), root)
                && root.isMember("games")) {
                for (int x = 0; x < root["games"].size(); x++) {
                    if (root["games"][x]["enabled"].asBool()) {
                        std::string model = root["games"][x]["model"].asString();
                        games[model].push_back(createGame(root["games"][x]));
                        if (games[model].back() != nullptr) {
                            games[model].back()->setIdx(++idx);
                        }
                    }
                }
            }
        }
        
        if (FileExists(FPP_DIR_CONFIG("/joysticks.json"))) {
            Json::Value root;
            if (LoadJsonFromFile(FPP_DIR_CONFIG("/joysticks.json"), root)) {
                for (int x = 0; x < root.size(); x++) {
                    if (root[x]["enabled"].asBool()) {
                        std::string controller = root[x]["controller"].asString();
                        
                        if (root[x].isMember("button")) {
                            int button =  root[x]["button"].asInt();
                            std::string ev = controller + ":" + std::to_string(button);
                            events[ev + ":1"] = root[x]["pressed"];
                            events[ev + ":0"] = root[x]["released"];
                        } else if (root[x].isMember("axis")) {
                            int button =  root[x]["axis"].asInt();
                            std::string ev = controller + ":a" + std::to_string(button);
                            events[ev] = root[x]["command"];
                        }
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
    std::unique_ptr<Command::Result>  selectGame(const std::vector<std::string> &args) {
        int idx = std::atoi(args[0].c_str());
        const std::string model = args.size() > 1 ? args[1] : "";
        if (games[model].front()->isRunning()) {
            games[model].front()->stop();
        }
        int max = games[model].size();
        for (int x = 0; x < max; x++) {
            FPPArcadeGame *g = games[model].front();
            if (g->getIdx() == idx) {
                return std::make_unique<Command::Result>("FPP Arcade Game " + g->getName() + " Selected");
            } else {
                games[model].pop_front();
                games[model].push_back(g);
            }
        }
        return std::make_unique<Command::ErrorResult>("FPP Arcade Could not find game matching " + args[0] + " for model " + model);
    }
    virtual std::unique_ptr<Command::Result> runAxisCommand(const std::vector<std::string> &args) {
        const std::string axis = args[0];
        const std::string model = args.size() > 1 ? args[1] : "";
        int value = args.size() > 2 ? std::atoi(args[2].c_str()) : 0;

        if (model != "") {
            if (!games[model].empty()) {
                games[model].front()->axis(axis, value);
            }
        } else {
            for (auto &a : games) {
                if (!a.second.empty()) {
                    a.second.front()->axis(axis, value);
                }
            }
        }
        return std::make_unique<Command::Result>("FPP Arcade Axis Processed");
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
    
    virtual HTTP_RESPONSE_CONST std::shared_ptr<httpserver::http_response> render_GET(const httpserver::http_request &req) override {
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

#ifdef USE_SDL_CONTROLLERS
    static int SDLCALL controller_event_filter(void *userdata, SDL_Event * event) {
        FPPArcadePlugin *p = (FPPArcadePlugin*)userdata;
        return p->handleSDLControllerEvent(event);
    }
    int handleSDLControllerEvent(SDL_Event * event) {
        switch (event->type) {
            case SDL_CONTROLLERAXISMOTION: {
                SDL_ControllerAxisEvent *ae = (SDL_ControllerAxisEvent*)event;
                for (auto &j : joysticks) {
                    if (j.joystickId == ae->which) {
                        std::string s = j.name;
                        s += " - ";
                        s += "axis: " + std::to_string(ae->axis);
                        s += ", value: " + std::to_string(ae->value);
                        lastEvents.push_back(s);
                        while (lastEvents.size() > 20) {
                            lastEvents.pop_front();
                        }
                        std::string evnt = j.name + ":a" + std::to_string(ae->axis);
                        processEvent(evnt, ae->value);
                    }
                }
                return 0;
            }
            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP: {
                SDL_ControllerButtonEvent *be = (SDL_ControllerButtonEvent*)event;
                for (auto &j : joysticks) {
                    if (j.joystickId == be->which) {
                        std::string s = j.name;
                        s += " - ";
                        s += "button: " + std::to_string(be->button);
                        s += ", value: " + std::to_string(be->state);
                        lastEvents.push_back(s);
                        while (lastEvents.size() > 20) {
                            lastEvents.pop_front();
                        }
                        std::string evnt = j.name + ":" + std::to_string(be->button) + ":" + std::to_string(be->state);
                        processEvent(evnt, be->state);
                    }
                }
                return 0;
            }
        }
        return 1;  // let all events be added to the queue since we always return 1.
    }
#endif

    void checkUniqueNames() {
        std::map<std::string, int> names;
        for (auto &j : joysticks) {
            names[j.name]++;
            if (names[j.name] != 1) {
                j.name = j.name + " - " + std::to_string(names[j.name]);
            }
        }
    }
    virtual void addControlCallbacks(std::map<int, std::function<bool(int)>> &callbacks) override {
        CommandManager::INSTANCE.addCommand(new FPPArcadeCommand(this));
        CommandManager::INSTANCE.addCommand(new FPPArcadeAxisCommand(this));
        CommandManager::INSTANCE.addCommand(new FPPArcadeSelectGameCommand(this));

#ifdef USE_SDL_CONTROLLERS
        SDL_Init(SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS);
        SDL_SetEventFilter(controller_event_filter, this);
        for (int x = 0; x < SDL_NumJoysticks(); x++) {
            if (SDL_IsGameController(x)) {
                joysticks.emplace_back(Joystick(x));
            }
        }
        checkUniqueNames();
#else
        for (int x = 0; x < 10; x++) {
            std::string js = "/dev/input/js" + std::to_string(x);
            if (FileExists(js)) {
                int i = open(js.c_str(), O_RDONLY | O_NONBLOCK);
                joysticks.emplace_back(Joystick(i));
            }
        }
        checkUniqueNames();
        for (auto & a : joysticks) {
            callbacks[a.file] = [&a, this] (int f) {
                struct js_event ev;
                while (read(f, &ev, sizeof(ev)) > 0) {
                    if (!(ev.type & JS_EVENT_INIT)) {
                        std::string s = a.name;
                        s += " - ";
                        if (ev.type == 1) {
                            s += "button: " + std::to_string(ev.number);
                        } else if (ev.type == 2) {
                            s += "axis: " + std::to_string(ev.number);
                        }
                        s += ", value: " + std::to_string(ev.value);
                        lastEvents.push_back(s);
                        while (lastEvents.size() > 20) {
                            lastEvents.pop_front();
                        }
                        
                        std::string evnt = a.name + ":" + (ev.type == 2 ? "a" : "") + std::to_string(ev.number);
                        if (ev.type == 1) {
                            evnt += ":" + std::to_string(ev.value);
                        }
                        processEvent(evnt, ev.value);
                    }
                }
                return false;
            };
        }
#endif
    }
    
    void processEvent(const std::string &ev, int value) {
        const auto &f = events.find(ev);
        if (f != events.end()) {
            if (f->second["command"] != "") {
                if (f->second["command"] == "FPP Arcade Axis") {
                    Json::Value val = f->second;
                    val["args"][2] = std::to_string(value);
                    CommandManager::INSTANCE.run(val);
                } else {
                    CommandManager::INSTANCE.run(f->second);
                }
            }
        }
    }
    
    std::map<std::string, std::list<FPPArcadeGame*>> games;
    
    class Joystick {
    public:
        Joystick(int f) : file(f) {
#ifndef USE_SDL_CONTROLLERS
            char buf[256] = {0};
            ioctl(file, JSIOCGNAME(sizeof(buf)), buf);
            name = buf;
            TrimWhiteSpace(name);

            char tmp;
            ioctl(file, JSIOCGAXES, &tmp);
            numAxis = tmp;
            ioctl(file, JSIOCGBUTTONS, &tmp);
            numButtons = tmp;
#else
            controller = SDL_GameControllerOpen(f);
            joystickId = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));
            name = SDL_GameControllerName(controller);
            numAxis = SDL_JoystickNumAxes(SDL_GameControllerGetJoystick(controller));
            numButtons = std::max(15, SDL_JoystickNumButtons(SDL_GameControllerGetJoystick(controller)));
#endif
        }
        Joystick(Joystick &&j) : file(j.file), name(j.name), numButtons(j.numButtons), numAxis(j.numAxis), controller(j.controller), joystickId(j.joystickId) {
            j.file = -1;
            j.controller = nullptr;
        }
        ~Joystick() {
            if (file != -1) {
                close(file);
            }
#ifdef USE_SDL_CONTROLLERS
            if (controller) {
                SDL_GameControllerClose(controller);
            }
#endif
        }
#ifdef USE_SDL_CONTROLLERS
        SDL_GameController *controller = nullptr;
        SDL_JoystickID joystickId = 0;
#else
        void *controller = nullptr;
        int joystickId = 0;
#endif
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
std::unique_ptr<Command::Result> FPPArcadeAxisCommand::run(const std::vector<std::string> &args) {
    return plugin->runAxisCommand(args);
}
std::unique_ptr<Command::Result> FPPArcadeSelectGameCommand::run(const std::vector<std::string> &args) {
    return plugin->selectGame(args);
}

extern "C" {
    FPPPlugin *createPlugin() {
        return new FPPArcadePlugin();
    }
}
