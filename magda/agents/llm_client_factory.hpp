#pragma once

#include <juce_llm/juce_llm.h>

#include "../daw/core/LLMClientProvider.hpp"
#include "llama_local_client.hpp"
#include "sunroom_mlx_client.hpp"
#include "llama_model_manager.hpp"

namespace magda {

inline void registerLocalLLMClientProvider() {
    setLLMClientProvider([](const Config::AgentLLMConfig& config,
                            const std::string& agentName) -> std::unique_ptr<llm::LLMClient> {
        // An explicit Local (Embedded) choice must NEVER silently fall back to a
        // cloud provider: providerFromString() maps "llama_local" to OpenAI, so
        // the old `isLoaded()`-gated fallback quietly billed OpenAI whenever the
        // GGUF wasn't loaded. Always return the local client — it surfaces a
        // clear "model not loaded" error instead of calling the cloud.
        if (config.provider == provider::SUNROOM_MLX)
            return std::make_unique<SunroomMlxClient>(1);
        if (config.provider == provider::SUNROOM_LUNA)
            return std::make_unique<SunroomMlxClient>(3);
        if (config.provider == provider::LOCAL_SERVER)
            return std::make_unique<SunroomMlxClient>(2, juce::String(config.baseUrl),
                                                      juce::String(config.model));
        if (config.provider == provider::LLAMA_LOCAL)
            return std::make_unique<LlamaLocalClient>();

        return llm::LLMClientFactory::create(toLLMProviderConfig(config, agentName));
    });
    setLLMProviderShutdownHandler([] { SunroomMlxClient::shutdown(); LlamaModelManager::getInstance().unloadModel(); });
}

}  // namespace magda
