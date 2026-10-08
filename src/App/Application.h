#pragma once
#include "../Common.h"
#include "../UI/GhostWindow.h"
#include "../Core/PeReader.h"
#include "../Core/InstructionScanner.h"
#include "../Core/StringScanner.h"
#include "../Core/ThreatAssessor.h"
#include <memory>
#include <thread>
#include <mutex>
#include <string>

namespace Koltzi {

class Application {
public:
    Application();
    ~Application();

    int Run(int argc, wchar_t* argv[]);

    // Triage a file asynchronously on worker thread
    void TriageFileAsync(const std::wstring& filePath);

    // Triage a buffer in memory
    void TriageMemoryAsync(const uint8_t* data, size_t size, const std::string& sampleName);

    // Synchronous CLI triage (for automated tests or headless inspection)
    bool TriageFileCli(const std::wstring& filePath);

private:
    void SetupCallbacks();
    void OnWorkerCompleted(std::shared_ptr<TriageReport> report);
    void HandleMenuCommand(int cmd);

    // Sample generators
    std::vector<uint8_t> GenerateCleanSample();
    std::vector<uint8_t> GeneratePackedSample();
    std::vector<uint8_t> GenerateSyscallPebSample();
    std::vector<uint8_t> GenerateCredStealerSample();
    std::vector<uint8_t> GenerateInjectionSample();

    std::unique_ptr<GhostWindow> m_window;
    std::jthread m_workerThread;
    std::mutex m_triageMutex;

    InstructionScanner m_instructionScanner;
    StringScanner m_stringScanner;
};

} // namespace Koltzi
