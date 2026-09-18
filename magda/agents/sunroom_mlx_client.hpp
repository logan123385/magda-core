#pragma once
#include <juce_llm/juce_llm.h>

namespace magda {
class SunroomMlxClient final : public llm::LLMClient {
  public:
    // 1 = on-device MLX, 2 = user's LAN server, 3 = OpenAI Luna.
    explicit SunroomMlxClient(int backend = 1, juce::String remoteUrl = {},
                              juce::String remoteModel = {});
    juce::String getName() const override;
    llm::Response sendRequest(const llm::Request&) const override;
    llm::Response sendStreamingRequest(const llm::Request&, llm::StreamCallback) const override;
    llm::Response sendStreamingRequestDetailed(const llm::Request&,
                                               llm::StreamDeltaCallback) const override;
    static juce::String knowledge();
    static llm::Response coach(const juce::String& user, const juce::String& context,
                               int backend = 1, const juce::String& url = {},
                               const juce::String& model = {});
    static bool storeOpenAIKey(const juce::String& key, juce::String& error);
    static bool hasOpenAIKey();
    // Cancel only companion/coach HTTP streams. Leaves specialist agent requests
    // and the local MLX worker running.
    static void cancelCoachRequests();
    // App teardown: cancel every stream and stop the local worker.
    static void shutdown();
    /** Does not start the worker or load weights. */
    static juce::String localModelStatus();

  protected:
    int backend_;
    juce::String remoteUrl_;
    juce::String remoteModel_;
    juce::String buildRequestBody(const llm::Request&) const override {
        return {};
    }
    juce::String getEndpointUrl() const override {
        return {};
    }
    juce::StringPairArray getHeaders() const override {
        return {};
    }
    llm::Response parseResponseBody(const juce::String&) const override {
        return {};
    }
};
}  // namespace magda
