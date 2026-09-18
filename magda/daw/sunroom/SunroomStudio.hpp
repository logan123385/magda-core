#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <atomic>
#include <thread>

#include "MusicTheory.hpp"
#include "core/ClipInfo.hpp"
#include "core/TypeIds.hpp"
#include "project/ProjectManager.hpp"

namespace magda {
class AudioEngine;
namespace sunroom {

enum class StarterKind { Beat, Song, Blank };

class SunroomStudio final : public juce::Component,
                            private juce::Timer,
                            private ProjectManagerListener {
  public:
    explicit SunroomStudio(AudioEngine* engine);
    ~SunroomStudio() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    std::function<void()> onOpenStudio, onSave, onExport, onNewProject, onOpenProject;
    std::function<void()> onShowSession;
    std::function<void()> onShowArrange;
    std::function<void()> onShowMix;
    std::function<void()> onCaptureJam;
    std::function<void()> onReturnToArrangement;
    std::function<void(TrackId, ClipId)> onEditClip;
    std::function<void()> onEnableQwerty;

  private:
    AudioEngine* engine_;
    Options options_;
    Options previewOptions_;
    Journey preview_;
    bool previewValid_ = false;
    StarterKind starter_ = StarterKind::Beat;
    int tab_ = 0;
    int anchor_ = 2;
    int previewLayer_ = 2;
    int sampleOffset_ = 0;
    TrackId previewTrack_ = INVALID_TRACK_ID;
    std::vector<int> sounding_;
    double noteOffTime_ = 0;
    juce::String status_ = "Choose Beat, Song, or Blank — then Create and Play.";
    juce::String pairText_ =
        "Click a note to hear it. Shift-click another to hear the relationship.";
    juce::String lastSummary_;
    juce::TextButton newProject_{"New project"}, openProject_{"Open project"},
        create_{"Create and Play"}, play_{"Play"}, stop_{"Stop"},
        makeBeat_{"Make a Beat"}, addChords_{"Add Chords"}, playSound_{"Play a Sound"},
        captureJam_{"Capture Jam"}, placeScene_{"Place Scene"}, returnArrange_{"Return to Arrangement"},
        openMix_{"Open Mix"}, sharedSpace_{"Shared Space"},
        journey_{"Mood journey"}, save_{"Save project"}, export_{"Export audio"}, undo_{"Undo"},
        studio_{"Full studio"}, skipGuide_{"Skip guide"}, ask_{"Ask SUNROOM"}, cancelAI_{"Stop"},
        addPhrase_{"Add melody to song"}, clearPhrase_{"Clear melody"},
        library_{"Open sound folder"}, applyRecipe_{"Use these settings"},
        applySuggestion_{"Apply suggestion"}, cancelSuggestion_{"Cancel suggestion"},
        hearBefore_{"Hear before"}, hearAfter_{"Hear after"},
        saveKey_{"Save to Keychain"};
    std::array<juce::TextButton, 4> tabs_;
    std::array<juce::TextButton, 3> starterButtons_;
    std::array<juce::TextButton, 4> moodButtons_;
    std::array<juce::ToggleButton, 7> layerButtons_;
    juce::ComboBox root_, length_, aiBackend_, soundFilter_;
    juce::Slider tempo_, motion_, space_, warmth_;
    juce::TextEditor prompt_, answer_, remoteUrl_, remoteModel_, apiKey_;
    juce::String coachHistory_;
    std::array<std::array<bool, 16>, 7> melody_{};
    juce::Rectangle<int> hero_, left_, centre_, right_, noteArea_, melodyArea_, journeyArea_;
    std::vector<std::pair<juce::Rectangle<int>, TrackId>> trackHitboxes_;
    std::vector<std::pair<juce::Rectangle<int>, juce::File>> sampleHitboxes_;
    juce::Array<juce::File> samples_;
    juce::var recipe_;
    std::unique_ptr<juce::LookAndFeel> skin_;
    std::atomic<bool> cancelled_{false};
    std::atomic<bool> busy_{false};
    std::atomic<bool> creating_{false};
    std::thread aiThread_;
    uint64_t projectRevision_ = 0;
    void timerCallback() override;
    void stopNotes();
    void audition(int note, bool together);
    void setTab(int);
    void setStarter(StarterKind);
    void createAndPlay();
    void buildJourney();
    void applyControls();
    void refreshSoundShelf();
    void askCoach();
    void addMelody();
    bool hasFixtureATracks() const;
    void openFixtureClip(const juce::String& trackName, const juce::String& clipName,
                         bool enableQwerty);
    void expandToFixtureB();
    void placeSceneInArrangement();
    void applySharedSpatialReturn();
    void applyStagedSuggestion();
    void cancelStagedSuggestion();
    void hearStagedSuggestion(bool after);
    juce::String playbackSourceSummary() const;
    void paintCreate(juce::Graphics&);
    void paintGarden(juce::Graphics&);
    void paintSounds(juce::Graphics&);
    void paintCoach(juce::Graphics&);
    void projectOpened(const ProjectInfo&) override;
    void projectClosed() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SunroomStudio)
};
// Shared vector artwork. Drawn at native resolution, never a scaled screenshot.
void drawSun(juce::Graphics& g, juce::Rectangle<float> bounds, float opacity = 1.0f);
}  // namespace sunroom
}  // namespace magda
