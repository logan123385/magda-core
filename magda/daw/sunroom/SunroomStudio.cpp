#include "SunroomStudio.hpp"

#include <cmath>

#include "SunroomActions.hpp"
#include "audio/plugins/InternalPluginRegistry.hpp"
#include "core/ClipManager.hpp"
#include "core/LLMClientProvider.hpp"
#include "core/MidiNoteCommands.hpp"
#include "core/TrackCommands.hpp"
#include "core/TrackManager.hpp"
#include "core/UndoManager.hpp"
#include "core/ViewModeController.hpp"
#include "engine/AudioEngine.hpp"
#include "magda/agents/sunroom_mlx_client.hpp"
#include "ui/state/TimelineController.hpp"

namespace magda::sunroom {
namespace {
const juce::Colour ink(0xff150d24), panel(0xff21152f), edge(0xff4a305d), paper(0xffffeee4),
    muted(0xffbca7c9), orange(0xffffa569), violet(0xffb995fa);
void text(juce::Graphics& g, const juce::String& s, juce::Rectangle<int> r, float size = 14,
          juce::Colour colour = paper, bool bold = false) {
    g.setColour(colour);
    g.setFont(juce::Font(
        juce::FontOptions("Avenir Next", size, bold ? juce::Font::bold : juce::Font::plain)));
    g.drawFittedText(s, r, juce::Justification::centredLeft, 2);
}
void card(juce::Graphics& g, juce::Rectangle<int> r, juce::Colour fill = panel) {
    g.setColour(fill);
    g.fillRoundedRectangle(r.toFloat(), 12);
    g.setColour(edge.withAlpha(.65f));
    g.drawRoundedRectangle(r.toFloat().reduced(.5f), 12, 1);
}
class Skin final : public juce::LookAndFeel_V4 {
  public:
    Skin() {
        setColour(juce::ComboBox::backgroundColourId, panel);
        setColour(juce::ComboBox::textColourId, paper);
        setColour(juce::ComboBox::outlineColourId, edge);
        setColour(juce::PopupMenu::backgroundColourId, panel);
        setColour(juce::PopupMenu::textColourId, paper);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, violet.withAlpha(.3f));
        setColour(juce::Slider::thumbColourId, orange);
        setColour(juce::Slider::trackColourId, violet);
        setColour(juce::Slider::backgroundColourId, edge);
        setColour(juce::Slider::textBoxTextColourId, paper);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::ToggleButton::textColourId, paper);
        setColour(juce::ToggleButton::tickColourId, orange);
    }
    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over,
                              bool down) override {
        const bool primary = b.getName() == "primary";
        auto fill = primary ? orange : b.getToggleState() ? juce::Colour(0xff543766) : panel;
        if (over)
            fill = fill.brighter(.13f);
        if (down)
            fill = fill.darker(.15f);
        if (!b.isEnabled())
            fill = fill.withAlpha(.4f);
        auto r = b.getLocalBounds().toFloat().reduced(.5f);
        g.setColour(fill);
        g.fillRoundedRectangle(r, 8);
        g.setColour(primary ? orange : b.getToggleState() ? violet : edge);
        g.drawRoundedRectangle(r, 8, 1);
        if (b.hasKeyboardFocus(true)) {
            g.setColour(paper);
            g.drawRoundedRectangle(r.reduced(2), 6, 1);
        }
    }
    void drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool) override {
        auto r = b.getLocalBounds().reduced(12, 3);
        auto lines = juce::StringArray::fromLines(b.getButtonText());
        auto colour = b.getName() == "primary" ? ink : paper;
        if (!b.isEnabled())
            colour = colour.withAlpha(.4f);
        if (lines.size() > 1) {
            text(g, lines[0], r.removeFromTop(22), 14, colour, true);
            text(g, lines[1], r, 11, muted);
        } else {
            g.setColour(colour);
            g.setFont(juce::Font(juce::FontOptions("Avenir Next", 13, juce::Font::bold)));
            g.drawFittedText(b.getButtonText(), r, juce::Justification::centred, 1);
        }
    }
};
// One undo record captures the complete user-created melody, including its
// instrument. Redo restores IDs rather than attaching notes to a new track.
class AddSampleCommand final : public UndoableCommand {
  public:
    AddSampleCommand(juce::File file, double seconds, double at)
        : file_(file), seconds_(seconds), at_(at) {}
    void execute() override {
        auto& tm = TrackManager::getInstance();
        auto& cm = ClipManager::getInstance();
        if (captured_) {
            tm.restoreTrack(track_);
            cm.restoreClip(clip_);
            return;
        }
        auto id = tm.createTrack(file_.getFileNameWithoutExtension());
        tm.setTrackColour(id, orange);
        tm.setTrackVolume(id, .35f);
        auto clip = cm.createAudioClip(id, at_, seconds_, file_.getFullPathName());
        cm.setClipName(clip, file_.getFileNameWithoutExtension());
        cm.setClipColour(clip, orange);
        track_ = *tm.getTrack(id);
        clip_ = *cm.getClip(clip);
        captured_ = true;
        tm.setSelectedTrack(id);
    }
    void undo() override {
        ClipManager::getInstance().deleteClip(clip_.id);
        TrackManager::getInstance().deleteTrack(track_.id);
    }
    juce::String getDescription() const override {
        return "Add sound " + file_.getFileNameWithoutExtension();
    }

  private:
    juce::File file_;
    double seconds_, at_;
    TrackInfo track_;
    ClipInfo clip_;
    bool captured_ = false;
};
class AddMelodyCommand final : public UndoableCommand {
  public:
    AddMelodyCommand(std::vector<Note> notes, Options options)
        : notes_(std::move(notes)), options_(options) {}
    void execute() override {
        auto& tm = TrackManager::getInstance();
        auto& cm = ClipManager::getInstance();
        if (captured_) {
            applyGuide(true);
            tm.restoreTrack(track_);
            cm.restoreClip(clip_);
            return;
        }
        before_ = ProjectManager::getInstance().getCurrentProjectInfo();
        applyGuide(true);
        auto id = tm.createTrack("My light droplets");
        tm.addDeviceToTrack(id, makeDevice("magda_marimba", "Light droplets", true));
        tm.addDeviceToTrack(
            id,
            makeDevice(
                "magda_delay", "Orbit / echoes", false,
                {{0, static_cast<float>(45000 / options_.tempo)}, {3, .35f}, {4, .23f}, {6, .7f}}));
        tm.setTrackColour(id, violet);
        tm.setTrackVolume(id, .3f);
        auto clip = cm.createMidiClipBeats(id, 0, 8);
        cm.setClipName(clip, "My note garden");
        cm.setClipColour(clip, violet);
        for (auto n : notes_) {
            MidiNote m;
            m.noteNumber = n.pitch;
            m.startBeat = n.beat;
            m.lengthBeats = n.length;
            m.velocity = n.velocity;
            cm.addMidiNote(clip, m);
        }
        track_ = *tm.getTrack(id);
        clip_ = *cm.getClip(clip);
        captured_ = true;
        tm.setSelectedTrack(id);
    }
    void undo() override {
        ClipManager::getInstance().deleteClip(clip_.id);
        TrackManager::getInstance().deleteTrack(track_.id);
        applyGuide(false);
    }
    juce::String getDescription() const override {
        return "Plant a note-garden melody";
    }

  private:
    void applyGuide(bool enabled) {
        auto& info = ProjectManager::getInstance().getMutableProjectInfo();
        info.sunroomGuide = enabled ? true : before_.sunroomGuide;
        info.sunroomMood = enabled ? options_.mood : before_.sunroomMood;
        info.keyRoot = enabled ? options_.root : before_.keyRoot;
        info.keyQuality = enabled ? (options_.mood == 2 ? 0 : 1) : before_.keyQuality;
    }
    ProjectInfo before_;
    std::vector<Note> notes_;
    Options options_;
    TrackInfo track_;
    ClipInfo clip_;
    bool captured_ = false;
};
}  // namespace

void drawSun(juce::Graphics& g, juce::Rectangle<float> b, float opacity) {
    const auto cx = b.getCentreX(), cy = b.getCentreY(),
               radius = std::min(b.getWidth(), b.getHeight()) * .5f;
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffffbd78).withAlpha(opacity), cx,
                                           cy - radius, juce::Colour(0xffe96c99).withAlpha(opacity),
                                           cx, cy + radius, false));
    // Bands are actual geometry; their gaps remain transparent over any sky.
    for (float y = -radius; y < radius; y += std::max(3.0f, radius * .125f)) {
        const float mid = y + radius * .036f;
        const float half = std::sqrt(std::max(0.0f, radius * radius - mid * mid));
        const float height = y < -radius * .12f ? radius * .13f : radius * .075f;
        g.fillRect(cx - half, cy + y, half * 2, height);
    }
}

SunroomStudio::SunroomStudio(AudioEngine* engine) : engine_(engine) {
    skin_ = std::make_unique<Skin>();
    setLookAndFeel(skin_.get());
    setOpaque(true);
    setWantsKeyboardFocus(true);
    for (auto* button :
         {&newProject_, &openProject_, &create_, &play_, &stop_, &makeBeat_, &addChords_,
          &playSound_, &captureJam_, &placeScene_, &returnArrange_, &openMix_, &sharedSpace_,
          &journey_, &save_, &export_,
          &undo_, &studio_, &skipGuide_, &ask_, &cancelAI_, &addPhrase_, &clearPhrase_, &library_,
          &applyRecipe_, &applySuggestion_, &cancelSuggestion_, &hearBefore_, &hearAfter_,
          &saveKey_})
        addAndMakeVisible(button);
    create_.setName("primary");
    play_.setName("primary");
    ask_.setName("primary");
    create_.setTooltip("Create editable drums, bass and chords, then play. Cmd-Z undoes the whole "
                       "addition. Rapid clicks will not double-insert.");
    journey_.setTooltip("Optional SUNROOM mood journey using the feeling controls below.");
    skipGuide_.setTooltip("Skip the guided studio and open Full studio.");
    makeBeat_.setTooltip("Open the Drum Grid clip so you can change a step.");
    addChords_.setTooltip("Open the Chords clip in the note editor.");
    playSound_.setTooltip("Open the Bass clip and turn on computer keyboard play.");
    captureJam_.setTooltip(
        "Arm real Session→Arrangement capture (transport Record). Launch clips while armed; stop "
        "recording to commit timed performance. Not a static scene copy.");
    placeScene_.setTooltip(
        "Deterministically place the first scene's Session clips into an empty arrangement range "
        "(after the loop). Distinct from Capture Jam.");
    returnArrange_.setTooltip(
        "Return tracks from Session override to Arrangement playback at the engine boundary.");
    openMix_.setTooltip(
        "Open the real Mixer (faders, pan, mute, solo, meters). Analyze stays on the mixer rail. "
        "Advanced sends/spectrum stay collapsed unless you expand them.");
    sharedSpace_.setTooltip(
        "Create or reuse one Shared Space Aux return with reverb, then set send amount from the "
        "Space slider. Undoable; does not duplicate returns.");
    makeBeat_.onClick = [this] {
        openFixtureClip("Drums", "Fixture A / Drums", false);
    };
    addChords_.onClick = [this] {
        openFixtureClip("Chords", "Fixture A / Chords", false);
    };
    playSound_.onClick = [this] {
        openFixtureClip("Bass", "Fixture A / Bass", true);
    };
    captureJam_.onClick = [this] {
        stopNotes();
        status_ = "Capture Jam: arming Session→Arrangement recorder. Launch clips, then stop "
                  "recording to commit.";
        if (onCaptureJam)
            onCaptureJam();
        else if (onShowSession)
            onShowSession();
        repaint();
    };
    placeScene_.onClick = [this] { placeSceneInArrangement(); };
    returnArrange_.onClick = [this] {
        stopNotes();
        if (onReturnToArrangement)
            onReturnToArrangement();
        status_ = playbackSourceSummary();
        repaint();
    };
    openMix_.onClick = [this] {
        stopNotes();
        status_ = "Mixer: balance with faders, pan, mute and solo. Analyze is on the mixer rail.";
        if (onShowMix)
            onShowMix();
        else if (onOpenStudio)
            onOpenStudio();
        repaint();
    };
    sharedSpace_.onClick = [this] { applySharedSpatialReturn(); };
    for (int i = 0; i < 3; ++i) {
        starterButtons_[i].setButtonText(juce::StringArray{"Beat", "Song", "Blank"}[i]);
        starterButtons_[i].setClickingTogglesState(true);
        starterButtons_[i].setRadioGroupId(71001);
        starterButtons_[i].onClick = [this, i] {
            setStarter(i == 0   ? StarterKind::Beat
                       : i == 1 ? StarterKind::Song
                                : StarterKind::Blank);
        };
        addAndMakeVisible(starterButtons_[i]);
    }
    starterButtons_[0].setToggleState(true, juce::dontSendNotification);
    for (int i = 0; i < 4; ++i) {
        tabs_[i].setButtonText(
            juce::StringArray{"Create", "Note garden", "Sound shelf", "AI companion"}[i]);
        tabs_[i].onClick = [this, i] { setTab(i); };
        addAndMakeVisible(tabs_[i]);
        moodButtons_[i].setButtonText(moods[i].name);
        moodButtons_[i].setTooltip(juce::String(moods[i].name) + " — " + moods[i].scaleName +
                                   ". " + moods[i].description);
        moodButtons_[i].onClick = [this, i] {
            options_.mood = i;
            options_.root = moods[i].suggestedRoot;
            options_.tempo = moods[i].suggestedTempo;
            anchor_ = options_.root;
            applyControls();
            status_ =
                juce::String(moods[i].description) + ". These settings shape your next journey.";
            repaint();
        };
        addAndMakeVisible(moodButtons_[i]);
    }
    for (int i = 0; i < 7; ++i) {
        layerButtons_[i].setButtonText(layers[i].name);
        layerButtons_[i].setToggleState(true, juce::dontSendNotification);
        layerButtons_[i].setTooltip(layers[i].purpose);
        layerButtons_[i].onClick = [this, i] {
            options_.enabled[i] = layerButtons_[i].getToggleState();
            repaint();
        };
        addAndMakeVisible(layerButtons_[i]);
    }
    for (int i = 0; i < 12; ++i)
        root_.addItem(noteName(i), i + 1);
    root_.onChange = [this] {
        options_.root = root_.getSelectedId() - 1;
        anchor_ = options_.root;
        repaint();
    };
    length_.addItem("8 bars / a loop", 8);
    length_.addItem("32 bars / a journey", 32);
    length_.addItem("64 bars / a longer drift", 64);
    length_.onChange = [this] {
        options_.bars = length_.getSelectedId();
        repaint();
    };
    aiBackend_.addItem("This Mac / MLX", 1);
    aiBackend_.addItem("Mini PC / local server", 2);
    aiBackend_.addItem("GPT-5.6 Luna / extra high", 3);
    auto savedProvider = Config::getInstance().getAgentLLMConfig("music").provider;
    aiBackend_.setSelectedId(savedProvider == provider::SUNROOM_LUNA   ? 3
                             : savedProvider == provider::LOCAL_SERVER ? 2
                                                                       : 1,
                             juce::dontSendNotification);
    aiBackend_.onChange = [this] {
        auto& config = Config::getInstance();
        Config::AgentLLMConfig selected;
        selected.provider = aiBackend_.getSelectedId() == 3   ? provider::SUNROOM_LUNA
                            : aiBackend_.getSelectedId() == 2 ? provider::LOCAL_SERVER
                                                              : provider::SUNROOM_MLX;
        selected.model = aiBackend_.getSelectedId() == 3 ? "gpt-5.6-luna" : "";
        if (aiBackend_.getSelectedId() == 2) {
            const auto url = remoteUrl_.getText().trim();
            const auto model = remoteModel_.getText().trim();
            if (url.isNotEmpty())
                config.setLocalServerUrl(url.toStdString());
            if (model.isNotEmpty())
                config.setLocalServerModel(model.toStdString());
            selected.baseUrl = config.getLocalServerUrl();
            selected.model = config.getLocalServerModel();
        }
        for (const auto* role : {"command", "music", "faust", "chord", "controller", "theme"})
            config.setAgentLLMConfig(role, selected);
        config.setAIPreset("advanced");
        config.save();
        resized();
        repaint();
        status_ = "The companion and Full studio agents now use " + aiBackend_.getText() + ".";
    };
    for (auto* box : {&root_, &length_, &aiBackend_, &soundFilter_})
        addAndMakeVisible(box);
    for (auto* s : {&tempo_, &motion_, &space_, &warmth_}) {
        s->setSliderStyle(juce::Slider::LinearHorizontal);
        s->setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 25);
        addAndMakeVisible(s);
    }
    tempo_.setRange(40, 180, 1);
    tempo_.setTextValueSuffix(" bpm");
    for (auto* s : {&motion_, &space_, &warmth_}) {
        s->setRange(0, 100, 1);
        s->setTextValueSuffix("%");
    }
    tempo_.onValueChange = [this] { options_.tempo = tempo_.getValue(); };
    motion_.onValueChange = [this] {
        options_.motion = static_cast<float>(motion_.getValue() / 100);
        repaint();
    };
    space_.onValueChange = [this] { options_.space = static_cast<float>(space_.getValue() / 100); };
    warmth_.onValueChange = [this] {
        options_.warmth = static_cast<float>(warmth_.getValue() / 100);
    };
    for (auto* editor : {&prompt_, &answer_, &remoteUrl_, &remoteModel_, &apiKey_}) {
        editor->setColour(juce::TextEditor::backgroundColourId, ink);
        editor->setColour(juce::TextEditor::textColourId, paper);
        editor->setColour(juce::TextEditor::outlineColourId, edge);
        editor->setColour(juce::TextEditor::focusedOutlineColourId, violet);
        editor->setFont(juce::Font(juce::FontOptions("Avenir Next", 15, juce::Font::plain)));
        editor->setIndents(16, 12);
        addAndMakeVisible(editor);
    }
    prompt_.setMultiLine(true);
    prompt_.setReturnKeyStartsNewLine(true);
    prompt_.setTextToShowWhenEmpty(
        "Give me a recipe for a warm, floating loop at 84 BPM, 8 bars...", muted);
    prompt_.setInputRestrictions(3000);
    answer_.setMultiLine(true);
    answer_.setReadOnly(true);
    answer_.setScrollbarsShown(true);
    answer_.setText(
        "A little guidance, a lot of room to explore.\n\nNew here? Choose Beat or Song, then "
        "Create and Play. Blank opens an empty project. Skip guide opens Full studio anytime.\n\n"
        "Try: 'Give me a recipe for a warm, floating loop at 84 BPM, 8 bars.'\n\nThe assistant "
        "receives your current musical settings. It cannot hear your audio.");
    remoteUrl_.setText(Config::getInstance().getLocalServerUrl());
    remoteUrl_.setTextToShowWhenEmpty("https://your-mini-pc:8080/v1", muted);
    remoteModel_.setText(Config::getInstance().getLocalServerModel());
    remoteModel_.setTextToShowWhenEmpty("Model ID from the mini PC's server", muted);
    remoteUrl_.setTooltip(
        "Prefer https:// on your LAN. Plain http:// is only for loopback addresses.");
    remoteModel_.setTooltip("Use the exact model identifier shown by the server.");
    apiKey_.setPasswordCharacter(0x2022);
    apiKey_.setInputRestrictions(512);
    apiKey_.setTextToShowWhenEmpty("Paste your OpenAI API key here", muted);
    saveKey_.onClick = [this] {
        juce::String error;
        if (SunroomMlxClient::storeOpenAIKey(apiKey_.getText(), error)) {
            apiKey_.clear();
            apiKey_.setTextToShowWhenEmpty("Key saved securely. Paste here to replace it.", muted);
            answer_.setText("Your key is saved in macOS Keychain. Luna uses gpt-5.6-luna with "
                            "extra-high reasoning. Ask a question to check your account's access.");
        } else
            answer_.setText(error);
    };
    create_.onClick = [this] { createAndPlay(); };
    journey_.onClick = [this] { buildJourney(); };
    skipGuide_.onClick = [this] {
        if (onOpenStudio)
            onOpenStudio();
    };
    play_.onClick = [this] {
        stopNotes();
        if (TrackManager::getInstance().getTracks().empty()) {
            status_ = "Nothing to hear yet — Create and Play first.";
            repaint();
            return;
        }
        if (engine_) {
            if (engine_->isPlaying())
                engine_->stop();
            else
                engine_->play();
        }
    };
    stop_.onClick = [this] {
        stopNotes();
        if (engine_) {
            engine_->stop();
            engine_->locate(0);
        }
    };
    undo_.onClick = [this] {
        stopNotes();
        UndoManager::getInstance().undo();
        status_ = "Undid the last edit.";
        repaint();
    };
    newProject_.onClick = [this] {
        if (onNewProject)
            onNewProject();
    };
    openProject_.onClick = [this] {
        if (onOpenProject)
            onOpenProject();
    };
    save_.onClick = [this] {
        if (onSave)
            onSave();
    };
    export_.onClick = [this] {
        if (onExport)
            onExport();
    };
    studio_.setTooltip("Coloured blocks are your song. Click one to edit notes. "
                       "SUNROOM / Guided studio brings you back.");
    studio_.onClick = [this] {
        stopNotes();
        status_ = "Coloured blocks are your song. Click one to edit notes. "
                  "SUNROOM / Guided studio brings you back.";
        if (onOpenStudio)
            onOpenStudio();
    };
    ask_.onClick = [this] { askCoach(); };
    cancelAI_.onClick = [this] {
        cancelled_ = true;
        SunroomMlxClient::cancelCoachRequests();
        answer_.setText("Stopped. Your music is unchanged.");
    };
    addPhrase_.onClick = [this] { addMelody(); };
    clearPhrase_.onClick = [this] {
        for (auto& row : melody_)
            row.fill(false);
        repaint();
    };
    library_.onClick = [] { soundLibrary().startAsProcess(); };
    soundFilter_.addItem("All kinds", 1);
    soundFilter_.addItem("Kick", 2);
    soundFilter_.addItem("Hand", 3);
    soundFilter_.addItem("Hat", 4);
    soundFilter_.addItem("Pitched bell", 5);
    soundFilter_.addItem("Texture", 6);
    soundFilter_.addItem("Transition", 7);
    soundFilter_.setSelectedId(1, juce::dontSendNotification);
    soundFilter_.onChange = [this] {
        refreshSoundShelf();
        repaint();
    };
    refreshSoundShelf();
    applyRecipe_.onClick = [this] {
        if (!recipe_.isObject())
            return;
        options_.mood = static_cast<int>(recipe_["mood"]);
        options_.root = static_cast<int>(recipe_["root"]);
        if (auto* notes = recipe_["melody"].getArray()) {
            for (auto& row : melody_)
                row.fill(false);
            for (const auto& note : *notes) {
                const int degree = static_cast<int>(note["degree"]);
                const int step = static_cast<int>(note["step"]);
                if (degree < 0 || degree > 6 || step < 0 || step > 15)
                    continue;
                melody_[static_cast<size_t>(6 - degree)][static_cast<size_t>(step)] = true;
            }
            applyControls();
            setTab(1);
            status_ =
                "AI melody planted. Listen to the notes, then Add melody to song when you like it.";
            return;
        }
        options_.tempo = static_cast<double>(recipe_["tempo"]);
        options_.bars = static_cast<int>(recipe_["bars"]);
        options_.motion = static_cast<float>(recipe_["motion"]);
        options_.space = static_cast<float>(recipe_["space"]);
        options_.warmth = static_cast<float>(recipe_["warmth"]);
        options_ = sanitise(options_);
        applyControls();
        setTab(0);
        status_ = "AI recipe loaded. Build my journey turns these settings into editable music.";
    };
    applySuggestion_.setTooltip(
        "Run the staged SUNROOM_DSL line through the real checker. Nothing changes until this.");
    cancelSuggestion_.setTooltip("Drop the staged suggestion. The song stays as it is.");
    hearBefore_.setTooltip("Undo only if the last step is this suggestion. Later edits stay.");
    hearAfter_.setTooltip("Redo only that suggestion. This is not a separate mix render.");
    applySuggestion_.onClick = [this] { applyStagedSuggestion(); };
    cancelSuggestion_.onClick = [this] { cancelStagedSuggestion(); };
    hearBefore_.onClick = [this] { hearStagedSuggestion(false); };
    hearAfter_.onClick = [this] { hearStagedSuggestion(true); };
    for (int i = 0; i < 16; ++i)
        if (i % 3 == 0)
            melody_[static_cast<size_t>((i * 2) % 7)][i] = true;
    ProjectManager::getInstance().addListener(this);
    if (ProjectManager::getInstance().getCurrentProjectInfo().sunroomGuide)
        projectOpened(ProjectManager::getInstance().getCurrentProjectInfo());
    applyControls();
    setTab(0);
    startTimerHz(20);
}
SunroomStudio::~SunroomStudio() {
    stopTimer();
    stopNotes();
    ProjectManager::getInstance().removeListener(this);
    cancelled_ = true;
    SunroomMlxClient::cancelCoachRequests();
    if (aiThread_.joinable())
        aiThread_.join();
    setLookAndFeel(nullptr);
}
void SunroomStudio::refreshSoundShelf() {
    StarterQuery query;
    switch (soundFilter_.getSelectedId()) {
        case 2:
            query.kind = "kick";
            break;
        case 3:
            query.kind = "hand";
            break;
        case 4:
            query.kind = "shaker";
            break;
        case 5:
            query.kind = "pitched";
            break;
        case 6:
            query.kind = "texture";
            break;
        case 7:
            query.kind = "transition";
            break;
        default:
            break;
    }
    samples_.clear();
    const auto root = soundLibrary();
    for (const auto& sound : queryStarterCatalog(query).matches)
        samples_.add(root.getChildFile(sound.file));
    sampleOffset_ = 0;
}
void SunroomStudio::applyControls() {
    root_.setSelectedId(options_.root + 1, juce::dontSendNotification);
    length_.setSelectedId(options_.bars, juce::dontSendNotification);
    tempo_.setValue(options_.tempo, juce::dontSendNotification);
    motion_.setValue(options_.motion * 100, juce::dontSendNotification);
    space_.setValue(options_.space * 100, juce::dontSendNotification);
    warmth_.setValue(options_.warmth * 100, juce::dontSendNotification);
    for (int i = 0; i < 4; ++i)
        moodButtons_[i].setToggleState(i == options_.mood, juce::dontSendNotification);
}
void SunroomStudio::setTab(int tab) {
    tab_ = tab;
    for (int i = 0; i < 4; ++i)
        tabs_[i].setToggleState(i == tab, juce::dontSendNotification);
    resized();
    repaint();
}

void SunroomStudio::resized() {
    const int w = getWidth(), h = getHeight();
    newProject_.setVisible(tab_ == 0);
    openProject_.setVisible(tab_ == 0);
    for (int i = 0; i < 4; ++i)
        tabs_[i].setBounds(230 + i * 112, 18, 106, 34);
    // studio_ / skipGuide_ placed in tab_==0 branch; keep defaults for other tabs
    if (tab_ != 0)
        studio_.setBounds(w - 155, 18, 125, 34);
    hero_ = {28, 74, w - 56, 158};
    auto body = juce::Rectangle<int>(28, 250, w - 56, h - 308);
    left_ = body.removeFromLeft(220);
    body.removeFromLeft(18);
    right_ = body.removeFromRight(238);
    body.removeFromRight(18);
    centre_ = body;
    for (auto* b : {&newProject_, &openProject_, &create_, &play_, &stop_, &makeBeat_, &addChords_,
                    &playSound_, &captureJam_, &placeScene_, &returnArrange_, &openMix_,
                    &sharedSpace_, &journey_, &save_, &export_, &undo_, &skipGuide_})
        b->setVisible(tab_ == 0);
    for (auto& b : starterButtons_)
        b.setVisible(tab_ == 0);
    for (auto& b : moodButtons_)
        b.setVisible(tab_ == 0);
    for (auto& b : layerButtons_)
        b.setVisible(tab_ == 0);
    root_.setVisible(tab_ == 0 || tab_ == 1);
    length_.setVisible(tab_ == 0);
    tempo_.setVisible(tab_ == 0);
    for (auto* s : {&motion_, &space_, &warmth_})
        s->setVisible(tab_ == 0);
    addPhrase_.setVisible(tab_ == 1);
    clearPhrase_.setVisible(tab_ == 1);
    library_.setVisible(tab_ == 2);
    soundFilter_.setVisible(tab_ == 2);
    for (auto* c : std::initializer_list<juce::Component*>{&prompt_, &answer_, &ask_, &aiBackend_,
                                                           &cancelAI_})
        c->setVisible(tab_ == 3);
    remoteUrl_.setVisible(tab_ == 3 && aiBackend_.getSelectedId() == 2);
    remoteModel_.setVisible(tab_ == 3 && aiBackend_.getSelectedId() == 2);
    apiKey_.setVisible(tab_ == 3 && aiBackend_.getSelectedId() == 3);
    saveKey_.setVisible(tab_ == 3 && aiBackend_.getSelectedId() == 3);
    applyRecipe_.setVisible(tab_ == 3 && recipe_.isObject());
    const auto* staged = pendingDslProposal();
    const bool stagedReady = tab_ == 3 && staged != nullptr && staged->phase == ProposalPhase::Ready;
    const bool stagedApplied =
        tab_ == 3 && staged != nullptr && staged->phase == ProposalPhase::Applied;
    applySuggestion_.setVisible(stagedReady);
    cancelSuggestion_.setVisible(stagedReady);
    hearBefore_.setVisible(stagedApplied);
    hearAfter_.setVisible(stagedApplied);
    if (tab_ == 0) {
        for (int i = 0; i < 3; ++i)
            starterButtons_[i].setBounds(52 + i * 92, 168, 86, 32);
        skipGuide_.setBounds(w - 288, 18, 110, 34);
        studio_.setBounds(w - 155, 18, 125, 34);
        create_.setBounds(52, 177, 177, 36);
        length_.setBounds(242, 177, 213, 36);
        for (int i = 0; i < 4; ++i)
            moodButtons_[i].setBounds(left_.getX() + 14, left_.getY() + 44 + i * 54,
                                      left_.getWidth() - 28, 48);
        root_.setBounds(left_.getX() + 75, left_.getY() + 270, 130, 32);
        tempo_.setBounds(left_.getX() + 10, left_.getY() + 338, left_.getWidth() - 20, 32);
        const int ry = right_.getY();
        warmth_.setBounds(right_.getX() + 12, ry + 80, 214, 30);
        space_.setBounds(right_.getX() + 12, ry + 160, 214, 30);
        motion_.setBounds(right_.getX() + 12, ry + 240, 214, 30);
        const int rowH = std::max(24, std::min(43, (centre_.getHeight() - 200) / 7));
        journeyArea_ = {centre_.getX() + 154, centre_.getY() + 52, centre_.getWidth() - 168,
                        rowH * 7};
        for (int i = 0; i < 7; ++i)
            layerButtons_[i].setBounds(centre_.getX() + 9, journeyArea_.getY() + i * rowH, 142,
                                       rowH);
        const int by = centre_.getBottom() - 44;
        makeBeat_.setBounds(centre_.getX() + 12, by - 80, 108, 28);
        addChords_.setBounds(centre_.getX() + 128, by - 80, 108, 28);
        playSound_.setBounds(centre_.getX() + 244, by - 80, 118, 28);
        captureJam_.setBounds(centre_.getX() + 12, by - 44, 100, 28);
        placeScene_.setBounds(centre_.getX() + 118, by - 44, 100, 28);
        returnArrange_.setBounds(centre_.getX() + 224, by - 44, 140, 28);
        openMix_.setBounds(centre_.getX() + 12, by - 8, 100, 28);
        sharedSpace_.setBounds(centre_.getX() + 118, by - 8, 120, 28);
        play_.setBounds(centre_.getX() + 248, by - 8, 70, 28);
        stop_.setBounds(centre_.getX() + 324, by - 8, 56, 28);
        journey_.setBounds(centre_.getX() + 386, by - 8, 110, 28);
        undo_.setBounds(centre_.getRight() - 68, by - 8, 56, 28);
        newProject_.setBounds(w - 504, h - 43, 112, 30);
        openProject_.setBounds(w - 386, h - 43, 112, 30);
        save_.setBounds(w - 268, h - 43, 112, 30);
        export_.setBounds(w - 148, h - 43, 120, 30);
    } else if (tab_ == 1) {
        root_.setBounds(w - 188, 96, 130, 34);
        noteArea_ = {36, 170, w - 72, 110};
        melodyArea_ = {115, 390, w - 155, std::max(140, h - 465)};
        addPhrase_.setBounds(w - 228, h - 61, 188, 35);
        clearPhrase_.setBounds(w - 369, h - 61, 130, 35);
    } else if (tab_ == 2) {
        library_.setBounds(w - 228, 100, 188, 34);
        soundFilter_.setBounds(w - 430, 100, 190, 34);
    } else {
        aiBackend_.setBounds(36, 159, 260, 34);
        remoteUrl_.setBounds(309, 159, (w - 359) / 2, 34);
        remoteModel_.setBounds(323 + (w - 359) / 2, 159, (w - 359) / 2, 34);
        apiKey_.setBounds(309, 159, w - 505, 34);
        saveKey_.setBounds(w - 183, 159, 147, 34);
        answer_.setBounds(36, 203, w - 72, std::max(180, h - 390));
        prompt_.setBounds(36, h - 171, w - 244, 105);
        ask_.setBounds(w - 192, h - 171, 156, 44);
        cancelAI_.setBounds(w - 192, h - 118, 70, 32);
        applyRecipe_.setBounds(w - 340, h - 222, 304, 32);
        applySuggestion_.setBounds(w - 340, h - 258, 148, 30);
        cancelSuggestion_.setBounds(w - 186, h - 258, 150, 30);
        hearBefore_.setBounds(w - 340, h - 294, 148, 30);
        hearAfter_.setBounds(w - 186, h - 294, 150, 30);
    }
}

void SunroomStudio::paint(juce::Graphics& g) {
    g.fillAll(ink);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff2d1743), 0, 0, ink, 0,
                                           static_cast<float>(getHeight()), false));
    g.fillAll();
    drawSun(g, {30, 17, 40, 35});
    text(g, "S U N R O O M", {82, 15, 160, 26}, 18, paper, true);
    text(g, "A PLACE TO MAKE SOMETHING", {83, 39, 160, 14}, 8, muted);
    g.setColour(edge.withAlpha(.7f));
    g.drawHorizontalLine(67, 28.0f, static_cast<float>(getWidth() - 28));
    trackHitboxes_.clear();
    sampleHitboxes_.clear();
    if (tab_ == 0)
        paintCreate(g);
    else if (tab_ == 1)
        paintGarden(g);
    else if (tab_ == 2)
        paintSounds(g);
    else
        paintCoach(g);
    text(g, status_, {30, getHeight() - 39, getWidth() - (tab_ == 0 ? 550 : 60), 28}, 11, muted);
}
void SunroomStudio::paintCreate(juce::Graphics& g) {
    auto r = hero_;
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff35204b), r.getX(), r.getY(),
                                           juce::Colour(0xff75394b), r.getRight(), r.getBottom(),
                                           false));
    g.fillRoundedRectangle(r.toFloat(), 16);
    // A quiet horizon behind the controls: orange sun, violet mountains and an infinite grid.
    const float sx = r.getRight() - 170.0f, sy = r.getY() + 78.0f;
    for (int i = 6; i > 0; --i) {
        g.setColour(orange.withAlpha(.013f));
        g.fillEllipse(sx - 50 - i * 7, sy - 50 - i * 7, 100 + i * 14, 100 + i * 14);
    }
    drawSun(g, {sx - 53, sy - 58, 106, 106}, .95f);
    juce::Path mountains;
    mountains.startNewSubPath(r.getRight() - 390, r.getBottom());
    mountains.lineTo(r.getRight() - 300, r.getY() + 105);
    mountains.lineTo(r.getRight() - 243, r.getY() + 130);
    mountains.lineTo(r.getRight() - 180, r.getY() + 94);
    mountains.lineTo(r.getRight() - 110, r.getY() + 123);
    mountains.lineTo(r.getRight() - 52, r.getY() + 99);
    mountains.lineTo(r.getRight(), r.getY() + 125);
    mountains.lineTo(r.getRight(), r.getBottom());
    mountains.closeSubPath();
    g.setColour(juce::Colour(0xff321d48));
    g.fillPath(mountains);
    g.setColour(violet.withAlpha(.19f));
    for (int i = 0; i < 8; ++i)
        g.drawLine(r.getRight() - 390 + i * 56, r.getBottom(), sx, r.getY() + 117, 1);
    for (int i = 0; i < 4; ++i)
        g.drawLine(r.getRight() - 380, r.getY() + 128 + i * i * 2, r.getRight(),
                   r.getY() + 128 + i * i * 2, 1);
    text(g, "SLOW MUSIC. DEEP COLOUR. YOUR WORLD.", {52, 90, 610, 18}, 10, juce::Colour(0xffefbece),
         true);
    text(g, "Find your own frequency.", {50, 112, 660, 45}, 34, paper, true);
    text(g, "An easy way into psybient, psychill, and beautiful little accidents.",
         {52, 154, 650, 20}, 13, paper);
    card(g, left_);
    card(g, centre_);
    card(g, right_);
    text(g, "01 / CHOOSE A FEELING", left_.withTrimmedLeft(15).withHeight(36), 11, muted, true);
    text(g, "Home note", {left_.getX() + 15, left_.getY() + 269, 70, 32}, 12, muted);
    text(g, "Pace / how fast time moves", {left_.getX() + 15, left_.getY() + 307, 190, 30}, 12,
         muted);
    text(g, "02 / YOUR LITTLE UNIVERSE",
         {centre_.getX() + 15, centre_.getY() + 9, centre_.getWidth() - 30, 28}, 11, muted, true);
    if (!previewValid_ || !(previewOptions_ == options_)) {
        preview_ = compose(options_);
        previewOptions_ = options_;
        previewValid_ = true;
    }
    const auto& sketch = preview_;
    const int rowH = journeyArea_.getHeight() / 7;
    const int cols = options_.bars == 8 ? 1 : 4;
    for (int s = 0; s < cols; ++s)
        text(g, options_.bars == 8 ? "LOOP" : juce::String(sectionName(s)).toUpperCase(),
             {journeyArea_.getX() + s * journeyArea_.getWidth() / cols, journeyArea_.getY() - 17,
              journeyArea_.getWidth() / cols, 17},
             9, muted, true);
    for (int l = 0; l < 7; ++l) {
        auto lane = juce::Rectangle<int>(journeyArea_.getX(), journeyArea_.getY() + l * rowH,
                                         journeyArea_.getWidth(), rowH - 5);
        g.setColour(ink.withAlpha(.65f));
        g.fillRoundedRectangle(lane.toFloat(), 5);
        for (const auto& phrase : sketch.phrases[l]) {
            const float x = lane.getX() + static_cast<float>(phrase.start / (options_.bars * 4) *
                                                             lane.getWidth());
            const float width =
                static_cast<float>(phrase.length / (options_.bars * 4) * lane.getWidth());
            auto clip = juce::Rectangle<float>(x + 2, lane.getY() + 4.0f, std::max(2.0f, width - 4),
                                               lane.getHeight() - 8.0f);
            auto colour = juce::Colour(layers[l].colour);
            g.setColour(colour.withAlpha(.18f));
            g.fillRoundedRectangle(clip, 3);
            g.setColour(colour.withAlpha(.78f));
            for (size_t n = 0; n < phrase.notes.size();
                 n += std::max<size_t>(1, phrase.notes.size() / 18)) {
                auto& note = phrase.notes[n];
                const float nx = clip.getX() + static_cast<float>(note.beat / 16) * clip.getWidth();
                const float ny =
                    clip.getY() + 3 + (pitchClass(note.pitch) / 12.0f) * (clip.getHeight() - 7);
                g.fillRoundedRectangle(
                    nx, ny,
                    std::max(2.0f, static_cast<float>(note.length / 16) * clip.getWidth() - 1),
                    2.5f, 1);
            }
        }
    }
    const auto& tracks = TrackManager::getInstance().getTracks();
    text(g,
         tracks.empty() ? "A sketch of your next journey. Build it to hear it."
                        : juce::String(tracks.size()) +
                              " tracks in the song. Above: your next journey preview.",
         {centre_.getX() + 15, journeyArea_.getBottom() + 4, centre_.getWidth() - 30, 26}, 11,
         muted);
    if (tracks.empty()) {
        // First-song path stays visible until something is built.
        const int y = getHeight() - 72;
        text(g, "FIRST SONG", {30, y, 90, 22}, 10, orange, true);
        text(g, "Beat or Song  →  Create and Play  →  edit a drum step in Session",
             {120, y, getWidth() - 160, 22}, 12, paper);
    }
    if (lastSummary_.isNotEmpty())
        text(g, lastSummary_, {centre_.getX() + 15, centre_.getY() + 34, centre_.getWidth() - 30, 18},
             11, orange);
    text(g, "03 / SHAPE THE ATMOSPHERE", {right_.getX() + 15, right_.getY() + 9, 215, 28}, 11,
         muted, true);
    const char* labels[]{"Warmth", "Space", "Motion"};
    const char* descriptions[]{"Soft edges, like late sunlight", "A room that opens into the sky",
                               "Still water to a gentle current"};
    for (int i = 0; i < 3; ++i) {
        int y = right_.getY() + 45 + i * 80;
        text(g, labels[i], {right_.getX() + 15, y, 200, 22}, 15, paper, true);
        text(g, descriptions[i], {right_.getX() + 15, y + 23, 210, 19}, 11, muted);
    }
    text(g, "These controls shape the next journey.\nYour notes and sounds stay editable.",
         {right_.getX() + 16, right_.getY() + 296, 208, 55}, 12, muted);
    if (right_.getHeight() > 380)
        text(g, "Start small. Leave some space.\nThere is no wrong place to begin.",
             {right_.getX() + 16, right_.getBottom() - 67, 210, 50}, 13, juce::Colour(0xffe6c3d2));
}

void SunroomStudio::paintGarden(juce::Graphics& g) {
    text(g, "Notes are colours you can hear.", {36, 91, getWidth() - 270, 40}, 29, paper, true);
    text(g,
         juce::String(noteName(options_.root)) + " " + moodAt(options_.mood).scaleName +
             " / bright notes belong to this palette. Muted notes add spice.",
         {37, 133, getWidth() - 74, 25}, 13, muted);
    const float cw = noteArea_.getWidth() / 12.0f;
    for (int n = 0; n < 12; ++n) {
        const auto f = feeling(n, options_.root, options_.mood);
        auto r = juce::Rectangle<int>(noteArea_.getX() + static_cast<int>(n * cw), noteArea_.getY(),
                                      static_cast<int>(cw) - 6, noteArea_.getHeight());
        auto c = juce::Colour(f.colour);
        card(g, r, c.withAlpha(n == anchor_ ? .27f : .11f));
        if (n == anchor_) {
            g.setColour(c);
            g.drawRoundedRectangle(r.toFloat().reduced(1), 12, 2);
        }
        text(g, noteName(n), r.reduced(13).withHeight(39), 25, c, true);
        text(g, f.label, {r.getX() + 13, r.getY() + 52, r.getWidth() - 18, 22}, 12, c, true);
        text(g, inScale(n, options_.root, options_.mood) ? "IN PALETTE" : "EXPLORE",
             {r.getX() + 13, r.getY() + 83, r.getWidth() - 18, 15}, 8, muted);
    }
    card(g, {36, 295, getWidth() - 72, 70});
    text(g,
         juce::String(feeling(anchor_, options_.root, options_.mood).label) + " / " +
             feeling(anchor_, options_.root, options_.mood).analogy,
         {52, 304, getWidth() - 104, 25}, 14,
         juce::Colour(feeling(anchor_, options_.root, options_.mood).colour), true);
    text(g, pairText_, {52, 334, getWidth() - 104, 23}, 12, muted);
    text(g, "PLANT A MELODY", {36, 368, 250, 22}, 10, muted, true);
    const float rh = melodyArea_.getHeight() / 7.0f, sw = melodyArea_.getWidth() / 16.0f;
    for (int row = 0; row < 7; ++row) {
        int note = degreeNote(6 - row, options_.root, options_.mood, 60);
        auto f = feeling(note, options_.root, options_.mood);
        text(g, juce::String(noteName(note)) + "  " + f.label,
             {36, melodyArea_.getY() + static_cast<int>(row * rh), 78, static_cast<int>(rh)}, 11,
             juce::Colour(f.colour));
        for (int col = 0; col < 16; ++col) {
            auto r = juce::Rectangle<float>(melodyArea_.getX() + col * sw + 2,
                                            melodyArea_.getY() + row * rh + 2, sw - 4, rh - 4);
            g.setColour(melody_[row][col] ? juce::Colour(f.colour)
                                          : ((col / 4) % 2 ? juce::Colour(0xff30213e) : panel));
            g.fillRoundedRectangle(r, 4);
            if (col % 4 == 0) {
                g.setColour(edge);
                g.drawVerticalLine(static_cast<int>(r.getX() - 2), r.getY(), r.getBottom());
            }
        }
    }
    text(g, "Click a square to plant or remove a note. Left to right = time. Up = higher.",
         {36, getHeight() - 60, getWidth() - 440, 36}, 12, muted);
}

void SunroomStudio::paintSounds(juce::Graphics& g) {
    text(g, "A shelf full of possibility.", {36, 93, getWidth() - 300, 42}, 29, paper, true);
    text(g,
         "Declared kinds only. Unknown BPM and key are not guessed. Drums are not key-matched.",
         {37, 137, getWidth() - 74, 28}, 13, muted);
    const int libraryX = getWidth() - 345, cardW = (libraryX - 66) / 2;
    for (int i = 0; i < 7; ++i) {
        int x = 36 + (i % 2) * (cardW + 12), y = 184 + (i / 2) * 127;
        auto r = juce::Rectangle<int>(x, y, cardW, 114);
        card(g, r);
        g.setColour(juce::Colour(layers[i].colour));
        g.fillRoundedRectangle(x + 16, y + 19, 5, 71, 2);
        text(g, layers[i].name, {x + 35, y + 15, cardW - 50, 29}, 19,
             juce::Colour(layers[i].colour), true);
        text(g, layers[i].purpose, {x + 35, y + 46, cardW - 50, 30}, 12, muted);
        text(g, "+ ADD INSTRUMENT", {x + 35, y + 84, cardW - 50, 18}, 9, paper, true);
    }
    auto r = juce::Rectangle<int>(libraryX, 184, 309, getHeight() - 246);
    card(g, r);
    text(g, "TEXTURES & LITTLE DETAILS", r.reduced(15).withHeight(25), 11, muted, true);
    text(g, juce::String(samples_.size()) + " original WAVs / free to use",
         {libraryX + 15, 219, 279, 23}, 12, paper);
    const int visible = std::max(0, std::min(samples_.size(), (r.getHeight() - 83) / 29));
    for (int i = 0; i < visible && i + sampleOffset_ < samples_.size(); ++i) {
        auto hit = juce::Rectangle<int>(libraryX + 13, 253 + i * 29, 283, 26);
        text(g, "+ " + samples_[i + sampleOffset_].getFileNameWithoutExtension(), hit, 12, muted);
        sampleHitboxes_.push_back({hit, samples_[i + sampleOffset_]});
    }
    text(g,
         "Click a WAV to add it at the playhead. Scroll the list for more sounds.\nMore built-in "
         "effects live in Full studio's plugin browser.",
         {36, getHeight() - 75, libraryX - 60, 43}, 12, muted);
}
void SunroomStudio::paintCoach(juce::Graphics& g) {
    text(g, "Your patient studio companion.", {36, 91, getWidth() - 72, 42}, 29, paper, true);
    text(g,
         aiBackend_.getSelectedId() == 3 ? "Luna / xhigh. Sends your question and project context "
                                           "to OpenAI. API usage is billed separately."
         : aiBackend_.getSelectedId() == 2
             ? "Sends questions to your mini PC. Keep both computers on your private network."
             : "Local MLX on this Mac. No cloud fallback. The assistant cannot hear your audio.",
         {37, 131, getWidth() - 74, 22}, 13, muted);
}

void SunroomStudio::mouseDown(const juce::MouseEvent& event) {
    grabKeyboardFocus();
    const auto p = event.getPosition();
    if (tab_ == 1 && noteArea_.contains(p)) {
        int n = juce::jlimit(0, 11, (p.x - noteArea_.getX()) * 12 / noteArea_.getWidth());
        if (event.mods.isShiftDown()) {
            pairText_ = pairFeeling(anchor_, n);
            audition(n, true);
        } else {
            anchor_ = n;
            pairText_ = "Shift-click a second note to hear how they feel together.";
            audition(n, false);
        }
        repaint();
    } else if (tab_ == 1 && melodyArea_.contains(p)) {
        int row = juce::jlimit(0, 6, (p.y - melodyArea_.getY()) * 7 / melodyArea_.getHeight());
        int col = juce::jlimit(0, 15, (p.x - melodyArea_.getX()) * 16 / melodyArea_.getWidth());
        melody_[row][col] = !melody_[row][col];
        if (melody_[row][col])
            audition(degreeNote(6 - row, options_.root, options_.mood, 60), false);
        repaint();
    } else if (tab_ == 2) {
        const int libraryX = getWidth() - 345, cardW = (libraryX - 66) / 2;
        for (int i = 0; i < 7; ++i) {
            auto r =
                juce::Rectangle<int>(36 + (i % 2) * (cardW + 12), 184 + (i / 2) * 127, cardW, 114);
            if (r.contains(p)) {
                auto id = addInstrument(i, options_);
                TrackManager::getInstance().setSelectedTrack(id);
                previewTrack_ = id;
                previewLayer_ = i;
                audition(i == 4   ? 36
                         : i == 5 ? 42
                         : i == 3 ? 48
                                  : (i == 1   ? 24
                                     : i == 0 ? 48
                                     : i == 6 ? 72
                                              : 60) +
                                        options_.root,
                         false);
                status_ = "Added " + juce::String(layers[i].name) +
                          ". Open Full studio to play and edit it.";
                repaint();
                return;
            }
        }
        for (auto& hit : sampleHitboxes_)
            if (hit.first.contains(p)) {
                juce::AudioFormatManager formats;
                formats.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> reader(
                    formats.createReaderFor(hit.second));
                if (reader) {
                    UndoManager::getInstance().executeCommand(std::make_unique<AddSampleCommand>(
                        hit.second, reader->lengthInSamples / reader->sampleRate,
                        engine_ ? engine_->getCurrentPosition() : 0));
                    status_ = "Added " + hit.second.getFileNameWithoutExtension() +
                              " at the playhead. Cmd-Z undoes this.";
                    repaint();
                }
                return;
            }
    }
}
void SunroomStudio::mouseWheelMove(const juce::MouseEvent& event,
                                   const juce::MouseWheelDetails& wheel) {
    if (tab_ == 2 && event.x > getWidth() - 345) {
        sampleOffset_ = juce::jlimit(0, std::max(0, samples_.size() - 3),
                                     sampleOffset_ + (wheel.deltaY < 0 ? 3 : -3));
        repaint();
    }
}
bool SunroomStudio::keyPressed(const juce::KeyPress& key) {
    if (key.getKeyCode() == juce::KeyPress::spaceKey) {
        play_.triggerClick();
        return true;
    }
    return false;
}
void SunroomStudio::stopNotes() {
    if (previewTrack_ != INVALID_TRACK_ID)
        for (int n : sounding_)
            TrackManager::getInstance().previewNote(previewTrack_, n, 0, false);
    sounding_.clear();
}
void SunroomStudio::audition(int note, bool together) {
    stopNotes();
    auto& tm = TrackManager::getInstance();
    if (!tm.getTrack(previewTrack_)) {
        // Reuse a real instrument if present. A first audition adds an undoable
        // instrument; it is visible and never writes hidden notes into the song.
        previewTrack_ = INVALID_TRACK_ID;
        for (const auto& t : tm.getTracks())
            if (t.name == "Light droplets" || t.name == "My light droplets") {
                previewTrack_ = t.id;
                break;
            }
        if (previewTrack_ == INVALID_TRACK_ID)
            previewTrack_ = addInstrument(2, options_);
    }
    sounding_.push_back(note >= 12 ? juce::jlimit(0, 127, note) : 60 + pitchClass(note));
    if (together && pitchClass(anchor_) != pitchClass(note))
        sounding_.push_back(60 + pitchClass(anchor_));
    for (int n : sounding_)
        tm.previewNote(previewTrack_, n, 70, true);
    noteOffTime_ = juce::Time::getMillisecondCounterHiRes() + 850;
}
void SunroomStudio::timerCallback() {
    if (!sounding_.empty() && juce::Time::getMillisecondCounterHiRes() > noteOffTime_)
        stopNotes();
    switch (starter_) {
        case StarterKind::Blank:
            create_.setButtonText("Start blank");
            break;
        case StarterKind::Beat:
        case StarterKind::Song:
            create_.setButtonText(hasFixtureATracks() ? "Play again" : "Create and Play");
            break;
    }
    play_.setButtonText(engine_ && engine_->isPlaying() ? "Pause" : "Play");
    undo_.setEnabled(UndoManager::getInstance().canUndo());
    ask_.setEnabled(!busy_.load());
    cancelAI_.setEnabled(busy_.load());
    create_.setEnabled(!creating_.load());
    if (isShowing() && tab_ == 0 && engine_ && engine_->isPlaying())
        repaint();
}

bool SunroomStudio::hasFixtureATracks() const {
    bool drums = false, bass = false, chords = false;
    auto& cm = ClipManager::getInstance();
    for (const auto& track : TrackManager::getInstance().getTracks()) {
        const char* role = track.name == "Drums"   ? "Drums"
                           : track.name == "Bass"   ? "Bass"
                           : track.name == "Chords" ? "Chords"
                                                    : nullptr;
        if (role == nullptr)
            continue;
        for (const auto& clip : cm.getClips()) {
            if (clip.trackId == track.id && clip.name == juce::String("Fixture A / ") + role) {
                if (track.name == "Drums")
                    drums = true;
                else if (track.name == "Bass")
                    bass = true;
                else
                    chords = true;
            }
        }
    }
    return drums && bass && chords;
}

void SunroomStudio::openFixtureClip(const juce::String& trackName, const juce::String& clipName,
                                    bool enableQwerty) {
    stopNotes();
    if (!hasFixtureATracks()) {
        status_ = "Create and Play first — then you can edit drums, chords, or play a sound.";
        repaint();
        return;
    }

    TrackId trackId = INVALID_TRACK_ID;
    for (const auto& track : TrackManager::getInstance().getTracks()) {
        if (track.name == trackName) {
            trackId = track.id;
            break;
        }
    }
    if (trackId == INVALID_TRACK_ID) {
        status_ = "Could not find the " + trackName + " track.";
        repaint();
        return;
    }

    ClipId clipId = INVALID_CLIP_ID;
    for (const auto& clip : ClipManager::getInstance().getClips()) {
        if (clip.trackId == trackId && clip.name == clipName) {
            clipId = clip.id;
            break;
        }
    }
    if (clipId == INVALID_CLIP_ID) {
        for (const auto& clip : ClipManager::getInstance().getClips()) {
            if (clip.trackId == trackId && clip.isMidi()) {
                clipId = clip.id;
                break;
            }
        }
    }
    if (clipId == INVALID_CLIP_ID) {
        status_ = "No editable clip on " + trackName + " yet.";
        repaint();
        return;
    }

    status_ = "Editing " + clipName + ". Full studio stays linked to the same clip data.";
    if (onEditClip)
        onEditClip(trackId, clipId);
    if (enableQwerty && onEnableQwerty)
        onEnableQwerty();
    repaint();
}

juce::String SunroomStudio::playbackSourceSummary() const {
    int session = 0, arrangement = 0;
    for (const auto& track : TrackManager::getInstance().getTracks()) {
        if (track.type == TrackType::Chord)
            continue;
        if (track.playbackMode == TrackPlaybackMode::Session)
            ++session;
        else
            ++arrangement;
    }
    if (session == 0)
        return "Playback source: Arrangement (all tracks).";
    if (arrangement == 0)
        return "Playback source: Session (all tracks). Return to Arrangement when ready.";
    return "Playback source: mixed — " + juce::String(session) + " Session, " +
           juce::String(arrangement) +
           " Arrangement. Return to Arrangement clears Session overrides.";
}

void SunroomStudio::expandToFixtureB() {
    const auto& info = ProjectManager::getInstance().getCurrentProjectInfo();
    bool hasSections = false;
    for (const auto& marker : info.markers) {
        if (marker.name == "Intro" || marker.name == "Main") {
            hasSections = true;
            break;
        }
    }
    if (hasSections && info.loopEndBeats >= 127.0) {
        status_ = "Song sections already present.";
        return;
    }

    auto command = std::make_unique<CreateFixtureBCommand>(engine_);
    auto* raw = command.get();
    UndoManager::getInstance().executeCommand(std::move(command));
    if (raw->failed()) {
        const auto reason = raw->failureReason().isNotEmpty()
                                ? raw->failureReason()
                                : juce::String("Could not build song sections.");
        UndoManager::getInstance().discardLastCommand("Create beginner Fixture B sections");
        status_ = reason;
        return;
    }
    lastSummary_ = raw->summary();
    status_ = lastSummary_ + "  Launch scenes, Capture Jam, or Place Scene.";
}

void SunroomStudio::placeSceneInArrangement() {
    stopNotes();
    if (!hasFixtureATracks()) {
        status_ = "Create a Song (or Fixture B) first so scenes exist.";
        repaint();
        return;
    }
    const double dest =
        juce::jmax(128.0, ProjectManager::getInstance().getCurrentProjectInfo().loopEndBeats);
    auto command = std::make_unique<PlaceSceneInArrangementCommand>(0, dest);
    auto* raw = command.get();
    UndoManager::getInstance().executeCommand(std::move(command));
    if (raw->failed()) {
        const auto reason = raw->failureReason().isNotEmpty() ? raw->failureReason()
                                                             : juce::String("Place Scene failed.");
        UndoManager::getInstance().discardLastCommand("Place Scene in Arrangement");
        status_ = reason;
        repaint();
        return;
    }
    status_ = raw->summary();
    if (onShowArrange)
        onShowArrange();
    repaint();
}

void SunroomStudio::applySharedSpatialReturn() {
    stopNotes();
    if (!hasFixtureATracks()) {
        status_ = "Create and Play first, then Shared Space can send tracks to one return.";
        repaint();
        return;
    }
    const float amount = static_cast<float>(space_.getValue() / 100.0);
    auto command = std::make_unique<ApplySharedSpatialReturnCommand>(amount);
    auto* raw = command.get();
    UndoManager::getInstance().executeCommand(std::move(command));
    if (raw->failed()) {
        const auto reason = raw->failureReason().isNotEmpty()
                                ? raw->failureReason()
                                : juce::String("Shared Space failed.");
        UndoManager::getInstance().discardLastCommand("Apply shared spatial return");
        status_ = reason;
        repaint();
        return;
    }
    status_ = raw->summary();
    repaint();
}

void SunroomStudio::applyStagedSuggestion() {
    if (engine_ == nullptr)
        return;
    const auto line = applyPendingDslProposal(engine_->getMagdaApi(), false);
    status_ = line.startsWith("Applied")
                  ? line + " Undo hears the song before this suggestion. Redo hears it after. "
                           "Then change one drum step (Make a Beat). That edit is not a learning score."
                  : line;
    resized();
    repaint();
}

void SunroomStudio::cancelStagedSuggestion() {
    if (engine_ != nullptr)
        applyPendingDslProposal(engine_->getMagdaApi(), true);
    status_ = "Suggestion canceled. Your music is unchanged.";
    resized();
    repaint();
}

void SunroomStudio::hearStagedSuggestion(bool after) {
    auto& undo = UndoManager::getInstance();
    if (!after) {
        if (undo.canUndo() && undo.getUndoDescription() == "Apply suggestion") {
            undo.undo();
            status_ = "Hearing the song before the suggestion. Hear after restores only that step.";
        } else {
            status_ = "The last undo step is not this suggestion. Later edits stay. "
                      "This will not restore the whole project.";
        }
    } else if (undo.canRedo() && undo.getRedoDescription() == "Apply suggestion") {
        undo.redo();
        status_ = "Hearing the song after the suggestion.";
    } else {
        status_ = "Nothing to restore. A later edit was not rolled back.";
    }
    resized();
    repaint();
}

void SunroomStudio::setStarter(StarterKind kind) {
    starter_ = kind;
    for (int i = 0; i < 3; ++i)
        starterButtons_[i].setToggleState(
            (i == 0 && kind == StarterKind::Beat) || (i == 1 && kind == StarterKind::Song) ||
                (i == 2 && kind == StarterKind::Blank),
            juce::dontSendNotification);
    switch (kind) {
        case StarterKind::Beat:
            status_ = "Beat: drums, bass and chords — eight bars at 100 BPM.";
            break;
        case StarterKind::Song:
            status_ = "Song: Fixture A plus Intro/Main/Variation/Ending sections (Fixture B).";
            break;
        case StarterKind::Blank:
            status_ = "Blank: keep an empty project and open Full studio when ready.";
            break;
    }
    repaint();
}

void SunroomStudio::createAndPlay() {
    stopNotes();
    if (creating_.exchange(true)) {
        status_ = "Still creating — one moment.";
        repaint();
        return;
    }

    struct ResetCreating {
        std::atomic<bool>& flag;
        ~ResetCreating() {
            flag.store(false);
        }
    } reset{creating_};

    switch (starter_) {
        case StarterKind::Blank: {
            if (!TrackManager::getInstance().getTracks().empty() && onNewProject)
                onNewProject();
            lastSummary_.clear();
            status_ = "Blank project ready. Full studio is open for your own tracks.";
            if (onShowSession)
                onShowSession();
            else if (onOpenStudio)
                onOpenStudio();
            repaint();
            return;
        }
        case StarterKind::Beat:
        case StarterKind::Song:
            break;
    }

    if (!hasFixtureATracks()) {
        auto command = std::make_unique<CreateFixtureACommand>(engine_);
        auto* raw = command.get();
        UndoManager::getInstance().executeCommand(std::move(command));
        if (raw->failed()) {
            const auto reason = raw->failureReason().isNotEmpty()
                                    ? raw->failureReason()
                                    : juce::String("Could not create starter.");
            UndoManager::getInstance().discardLastCommand("Create beginner Fixture A");
            status_ = reason;
            lastSummary_.clear();
            repaint();
            return;
        }
        lastSummary_ = raw->summary() + "  Next: open the Drums clip to change a step.";
        status_ = lastSummary_;
    } else {
        status_ = "Starter already in the project — playing it.";
    }

    if (starter_ == StarterKind::Song)
        expandToFixtureB();

    ViewModeController::getInstance().setViewMode(ViewMode::Live);
    if (onShowSession)
        onShowSession();

    if (engine_) {
        engine_->locate(0);
        // Device init is separate from content creation: try play, keep music if it fails.
        engine_->play();
        if (!engine_->isPlaying())
            status_ = lastSummary_.isNotEmpty()
                          ? lastSummary_ + " Music is ready — check audio output, then Play."
                          : juce::String("Music is ready — check audio output, then Play.");
    }
    repaint();
}

void SunroomStudio::buildJourney() {
    stopNotes();
    if (std::none_of(options_.enabled.begin(), options_.enabled.end(), [](bool v) { return v; })) {
        status_ = "Choose at least one sound layer, then build your journey.";
        return;
    }
    UndoManager::getInstance().executeCommand(
        std::make_unique<CreateJourneyCommand>(options_, engine_));
    status_ = juce::String(options_.bars) + " bars of " + moodAt(options_.mood).name +
              " music added. Press Play.";
    if (options_.bars > 8)
        status_ += " The opening is mostly atmosphere — the pulse arrives later. "
                   "Try 8 bars for a fuller start.";
    status_ += " Cmd-Z undoes this whole journey.";
    repaint();
}
void SunroomStudio::addMelody() {
    std::vector<Note> notes;
    for (int row = 0; row < 7; ++row)
        for (int col = 0; col < 16; ++col)
            if (melody_[row][col])
                notes.push_back(
                    {degreeNote(6 - row, options_.root, options_.mood, 60), col * .5, .42, 72});
    if (notes.empty()) {
        status_ = "Plant a few notes before adding your melody.";
        return;
    }
    UndoManager::getInstance().executeCommand(
        std::make_unique<AddMelodyCommand>(std::move(notes), options_));
    status_ = "Your two-bar melody is now a real MIDI clip. Open Full studio — coloured blocks "
              "are your song; Guided studio brings you back.";
}
void SunroomStudio::askCoach() {
    const auto question = prompt_.getText().trim();
    if (question.isEmpty() || busy_.exchange(true))
        return;
    const int backend = aiBackend_.getSelectedId();
    if (backend == 1) {
        const auto readiness = SunroomMlxClient::localModelStatus();
        if (readiness.startsWith("The local AI model is not installed")) {
            busy_ = false;
            answer_.setText(readiness);
            status_ = readiness;
            return;
        }
    } else if (backend == 2 && remoteUrl_.getText().trim().isEmpty()) {
        busy_ = false;
        status_ = "No mini PC address. Your music is unchanged. This is not an AI answer.";
        answer_.setText(status_);
        return;
    } else if (backend == 3 && !SunroomMlxClient::hasOpenAIKey()) {
        busy_ = false;
        status_ = "Luna needs a saved key. Your music is unchanged. This is not an AI answer.";
        answer_.setText(status_);
        return;
    }
    if (aiThread_.joinable())
        aiThread_.join();
    cancelled_ = false;
    recipe_ = juce::var{};
    applyRecipe_.setVisible(false);
    answer_.setText(
        backend == 3 ? "Luna is thinking with extra-high reasoning..."
        : backend == 2
            ? "Asking your mini PC..."
            : "Finding a little direction... The first answer also loads the local model.");
    auto& pm = ProjectManager::getInstance();
    const auto& info = pm.getCurrentProjectInfo();
    juce::String context = "Create-page settings: root " + juce::String(noteName(options_.root)) +
                           ", mode " + moodAt(options_.mood).scaleName + ", " +
                           juce::String(options_.tempo, 0) + " BPM, " +
                           juce::String(options_.bars) + " bars. ";
    context += "Scale notes: ";
    for (int degree = 0; degree < 7; ++degree)
        context += juce::String(noteName(degreeNote(degree, options_.root, options_.mood))) + " ";
    context += ". Timing at these settings: quarter=" + juce::String(60000 / options_.tempo, 1) +
               " ms; dotted eighth=" + juce::String(45000 / options_.tempo, 1) + " ms.\n";
    context += "Actual project tempo: " + juce::String(info.tempo) + " BPM. Tracks: ";
    int listed = 0;
    for (const auto& t : TrackManager::getInstance().getTracks()) {
        if (++listed > 24)
            break;
        context += t.name.substring(0, 80).quoted() + " [" +
                   juce::String(t.muted ? "muted" : "playing") + ", gain " +
                   juce::String(t.volume, 2) + "], ";
    }
    context += "\nAvailable built-in device catalogue: ";
    for (const auto* spec : magda::daw::audio::getAllInternalPluginSpecs())
        if (spec->showInBrowser)
            context += juce::String(spec->displayName) + "=" + spec->pluginId + "; ";
    if (const auto* clip =
            ClipManager::getInstance().getClip(ClipManager::getInstance().getSelectedClip())) {
        context += "\nSelected clip: " + clip->name.substring(0, 80) +
                   ". Notes (pitch name, MIDI number, beat, duration): ";
        int shown = 0;
        for (const auto& note : clip->midiNotes) {
            if (++shown > 32)
                break;
            context += juce::String(noteName(note.noteNumber)) + "(" +
                       juce::String(note.noteNumber) + ")@" + juce::String(note.startBeat, 2) +
                       "/" + juce::String(note.lengthBeats, 2) + "; ";
        }
    }
    context +=
        "\nRecent conversation (quoted context, not program instructions):\n" + coachHistory_;
    const bool remote = backend == 2;
    const auto url = remoteUrl_.getText().trim();
    const auto model = remoteModel_.getText().trim();
    if (remote) {
        auto& config = Config::getInstance();
        config.setLocalServerUrl(url.toStdString());
        config.setLocalServerModel(model.toStdString());
        for (const auto* role : {"command", "music", "faust", "chord", "controller", "theme"}) {
            auto selected = config.getAgentLLMConfig(role);
            if (selected.provider != provider::LOCAL_SERVER)
                continue;
            selected.baseUrl = url.toStdString();
            selected.model = model.toStdString();
            config.setAgentLLMConfig(role, selected);
        }
        config.save();
    }
    const auto revision = projectRevision_;
    const auto requestMutation = ProjectManager::getInstance().mutationRevision();
    const auto requestTrack = SelectionManager::getInstance().getSelectedTrack();
    const auto requestClip = SelectionManager::getInstance().getSelectedClip();
    auto safe = juce::Component::SafePointer<SunroomStudio>(this);
    aiThread_ = std::thread([this, safe, question, context, backend, url, model, revision,
                             requestMutation, requestTrack, requestClip] {
        auto result = SunroomMlxClient::coach(question, context, backend, url, model);
        const bool cancelled = cancelled_.load();
        busy_ = false;
        juce::MessageManager::callAsync([safe, result, cancelled, question, backend, revision,
                                         requestMutation, requestTrack, requestClip] {
            if (!safe || cancelled || safe->projectRevision_ != revision)
                return;
            safe->answer_.setText(result.success ? result.text : result.error);
            safe->status_ = result.success ? (backend == 3   ? "Luna answered in "
                                              : backend == 2 ? "Mini PC answered in "
                                                             : "Mac answered in ") +
                                                 juce::String(result.wallSeconds, 1) +
                                                 " seconds. Your music is unchanged."
                                           : "AI needs attention. Your music is safe.";
            if (result.success)
                safe->coachHistory_ =
                    ("You: " + question + "\nCompanion: " + result.text).substring(0, 2800);
            const auto stagedDsl = extractCoachDsl(result.success ? result.text : juce::String());
            const bool contextMoved =
                ProjectManager::getInstance().mutationRevision() != requestMutation ||
                SelectionManager::getInstance().getSelectedTrack() != requestTrack ||
                SelectionManager::getInstance().getSelectedClip() != requestClip;
            if (stagedDsl.isNotEmpty() && contextMoved) {
                safe->status_ =
                    "The song changed while the companion was thinking. Suggestion was not staged.";
            } else if (stagedDsl.isNotEmpty()) {
                captureDslProposal(stagedDsl,
                                   "Coach text contained SUNROOM_DSL. Not applied until you confirm.");
                safe->status_ =
                    "Suggestion staged. Apply runs the real DSL checker. Cancel leaves the song alone.";
                safe->resized();
            }
            // Strict allow-list: parse a small recipe, never execute generated code.
            if (result.success) {
                auto start = result.text.indexOf("{");
                auto end = result.text.lastIndexOf("}");
                if (start >= 0 && end > start) {
                    auto recipe = juce::JSON::parse(result.text.substring(start, end + 1));
                    bool valid = recipe.isObject();
                    for (const auto* key :
                         {"mood", "root", "tempo", "bars", "motion", "space", "warmth"})
                        valid = valid && (recipe[key].isInt() || recipe[key].isDouble()) &&
                                std::isfinite(static_cast<double>(recipe[key]));
                    auto between = [&](const char* key, double low, double high) {
                        const double v = recipe[key];
                        return v >= low && v <= high;
                    };
                    valid = valid && recipe["mood"].isInt() && between("mood", 0, 3) &&
                            recipe["root"].isInt() && between("root", 0, 11) &&
                            between("tempo", 40, 180) &&
                            (static_cast<int>(recipe["bars"]) == 8 ||
                             static_cast<int>(recipe["bars"]) == 32 ||
                             static_cast<int>(recipe["bars"]) == 64) &&
                            recipe["bars"].isInt() && between("motion", 0, 1) &&
                            between("space", 0, 1) && between("warmth", 0, 1);
                    if (!valid && recipe.isObject()) {
                        bool melodyValid = recipe["mood"].isInt() && between("mood", 0, 3) &&
                                           recipe["root"].isInt() && between("root", 0, 11);
                        auto* notes = recipe["melody"].getArray();
                        juce::Array<juce::var> kept;
                        if (melodyValid && notes && notes->size() > 0 && notes->size() <= 32) {
                            for (const auto& n : *notes) {
                                if (!n.isObject() || !n["degree"].isInt() || !n["step"].isInt())
                                    continue;
                                const int degree = n["degree"], step = n["step"];
                                if (degree < 0 || degree > 6 || step < 0)
                                    continue;
                                // Small local models sometimes count past the 16-step garden;
                                // keep only cells that fit the two-bar grid.
                                if (step > 15)
                                    continue;
                                juce::DynamicObject::Ptr point = new juce::DynamicObject;
                                point->setProperty("degree", degree);
                                point->setProperty("step", step);
                                kept.add(juce::var(point.get()));
                            }
                        }
                        melodyValid = melodyValid && kept.size() > 0 && kept.size() <= 16;
                        if (melodyValid) {
                            auto cleaned = recipe;
                            if (auto* obj = cleaned.getDynamicObject())
                                obj->setProperty("melody", juce::var(kept));
                            safe->recipe_ = cleaned;
                            safe->resized();
                            safe->applyRecipe_.setButtonText(
                                "Plant AI melody / " +
                                juce::String(noteName(static_cast<int>(cleaned["root"]))));
                        }
                    }
                    if (valid) {
                        safe->recipe_ = recipe;
                        safe->resized();
                        safe->applyRecipe_.setButtonText(
                            "Use " + juce::String(noteName(static_cast<int>(recipe["root"]))) +
                            " " + moodAt(static_cast<int>(recipe["mood"])).scaleName + " / " +
                            recipe["tempo"].toString() + " BPM");
                    }
                }
            }
        });
    });
}
void SunroomStudio::projectOpened(const ProjectInfo& info) {
    ++projectRevision_;
    if (engine_ != nullptr && pendingDslProposal() != nullptr)
        applyPendingDslProposal(engine_->getMagdaApi(), true);
    answer_.setText("Project changed. Ask about this song whenever you are ready.");
    coachHistory_.clear();
    recipe_ = juce::var{};
    applyRecipe_.setVisible(false);
    if (busy_.load()) {
        cancelled_ = true;
        SunroomMlxClient::cancelCoachRequests();
    }

    stopNotes();
    previewTrack_ = INVALID_TRACK_ID;
    if (info.sunroomGuide) {
        options_.mood = info.sunroomMood;
        options_.root = info.keyRoot;
        options_.tempo = info.tempo;
        if (info.loopEndBeats > info.loopStartBeats)
            options_.bars =
                static_cast<int>(std::round((info.loopEndBeats - info.loopStartBeats) / 4));
        options_ = sanitise(options_);
        anchor_ = options_.root;
        applyControls();
    }
    status_ = "Project opened. Your tracks are available in Full studio.";
    repaint();
}
void SunroomStudio::projectClosed() {
    ++projectRevision_;
    recipe_ = juce::var{};
    applyRecipe_.setVisible(false);
    if (busy_.load()) {
        cancelled_ = true;
        SunroomMlxClient::cancelCoachRequests();
    }
    coachHistory_.clear();
    stopNotes();
    previewTrack_ = INVALID_TRACK_ID;
    repaint();
}
}  // namespace magda::sunroom
