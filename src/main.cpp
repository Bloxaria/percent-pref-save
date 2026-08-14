#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CreatorLayer.hpp>
#include <Geode/ui/TextInput.hpp>

using namespace geode::prelude;

// Structure pour stocker les données d'une run
struct SavedRun {
    std::string levelName;
    std::string creatorName;
    int levelID = 0;
    float percentage = 0.0f;
    double time = 0.0;
    bool isPlatformer = false;
};

// Fonction pour enregistrer la run actuelle
void saveCurrentRun(PlayLayer* playLayer) {
    if (!playLayer || !playLayer->m_level) return;

    auto level = playLayer->m_level;
    
    SavedRun run;
    run.levelName = level->m_levelName;
    run.creatorName = level->m_creatorName;
    run.levelID = level->m_levelID;
    run.isPlatformer = level->isPlatformer();
    
    if (run.isPlatformer) {
        run.time = playLayer->m_time;
        run.percentage = 0.0f;
    } else {
        run.percentage = playLayer->getCurrentPercent();
        run.time = 0.0;
    }

    auto mod = Mod::get();
    auto savedRuns = mod->getSavedValue<std::vector<matjson::Value>>("saved_runs");

    matjson::Value runJson;
    runJson["levelName"] = run.levelName;
    runJson["creatorName"] = run.creatorName;
    runJson["levelID"] = run.levelID;
    runJson["percentage"] = run.percentage;
    runJson["time"] = run.time;
    runJson["isPlatformer"] = run.isPlatformer;

    savedRuns.push_back(runJson);
    mod->setSavedValue("saved_runs", savedRuns);

    log::info("Run enregistree pour {} par {}", run.levelName, run.creatorName);
}

// 1. Popup de Confirmation / Details de la Run (Triangle Bleu et Triangle Jaune)
class RunDetailsPopup : public Popup<SavedRun const&> {
protected:
    SavedRun m_runData;

    bool setup(SavedRun const& run) override {
        m_runData = run;
        this->setTitle(m_runData.levelName);

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        // Formatage du texte (Pourcentage ou Temps)
        std::string infoText = "";
        if (m_runData.isPlatformer) {
            int totalSec = static_cast<int>(m_runData.time);
            int hours = totalSec / 3600;
            int mins = (totalSec % 3600) / 60;
            int secs = totalSec % 60;
            infoText = fmt::format("{:02d}:{:02d}:{:02d}", hours, mins, secs);
        } else {
            infoText = fmt::format("{:.1f}%", m_runData.percentage);
        }

        // --- Bouton Bleu (Triangle / Resume Checkpoint) ---
        auto blueSpr = CCSprite::createWithSpriteFrameName("GJ_playBtn2_001.png");
        blueSpr->setColor({ 0, 180, 255 }); // Teinte bleue
        blueSpr->setScale(0.8f);

        auto blueBtn = CCMenuItemSpriteExtra::create(
            blueSpr,
            this,
            menu_selector(RunDetailsPopup::onBluePlay)
        );

        auto blueLabel = CCLabelBMFont::create(infoText.c_str(), "bigFont.fnt");
        blueLabel->setScale(0.4f);
        blueLabel->setPosition({ winSize.width / 2.0f - 60.0f, winSize.height / 2.0f - 40.0f });

        // --- Bouton Jaune (Triangle / Resume Exact Position) ---
        auto yellowSpr = CCSprite::createWithSpriteFrameName("GJ_playBtn2_001.png");
        yellowSpr->setScale(0.8f);

        auto yellowBtn = CCMenuItemSpriteExtra::create(
            yellowSpr,
            this,
            menu_selector(RunDetailsPopup::onYellowPlay)
        );

        auto yellowLabel = CCLabelBMFont::create(infoText.c_str(), "bigFont.fnt");
        yellowLabel->setScale(0.4f);
        yellowLabel->setPosition({ winSize.width / 2.0f + 60.0f, winSize.height / 2.0f - 40.0f });

        // Menu pour aligner les deux boutons
        auto playMenu = CCMenu::create();
        playMenu->setPosition({ winSize.width / 2.0f, winSize.height / 2.0f + 10.0f });
        playMenu->addChild(blueBtn);
        playMenu->addChild(yellowBtn);
        playMenu->setLayout(RowLayout::create()->setGap(60.0f));

        m_mainLayer->addChild(playMenu);
        m_mainLayer->addChild(blueLabel);
        m_mainLayer->addChild(yellowLabel);

        return true;
    }

    // Disclaimer Bouton Bleu
    void onBluePlay(CCObject*) {
        createQuickPopup(
            "Warning",
            "By starting the level, you will return to your last checkpoint.",
            "Back", "Play",
            [this](FLAlertLayer*, bool btn2) {
                if (btn2) {
                    log::info("Lancement du niveau au dernier checkpoint !");
                }
            }
        );
    }

    // Disclaimer Bouton Jaune
    void onYellowPlay(CCObject*) {
        createQuickPopup(
            "Warning",
            "By starting the level, you will spawn directly where you stopped.",
            "Back", "Play",
            [this](FLAlertLayer*, bool btn2) {
                if (btn2) {
                    log::info("Lancement du niveau a la position exacte !");
                }
            }
        );
    }

public:
    static RunDetailsPopup* create(SavedRun const& run) {
        auto ret = new RunDetailsPopup();
        if (ret && ret->initAnchored(340.0f, 200.0f, run)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};

// 2. Interface Popup de la Page de Recherche "PS"
class PSSearchPopup : public Popup<std::string const&> {
protected:
    TextInput* m_searchInput = nullptr;

    bool setup(std::string const& value) override {
        this->setTitle("SAVED RUNS");

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        // Barre de recherche
        m_searchInput = TextInput::create(220.0f, "Enter level or user...");
        m_searchInput->setPosition({ winSize.width / 2.0f - 20.0f, winSize.height / 2.0f + 80.0f });
        m_mainLayer->addChild(m_searchInput);

        // Loupe
        auto searchSpr = CCSprite::createWithSpriteFrameName("GJ_searchBtn_001.png");
        searchSpr->setScale(0.7f);
        auto searchBtn = CCMenuItemSpriteExtra::create(
            searchSpr,
            this,
            menu_selector(PSSearchPopup::onSearch)
        );

        // Croix
        auto clearSpr = CCSprite::createWithSpriteFrameName("GJ_deleteIcon_001.png");
        clearSpr->setScale(0.7f);
        auto clearBtn = CCMenuItemSpriteExtra::create(
            clearSpr,
            this,
            menu_selector(PSSearchPopup::onClearInput)
        );

        // Profil
        auto userSpr = CCSprite::createWithSpriteFrameName("GJ_friendsBtn_001.png");
        userSpr->setScale(0.6f);
        auto userBtn = CCMenuItemSpriteExtra::create(
            userSpr,
            this,
            menu_selector(PSSearchPopup::onSearchUser)
        );

        auto searchMenu = CCMenu::create();
        searchMenu->setPosition({ winSize.width / 2.0f + 110.0f, winSize.height / 2.0f + 80.0f });
        searchMenu->addChild(searchBtn);
        searchMenu->addChild(userBtn);
        searchMenu->addChild(clearBtn);
        searchMenu->setLayout(RowLayout::create()->setGap(8.0f));
        
        m_mainLayer->addChild(searchMenu);

        return true;
    }

    void onSearch(CCObject*) {
        // Test d'ouverture de la carte du niveau
        SavedRun testRun;
        testRun.levelName = "Stereo Madness";
        testRun.creatorName = "RobTop";
        testRun.percentage = 74.5f;
        testRun.isPlatformer = false;

        RunDetailsPopup::create(testRun)->show();
    }

    void onSearchUser(CCObject*) {}

    void onClearInput(CCObject*) {
        if (m_searchInput) m_searchInput->setString("");
    }

public:
    static PSSearchPopup* create() {
        auto ret = new PSSearchPopup();
        if (ret && ret->initAnchored(380.0f, 250.0f, "PS Runs")) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};

// 3. Hook du Menu Pause (Bouton "S")
class $modify(MyPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto playLayer = PlayLayer::get();
        if (!playLayer) return;

        bool isPractice = playLayer->m_isPracticeMode;
        bool isPlatformer = playLayer->m_level->isPlatformer();

        if (!isPractice && !isPlatformer) return;

        auto centerMenu = this->getChildByID("center-button-menu");
        if (!centerMenu) return;

        auto btnSprite = ButtonSprite::create("S", 15, true, "goldFont.fnt", "GJ_button_01.png", 30.0f, 0.6f);
        
        auto saveBtn = CCMenuItemSpriteExtra::create(
            btnSprite,
            this,
            menu_selector(MyPauseLayer::onSaveRun)
        );
        saveBtn->setID("save-run-button"_spr);

        centerMenu->addChild(saveBtn);
        centerMenu->updateLayout();
    }

    void onSaveRun(CCObject* sender) {
        auto playLayer = PlayLayer::get();
        if (playLayer) {
            saveCurrentRun(playLayer);
        }

        this->onQuit(sender);
    }
};

// 4. Hook du Menu Créateur (Bouton "PS")
class $modify(MyCreatorLayer, CreatorLayer) {
    bool init() {
        if (!CreatorLayer::init()) return false;

        auto menu = this->getChildByID("creator-buttons-menu");
        if (!menu) return true;

        auto btnSprite = ButtonSprite::create("PS", 15, true, "goldFont.fnt", "GJ_button_01.png", 30.0f, 0.8f);

        auto psBtn = CCMenuItemSpriteExtra::create(
            btnSprite,
            this,
            menu_selector(MyCreatorLayer::onPSButton)
        );
        psBtn->setID("ps-button"_spr);
        psBtn->setPosition({ -200.0f, 0.0f });

        menu->addChild(psBtn);

        return true;
    }

    void onPSButton(CCObject* sender) {
        PSSearchPopup::create()->show();
    }
};