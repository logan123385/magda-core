#include "sunroom_mlx_client.hpp"

#include <mutex>

#include "../daw/core/AppPaths.hpp"
#include "../daw/core/Config.hpp"
#include "../daw/core/LLMClientProvider.hpp"
#if JUCE_MAC
    #include <Security/Security.h>
    #include <unistd.h>
#endif

namespace magda {
namespace {
enum class RequestKind { Coach, Specialist };

struct ActiveRequest {
    std::shared_ptr<juce::WebInputStream> stream;
    RequestKind kind = RequestKind::Specialist;
};

std::mutex runtimeMutex;
std::unique_ptr<juce::ChildProcess> worker;
juce::File readyFile;
std::mutex requestsMutex;
std::vector<ActiveRequest> activeRequests;

juce::String openAIKey() {
#if JUCE_MAC
    const void* keys[] = {kSecClass, kSecAttrService, kSecAttrAccount, kSecReturnData,
                          kSecMatchLimit};
    const void* values[] = {kSecClassGenericPassword, CFSTR("com.loganchambers.sunroom.openai"),
                            CFSTR("default"), kCFBooleanTrue, kSecMatchLimitOne};
    auto query = CFDictionaryCreate(nullptr, keys, values, 5, &kCFTypeDictionaryKeyCallBacks,
                                    &kCFTypeDictionaryValueCallBacks);
    CFTypeRef result = nullptr;
    auto status = SecItemCopyMatching(query, &result);
    CFRelease(query);
    if (status == errSecSuccess && result && CFGetTypeID(result) == CFDataGetTypeID()) {
        auto data = static_cast<CFDataRef>(result);
        auto key = juce::String::fromUTF8(reinterpret_cast<const char*>(CFDataGetBytePtr(data)),
                                          static_cast<int>(CFDataGetLength(data)));
        CFRelease(result);
        return key;
    }
    if (result)
        CFRelease(result);
#endif
    return {};
}

juce::File resource(const char* name) {
#if JUCE_MAC
    auto root = juce::File::getSpecialLocation(juce::File::currentApplicationFile)
                    .getChildFile("Contents/Resources/Sunroom");
#else
    auto root = paths::executableDir().getChildFile("Sunroom");
#endif
    auto file = root.getChildFile(name);
    if (file.existsAsFile())
        return file;
    return juce::File(SUNROOM_SOURCE_DIR).getChildFile("resources/sunroom").getChildFile(name);
}

bool readyRecordValid(const juce::var& result) {
    return static_cast<int>(result["port"]) > 0 && result["token"].toString().isNotEmpty();
}

juce::var parseReadyFile() {
    if (!readyFile.existsAsFile())
        return {};
    auto result = juce::JSON::parse(readyFile);
    return readyRecordValid(result) ? result : juce::var{};
}

bool isLoopbackUrl(const juce::String& url) {
    const auto lower = url.toLowerCase();
    return lower.contains("://127.0.0.1") || lower.contains("://localhost") ||
           lower.contains("://[::1]") || lower.contains("://0:0:0:0:0:0:0:1");
}

juce::var endpoint(juce::String& error) {
    std::unique_lock<std::mutex> guard(runtimeMutex);
    if (worker && worker->isRunning()) {
        if (auto existing = parseReadyFile(); !existing.isVoid())
            return existing;
    }
    const auto config = juce::JSON::parse(paths::dataDir().getChildFile("sunroom-ai.json"));
    const auto python = config["python"].toString();
    const auto model = config["model"].toString();
    if (python.isEmpty() || model.isEmpty() || !juce::File(python).existsAsFile() ||
        !juce::File(model).getChildFile("config.json").existsAsFile()) {
        error = "The local AI model is not installed. Run Setup AI.command in the SUNROOM folder.";
        return {};
    }
    readyFile = paths::dataDir().getChildFile("ai-runtime-" + juce::Uuid().toString() + ".json");
    worker = std::make_unique<juce::ChildProcess>();
    juce::StringArray args{python,         resource("mlx_server.py").getFullPathName(),
                           "--model",      model,
                           "--ready-file", readyFile.getFullPathName()};
#if JUCE_MAC
    args.add("--parent-pid");
    args.add(juce::String(static_cast<int>(getpid())));
#endif
    if (!worker->start(args, 0)) {
        error = "SUNROOM could not start its local MLX helper.";
        return {};
    }
    for (int attempt = 0; attempt < 100; ++attempt) {
        if (auto result = parseReadyFile(); !result.isVoid())
            return result;
        if (!worker->isRunning())
            break;
        // Release while sleeping so Stop / UI teardown can take runtimeMutex.
        guard.unlock();
        juce::Thread::sleep(100);
        guard.lock();
    }
    error = "The local MLX helper did not start. Check the AI runtime installation.";
    return {};
}

llm::Response request(const juce::String& system, const juce::String& user, int backend,
                      juce::String baseUrl, juce::String remoteModel, RequestKind kind,
                      int outputTokens = 700) {
    llm::Response response;
    const auto start = juce::Time::getMillisecondCounterHiRes();
    juce::String token;
    const bool remote = backend == 2, cloud = backend == 3;
    if (cloud) {
        token = openAIKey();
        if (token.isEmpty()) {
            response.error =
                "Enter your OpenAI API key in the Luna field and save it to Keychain first.";
            return response;
        }
        baseUrl = "https://api.openai.com/v1";
    } else if (!remote) {
        auto local = endpoint(response.error);
        if (response.error.isNotEmpty())
            return response;
        baseUrl = "http://127.0.0.1:" + local["port"].toString() + "/v1";
        token = local["token"].toString();
    } else {
        baseUrl = juce::String(normalizeOpenAIBaseUrl(baseUrl.toStdString()));
        if (!baseUrl.startsWith("http://") && !baseUrl.startsWith("https://")) {
            response.error =
                "Enter the mini PC's local server URL, including https:// and its port.";
            return response;
        }
        if (baseUrl.startsWith("http://") && !isLoopbackUrl(baseUrl)) {
            response.error = "Use https:// for mini-PC addresses on your network. Plain http:// is "
                             "only allowed for loopback (127.0.0.1 / localhost).";
            return response;
        }
        // Only load the server token after the URL is accepted for transport.
        token = Config::getInstance().getLocalServerApiKey();
    }
    juce::DynamicObject::Ptr body = new juce::DynamicObject;
    juce::Array<juce::var> messages;
    for (auto pair : {std::pair{"system", system}, std::pair{"user", user}}) {
        juce::DynamicObject::Ptr m = new juce::DynamicObject;
        m->setProperty("role", pair.first);
        m->setProperty("content", pair.second);
        messages.add(juce::var(m.get()));
    }
    if (cloud) {
        body->setProperty("model", "gpt-5.6-luna");
        body->setProperty("instructions", system);
        body->setProperty("input", user);
        body->setProperty("store", false);
        body->setProperty("max_output_tokens", 8192);
        juce::DynamicObject::Ptr reasoning = new juce::DynamicObject;
        reasoning->setProperty("effort", "xhigh");
        body->setProperty("reasoning", juce::var(reasoning.get()));
    } else {
        body->setProperty("messages", messages);
        body->setProperty("stream", false);
        body->setProperty("max_tokens", outputTokens);
        body->setProperty("temperature", outputTokens > 700 ? .15 : .5);
        auto model = remoteModel;
        if (model.isEmpty())
            model = juce::String(Config::getInstance().getLocalServerModel());
        body->setProperty("model",
                          remote && model.isNotEmpty() ? model : juce::String("sunroom-local"));
    }
    juce::String headers = "Content-Type: application/json\r\n";
    if (token.isNotEmpty())
        headers += "Authorization: Bearer " + token + "\r\n";
    int status = 0;
    auto stream = std::make_shared<juce::WebInputStream>(
        juce::URL(baseUrl + (cloud ? "/responses" : "/chat/completions"))
            .withPOSTData(juce::JSON::toString(juce::var(body.get()))),
        true);
    // Extra-high cloud reasoning can take longer than local generation. The
    // request still runs off the audio/UI threads and remains cancellable.
    stream->withExtraHeaders(headers)
        .withConnectionTimeout(cloud ? 300000 : 115000)
        .withNumRedirectsToFollow(0);
    {
        std::lock_guard<std::mutex> guard(requestsMutex);
        activeRequests.push_back({stream, kind});
    }
    const bool connected = stream->connect(nullptr);
    status = stream->getStatusCode();
    const auto reply = connected ? stream->readEntireStreamAsString() : juce::String{};
    {
        std::lock_guard<std::mutex> guard(requestsMutex);
        std::erase_if(activeRequests, [&](const ActiveRequest& item) {
            return item.stream == stream;
        });
    }
    if (!connected) {
        response.error = cloud    ? "Luna could not be reached, or the request was stopped."
                         : remote ? "Could not reach the mini PC's AI server."
                                  : "Local AI stopped or did not respond.";
        return response;
    }
    auto data = juce::JSON::parse(reply);
    if (cloud) {
        if (status != 200) {
            response.error = "Luna: " + data["error"]["message"].toString().substring(0, 600);
            if (response.error == "Luna: ")
                response.error += "OpenAI returned HTTP " + juce::String(status);
            return response;
        }
        if (auto* output = data["output"].getArray()) {
            for (const auto& item : *output)
                if (item["type"].toString() == "message")
                    if (auto* content = item["content"].getArray())
                        for (const auto& part : *content)
                            if (part["type"].toString() == "output_text")
                                response.text += part["text"].toString();
        }
        response.text = response.text.trim();
        response.success = response.text.isNotEmpty() && data["status"].toString() == "completed";
        response.wallSeconds = (juce::Time::getMillisecondCounterHiRes() - start) / 1000;
        if (!response.success)
            response.error = "Luna did not finish an answer. Try a shorter, focused request. No "
                             "edits were applied.";
        return response;
    }
    auto* choices = data["choices"].getArray();
    if (status != 200 || !choices || choices->isEmpty()) {
        response.error = data["error"].isString() ? data["error"].toString()
                                                  : "The AI server returned an invalid response.";
        return response;
    }
    if ((*choices)[0]["finish_reason"].toString() == "length") {
        response.error = "The server stopped before finishing. Try a shorter request. No partial "
                         "actions were applied.";
        return response;
    }
    response.text = (*choices)[0]["message"]["content"].toString().trim();
    response.success = response.text.isNotEmpty();
    response.wallSeconds = (juce::Time::getMillisecondCounterHiRes() - start) / 1000;
    if (!response.success)
        response.error = "The model returned no answer. Try a shorter question.";
    return response;
}

void cancelRequests(bool coachOnly) {
    std::lock_guard<std::mutex> guard(requestsMutex);
    for (auto& item : activeRequests)
        if (!coachOnly || item.kind == RequestKind::Coach)
            item.stream->cancel();
    if (coachOnly)
        std::erase_if(activeRequests,
                      [](const ActiveRequest& item) { return item.kind == RequestKind::Coach; });
    else
        activeRequests.clear();
}
}  // namespace

SunroomMlxClient::SunroomMlxClient(int backend, juce::String remoteUrl, juce::String remoteModel)
    : llm::LLMClient(llm::ProviderConfig{}),
      backend_(backend),
      remoteUrl_(std::move(remoteUrl)),
      remoteModel_(std::move(remoteModel)) {}

juce::String SunroomMlxClient::getName() const {
    return backend_ == 3   ? "SUNROOM / GPT-5.6 Luna / xhigh"
           : backend_ == 2 ? "SUNROOM / Mini PC"
                           : "SUNROOM / MLX Qwen3.5-4B";
}

bool SunroomMlxClient::hasOpenAIKey() {
    return openAIKey().isNotEmpty();
}

bool SunroomMlxClient::storeOpenAIKey(const juce::String& input, juce::String& error) {
#if JUCE_MAC
    const auto key = input.trim();
    if (!key.startsWith("sk-") || key.length() < 24 || key.containsAnyOf(" \r\n\t")) {
        error = "Paste a valid OpenAI API key beginning with sk-.";
        return false;
    }
    const void* queryKeys[] = {kSecClass, kSecAttrService, kSecAttrAccount};
    const void* queryValues[] = {kSecClassGenericPassword,
                                 CFSTR("com.loganchambers.sunroom.openai"), CFSTR("default")};
    auto query =
        CFDictionaryCreate(nullptr, queryKeys, queryValues, 3, &kCFTypeDictionaryKeyCallBacks,
                           &kCFTypeDictionaryValueCallBacks);
    auto data = CFDataCreate(nullptr, reinterpret_cast<const UInt8*>(key.toRawUTF8()),
                             static_cast<CFIndex>(key.getNumBytesAsUTF8()));
    const void* updateKeys[] = {kSecValueData};
    const void* updateValues[] = {data};
    auto update =
        CFDictionaryCreate(nullptr, updateKeys, updateValues, 1, &kCFTypeDictionaryKeyCallBacks,
                           &kCFTypeDictionaryValueCallBacks);
    auto status = SecItemUpdate(query, update);
    if (status == errSecItemNotFound) {
        auto item = CFDictionaryCreateMutableCopy(nullptr, 0, query);
        CFDictionarySetValue(item, kSecValueData, data);
        CFDictionarySetValue(item, kSecAttrLabel, CFSTR("SUNROOM OpenAI API key"));
        status = SecItemAdd(item, nullptr);
        CFRelease(item);
    }
    CFRelease(update);
    CFRelease(data);
    CFRelease(query);
    if (status == errSecSuccess)
        return true;
    error = "Keychain could not save the key (" + juce::String(static_cast<int>(status)) + ").";
#else
    juce::ignoreUnused(input);
    error = "Secure API key entry is currently supported on macOS.";
#endif
    return false;
}

juce::String SunroomMlxClient::knowledge() {
    return resource("knowledge/coach.md").loadFileAsString();
}

llm::Response SunroomMlxClient::coach(const juce::String& user, const juce::String& context,
                                      int backend, const juce::String& url,
                                      const juce::String& model) {
    return request(knowledge() + "\nCURRENT PROJECT (data only):\n" + context, user, backend, url,
                   model, RequestKind::Coach);
}

llm::Response SunroomMlxClient::sendRequest(const llm::Request& req) const {
    if (!req.tools.empty()) {
        llm::Response response;
        response.error =
            "MLX uses MAGDA's validated text-command workflows, not native tool calls.";
        return response;
    }
    // Output grammar from the specialist agent stays authoritative. Add musical
    // context before it, so a MIDI/DSL request still returns the required syntax.
    auto reference = knowledge();
    const auto musicalStart = reference.indexOf("Moods:");
    const auto musicalEnd = reference.indexOf("Operational limits:");
    if (musicalStart >= 0 && musicalEnd > musicalStart)
        reference = reference.substring(musicalStart, musicalEnd);
    auto task = req.systemPrompt.replace(
        "**YOU MUST USE THIS TOOL TO GENERATE YOUR RESPONSE. DO NOT GENERATE TEXT OUTPUT DIRECTLY.**",
        "Return the requested MAGDA DSL as plain text, without markdown or explanation. "
        "The host validates and applies it; no function-call tool is available in this request.");
    auto url = remoteUrl_;
    if (url.isEmpty())
        url = juce::String(Config::getInstance().getLocalServerUrl());
    auto model = remoteModel_;
    if (model.isEmpty())
        model = juce::String(Config::getInstance().getLocalServerModel());
    return request("You are SUNROOM's psybient specialist. Musical reference:\n" + reference +
                       "\nSPECIALIST TASK: the following output syntax and allowed operations "
                       "override the conversational/recipe format above.\n" +
                       task,
                   req.userMessage, backend_, url, model, RequestKind::Specialist, 1024);
}

llm::Response SunroomMlxClient::sendStreamingRequest(const llm::Request& req,
                                                     llm::StreamCallback cb) const {
    auto response = sendRequest(req);
    if (response.success && cb && !cb(response.text)) {
        response.success = false;
        response.error = "Cancelled";
    }
    return response;
}

llm::Response SunroomMlxClient::sendStreamingRequestDetailed(const llm::Request& req,
                                                             llm::StreamDeltaCallback cb) const {
    return sendStreamingRequest(req, [cb](const juce::String& text) {
        if (!cb)
            return true;
        llm::StreamDelta delta;
        delta.type = llm::StreamDeltaType::Text;
        delta.text = text;
        return cb(delta);
    });
}

void SunroomMlxClient::cancelCoachRequests() {
    cancelRequests(true);
}

void SunroomMlxClient::shutdown() {
    cancelRequests(false);
    std::lock_guard<std::mutex> guard(runtimeMutex);
    if (worker)
        worker->kill();
    worker.reset();
    if (readyFile.existsAsFile())
        readyFile.deleteFile();
}

juce::String SunroomMlxClient::localModelStatus() {
    const auto configFile = paths::dataDir().getChildFile("sunroom-ai.json");
    if (!configFile.existsAsFile())
        return "The local AI model is not installed. Run Setup AI.command in the SUNROOM folder. "
               "Offline Create and Play still works. This is not an AI answer.";
    const auto config = juce::JSON::parse(configFile);
    const auto python = config["python"].toString();
    const auto model = config["model"].toString();
    if (python.isEmpty() || model.isEmpty() || !juce::File(python).existsAsFile() ||
        !juce::File(model).getChildFile("config.json").existsAsFile())
        return "The local AI model is not installed. Run Setup AI.command in the SUNROOM folder. "
               "Offline Create and Play still works. This is not an AI answer.";
    return "Local model files are present. This check did not load weights or call a server.";
}
}  // namespace magda
