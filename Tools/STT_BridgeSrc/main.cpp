// WITH:OUT STT Bridge - NVIDIA NVIGI(Whisper) 기반 로컬 STT 서버.
// CompanionListenComponent(언리얼)가 POST /stt 로 WAV 바이트를 보내면, Whisper로 텍스트를 뽑아
// {"text":"..."} JSON으로 응답한다. GET /health 는 살아있는지 확인용.
//
// 원래 있던 STT_Bridge.exe는 소스가 없는 블랙박스였고, NVIDIA 공식 샘플의 core.hpp에 있던
// sdkPath 댕글링 string_view 버그(임시 std::string.substr() 결과를 string_view에 대입) 때문에
// "NVIGI 코어 라이브러리를 못 불러옴"으로 죽고 있었다. 이 파일은 그 문제를 피해 처음부터 다시 짠
// 버전이다.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
extern "C" __declspec(dllexport) UINT D3D12SDKVersion = 615;
extern "C" __declspec(dllexport) const char* D3D12SDKPath = ".\\D3D12\\";

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <cstring>
#include <cstdint>

#include "cxx_wrappers/core.hpp"
#include "cxx_wrappers/asr/asr.hpp"

namespace
{
    // ---------------------------------------------------------------------
    // WAV 파싱 (CompanionAudioUtils::ParseWavPCM과 동일한 청크 파서를 C++ 표준으로 재구현)
    // ---------------------------------------------------------------------
    struct WavPCM
    {
        std::vector<int16_t> Samples; // interleaved
        int32_t SampleRate = 0;
        int32_t NumChannels = 0;
        bool bValid = false;
    };

    uint32_t ReadU32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
    uint16_t ReadU16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

    WavPCM ParseWav(const std::vector<uint8_t>& Data)
    {
        WavPCM Out;
        if (Data.size() < 44) return Out;

        const uint8_t* D = Data.data();
        if (D[0] != 'R' || D[1] != 'I' || D[2] != 'F' || D[3] != 'F' ||
            D[8] != 'W' || D[9] != 'A' || D[10] != 'V' || D[11] != 'E')
        {
            return Out;
        }

        bool bFoundFmt = false;
        bool bFoundData = false;
        int32_t BitsPerSample = 16;
        size_t Offset = 12;

        while (Offset + 8 <= Data.size())
        {
            const char ChunkId[5] = { (char)D[Offset], (char)D[Offset + 1], (char)D[Offset + 2], (char)D[Offset + 3], 0 };
            const uint32_t ChunkSize = ReadU32(D + Offset + 4);
            const size_t ChunkDataOffset = Offset + 8;

            if (std::strcmp(ChunkId, "fmt ") == 0 && ChunkDataOffset + 16 <= Data.size())
            {
                Out.NumChannels = ReadU16(D + ChunkDataOffset + 2);
                Out.SampleRate = (int32_t)ReadU32(D + ChunkDataOffset + 4);
                BitsPerSample = ReadU16(D + ChunkDataOffset + 14);
                bFoundFmt = true;
            }
            else if (std::strcmp(ChunkId, "data") == 0)
            {
                const size_t SafeSize = (std::min)((size_t)ChunkSize, Data.size() - ChunkDataOffset);
                if (SafeSize > 0 && BitsPerSample == 16)
                {
                    const size_t NumSamples = SafeSize / 2;
                    Out.Samples.resize(NumSamples);
                    std::memcpy(Out.Samples.data(), D + ChunkDataOffset, NumSamples * 2);
                    bFoundData = true;
                }
            }

            Offset = ChunkDataOffset + ChunkSize + (ChunkSize % 2);
        }

        Out.bValid = bFoundFmt && bFoundData && Out.NumChannels > 0 && Out.SampleRate > 0;
        return Out;
    }

    // 스테레오 이상이면 모노로 다운믹스(첫 채널만 사용 - Unreal 쪽 인코딩과 동일하게).
    std::vector<int16_t> ToMono(const WavPCM& Wav)
    {
        if (Wav.NumChannels <= 1) return Wav.Samples;

        std::vector<int16_t> Mono;
        Mono.reserve(Wav.Samples.size() / Wav.NumChannels);
        for (size_t i = 0; i + Wav.NumChannels <= Wav.Samples.size(); i += Wav.NumChannels)
        {
            Mono.push_back(Wav.Samples[i]);
        }
        return Mono;
    }

    // Whisper 계열 모델은 16kHz 입력을 기대한다 - 선형보간으로 리샘플링한다(간단하지만 음성인식엔 충분).
    std::vector<int16_t> ResampleTo16k(const std::vector<int16_t>& In, int32_t InRate)
    {
        constexpr int32_t OutRate = 16000;
        if (InRate == OutRate || In.empty()) return In;

        const double Ratio = (double)InRate / (double)OutRate;
        const size_t OutCount = (size_t)((double)In.size() / Ratio);

        std::vector<int16_t> Out(OutCount);
        for (size_t i = 0; i < OutCount; ++i)
        {
            const double SrcPos = (double)i * Ratio;
            const size_t Idx0 = (size_t)SrcPos;
            const size_t Idx1 = (std::min)(Idx0 + 1, In.size() - 1);
            const double Frac = SrcPos - (double)Idx0;
            Out[i] = (int16_t)((double)In[Idx0] * (1.0 - Frac) + (double)In[Idx1] * Frac);
        }
        return Out;
    }

    std::string JsonEscape(const std::string& In)
    {
        std::string Out;
        Out.reserve(In.size() + 8);
        for (char c : In)
        {
            switch (c)
            {
            case '"': Out += "\\\""; break;
            case '\\': Out += "\\\\"; break;
            case '\n': Out += "\\n"; break;
            case '\r': Out += "\\r"; break;
            case '\t': Out += "\\t"; break;
            default:
                if ((unsigned char)c < 0x20) { char buf[8]; std::snprintf(buf, sizeof(buf), "\\u%04x", c); Out += buf; }
                else Out += c;
            }
        }
        return Out;
    }

    // ---------------------------------------------------------------------
    // 아주 작은 블로킹 HTTP/1.1 서버 (POST /stt, GET /health만 처리)
    // ---------------------------------------------------------------------
    bool RecvAll(SOCKET S, char* Buf, int Len)
    {
        int Received = 0;
        while (Received < Len)
        {
            const int N = recv(S, Buf + Received, Len - Received, 0);
            if (N <= 0) return false;
            Received += N;
        }
        return true;
    }

    void SendAll(SOCKET S, const std::string& Data)
    {
        int Sent = 0;
        while (Sent < (int)Data.size())
        {
            const int N = send(S, Data.data() + Sent, (int)Data.size() - Sent, 0);
            if (N <= 0) return;
            Sent += N;
        }
    }

    void SendJson(SOCKET S, int StatusCode, const char* StatusText, const std::string& JsonBody)
    {
        std::string Response =
            "HTTP/1.1 " + std::to_string(StatusCode) + " " + StatusText + "\r\n"
            "Content-Type: application/json; charset=utf-8\r\n"
            "Content-Length: " + std::to_string(JsonBody.size()) + "\r\n"
            "Connection: close\r\n\r\n" + JsonBody;
        SendAll(S, Response);
    }

    // 헤더까지 다 받아서 (요청 라인, 헤더 맵, 헤더 뒤에 이미 도착해 있던 바디 조각)을 돌려준다.
    struct HttpRequestHead
    {
        std::string Method;
        std::string Path;
        long ContentLength = 0;
        std::vector<uint8_t> BodyStart; // 헤더 파싱 중 같이 수신된 바디 앞부분
        bool bOk = false;
    };

    HttpRequestHead ReadRequestHead(SOCKET S)
    {
        HttpRequestHead Head;
        std::string Buffer;
        char Chunk[4096];

        size_t HeaderEnd = std::string::npos;
        while (HeaderEnd == std::string::npos)
        {
            const int N = recv(S, Chunk, sizeof(Chunk), 0);
            if (N <= 0) return Head;
            Buffer.append(Chunk, N);
            if (Buffer.size() > 1 << 20) return Head; // 헤더가 이 정도로 클 리 없음 - 방어적 종료
            HeaderEnd = Buffer.find("\r\n\r\n");
        }

        const std::string HeaderPart = Buffer.substr(0, HeaderEnd);
        const size_t BodyStartOffset = HeaderEnd + 4;
        if (BodyStartOffset < Buffer.size())
        {
            Head.BodyStart.assign(Buffer.begin() + BodyStartOffset, Buffer.end());
        }

        // 요청 라인
        const size_t FirstLineEnd = HeaderPart.find("\r\n");
        const std::string RequestLine = HeaderPart.substr(0, FirstLineEnd);
        {
            const size_t Sp1 = RequestLine.find(' ');
            const size_t Sp2 = RequestLine.find(' ', Sp1 == std::string::npos ? std::string::npos : Sp1 + 1);
            if (Sp1 == std::string::npos || Sp2 == std::string::npos) return Head;
            Head.Method = RequestLine.substr(0, Sp1);
            Head.Path = RequestLine.substr(Sp1 + 1, Sp2 - Sp1 - 1);
        }

        // Content-Length 헤더
        size_t LineStart = FirstLineEnd == std::string::npos ? HeaderPart.size() : FirstLineEnd + 2;
        while (LineStart < HeaderPart.size())
        {
            size_t LineEnd = HeaderPart.find("\r\n", LineStart);
            if (LineEnd == std::string::npos) LineEnd = HeaderPart.size();
            const std::string Line = HeaderPart.substr(LineStart, LineEnd - LineStart);

            const size_t Colon = Line.find(':');
            if (Colon != std::string::npos)
            {
                std::string Key = Line.substr(0, Colon);
                for (char& c : Key) c = (char)tolower((unsigned char)c);
                if (Key == "content-length")
                {
                    Head.ContentLength = std::strtol(Line.c_str() + Colon + 1, nullptr, 10);
                }
            }
            LineStart = LineEnd + 2;
        }

        Head.bOk = true;
        return Head;
    }

    std::vector<uint8_t> ReadBody(SOCKET S, const HttpRequestHead& Head)
    {
        std::vector<uint8_t> Body = Head.BodyStart;
        if ((long)Body.size() >= Head.ContentLength)
        {
            Body.resize(Head.ContentLength);
            return Body;
        }

        const size_t Remaining = (size_t)Head.ContentLength - Body.size();
        const size_t OldSize = Body.size();
        Body.resize(Head.ContentLength);
        if (!RecvAll(S, (char*)Body.data() + OldSize, (int)Remaining))
        {
            Body.clear();
        }
        return Body;
    }
}

int main(int argc, char** argv)
{
    // exe 자신의 폴더를 SDK/모델 경로로 사용한다. NVIDIA 샘플 core.hpp의 기본 경로 자동 감지는
    // 임시 std::string.substr()을 string_view에 대입하는 댕글링 버그가 있어서 직접 계산해 넘긴다.
    char ModulePathBuf[MAX_PATH];
    GetModuleFileNameA(NULL, ModulePathBuf, MAX_PATH);
    std::string ExeDir(ModulePathBuf);
    ExeDir = ExeDir.substr(0, ExeDir.find_last_of("\\/"));
    const std::string ModelsDir = ExeDir + "\\data\\nvigi.models";

    std::string Backend = "cuda";
    std::string Language = "ko";
    int Port = 8090;
    for (int i = 1; i < argc; ++i)
    {
        std::string Arg = argv[i];
        if (Arg == "--backend" && i + 1 < argc) Backend = argv[++i];
        else if (Arg == "--language" && i + 1 < argc) Language = argv[++i];
        else if (Arg == "--port" && i + 1 < argc) Port = std::atoi(argv[++i]);
    }

    std::cout << "=== WITH:OUT STT Bridge (NVIDIA NVIGI Whisper, rebuilt) ===\n";
    std::cout << "SDK/Exe dir: " << ExeDir << "\n";
    std::cout << "Models dir : " << ModelsDir << "\n";
    std::cout << "Backend    : " << Backend << "\n";
    std::cout << "Language   : " << Language << "\n";

    std::unique_ptr<nvigi::Core> Core;
    std::unique_ptr<nvigi::asr::Instance> Asr;

    try
    {
        Core = std::make_unique<nvigi::Core>(nvigi::Core::Config{
            .sdkPath = ExeDir,
            .logLevel = nvigi::LogLevel::eOff,
            .showConsole = true,
        });

        nvigi::d3d12::D3D12Config D3D12Cfg{};
        nvigi::vulkan::VulkanConfig VkCfg{};
        nvigi::d3d12::DeviceAndQueue DeviceAndQueue{};

        if (Backend != "vulkan")
        {
            DeviceAndQueue = nvigi::d3d12::D3D12Helper::create_best_compute_device();
            D3D12Cfg = {
                .device = DeviceAndQueue.device.Get(),
                .compute_queue = DeviceAndQueue.compute_queue.Get(),
                .create_committed_resource_callback = nvigi::d3d12::default_create_committed_resource,
                .destroy_resource_callback = nvigi::d3d12::default_destroy_resource,
                .create_resource_user_context = nullptr,
                .destroy_resource_user_context = nullptr
            };
        }

        auto InstanceResult = nvigi::asr::Instance::create(
            nvigi::asr::ModelConfig{
                .backend = Backend,
                .guid = "{5CAD3A03-1272-4D43-9F3D-655417526170}",
                .model_path = ModelsDir,
                .model_card_json = {},
                .num_threads = 8,
                .vram_budget_mb = 2048,
                .flash_attention = true,
                .language = Language,
                .translate = false,
                .detect_language = false,
            },
            D3D12Cfg, VkCfg, {},
            Core->loadInterface(), Core->unloadInterface(), ExeDir
        );

        if (!InstanceResult)
        {
            std::cerr << "오류: ASR 인스턴스 생성 실패 - " << InstanceResult.error().what() << "\n";
            return 1;
        }
        Asr = std::move(InstanceResult.value());
    }
    catch (const std::exception& e)
    {
        std::cerr << "오류: 초기화 실패 - " << e.what() << "\n";
        return 1;
    }

    std::cout << "ASR 준비 완료. 서버 시작 중...\n";

    WSADATA Wsa;
    if (WSAStartup(MAKEWORD(2, 2), &Wsa) != 0)
    {
        std::cerr << "오류: WSAStartup 실패\n";
        return 1;
    }

    SOCKET ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ListenSocket == INVALID_SOCKET)
    {
        std::cerr << "오류: 소켓 생성 실패\n";
        return 1;
    }

    sockaddr_in Addr{};
    Addr.sin_family = AF_INET;
    Addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    Addr.sin_port = htons((u_short)Port);

    if (bind(ListenSocket, (sockaddr*)&Addr, sizeof(Addr)) == SOCKET_ERROR)
    {
        std::cerr << "오류: bind 실패 (포트 " << Port << " 이미 사용 중일 수 있음)\n";
        return 1;
    }

    if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        std::cerr << "오류: listen 실패\n";
        return 1;
    }

    std::cout << "STT Bridge listening on http://127.0.0.1:" << Port << "\n";

    while (true)
    {
        SOCKET Client = accept(ListenSocket, nullptr, nullptr);
        if (Client == INVALID_SOCKET) continue;

        std::thread([Client, &Asr]()
        {
            HttpRequestHead Head = ReadRequestHead(Client);
            if (!Head.bOk)
            {
                closesocket(Client);
                return;
            }

            if (Head.Method == "GET" && Head.Path == "/health")
            {
                SendJson(Client, 200, "OK", "{\"status\":\"ok\",\"engine\":\"nvigi-whisper\"}");
            }
            else if (Head.Method == "POST" && Head.Path == "/stt")
            {
                std::vector<uint8_t> WavBytes = ReadBody(Client, Head);
                WavPCM Wav = ParseWav(WavBytes);

                if (!Wav.bValid)
                {
                    SendJson(Client, 400, "Bad Request", "{\"error\":\"invalid wav\"}");
                    closesocket(Client);
                    return;
                }

                std::vector<int16_t> Mono = ToMono(Wav);
                std::vector<int16_t> Pcm16k = ResampleTo16k(Mono, Wav.SampleRate);

                std::string Transcript;
                auto Result = Asr->transcribe(
                    Pcm16k.data(), Pcm16k.size() * sizeof(int16_t),
                    nvigi::asr::RuntimeConfig{}
                        .set_sampling(nvigi::asr::SamplingStrategy::Greedy)
                        .set_temperature(0.0f),
                    [&Transcript](std::string_view Text, nvigi::asr::ExecutionState State) -> nvigi::asr::ExecutionState
                    {
                        if (State == nvigi::asr::ExecutionState::DataPending || State == nvigi::asr::ExecutionState::Done)
                        {
                            Transcript += Text;
                        }
                        return State;
                    }
                );

                if (!Result)
                {
                    std::cerr << "[STT] 인식 실패: " << Result.error().what() << "\n";
                    SendJson(Client, 500, "Internal Server Error", "{\"error\":\"transcription failed\"}");
                }
                else
                {
                    std::cout << "[STT] 인식 결과: " << Transcript << "\n";
                    SendJson(Client, 200, "OK", "{\"text\":\"" + JsonEscape(Transcript) + "\"}");
                }
            }
            else
            {
                SendJson(Client, 404, "Not Found", "{\"error\":\"not found\"}");
            }

            closesocket(Client);
        }).detach();
    }

    return 0;
}
