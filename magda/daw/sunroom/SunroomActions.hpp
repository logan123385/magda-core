#pragma once
#include "MusicTheory.hpp"
#include "core/ClipInfo.hpp"
#include "core/DeviceInfo.hpp"
#include "core/TrackInfo.hpp"
#include "core/UndoManager.hpp"
#include "project/ProjectInfo.hpp"

#include <optional>
#include <vector>

namespace magda {
class AudioEngine;
class MagdaApi;
namespace sunroom {
DeviceInfo makeDevice(const juce::String& id, const juce::String& name, bool instrument,
                      const std::vector<std::pair<int, float>>& params = {});
class CreateJourneyCommand final : public UndoableCommand {
  public:
    CreateJourneyCommand(Options options, AudioEngine* engine);
    void execute() override;
    void undo() override;
    juce::String getDescription() const override {
        return "Create SUNROOM journey";
    }
    const std::vector<TrackId>& trackIds() const {
        return ids_;
    }

  private:
    Options options_;
    AudioEngine* engine_;
    ProjectInfo before_;
    ProjectInfo after_;
    std::vector<TrackId> ids_;
    std::vector<TrackInfo> tracks_;
    std::vector<ClipInfo> clips_;
    bool captured_ = false;
    void applyProject(const ProjectInfo& info);
};

/** Offline Magda Fixture A: drums + bass + chords, 8 bars @ 100 BPM A minor. */
class CreateFixtureACommand final : public UndoableCommand {
  public:
    explicit CreateFixtureACommand(AudioEngine* engine);
    void execute() override;
    void undo() override;
    juce::String getDescription() const override {
        return "Create beginner Fixture A";
    }
    const std::vector<TrackId>& trackIds() const {
        return ids_;
    }
    const juce::String& summary() const {
        return summary_;
    }
    bool failed() const {
        return failed_;
    }
    const juce::String& failureReason() const {
        return failureReason_;
    }

  private:
    AudioEngine* engine_;
    ProjectInfo before_;
    ProjectInfo after_;
    std::vector<TrackId> ids_;
    std::vector<TrackInfo> tracks_;
    std::vector<ClipInfo> clips_;
    juce::String summary_;
    juce::String failureReason_;
    bool captured_ = false;
    bool failed_ = false;
    void applyProject(const ProjectInfo& info);
    void rollbackPartial();
};

/**
 * Fixture B: expand Fixture A into a 32-bar song with Intro/Main/Variation/Ending
 * markers, session scenes 0–3, and arrangement section clips. Does not invent a
 * fourth composition model — duplicates editable MIDI clips.
 */
class CreateFixtureBCommand final : public UndoableCommand {
  public:
    explicit CreateFixtureBCommand(AudioEngine* engine);
    void execute() override;
    void undo() override;
    juce::String getDescription() const override {
        return "Create beginner Fixture B sections";
    }
    const juce::String& summary() const {
        return summary_;
    }
    bool failed() const {
        return failed_;
    }
    const juce::String& failureReason() const {
        return failureReason_;
    }

  private:
    AudioEngine* engine_;
    ProjectInfo before_;
    ProjectInfo after_;
    std::vector<ClipInfo> createdClips_;
    std::vector<ClipInfo> removedClips_;
    juce::String summary_;
    juce::String failureReason_;
    bool captured_ = false;
    bool failed_ = false;
    void applyProject(const ProjectInfo& info);
};

/**
 * Deterministic Place Scene in Arrangement — copies session-slot clips into the
 * arrangement at a beat destination. Refuses silent overwrite of occupied ranges.
 * Distinct from Capture Jam (performance timing via SessionRecorder).
 */
class PlaceSceneInArrangementCommand final : public UndoableCommand {
  public:
    PlaceSceneInArrangementCommand(int sceneIndex, double destStartBeats);
    void execute() override;
    void undo() override;
    juce::String getDescription() const override {
        return "Place Scene in Arrangement";
    }
    const juce::String& summary() const {
        return summary_;
    }
    bool failed() const {
        return failed_;
    }
    const juce::String& failureReason() const {
        return failureReason_;
    }

  private:
    int sceneIndex_ = 0;
    double destStartBeats_ = 0.0;
    std::vector<ClipInfo> createdClips_;
    juce::String summary_;
    juce::String failureReason_;
    bool captured_ = false;
    bool failed_ = false;
};

/**
 * Fixture C: one track with sparse arrangement vs different Session rhythm;
 * second track stays Arrangement-only — for mixed-source badge checks.
 */
class CreateFixtureCCommand final : public UndoableCommand {
  public:
    explicit CreateFixtureCCommand(AudioEngine* engine);
    void execute() override;
    void undo() override;
    juce::String getDescription() const override {
        return "Create beginner Fixture C";
    }
    const juce::String& summary() const {
        return summary_;
    }
    bool failed() const {
        return failed_;
    }
    const juce::String& failureReason() const {
        return failureReason_;
    }

  private:
    AudioEngine* engine_;
    ProjectInfo before_;
    ProjectInfo after_;
    std::vector<TrackId> ids_;
    void rollbackPartial();
    std::vector<TrackInfo> tracks_;
    std::vector<ClipInfo> clips_;
    juce::String summary_;
    juce::String failureReason_;
    bool captured_ = false;
    bool failed_ = false;
};

/**
 * Ensure one shared Aux return hosting magda_reverb, then set send level from
 * starter instrument tracks. Reuses an existing Aux+reverb; does not duplicate
 * returns. Beginner control is send amount (not insert wet/dry).
 */
class ApplySharedSpatialReturnCommand final : public UndoableCommand {
  public:
    explicit ApplySharedSpatialReturnCommand(float sendLevel);
    void execute() override;
    void undo() override;
    juce::String getDescription() const override {
        return "Apply shared spatial return";
    }
    const juce::String& summary() const {
        return summary_;
    }
    bool failed() const {
        return failed_;
    }
    const juce::String& failureReason() const {
        return failureReason_;
    }
    TrackId auxTrackId() const {
        return auxTrackId_;
    }

  private:
    struct SendSnapshot {
        TrackId sourceId = INVALID_TRACK_ID;
        int busIndex = -1;
        float previousLevel = 0.0f;
        bool hadSend = false;
        bool addedSend = false;
    };

    float sendLevel_ = 0.35f;
    TrackId auxTrackId_ = INVALID_TRACK_ID;
    TrackInfo createdAuxTrack_;
    bool didCreateAux_ = false;
    DeviceId createdReverbId_ = INVALID_DEVICE_ID;
    bool didAddReverb_ = false;
    std::vector<SendSnapshot> sends_;
    juce::String summary_;
    juce::String failureReason_;
    bool captured_ = false;
    bool failed_ = false;
};

// Appends a new instrument track through MAGDA's normal undo stack.
TrackId addInstrument(int layer, const Options& options);
juce::File soundLibrary();

/**
 * Offline starter shelf. Reads manifest.json next to the WAVs.
 * BPM and key are used only when those fields are present. Kind text that
 * says "pitched" is declared pitched with an unknown key, not a detected key.
 * Everything else in this pack is declared unpitched. No analysis, no network.
 */
struct StarterSound {
    juce::String file;
    juce::String kind;
    bool unpitched = true;
    std::optional<double> declaredBpm;
    std::optional<int> declaredKeyRoot;
};

struct StarterQuery {
    juce::String text;
    juce::String kind;
    std::optional<double> bpmMin;
    std::optional<double> bpmMax;
    std::optional<int> keyRoot;
};

struct StarterQueryReport {
    std::vector<StarterSound> matches;
    int unknownBpm = 0;
    int unknownKey = 0;
    int unpitchedSkipped = 0;
    juce::String note;
};

std::vector<StarterSound> loadStarterCatalog();
StarterQueryReport queryStarterCatalog(const StarterQuery& query);
juce::String formatStarterQuery(const StarterQueryReport& report);

/** File must be a direct child of the starter folder. Missing files are empty. */
juce::File resolveStarterFile(const juce::String& fileName);

/**
 * Import one starter WAV as a normal audio clip. Undo removes the track and
 * clip only. The source WAV is never deleted.
 */
class ImportStarterSampleCommand final : public UndoableCommand {
  public:
    explicit ImportStarterSampleCommand(juce::String fileName);
    void execute() override;
    void undo() override;
    juce::String getDescription() const override {
        return "Add starter sound";
    }
    bool failed() const {
        return failed_;
    }
    const juce::String& failureReason() const {
        return failureReason_;
    }
    const juce::String& summary() const {
        return summary_;
    }

  private:
    juce::String fileName_;
    juce::String summary_;
    juce::String failureReason_;
    TrackInfo track_;
    ClipInfo clip_;
    bool captured_ = false;
    bool failed_ = false;
};

/**
 * Staged DSL proposal. Capture does not mutate. Apply re-checks project path,
 * mutation revision and selection, then runs dsl::Interpreter inside one undo
 * compound. Model text is not a permission grant.
 */
enum class ProposalPhase { Ready, Applying, Applied, Rejected, Canceled, Failed, Stale };

struct StagedDslProposal {
    std::uint64_t id = 0;
    juce::String projectPath;
    std::uint64_t mutationRevision = 0;
    TrackId selectedTrack = INVALID_TRACK_ID;
    ClipId selectedClip = INVALID_CLIP_ID;
    juce::String dsl;
    juce::String explanation;
    ProposalPhase phase = ProposalPhase::Ready;
};

StagedDslProposal captureDslProposal(const juce::String& dsl, const juce::String& explanation);
juce::String applyPendingDslProposal(MagdaApi& api, bool cancelled);
const StagedDslProposal* pendingDslProposal();
/** One statement after SUNROOM_DSL:. Empty if the coach text has no staged action. */
juce::String extractCoachDsl(const juce::String& text);

}  // namespace sunroom
}  // namespace magda
