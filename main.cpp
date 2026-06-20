// =============================================================================
// PINKMOON LAUNCHER - ULTIMATE UI REDESIGN v2
// Premium Glass + Segoe UI + Full Settings
// =============================================================================

#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wininet.h>
#include <shlobj.h>
#include <shellapi.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <unordered_set>
#include <unordered_map>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <iphlpapi.h>
#include <iomanip>
#include <psapi.h>
#include <versionhelpers.h>
#include <cctype>
#include <wincodec.h>
#include <comdef.h>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "windowscodecs.lib")

// =============================================================================
// CONFIGURATION
// =============================================================================
namespace Config {
    constexpr const char* SERVER_HOST = "26.157.252.111";
    constexpr const char* SERVER_PORT = "7777";
    constexpr const char* MANIFEST_URL = "https://verte225066.github.io/pinkmoon-patch/update.txt";
    constexpr const char* FILE_BASE_URL = "https://verte225066.github.io/pinkmoon-patch/";
    constexpr const char* DEFAULT_PATH = "C:\\Program Files (x86)\\Pinkmoon Town\\gamefile";
    constexpr const char* GAMEFILE_DIR = "gamefile";
    constexpr int         MAX_VERIFY_LOOPS = 3;
    constexpr const char* DISCORD_WEBHOOK_URL = "https://discord.com/api/webhooks/1516798972323430410/nOAq8cQffenYENfN9zHzmhPB5T50tbjsl63xJhKJYFUTaCnv2XjqGvknkoOMGvmbQnQM";
    constexpr bool        DISCORD_ALERTS_ENABLED = true;
    constexpr const char* REINSTALL_URL = "https://example.com/download/Pinkmoon_Setup.exe";

    static const std::vector<std::string> IGNORE_LIST = {
        "desktop.ini", "thumbs.db",
        "7z.exe", "7zr.exe",
        "Pinkmoon_Setup.exe",
        "pinkmoon_debug.txt", "launcher_debug.txt",
        "filelist.csv",
        "modloader.log", "cleo.log", "modloader.ini",
    };
}

// =============================================================================
// DATA STRUCTURES
// =============================================================================
struct ManifestEntry {
    std::string fileName;
    std::string subPath;
    std::string fullPath;
    size_t      expectedSize = 0;
    std::string requiredMod;
    std::string url;
    bool        isCore = true;
};

// =============================================================================
// SHARED STATE
// =============================================================================
static std::atomic<bool>   g_isProcessing{ false };
static std::atomic<bool>   g_isGameRunning{ false };
static std::atomic<bool>   g_isMonitorRunning{ false };
static std::atomic<float>  g_progress{ 0.0f };
static std::atomic<size_t> g_downloadedBytes{ 0 };
static std::atomic<size_t> g_totalBytes{ 0 };
static std::atomic<float>  g_downloadSpeed{ 0.0f };
static std::mutex          g_logMutex;
static std::mutex          g_statusMutex;
static std::string         g_statusText = "Ready";
static std::vector<std::string> g_logs;

static std::string g_gamePath = Config::DEFAULT_PATH;
static char        g_playerName[64] = "Pinkmoon_Player";
static bool        g_modMic = false;
static bool        g_modENB = false;
static std::string g_publicIP = "";
static std::string g_hardwareID = "";

static std::vector<ManifestEntry> g_manifest;
static std::mutex  g_manifestMutex;
static std::thread g_workerThread;
static std::thread g_monitorThread;
static HWND        g_hwnd = nullptr;
static HANDLE      g_gameProcess = nullptr;
static HANDLE      g_hJob = nullptr;

static float g_pulseTime = 0.0f;
static float g_mouseX = 0.0f, g_mouseY = 0.0f;

// Splash state
static std::atomic<bool> g_manifestReady{ false };
static std::atomic<bool> g_manifestFetchSuccess{ false };
static std::mutex        g_statusMsgMutex;
static std::string       g_manifestStatusMsg = "Initializing...";
static std::atomic<float> g_manifestProgress{ 0.0f };
static bool              g_manifestError = false;

// Server Status
static std::atomic<bool> g_serverQueryRunning{ false };
static std::mutex        g_serverStatusMutex;
static std::string       g_serverHostname = "Querying...";
static int               g_serverPlayers = 0;
static int               g_serverMaxPlayers = 0;
static std::string       g_serverGamemode = "";
static std::string       g_serverMapname = "";

// =============================================================================
// DX11 GLOBALS
// =============================================================================
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

// =============================================================================
// UI GLOBALS
// =============================================================================
static ID3D11ShaderResourceView* g_wallpaperTexture = nullptr;
static int g_wallpaperWidth = 0, g_wallpaperHeight = 0;

// Page management
enum Page { PAGE_HOME, PAGE_SETTINGS };
static Page g_currentPage = PAGE_HOME;

// Fonts
static ImFont* g_fontRegular = nullptr;
static ImFont* g_fontBold = nullptr;
static ImFont* g_fontTitle = nullptr;
static ImFont* g_fontSmall = nullptr;

// =============================================================================
// COLOR SYSTEM
// =============================================================================
namespace Color {
    static const ImU32 Pink = IM_COL32(255, 79, 216, 255);
    static const ImU32 PinkLight = IM_COL32(255, 150, 230, 255);
    static const ImU32 PinkDark = IM_COL32(200, 50, 170, 255);
    static const ImU32 Purple = IM_COL32(168, 85, 247, 255);
    static const ImU32 PurpleGlow = IM_COL32(130, 60, 220, 200);
    static const ImU32 Accent = IM_COL32(255, 100, 220, 255);

    static const ImU32 White = IM_COL32(255, 255, 255, 255);
    static const ImU32 WhiteDim = IM_COL32(255, 255, 255, 200);
    static const ImU32 Text = IM_COL32(240, 240, 245, 255);
    static const ImU32 TextDim = IM_COL32(180, 180, 195, 255);
    static const ImU32 TextSoft = IM_COL32(140, 140, 160, 255);

    static const ImU32 Glass = IM_COL32(255, 255, 255, 20);
    static const ImU32 GlassBorder = IM_COL32(255, 255, 255, 35);
    static const ImU32 GlassHover = IM_COL32(255, 255, 255, 45);

    static const ImU32 ShadowLight = IM_COL32(0, 0, 0, 80);
    static const ImU32 ShadowMedium = IM_COL32(0, 0, 0, 150);
    static const ImU32 ShadowHeavy = IM_COL32(0, 0, 0, 220);

    static const ImU32 BgDark = IM_COL32(10, 6, 14, 255);
    static const ImU32 BgCard = IM_COL32(20, 16, 28, 220);
    static const ImU32 BgInput = IM_COL32(30, 26, 40, 200);

    static const ImU32 Success = IM_COL32(34, 197, 94, 255);
    static const ImU32 Warning = IM_COL32(245, 158, 11, 255);
    static const ImU32 Danger = IM_COL32(239, 68, 68, 255);
    static const ImU32 Info = IM_COL32(56, 189, 248, 255);
}

// =============================================================================
// UTILITY
// =============================================================================
static std::string Trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

static std::string SanitizeNick(const std::string& input) {
    std::string trimmed = Trim(input);
    if (trimmed.empty()) trimmed = "Pinkmoon_Player";
    std::replace(trimmed.begin(), trimmed.end(), ' ', '_');
    trimmed.erase(std::remove_if(trimmed.begin(), trimmed.end(),
        [](char c) { return !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')); }), trimmed.end());
    if (trimmed.length() < 3) trimmed = "Pinkmoon_Player";
    if (trimmed.length() > 20) trimmed = trimmed.substr(0, 20);
    return trimmed;
}

static ImU32 ColorLerp(ImU32 c1, ImU32 c2, float t) {
    int r1 = (c1 >> 0) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = (c1 >> 16) & 0xFF, a1 = (c1 >> 24) & 0xFF;
    int r2 = (c2 >> 0) & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = (c2 >> 16) & 0xFF, a2 = (c2 >> 24) & 0xFF;
    return IM_COL32((int)(r1 + (r2 - r1) * t), (int)(g1 + (g2 - g1) * t),
        (int)(b1 + (b2 - b1) * t), (int)(a1 + (a2 - a1) * t));
}

static ImVec2 GetCenter(const ImVec2& size) {
    ImVec2 display = ImGui::GetIO().DisplaySize;
    return ImVec2((display.x - size.x) * 0.5f, (display.y - size.y) * 0.5f);
}

static float EaseOutExpo(float t) {
    return (t == 1.0f) ? 1.0f : 1.0f - powf(2.0f, -10.0f * t);
}

// =============================================================================
// CONFIG I/O
// =============================================================================
static std::string GetConfigFilePath() {
    char self[MAX_PATH]; GetModuleFileNameA(nullptr, self, MAX_PATH);
    std::string exePath(self);
    size_t pos = exePath.find_last_of("\\/");
    if (pos != std::string::npos) exePath = exePath.substr(0, pos + 1);
    return exePath + "launcher.ini";
}

static void LoadConfig() {
    std::string path = GetConfigFilePath();
    std::ifstream ifs(path);
    if (!ifs) return;
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        if (key == "PlayerName") { val = Trim(val); strncpy_s(g_playerName, val.c_str(), sizeof(g_playerName) - 1); }
        else if (key == "GamePath") { g_gamePath = Trim(val); }
        else if (key == "ModMic") { g_modMic = (Trim(val) == "1"); }
        else if (key == "ModENB") { g_modENB = (Trim(val) == "1"); }
    }
}

static void SaveConfig() {
    std::string path = GetConfigFilePath();
    std::ofstream ofs(path, std::ios::trunc);
    if (!ofs) return;
    ofs << "# Pinkmoon Launcher Config\n";
    ofs << "PlayerName=" << g_playerName << "\n";
    ofs << "GamePath=" << g_gamePath << "\n";
    ofs << "ModMic=" << (g_modMic ? "1" : "0") << "\n";
    ofs << "ModENB=" << (g_modENB ? "1" : "0") << "\n";
}

// =============================================================================
// HELPERS
// =============================================================================
static void SetStatus(const std::string& s) { std::lock_guard<std::mutex> lk(g_statusMutex); g_statusText = s; }
static void AddLog(const std::string& s) {
    std::lock_guard<std::mutex> lk(g_logMutex);
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    char buf[20]; struct tm tm; localtime_s(&tm, &t);
    strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
    g_logs.push_back(std::string("[") + buf + "] " + s);
    if (g_logs.size() > 512) g_logs.erase(g_logs.begin());
}

static size_t GetFileSizeEx(const std::string& path) {
    WIN32_FILE_ATTRIBUTE_DATA fad = {};
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fad)) return (size_t)-1;
    return (size_t)(((uint64_t)fad.nFileSizeHigh << 32) | fad.nFileSizeLow);
}

static bool FileExistsCheck(const std::string& path) {
    return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

static void CreateDirectoryIfNotExists(const std::string& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        std::filesystem::create_directories(path, ec);
        if (!ec) AddLog("[INFO] Created folder: " + path);
    }
}

static std::string FormatSize(size_t bytes) {
    if (bytes == (size_t)-1) return "N/A";
    const char* u[] = { "B","KB","MB","GB" };
    int i = 0; double s = (double)bytes;
    while (s >= 1024 && i < 3) { s /= 1024; i++; }
    char buf[32];
    snprintf(buf, sizeof(buf), i == 0 ? "%.0f %s" : "%.2f %s", s, u[i]);
    return buf;
}

static std::string FormatSpeed(float bps) {
    if (bps < 1024) return std::to_string((int)bps) + " B/s";
    if (bps < 1024 * 1024) return std::to_string((int)(bps / 1024)) + " KB/s";
    return std::to_string((int)(bps / 1048576)) + " MB/s";
}

static std::string GetTimestampISO8601() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    struct tm tm; localtime_s(&tm, &t);
    char buf[30]; strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

// =============================================================================
// TEXTURE LOADING (WIC)
// =============================================================================
static bool LoadTextureFromFile(const char* filename, ID3D11ShaderResourceView** out_srv, int* out_width = nullptr, int* out_height = nullptr) {
    if (!out_srv) return false;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool coinit = SUCCEEDED(hr);

    IWICImagingFactory* pFactory = nullptr;
    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFactory));
    if (FAILED(hr)) { if (coinit) CoUninitialize(); return false; }

    int wlen = MultiByteToWideChar(CP_UTF8, 0, filename, -1, nullptr, 0);
    std::vector<wchar_t> wfilename(wlen);
    MultiByteToWideChar(CP_UTF8, 0, filename, -1, wfilename.data(), wlen);

    IWICBitmapDecoder* pDecoder = nullptr;
    hr = pFactory->CreateDecoderFromFilename(wfilename.data(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder);
    if (FAILED(hr)) { pFactory->Release(); if (coinit) CoUninitialize(); return false; }

    IWICBitmapFrameDecode* pFrame = nullptr;
    hr = pDecoder->GetFrame(0, &pFrame);
    if (FAILED(hr)) { pDecoder->Release(); pFactory->Release(); if (coinit) CoUninitialize(); return false; }

    UINT width, height; pFrame->GetSize(&width, &height);
    if (out_width) *out_width = (int)width; if (out_height) *out_height = (int)height;

    IWICFormatConverter* pConverter = nullptr;
    hr = pFactory->CreateFormatConverter(&pConverter);
    if (FAILED(hr)) { pFrame->Release(); pDecoder->Release(); pFactory->Release(); if (coinit) CoUninitialize(); return false; }

    hr = pConverter->Initialize(pFrame, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) { pConverter->Release(); pFrame->Release(); pDecoder->Release(); pFactory->Release(); if (coinit) CoUninitialize(); return false; }

    UINT cbStride = width * 4, cbSize = cbStride * height;
    std::vector<BYTE> buffer(cbSize);
    hr = pConverter->CopyPixels(nullptr, cbStride, cbSize, buffer.data());
    if (FAILED(hr)) { pConverter->Release(); pFrame->Release(); pDecoder->Release(); pFactory->Release(); if (coinit) CoUninitialize(); return false; }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width; desc.Height = height; desc.MipLevels = 1; desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = buffer.data(); initData.SysMemPitch = cbStride;

    ID3D11Texture2D* pTexture = nullptr;
    hr = g_pd3dDevice->CreateTexture2D(&desc, &initData, &pTexture);
    if (FAILED(hr)) { pConverter->Release(); pFrame->Release(); pDecoder->Release(); pFactory->Release(); if (coinit) CoUninitialize(); return false; }

    hr = g_pd3dDevice->CreateShaderResourceView(pTexture, nullptr, out_srv);
    pTexture->Release();

    pConverter->Release(); pFrame->Release(); pDecoder->Release(); pFactory->Release();
    if (coinit) CoUninitialize();
    return SUCCEEDED(hr);
}

static void LoadTextures() {
    std::string wallpaperPath = "wallpaper.png";
    if (!std::filesystem::exists(wallpaperPath)) wallpaperPath = "wallpaper.jpg";
    if (std::filesystem::exists(wallpaperPath)) {
        LoadTextureFromFile(wallpaperPath.c_str(), &g_wallpaperTexture, &g_wallpaperWidth, &g_wallpaperHeight);
        AddLog("[TEXTURE] Wallpaper loaded: " + wallpaperPath);
    }
    else {
        AddLog("[TEXTURE] No wallpaper found.");
    }
}

// =============================================================================
// HARDWARE ID & PUBLIC IP
// =============================================================================
static std::string GetHardwareID() {
    DWORD serial = 0;
    if (GetVolumeInformationA("C:\\", nullptr, 0, &serial, nullptr, nullptr, nullptr, 0)) {
        char buf[20]; snprintf(buf, sizeof(buf), "%08X", serial); return buf;
    }
    char cn[MAX_COMPUTERNAME_LENGTH + 1]; DWORD sz = sizeof(cn);
    if (GetComputerNameA(cn, &sz)) return cn;
    return "UNKNOWN";
}

static std::string GetPublicIP() {
    HINTERNET hI = InternetOpenA("PinkmoonLauncher/ULTIMATE", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hI) return "0.0.0.0";
    HINTERNET hU = InternetOpenUrlA(hI, "http://api.ipify.org", nullptr, 0, INTERNET_FLAG_RELOAD, 0);
    if (!hU) { InternetCloseHandle(hI); return "0.0.0.0"; }
    char buf[64]; DWORD br = 0; std::string ip;
    while (InternetReadFile(hU, buf, sizeof(buf) - 1, &br) && br > 0) { buf[br] = '\0'; ip += buf; }
    InternetCloseHandle(hU); InternetCloseHandle(hI);
    return ip.empty() ? "0.0.0.0" : ip;
}

// =============================================================================
// BROWSE FOLDER
// =============================================================================
static void BrowseFolder() {
    BROWSEINFOA bi = {};
    bi.lpszTitle = "Select Pinkmoon Town installation folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl) {
        char path[MAX_PATH];
        if (SHGetPathFromIDListA(pidl, path)) {
            std::string selected = std::string(path) + "\\" + Config::GAMEFILE_DIR;
            g_gamePath = selected;
            CreateDirectoryIfNotExists(g_gamePath);
            AddLog("[INFO] Game path set: " + g_gamePath);
            SaveConfig();
        }
        CoTaskMemFree(pidl);
    }
}

// =============================================================================
// DISCORD WEBHOOK
// =============================================================================
static void SendDiscordWebhookEmbed(const std::string& title, const std::string& description,
    int color, const std::vector<std::pair<std::string, std::string>>& fields,
    const std::string& filePath = "") {
    if (strlen(Config::DISCORD_WEBHOOK_URL) < 10) return;
    std::string playerName = g_playerName;
    if (playerName.empty()) playerName = "Unknown";
    std::string webhookUsername = playerName + " (" + g_publicIP + ")";

    std::string json = "{\"username\":\"" + webhookUsername + "\", \"embeds\":[{\"title\":\"" + title + "\",\"description\":\"" + description + "\",\"color\":" + std::to_string(color) + ",\"timestamp\":\"" + GetTimestampISO8601() + "\",\"fields\":[";
    json += "{\"name\":\"Player\",\"value\":\"" + playerName + "\",\"inline\":true},";
    json += "{\"name\":\"IP\",\"value\":\"" + g_publicIP + "\",\"inline\":true},";
    json += "{\"name\":\"HWID\",\"value\":\"" + g_hardwareID + "\",\"inline\":true},";
    if (!filePath.empty()) json += "{\"name\":\"File\",\"value\":\"" + filePath + "\",\"inline\":false},";
    for (const auto& f : fields) {
        std::string val = f.second;
        size_t pos = 0;
        while ((pos = val.find('"', pos)) != std::string::npos) { val.replace(pos, 1, "\\\""); pos += 2; }
        pos = 0;
        while ((pos = val.find('\n', pos)) != std::string::npos) { val.replace(pos, 1, "\\n"); pos += 2; }
        json += "{\"name\":\"" + f.first + "\",\"value\":\"" + val + "\",\"inline\":false},";
    }
    if (json.back() == ',') json.pop_back();
    json += "]}]}";

    HINTERNET hI = InternetOpenA("PinkmoonLauncher/ULTIMATE", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hI) return;
    std::string url = Config::DISCORD_WEBHOOK_URL, host, path;
    size_t s = url.find("://");
    if (s != std::string::npos) {
        s += 3; size_t e = url.find("/", s);
        if (e != std::string::npos) { host = url.substr(s, e - s); path = url.substr(e); }
    }
    if (host.empty() || path.empty()) { InternetCloseHandle(hI); return; }
    HINTERNET hC = InternetConnectA(hI, host.c_str(), 443, nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hC) { InternetCloseHandle(hI); return; }
    HINTERNET hR = HttpOpenRequestA(hC, "POST", path.c_str(), nullptr, nullptr, nullptr, INTERNET_FLAG_SECURE, 0);
    if (!hR) { InternetCloseHandle(hC); InternetCloseHandle(hI); return; }
    const char* hdr = "Content-Type: application/json\r\n";
    HttpSendRequestA(hR, hdr, (DWORD)strlen(hdr), (LPVOID)json.c_str(), (DWORD)json.size());
    InternetCloseHandle(hR); InternetCloseHandle(hC); InternetCloseHandle(hI);
}

// =============================================================================
// SERVER QUERY (UDP)
// =============================================================================
static bool ResolveServerHost(const char* host, sockaddr_in& addr) {
    if (inet_pton(AF_INET, host, &addr.sin_addr) == 1) return true;
    addrinfo hints = {}; hints.ai_family = AF_INET; hints.ai_socktype = SOCK_DGRAM;
    addrinfo* result = nullptr;
    if (getaddrinfo(host, nullptr, &hints, &result) != 0 || result == nullptr) return false;
    sockaddr_in* resolved = reinterpret_cast<sockaddr_in*>(result->ai_addr);
    addr.sin_addr = resolved->sin_addr;
    freeaddrinfo(result);
    return true;
}

static bool ResolveServerIp(const char* host, std::string& outIp) {
    sockaddr_in addr = {}; addr.sin_family = AF_INET;
    if (!ResolveServerHost(host, addr)) return false;
    char ipBuffer[INET_ADDRSTRLEN] = {};
    if (inet_ntop(AF_INET, &addr.sin_addr, ipBuffer, sizeof(ipBuffer)) == nullptr) return false;
    outIp = ipBuffer;
    return true;
}

static bool QueryServerInfo(const char* host, const char* port,
    std::string& hostname, int& players, int& maxplayers,
    std::string& gamemode, std::string& mapname) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) { WSACleanup(); return false; }
    int timeout = 3000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<u_short>(std::stoi(port)));
    if (!ResolveServerHost(host, addr)) { closesocket(sock); WSACleanup(); return false; }
    unsigned char packet[] = { 0x53, 0x41, 0x4D, 0x50, 0x69 };
    if (sendto(sock, (char*)packet, sizeof(packet), 0, (sockaddr*)&addr, sizeof(addr)) <= 0) {
        closesocket(sock); WSACleanup(); return false;
    }
    char buffer[2048];
    sockaddr_in from; int fromLen = sizeof(from);
    int len = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, (sockaddr*)&from, &fromLen);
    closesocket(sock); WSACleanup();
    if (len < 11) return false;
    unsigned char* p = (unsigned char*)buffer;
    if (memcmp(p, "SAMP", 4) != 0 || p[4] != 0x69) return false;
    p += 5;
    players = p[0] | (p[1] << 8); p += 2;
    maxplayers = p[0] | (p[1] << 8); p += 2;
    if (p >= (unsigned char*)buffer + len) return false;
    int hLen = p[0]; p++;
    if (p + hLen >= (unsigned char*)buffer + len) return false;
    hostname.assign((char*)p, hLen); p += hLen;
    if (p >= (unsigned char*)buffer + len) return false;
    int gLen = p[0]; p++;
    if (p + gLen >= (unsigned char*)buffer + len) return false;
    gamemode.assign((char*)p, gLen); p += gLen;
    if (p >= (unsigned char*)buffer + len) return false;
    int mLen = p[0]; p++;
    if (p + mLen >= (unsigned char*)buffer + len) return false;
    mapname.assign((char*)p, mLen);
    return true;
}

static void ServerQueryThread() {
    g_serverQueryRunning.store(true);
    while (g_serverQueryRunning.load()) {
        std::string hostname, gamemode, mapname;
        int players = 0, maxplayers = 0;
        bool ok = QueryServerInfo(Config::SERVER_HOST, Config::SERVER_PORT,
            hostname, players, maxplayers, gamemode, mapname);
        {
            std::lock_guard<std::mutex> lock(g_serverStatusMutex);
            if (ok) {
                g_serverHostname = hostname;
                g_serverPlayers = players;
                g_serverMaxPlayers = maxplayers;
                g_serverGamemode = gamemode;
                g_serverMapname = mapname;
            }
            else {
                g_serverHostname = "Offline";
                g_serverPlayers = 0;
                g_serverMaxPlayers = 0;
                g_serverGamemode = "-";
                g_serverMapname = "-";
            }
        }
        for (int i = 0; i < 30 && g_serverQueryRunning.load(); i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

// =============================================================================
// MANIFEST CACHE & FETCH
// =============================================================================
static std::string GetCacheFilePath() {
    char self[MAX_PATH]; GetModuleFileNameA(nullptr, self, MAX_PATH);
    std::string exePath(self);
    size_t pos = exePath.find_last_of("\\/");
    if (pos != std::string::npos) exePath = exePath.substr(0, pos + 1);
    return exePath + "manifest.cache";
}

static bool SaveManifestToCache(const std::vector<ManifestEntry>& entries) {
    std::string path = GetCacheFilePath();
    std::ofstream ofs(path, std::ios::trunc);
    if (!ofs) return false;
    for (const auto& e : entries) {
        ofs << e.fileName << "," << e.expectedSize << "," << e.requiredMod << ","
            << e.subPath << "," << e.url << "," << (e.isCore ? "1" : "0") << "\n";
    }
    return true;
}

static bool LoadManifestFromCache(std::vector<ManifestEntry>& out) {
    std::string path = GetCacheFilePath();
    std::ifstream ifs(path);
    if (!ifs) return false;
    out.clear();
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        std::istringstream ls(line);
        std::string name, sizeStr, mod, subPath, url, isCoreStr;
        if (!std::getline(ls, name, ',')) continue;
        if (!std::getline(ls, sizeStr, ',')) continue;
        if (!std::getline(ls, mod, ',')) continue;
        if (!std::getline(ls, subPath, ',')) continue;
        if (!std::getline(ls, url, ',')) continue;
        if (!std::getline(ls, isCoreStr, ',')) continue;
        try {
            ManifestEntry e;
            e.fileName = name; e.subPath = subPath;
            if (!e.subPath.empty() && e.subPath.back() != '/') e.subPath += '/';
            e.fullPath = e.subPath + e.fileName;
            std::replace(e.fullPath.begin(), e.fullPath.end(), '\\', '/');
            while (e.fullPath.find("//") != std::string::npos)
                e.fullPath.replace(e.fullPath.find("//"), 2, "/");
            e.expectedSize = static_cast<size_t>(std::stoull(sizeStr));
            e.requiredMod = mod; e.url = url; e.isCore = (isCoreStr == "1");
            out.push_back(e);
        }
        catch (...) { /* ignore */ }
    }
    return !out.empty();
}

static bool FetchManifestWithProgress(std::vector<ManifestEntry>& out, std::string& status, float& progress) {
    status = "Connecting..."; progress = 0.0f; out.clear();
    for (int attempt = 0; attempt < 3; attempt++) {
        status = "Downloading manifest (attempt " + std::to_string(attempt + 1) + ")...";
        progress = 0.2f + attempt * 0.1f;
        HINTERNET hI = InternetOpenA("PinkmoonLauncher/ULTIMATE", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
        if (!hI) { status = "InternetOpen failed, retrying..."; continue; }
        HINTERNET hU = InternetOpenUrlA(hI, Config::MANIFEST_URL, nullptr, 0,
            INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE, 0);
        if (!hU) { InternetCloseHandle(hI); status = "Cannot open URL, retrying..."; continue; }
        std::string raw; char buf[4096]; DWORD br = 0;
        while (InternetReadFile(hU, buf, sizeof(buf), &br) && br > 0) raw.append(buf, br);
        InternetCloseHandle(hU); InternetCloseHandle(hI);
        if (raw.empty()) { status = "Manifest empty, retrying..."; continue; }
        status = "Parsing manifest..."; progress = 0.7f;
        std::istringstream ss(raw); std::string line;
        while (std::getline(ss, line)) {
            if (line.empty() || line[0] == '#') continue;
            if (!line.empty() && line.back() == '\r') line.pop_back();
            std::istringstream ls(line);
            std::string name, sizeStr, mod, subPath, url, isCoreStr;
            if (!std::getline(ls, name, ',')) continue;
            if (!std::getline(ls, sizeStr, ',')) continue;
            if (!std::getline(ls, mod, ',')) continue;
            if (!std::getline(ls, subPath, ',')) continue;
            if (!std::getline(ls, url, ',')) continue;
            if (!std::getline(ls, isCoreStr, ',')) continue;
            if (name.empty()) continue;
            sizeStr.erase(std::remove(sizeStr.begin(), sizeStr.end(), ','), sizeStr.end());
            try {
                ManifestEntry e;
                e.fileName = name; e.subPath = subPath;
                if (!e.subPath.empty() && e.subPath.back() != '/') e.subPath += '/';
                e.fullPath = e.subPath + e.fileName;
                std::replace(e.fullPath.begin(), e.fullPath.end(), '\\', '/');
                while (e.fullPath.find("//") != std::string::npos)
                    e.fullPath.replace(e.fullPath.find("//"), 2, "/");
                e.expectedSize = static_cast<size_t>(std::stoull(sizeStr));
                e.requiredMod = mod;
                e.url = url.empty() ? std::string(Config::FILE_BASE_URL) + e.fileName : url;
                e.isCore = (isCoreStr == "1");
                out.push_back(e);
                progress = 0.7f + 0.25f * (float)raw.size() / 200000.f;
                if (progress > 0.95f) progress = 0.95f;
            }
            catch (...) { /* skip */ }
        }
        if (out.empty()) { status = "No valid entries, retrying..."; continue; }
        progress = 1.0f; status = "Manifest loaded (" + std::to_string(out.size()) + " entries)";
        return true;
    }
    status = "Failed to fetch manifest after 3 attempts."; progress = 1.0f;
    return false;
}

static void FetchManifestThread() {
    std::vector<ManifestEntry> fetched; std::string status; float prog = 0.0f;
    bool success = FetchManifestWithProgress(fetched, status, prog);
    { std::lock_guard<std::mutex> lock(g_statusMsgMutex); g_manifestStatusMsg = status; }
    g_manifestProgress.store(prog);
    if (success) {
        SaveManifestToCache(fetched);
        { std::lock_guard<std::mutex> lk(g_manifestMutex); g_manifest = fetched; }
        g_manifestFetchSuccess.store(true);
        AddLog("[MANIFEST] Fetched " + std::to_string(fetched.size()) + " entries.");
    }
    else {
        std::vector<ManifestEntry> cached;
        if (LoadManifestFromCache(cached)) {
            { std::lock_guard<std::mutex> lk(g_manifestMutex); g_manifest = cached; }
            g_manifestFetchSuccess.store(true);
            {
                std::lock_guard<std::mutex> lock(g_statusMsgMutex);
                g_manifestStatusMsg = "Using cached manifest (update failed). " + status;
            }
            AddLog("[MANIFEST] Using cached manifest (" + std::to_string(cached.size()) + " entries).");
        }
        else {
            g_manifestError = true; g_manifestFetchSuccess.store(false);
            {
                std::lock_guard<std::mutex> lock(g_statusMsgMutex);
                g_manifestStatusMsg = "ERROR: Cannot fetch manifest and no cache available.";
            }
            AddLog("[MANIFEST] Fatal: no manifest.");
        }
    }
    g_manifestReady.store(true);
}

// =============================================================================
// DOWNLOAD FILE
// =============================================================================
static bool DownloadFileWithProgress(
    const std::string& url, const std::string& destPath, const std::string& fileName,
    std::atomic<size_t>& downloaded, std::atomic<size_t>& total, std::atomic<float>& speed)
{
    AddLog("[DL] " + fileName); SetStatus("Downloading: " + fileName);
    HINTERNET hI = InternetOpenA("PinkmoonLauncher/ULTIMATE", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hI) { AddLog("[ERROR] InternetOpen failed"); return false; }
    HINTERNET hU = InternetOpenUrlA(hI, url.c_str(), nullptr, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hU) { InternetCloseHandle(hI); AddLog("[ERROR] URL: " + url); return false; }
    DWORD clen = 0; DWORD dlen = sizeof(clen);
    if (!HttpQueryInfoA(hU, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &clen, &dlen, nullptr)) clen = 0;
    total.store(clen > 0 ? clen : 1);
    { std::error_code _ec2; std::filesystem::create_directories(std::filesystem::path(destPath).parent_path(), _ec2); }
    std::ofstream ofs(destPath, std::ios::binary | std::ios::trunc);
    if (!ofs) { InternetCloseHandle(hU); InternetCloseHandle(hI); AddLog("[ERROR] Cannot create: " + destPath); return false; }
    char cbuf[8192]; DWORD rd = 0; size_t tot = 0;
    auto t0 = std::chrono::steady_clock::now();
    while (InternetReadFile(hU, cbuf, sizeof(cbuf), &rd) && rd > 0) {
        ofs.write(cbuf, rd); tot += rd; downloaded.store(tot);
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - t0).count();
        if (ms > 200) speed.store((float)tot / ((float)ms / 1000.f));
        if (clen > 0) g_progress.store((float)tot / (float)clen);
    }
    ofs.close(); InternetCloseHandle(hU); InternetCloseHandle(hI);
    size_t actual = GetFileSizeEx(destPath);
    if (clen > 0 && actual != (size_t)clen) {
        DeleteFileA(destPath.c_str());
        AddLog("[ERROR] Size mismatch on " + fileName + " expected=" + std::to_string(clen) + " got=" + std::to_string(actual));
        return false;
    }
    AddLog("[OK] Downloaded: " + fileName + " (" + FormatSize(actual) + ")");
    return true;
}

// =============================================================================
// WHITELIST SCAN & VALIDATE FILES
// =============================================================================
static void WhitelistScan(const std::string& gamefilePath,
    const std::vector<ManifestEntry>& manifest,
    bool modMic, bool modENB, const std::string& playerName)
{
    std::unordered_set<std::string> allowed;
    for (const auto& e : manifest) {
        bool ok = e.requiredMod.empty() || (e.requiredMod == "MIC" && modMic) || (e.requiredMod == "ENB" && modENB);
        if (!ok) continue;
        std::string n = e.fullPath; std::replace(n.begin(), n.end(), '\\', '/');
        allowed.insert(n);
    }
    std::unordered_set<std::string> ignoreNames;
    for (const auto& f : Config::IGNORE_LIST) ignoreNames.insert(f);
    AddLog("[SCAN] Whitelist=" + std::to_string(allowed.size()) + " entries");
    std::error_code ec;
    for (auto& de : std::filesystem::recursive_directory_iterator(gamefilePath, ec)) {
        if (!de.is_regular_file(ec)) continue;
        std::string rel = std::filesystem::relative(de.path(), gamefilePath).string();
        std::replace(rel.begin(), rel.end(), '\\', '/');
        if (ignoreNames.count(de.path().filename().string())) continue;
        if (allowed.find(rel) == allowed.end()) {
            AddLog("[UNAUTHORIZED] " + rel);
            if (Config::DISCORD_ALERTS_ENABLED) {
                std::vector<std::pair<std::string, std::string>> fields;
                fields.push_back({ "Event", "Unauthorized file detected" });
                fields.push_back({ "Size", FormatSize(GetFileSizeEx(de.path().string())) });
                SendDiscordWebhookEmbed("⚠️ Unauthorized File", "A file not in manifest was found.", 0xFF2D87, fields, rel);
            }
        }
    }
}

static bool ValidateFiles(const std::string& gamefilePath,
    const std::vector<ManifestEntry>& manifest,
    bool modMic, bool modENB,
    std::vector<std::string>& coreErrors,
    const std::string& /*playerName*/)
{
    coreErrors.clear(); bool allOk = true;
    size_t total = manifest.size(), cur = 0;
    for (const auto& entry : manifest) {
        cur++; g_progress.store((float)cur / (float)total);
        bool shouldExist = entry.requiredMod.empty() || (entry.requiredMod == "MIC" && modMic) || (entry.requiredMod == "ENB" && modENB);
        if (!shouldExist) continue;
        std::string fp = gamefilePath + "\\" + entry.fullPath;
        { std::error_code _ec; std::filesystem::create_directories(std::filesystem::path(fp).parent_path(), _ec); }
        bool exists = FileExistsCheck(fp);
        size_t sz = exists ? GetFileSizeEx(fp) : (size_t)-1;
        bool sizeOk = exists && (sz == entry.expectedSize);
        if (!sizeOk) {
            if (entry.isCore) {
                std::string detail = entry.fullPath + " (have=" + (exists ? std::to_string(sz) : "missing") + " need=" + std::to_string(entry.expectedSize) + ")";
                coreErrors.push_back(detail); AddLog("[CORE ERROR] " + detail); allOk = false;
            }
            else {
                AddLog("[NEED] " + entry.fullPath + " (have=" + (exists ? std::to_string(sz) : "missing") + " need=" + std::to_string(entry.expectedSize) + ")");
                SetStatus("Downloading: " + entry.fileName);
                g_downloadedBytes.store(0); g_totalBytes.store(0); g_downloadSpeed.store(0);
                if (!DownloadFileWithProgress(entry.url, fp, entry.fileName, g_downloadedBytes, g_totalBytes, g_downloadSpeed)) {
                    AddLog("[ERROR] Download failed: " + entry.fullPath);
                }
            }
        }
    }
    if (!coreErrors.empty() && Config::DISCORD_ALERTS_ENABLED) {
        std::string details;
        for (const auto& e : coreErrors) details += "  - " + e + "\n";
        std::vector<std::pair<std::string, std::string>> fields;
        fields.push_back({ "Errors", details });
        SendDiscordWebhookEmbed("🚨 Core File Error", "One or more core files are corrupted or missing.", 0xFF0000, fields);
    }
    return allOk;
}

// =============================================================================
// RUN INSTALLER
// =============================================================================
static void RunInstallerAndExit() {
    AddLog("[INSTALLER] Opening download page: " + std::string(Config::REINSTALL_URL));
    SetStatus("Opening download page...");
    ShellExecuteA(nullptr, "open", Config::REINSTALL_URL, nullptr, nullptr, SW_SHOW);
    PostQuitMessage(0);
}

// =============================================================================
// BACKGROUND MONITOR
// =============================================================================
static bool IsInWhitelist(const std::string& rel, const std::vector<ManifestEntry>& manifest,
    bool modMic, bool modENB)
{
    for (const auto& e : manifest) {
        bool ok = e.requiredMod.empty() || (e.requiredMod == "MIC" && modMic) || (e.requiredMod == "ENB" && modENB);
        if (!ok) continue;
        std::string n = e.fullPath; std::replace(n.begin(), n.end(), '\\', '/');
        if (n == rel) return true;
    }
    return false;
}

static void MonitorThread(std::string gamefilePath, std::vector<ManifestEntry> manifest,
    bool modMic, bool modENB, std::string /*playerName*/)
{
    AddLog("[MONITOR] Started (5s interval)");
    g_isMonitorRunning.store(true);
    std::unordered_set<std::string> ignoreNames;
    for (const auto& f : Config::IGNORE_LIST) ignoreNames.insert(f);
    while (g_isMonitorRunning.load() && g_isGameRunning.load()) {
        std::error_code ec;
        for (auto& de : std::filesystem::recursive_directory_iterator(gamefilePath, ec)) {
            if (!de.is_regular_file(ec)) continue;
            std::string rel = std::filesystem::relative(de.path(), gamefilePath).string();
            std::replace(rel.begin(), rel.end(), '\\', '/');
            if (ignoreNames.count(de.path().filename().string())) continue;
            if (!IsInWhitelist(rel, manifest, modMic, modENB)) {
                AddLog("[CHEAT] Unauthorized while running: " + rel);
                if (Config::DISCORD_ALERTS_ENABLED) {
                    std::vector<std::pair<std::string, std::string>> fields;
                    fields.push_back({ "Event", "Cheat file injected while game running" });
                    fields.push_back({ "Size", FormatSize(GetFileSizeEx(de.path().string())) });
                    SendDiscordWebhookEmbed("🚨 CHEAT DETECTED (Runtime)", "A file was added/modified while game was running.", 0xFF0000, fields, rel);
                }
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    g_isMonitorRunning.store(false);
    AddLog("[MONITOR] Stopped");
}

// =============================================================================
// SAMP REGISTRY NICKNAME HELPERS
// =============================================================================
static bool SetSampRegistryNick(const std::string& name) {
    HKEY hKey;
    LONG res = RegCreateKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\SAMP", 0, nullptr, REG_OPTION_NON_VOLATILE,
        KEY_WRITE | KEY_WOW64_32KEY, nullptr, &hKey, nullptr);
    if (res != ERROR_SUCCESS) { AddLog("[REG] Failed to open HKCU\\SOFTWARE\\SAMP"); return false; }
    LONG setRes = RegSetValueExA(hKey, "PlayerName", 0, REG_SZ,
        reinterpret_cast<const BYTE*>(name.c_str()), static_cast<DWORD>(name.size() + 1));
    RegCloseKey(hKey);
    if (setRes != ERROR_SUCCESS) { AddLog("[REG] Failed to write PlayerName"); return false; }
    AddLog("[REG] PlayerName set to '" + name + "'");
    return true;
}

static std::string ReadSampRegistryNick() {
    HKEY hKey; std::string result;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\SAMP", 0, KEY_READ | KEY_WOW64_32KEY, &hKey) == ERROR_SUCCESS) {
        char buf[256] = {}; DWORD size = sizeof(buf); DWORD type = 0;
        if (RegQueryValueExA(hKey, "PlayerName", nullptr, &type, reinterpret_cast<BYTE*>(buf), &size) == ERROR_SUCCESS && type == REG_SZ) {
            result = buf;
        }
        RegCloseKey(hKey);
    }
    return result;
}

// =============================================================================
// LAUNCH GAME
// =============================================================================
static void LaunchGame(const std::string& gamefilePath, const std::string& playerName,
    const std::vector<ManifestEntry>& manifest, bool modMic, bool modENB)
{
    std::string gtaExe = gamefilePath + "\\gta_sa.exe";
    std::string sampDll = gamefilePath + "\\samp.dll";
    if (!FileExistsCheck(gtaExe)) { SetStatus("ERROR: gta_sa.exe not found"); AddLog("[ERROR] " + gtaExe); return; }
    if (!FileExistsCheck(sampDll)) { SetStatus("ERROR: samp.dll not found"); AddLog("[ERROR] " + sampDll); return; }
    std::string safeName = SanitizeNick(playerName);
    if (safeName != playerName) { AddLog("[WARN] Nickname adjusted to '" + safeName + "'"); strncpy_s(g_playerName, safeName.c_str(), sizeof(g_playerName) - 1); SaveConfig(); }
    if (!SetSampRegistryNick(safeName)) { AddLog("[ERROR] Failed to write PlayerName to registry"); }
    {
        HKEY hKey;
        if (RegCreateKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\SAMP", 0, nullptr, REG_OPTION_NON_VOLATILE,
            KEY_WRITE | KEY_WOW64_32KEY, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
            RegSetValueExA(hKey, "gta_sa_exe", 0, REG_SZ, (const BYTE*)gtaExe.c_str(), (DWORD)(gtaExe.size() + 1));
            RegCloseKey(hKey);
        }
    }
    std::string serverIp;
    if (!ResolveServerIp(Config::SERVER_HOST, serverIp)) { serverIp = Config::SERVER_HOST; }
    std::string portStr = Config::SERVER_PORT;
    AddLog("[LAUNCH] Server: " + serverIp + ":" + portStr + " | Nick: " + safeName);
    std::string commandLine = "-c -h " + serverIp + " -p " + portStr + " -n " + safeName;
    std::vector<char> cmdLine(commandLine.begin(), commandLine.end()); cmdLine.push_back('\0');
    STARTUPINFOA si = {}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessA(gtaExe.c_str(), cmdLine.data(), nullptr, nullptr, FALSE,
        CREATE_SUSPENDED | DETACHED_PROCESS, nullptr, gamefilePath.c_str(), &si, &pi)) {
        DWORD err = GetLastError();
        SetStatus("ERROR: CreateProcess failed (" + std::to_string(err) + ")");
        AddLog("[ERROR] CreateProcess: " + std::to_string(err));
        return;
    }
    SIZE_T pathSize = sampDll.size() + 1;
    LPVOID remoteMem = VirtualAllocEx(pi.hProcess, nullptr, pathSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!remoteMem) { TerminateProcess(pi.hProcess, 0); CloseHandle(pi.hThread); CloseHandle(pi.hProcess); return; }
    if (!WriteProcessMemory(pi.hProcess, remoteMem, sampDll.c_str(), pathSize, nullptr)) {
        VirtualFreeEx(pi.hProcess, remoteMem, 0, MEM_RELEASE);
        TerminateProcess(pi.hProcess, 0); CloseHandle(pi.hThread); CloseHandle(pi.hProcess); return;
    }
    HMODULE hKernel32 = GetModuleHandleA("kernel32");
    if (hKernel32 == NULL) { TerminateProcess(pi.hProcess, 0); CloseHandle(pi.hThread); CloseHandle(pi.hProcess); return; }
    LPTHREAD_START_ROUTINE loadLibraryAddr = (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel32, "LoadLibraryA");
    if (loadLibraryAddr == NULL) { TerminateProcess(pi.hProcess, 0); CloseHandle(pi.hThread); CloseHandle(pi.hProcess); return; }
    HANDLE hRemoteThread = CreateRemoteThread(pi.hProcess, nullptr, 0, loadLibraryAddr, remoteMem, 0, nullptr);
    if (!hRemoteThread) { VirtualFreeEx(pi.hProcess, remoteMem, 0, MEM_RELEASE); TerminateProcess(pi.hProcess, 0); CloseHandle(pi.hThread); CloseHandle(pi.hProcess); return; }
    WaitForSingleObject(hRemoteThread, INFINITE);
    DWORD loadResult = 0; GetExitCodeThread(hRemoteThread, &loadResult);
    CloseHandle(hRemoteThread); VirtualFreeEx(pi.hProcess, remoteMem, 0, MEM_RELEASE);
    if (loadResult == 0) { AddLog("[ERROR] DLL injection failed"); TerminateProcess(pi.hProcess, 0); CloseHandle(pi.hThread); CloseHandle(pi.hProcess); SetStatus("ERROR: DLL injection failed"); return; }
    AddLog("[LAUNCH] DLL injected successfully");
    ResumeThread(pi.hThread); CloseHandle(pi.hThread);
    AddLog("[LAUNCH] Game PID=" + std::to_string(pi.dwProcessId));
    SetStatus("Game running...");
    if (g_hJob == nullptr) {
        g_hJob = CreateJobObject(nullptr, nullptr);
        if (g_hJob) {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {};
            jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(g_hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
        }
    }
    if (g_hJob) AssignProcessToJobObject(g_hJob, pi.hProcess);
    g_gameProcess = pi.hProcess;
    g_isGameRunning.store(true);
    ShowWindow(g_hwnd, SW_HIDE);
    g_monitorThread = std::thread(MonitorThread, gamefilePath, manifest, modMic, modENB, playerName);
    WaitForSingleObject(g_gameProcess, INFINITE);
    CloseHandle(g_gameProcess); g_gameProcess = nullptr;
    g_isGameRunning.store(false);
    AddLog("[LAUNCH] Game exited");
    SetStatus("Game closed");
    g_isMonitorRunning.store(false);
    if (g_monitorThread.joinable()) g_monitorThread.join();
    ShowWindow(g_hwnd, SW_SHOW);
}

// =============================================================================
// VALIDATION THREAD
// =============================================================================
static void ValidationThread(std::string gamefilePath, std::string playerName, bool modMic, bool modENB) {
    AddLog("=== VALIDATION START ===");
    SetStatus("Starting validation...");
    g_progress.store(0.f);
    if (!std::filesystem::exists(gamefilePath)) {
        SetStatus("ERROR: gamefile folder not found!"); AddLog("[ERROR] Not found: " + gamefilePath);
        g_isProcessing.store(false); return;
    }
    std::vector<ManifestEntry> manifest;
    { std::lock_guard<std::mutex> lk(g_manifestMutex); manifest = g_manifest; }
    if (manifest.empty()) { SetStatus("ERROR: No manifest available"); AddLog("[ERROR] Manifest empty"); g_isProcessing.store(false); return; }
    WhitelistScan(gamefilePath, manifest, modMic, modENB, playerName);
    bool verified = false; int loop = 0; std::vector<std::string> coreErrors;
    while (!verified && loop < Config::MAX_VERIFY_LOOPS) {
        loop++;
        AddLog("=== Round " + std::to_string(loop) + " ===");
        bool dlOk = ValidateFiles(gamefilePath, manifest, modMic, modENB, coreErrors, playerName);
        if (!coreErrors.empty()) {
            std::string msg = "Found Core file errors:\n";
            for (const auto& e : coreErrors) msg += "  - " + e + "\n";
            msg += "\nDo you want to reinstall or continue playing?";
            int choice = MessageBoxA(g_hwnd, msg.c_str(), "Pinkmoon Town - Core Error",
                MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
            if (choice == IDYES) { RunInstallerAndExit(); g_isProcessing.store(false); return; }
            else { AddLog("[WARN] User ignored core errors."); verified = true; break; }
        }
        if (dlOk) {
            bool allOk = true;
            for (const auto& e : manifest) {
                bool ok = e.requiredMod.empty() || (e.requiredMod == "MIC" && modMic) || (e.requiredMod == "ENB" && modENB);
                if (!ok) continue;
                if (e.isCore) continue;
                if (e.expectedSize == 0) continue;
                std::string fp = gamefilePath + "\\" + e.fullPath;
                size_t sz = GetFileSizeEx(fp);
                if (sz != e.expectedSize) { AddLog("[FAIL] Patch still wrong: " + e.fullPath); allOk = false; }
            }
            if (allOk) { verified = true; AddLog("[OK] All verified"); }
            else { AddLog("[WARN] Retry patch download..."); }
        }
        else { AddLog("[WARN] Retry..."); }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    if (!verified) {
        SetStatus("ERROR: Verification failed after " + std::to_string(Config::MAX_VERIFY_LOOPS) + " rounds");
        AddLog("[ERROR] Give up");
        g_isProcessing.store(false); return;
    }
    LaunchGame(gamefilePath, playerName, manifest, modMic, modENB);
    g_isProcessing.store(false);
    AddLog("=== VALIDATION END ===");
}

// =============================================================================
// DX11
// =============================================================================
static void CreateRenderTarget() {
    ID3D11Texture2D* pB = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pB));
    if (pB) { g_pd3dDevice->CreateRenderTargetView(pB, nullptr, &g_mainRenderTargetView); pB->Release(); }
}
static void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}
static void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
    if (g_wallpaperTexture) { g_wallpaperTexture->Release(); g_wallpaperTexture = nullptr; }
}
static bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2; sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60; sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd; sd.SampleDesc.Count = 1; sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    const D3D_FEATURE_LEVEL fla[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL fl;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        fla, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &fl, &g_pd3dDeviceContext);
    if (FAILED(hr)) return false;
    CreateRenderTarget();
    LoadTextures();
    return true;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {
    case WM_NCHITTEST: {
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };
        RECT rc; GetClientRect(hWnd, &rc); MapWindowPoints(hWnd, nullptr, (POINT*)&rc, 2);
        int border = 6, titleH = 55;
        if (pt.y < rc.top + titleH && pt.x > rc.left + 50 && pt.x < rc.right - 100) return HTCAPTION;
        if (pt.x < rc.left + border) return HTLEFT;
        if (pt.x > rc.right - border) return HTRIGHT;
        if (pt.y < rc.top + border) return HTTOP;
        if (pt.y > rc.bottom - border) return HTBOTTOM;
        if (pt.x < rc.left + border && pt.y < rc.top + border) return HTTOPLEFT;
        if (pt.x > rc.right - border && pt.y < rc.top + border) return HTTOPRIGHT;
        if (pt.x < rc.left + border && pt.y > rc.bottom - border) return HTBOTTOMLEFT;
        if (pt.x > rc.right - border && pt.y > rc.bottom - border) return HTBOTTOMRIGHT;
        return HTCLIENT;
    }
    case WM_SIZE:
        if (g_pd3dDevice && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        } return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_KEYMENU) return 0; break;
    case WM_CLOSE:
        if (g_isGameRunning.load()) { ShowWindow(g_hwnd, SW_HIDE); return 0; } break;
    case WM_DESTROY:
        if (g_hJob) { CloseHandle(g_hJob); g_hJob = nullptr; }
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// =============================================================================
// =============================================================================
//  ULTIMATE UI RENDER ENGINE
//  Premium Fonts | Glassmorphism | Full Settings | Readable Layout
// =============================================================================
// =============================================================================

// -----------------------------------------------------------------------------
// DRAWING HELPERS
// -----------------------------------------------------------------------------
static void DrawGlassCard(ImDrawList* dl, const ImVec2& pos, const ImVec2& size,
    float rounding, float blurAlpha = 0.65f, bool border = true)
{
    ImU32 bg = IM_COL32(255, 255, 255, (int)(18 * blurAlpha));
    ImU32 borderCol = IM_COL32(255, 255, 255, (int)(30 * blurAlpha));
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), bg, rounding);
    if (border) {
        dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), borderCol, rounding, 0, 1.5f);
    }
}

static void DrawGlassButton(ImDrawList* dl, const ImVec2& pos, const ImVec2& size,
    const char* label, bool hovered, bool pressed, float pulse, bool active,
    ImU32 color1 = Color::Pink, ImU32 color2 = Color::Purple)
{
    float rounding = 16.f;
    float t = pulse * 0.5f;

    // Shadow
    dl->AddRectFilled(
        ImVec2(pos.x + 6, pos.y + 8),
        ImVec2(pos.x + size.x + 6, pos.y + size.y + 8),
        Color::ShadowLight, rounding + 2);

    // Background
    ImU32 bg;
    if (!active) {
        bg = IM_COL32(80, 80, 90, 120);
    }
    else if (pressed) {
        bg = color1;
    }
    else if (hovered) {
        bg = ColorLerp(Color::Glass, color1, 0.35f);
    }
    else {
        bg = ColorLerp(Color::Glass, color1, 0.10f);
    }
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), bg, rounding);

    // Border
    if (hovered && active) {
        ImU32 borderCol = ColorLerp(color1, color2, sinf(pulse * 2.0f) * 0.5f + 0.5f);
        dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), borderCol, rounding, 0, 2.5f);
    }
    else {
        dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), Color::GlassBorder, rounding, 0, 1.5f);
    }

    // Glow on hover
    if (hovered && active) {
        float glow = 0.3f + 0.2f * sinf(pulse * 3.0f);
        dl->AddRectFilled(
            ImVec2(pos.x - 8, pos.y - 8),
            ImVec2(pos.x + size.x + 8, pos.y + size.y + 8),
            IM_COL32(255, 100, 200, (int)(20 * glow)), rounding + 4);
    }

    // Reflection sweep
    if (hovered && active) {
        float sweepX = (sinf(pulse * 1.5f) * 0.5f + 0.5f) * size.x;
        dl->AddRectFilled(
            ImVec2(pos.x + sweepX - 20, pos.y + 4),
            ImVec2(pos.x + sweepX + 20, pos.y + size.y - 4),
            IM_COL32(255, 255, 255, 35), 6.f);
    }

    // Text - use bold font
    ImGui::PushFont(g_fontBold);
    ImVec2 textSize = ImGui::CalcTextSize(label);
    ImVec2 textPos(pos.x + (size.x - textSize.x) * 0.5f, pos.y + (size.y - textSize.y) * 0.5f);
    ImU32 textCol = active ? Color::White : Color::TextSoft;
    dl->AddText(textPos, textCol, label);
    ImGui::PopFont();
}

// -----------------------------------------------------------------------------
// SPLASH SCREEN
// -----------------------------------------------------------------------------
static void RenderSplash() {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##splash", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoBackground);
    ImGui::PopStyleVar();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 winPos = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();

    // Background
    dl->AddRectFilledMultiColor(winPos, ImVec2(winPos.x + winSize.x, winPos.y + winSize.y),
        IM_COL32(8, 4, 12, 255), IM_COL32(25, 10, 35, 255),
        IM_COL32(25, 10, 35, 255), IM_COL32(8, 4, 12, 255));

    // Particles
    for (int i = 0; i < 40; i++) {
        float x = winPos.x + fmodf(i * 173.0f + 50 * i, winSize.x);
        float y = winPos.y + fmodf(i * 97.0f + 30 * i * 0.7f, winSize.y);
        float size = 1.0f + 2.0f * sinf(g_pulseTime * 0.4f + i * 0.5f);
        float alpha = 0.2f + 0.2f * sinf(g_pulseTime * 0.6f + i * 0.7f);
        dl->AddCircleFilled(ImVec2(x, y), size, IM_COL32(255, 150, 220, (int)(alpha * 80)));
    }

    // Main card
    float boxW = 540, boxH = 340;
    ImVec2 center = GetCenter(ImVec2(boxW, boxH));
    ImVec2 boxPos = center;

    // Glass card
    dl->AddRectFilled(boxPos, ImVec2(boxPos.x + boxW, boxPos.y + boxH),
        IM_COL32(255, 255, 255, 15), 28.f);
    dl->AddRect(boxPos, ImVec2(boxPos.x + boxW, boxPos.y + boxH),
        IM_COL32(255, 255, 255, 25), 28.f, 0, 1.5f);

    // Animated glow border
    float glowPhase = sinf(g_pulseTime * 0.6f) * 0.5f + 0.5f;
    ImU32 glowColor = ColorLerp(Color::Pink, Color::Purple, glowPhase);
    dl->AddRect(ImVec2(boxPos.x - 3, boxPos.y - 3),
        ImVec2(boxPos.x + boxW + 3, boxPos.y + boxH + 3),
        glowColor, 30.f, 0, 2.5f);

    // Logo - use bold font
    float x0 = boxPos.x + 35, y0 = boxPos.y + 35;
    ImGui::PushFont(g_fontTitle);
    dl->AddText(ImVec2(x0, y0), Color::Pink, "PINKMOON");
    ImGui::PopFont();

    ImGui::PushFont(g_fontRegular);
    dl->AddText(ImVec2(x0, y0 + 42), Color::TextSoft, "LAUNCHER ULTIMATE");
    ImGui::PopFont();

    // Status
    std::string status;
    { std::lock_guard<std::mutex> lock(g_statusMsgMutex); status = g_manifestStatusMsg; }
    ImGui::PushFont(g_fontSmall);
    dl->AddText(ImVec2(x0, y0 + 90), Color::TextDim, status.c_str());
    ImGui::PopFont();

    // Progress bar
    float prog = g_manifestProgress.load();
    ImVec2 barPos(x0, y0 + 130);
    float barW = boxW - 70, barH = 16;
    dl->AddRectFilled(barPos, ImVec2(barPos.x + barW, barPos.y + barH),
        IM_COL32(50, 50, 60, 180), 10.f);
    if (prog > 0) {
        ImVec2 fillEnd(barPos.x + barW * prog, barPos.y + barH);
        dl->AddRectFilledMultiColor(barPos, fillEnd,
            IM_COL32(255, 102, 204, 255),
            IM_COL32(200, 80, 230, 255),
            IM_COL32(200, 80, 230, 255),
            IM_COL32(255, 102, 204, 255));
        dl->AddRectFilled(
            ImVec2(barPos.x + barW * prog * 0.2f, barPos.y + 3),
            ImVec2(barPos.x + barW * prog * 0.8f, barPos.y + barH - 3),
            IM_COL32(255, 255, 255, 50), 6.f);
    }

    // Spinner
    if (prog < 1.0f) {
        float angle = g_pulseTime * 2.5f;
        ImVec2 spinCenter(boxPos.x + boxW - 40, boxPos.y + boxH - 40);
        dl->PathArcTo(spinCenter, 14, angle, angle + 3.14159f * 1.8f, 32);
        dl->PathStroke(Color::Pink, false, 3.f);
        ImVec2 dotPos(spinCenter.x + 14 * cosf(angle + 3.14159f * 1.8f),
            spinCenter.y + 14 * sinf(angle + 3.14159f * 1.8f));
        dl->AddCircleFilled(dotPos, 3.5f, Color::Pink);
    }
    else {
        dl->AddText(ImVec2(boxPos.x + boxW - 50, boxPos.y + boxH - 50),
            Color::Success, "✓");
    }

    ImGui::End();
}

// -----------------------------------------------------------------------------
// HOME PAGE
// -----------------------------------------------------------------------------
static void DrawHomePage(ImDrawList* dl, const ImVec2& contentPos, const ImVec2& contentSize) {
    float cx = contentPos.x + 28, cy = contentPos.y + 24;

    // Welcome
    ImGui::PushFont(g_fontTitle);
    dl->AddText(ImVec2(cx, cy), Color::White, "WELCOME BACK");
    ImGui::PopFont();

    ImGui::PushFont(g_fontRegular);
    dl->AddText(ImVec2(cx, cy + 40), Color::TextDim, "Ready to play Pinkmoon Town?");
    ImGui::PopFont();

    cy += 90;

    // Nickname input
    ImGui::PushFont(g_fontRegular);
    dl->AddText(ImVec2(cx, cy), Color::TextSoft, "NICKNAME");
    ImGui::PopFont();
    cy += 24;
    ImVec2 inputPos(cx, cy);
    ImVec2 inputSize(contentSize.x - 56, 42);
    dl->AddRectFilled(inputPos, ImVec2(inputPos.x + inputSize.x, inputPos.y + inputSize.y),
        IM_COL32(25, 20, 35, 220), 12.f);
    dl->AddRect(inputPos, ImVec2(inputPos.x + inputSize.x, inputPos.y + inputSize.y),
        IM_COL32(255, 255, 255, 20), 12.f, 0, 1.5f);

    ImGui::SetCursorScreenPos(inputPos);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::PushFont(g_fontRegular);
    ImGui::PushItemWidth(inputSize.x);
    if (ImGui::InputText("##nickname", g_playerName, sizeof(g_playerName))) SaveConfig();
    ImGui::PopItemWidth();
    ImGui::PopFont();
    ImGui::PopStyleColor(3);
    cy += inputSize.y + 28;

    // PLAY BUTTON
    bool busy = g_isProcessing.load() || g_isGameRunning.load();
    ImVec2 btnSize(340, 72);
    ImVec2 btnPos(cx + (contentSize.x - btnSize.x - 56) * 0.5f, cy);
    bool hover = ImGui::IsMouseHoveringRect(btnPos, ImVec2(btnPos.x + btnSize.x, btnPos.y + btnSize.y));
    bool pressed = ImGui::IsMouseClicked(0) && hover && !busy;

    float scale = 1.0f + 0.015f * sinf(g_pulseTime * 1.5f);
    if (hover) scale = 1.04f;
    if (pressed) scale = 0.96f;
    ImVec2 actualSize(btnSize.x * scale, btnSize.y * scale);
    ImVec2 actualPos(btnPos.x + (btnSize.x - actualSize.x) * 0.5f,
        btnPos.y + (btnSize.y - actualSize.y) * 0.5f);

    // Glow ring
    for (int i = 0; i < 3; i++) {
        float r = 55.f + 25.f * sinf(g_pulseTime * 1.2f + i * 2.1f);
        float x = btnPos.x + btnSize.x * 0.5f + r * cosf(g_pulseTime * 0.6f + i * 2.094f);
        float y = btnPos.y + btnSize.y * 0.5f + r * sinf(g_pulseTime * 0.6f + i * 2.094f);
        dl->AddCircleFilled(ImVec2(x, y), 3.5f, IM_COL32(255, 150, 220, 60));
    }

    DrawGlassButton(dl, actualPos, actualSize,
        busy ? "PLAYING..." : "PLAY",
        hover, pressed, g_pulseTime, !busy);

    if (pressed) {
        g_isProcessing.store(true);
        g_progress.store(0.f);
        g_downloadedBytes.store(0); g_totalBytes.store(0); g_downloadSpeed.store(0);
        SetStatus("Starting...");
        { std::lock_guard<std::mutex> lk(g_logMutex); g_logs.clear(); }
        if (g_workerThread.joinable()) g_workerThread.join();
        std::string path = g_gamePath, name = g_playerName;
        bool mic = g_modMic, enb = g_modENB;
        g_workerThread = std::thread(ValidationThread, path, name, mic, enb);
    }

    cy += btnSize.y + 30;

    // Server Status Card
    ImVec2 serverPos(cx, cy);
    ImVec2 serverSize(contentSize.x - 56, 80);
    dl->AddRectFilled(serverPos, ImVec2(serverPos.x + serverSize.x, serverPos.y + serverSize.y),
        IM_COL32(25, 20, 35, 200), 14.f);
    dl->AddRect(serverPos, ImVec2(serverPos.x + serverSize.x, serverPos.y + serverSize.y),
        IM_COL32(255, 255, 255, 15), 14.f, 0, 1.5f);

    std::lock_guard<std::mutex> lock(g_serverStatusMutex);
    bool online = g_serverMaxPlayers > 0;
    ImU32 dotColor = online ? Color::Success : Color::Danger;
    float dotPulse = 0.8f + 0.2f * sinf(g_pulseTime * 2.5f);
    dl->AddCircleFilled(ImVec2(serverPos.x + 20, serverPos.y + 18), 6 * dotPulse, dotColor);
    dl->AddCircle(ImVec2(serverPos.x + 20, serverPos.y + 18), 10, dotColor, 0, 2.f);

    ImGui::PushFont(g_fontBold);
    dl->AddText(ImVec2(serverPos.x + 40, serverPos.y + 8), Color::TextSoft, "SERVER");
    ImGui::PopFont();

    ImGui::PushFont(g_fontRegular);
    dl->AddText(ImVec2(serverPos.x + 40, serverPos.y + 32), online ? Color::White : Color::Danger,
        online ? "ONLINE" : "OFFLINE");

    if (online) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Players: %d / %d", g_serverPlayers, g_serverMaxPlayers);
        dl->AddText(ImVec2(serverPos.x + 130, serverPos.y + 8), Color::TextDim, buf);
        snprintf(buf, sizeof(buf), "Mode: %s", g_serverGamemode.c_str());
        dl->AddText(ImVec2(serverPos.x + 130, serverPos.y + 32), Color::TextDim, buf);
    }
    else {
        dl->AddText(ImVec2(serverPos.x + 130, serverPos.y + 20), Color::TextSoft,
            "Server is not responding.");
    }
    ImGui::PopFont();
}

// -----------------------------------------------------------------------------
// SETTINGS PAGE
// -----------------------------------------------------------------------------
static void DrawSettingsPage(ImDrawList* dl, const ImVec2& contentPos, const ImVec2& contentSize) {
    float cx = contentPos.x + 28, cy = contentPos.y + 24;

    ImGui::PushFont(g_fontTitle);
    dl->AddText(ImVec2(cx, cy), Color::White, "SETTINGS");
    ImGui::PopFont();

    cy += 60;

    // Game Path
    ImGui::PushFont(g_fontRegular);
    dl->AddText(ImVec2(cx, cy), Color::TextSoft, "Game Path");
    cy += 24;
    ImVec2 pathInputPos(cx, cy);
    ImVec2 pathInputSize(contentSize.x - 180, 38);
    dl->AddRectFilled(pathInputPos, ImVec2(pathInputPos.x + pathInputSize.x, pathInputPos.y + pathInputSize.y),
        IM_COL32(25, 20, 35, 200), 10.f);
    dl->AddRect(pathInputPos, ImVec2(pathInputPos.x + pathInputSize.x, pathInputPos.y + pathInputSize.y),
        IM_COL32(255, 255, 255, 15), 10.f, 0, 1.5f);

    ImGui::SetCursorScreenPos(pathInputPos);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::PushFont(g_fontSmall);
    ImGui::PushItemWidth(pathInputSize.x);
    ImGui::InputText("##gamepath", (char*)g_gamePath.c_str(), g_gamePath.size() + 1, ImGuiInputTextFlags_ReadOnly);
    ImGui::PopItemWidth();
    ImGui::PopFont();
    ImGui::PopStyleColor(3);

    // Browse button
    ImVec2 browseBtnPos(pathInputPos.x + pathInputSize.x + 12, cy);
    ImVec2 browseBtnSize(100, 38);
    bool hoverBrowse = ImGui::IsMouseHoveringRect(browseBtnPos, ImVec2(browseBtnPos.x + browseBtnSize.x, browseBtnPos.y + browseBtnSize.y));
    bool pressedBrowse = ImGui::IsMouseClicked(0) && hoverBrowse;
    DrawGlassButton(dl, browseBtnPos, browseBtnSize, "Browse", hoverBrowse, pressedBrowse, g_pulseTime, true, Color::Purple);
    if (pressedBrowse) BrowseFolder();

    cy += 50;

    // Checkboxes
    ImGui::PushFont(g_fontRegular);
    dl->AddText(ImVec2(cx, cy), Color::TextSoft, "Optional Mods");
    cy += 30;

    // ENB
    ImGui::SetCursorScreenPos(ImVec2(cx, cy));
    ImGui::PushFont(g_fontRegular);
    ImGui::PushStyleColor(ImGuiCol_Text, Color::White);
    if (ImGui::Checkbox("Install ENB Graphics (d3d9.dll)", &g_modENB)) SaveConfig();
    ImGui::PopStyleColor();
    ImGui::PopFont();
    cy += 34;

    // MIC
    ImGui::SetCursorScreenPos(ImVec2(cx, cy));
    ImGui::PushFont(g_fontRegular);
    ImGui::PushStyleColor(ImGuiCol_Text, Color::White);
    if (ImGui::Checkbox("Install MIC / Voice (SAMPVoice.dll)", &g_modMic)) SaveConfig();
    ImGui::PopStyleColor();
    ImGui::PopFont();
    cy += 40;

    // Save button
    ImVec2 saveBtnPos(cx, cy);
    ImVec2 saveBtnSize(180, 44);
    bool hoverSave = ImGui::IsMouseHoveringRect(saveBtnPos, ImVec2(saveBtnPos.x + saveBtnSize.x, saveBtnPos.y + saveBtnSize.y));
    bool pressedSave = ImGui::IsMouseClicked(0) && hoverSave;
    DrawGlassButton(dl, saveBtnPos, saveBtnSize, "Save Config", hoverSave, pressedSave, g_pulseTime, true);
    if (pressedSave) {
        SaveConfig();
        SetStatus("Settings saved!");
        AddLog("[SETTINGS] Config saved.");
    }

    cy += 60;
    ImGui::PushFont(g_fontSmall);
    dl->AddText(ImVec2(cx, cy), Color::TextSoft, "Settings are saved automatically when changed.");
    ImGui::PopFont();
}

// -----------------------------------------------------------------------------
// MAIN UI
// -----------------------------------------------------------------------------
static void RenderMainUI() {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::Begin("##root", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoBackground);
    ImGui::PopStyleVar(2);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 winPos = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();

    // =========================================================================
    // 1. BACKGROUND (Wallpaper + Overlay + Aurora + Particles)
    // =========================================================================
    if (g_wallpaperTexture) {
        ImGui::Image((void*)g_wallpaperTexture, winSize);
        dl->AddRectFilledMultiColor(winPos, ImVec2(winPos.x + winSize.x, winPos.y + winSize.y),
            IM_COL32(0, 0, 0, 120), IM_COL32(0, 0, 0, 100),
            IM_COL32(0, 0, 0, 100), IM_COL32(0, 0, 0, 120));
        dl->AddRectFilledMultiColor(winPos, ImVec2(winPos.x + winSize.x, winPos.y + winSize.y),
            IM_COL32(0, 0, 0, 80), IM_COL32(0, 0, 0, 20),
            IM_COL32(0, 0, 0, 20), IM_COL32(0, 0, 0, 80));
    }
    else {
        dl->AddRectFilledMultiColor(winPos, ImVec2(winPos.x + winSize.x, winPos.y + winSize.y),
            IM_COL32(8, 4, 12, 255), IM_COL32(20, 8, 30, 255),
            IM_COL32(20, 8, 30, 255), IM_COL32(8, 4, 12, 255));
    }

    // Aurora glow
    for (int i = 0; i < 3; i++) {
        float x = winPos.x + (0.2f + i * 0.3f) * winSize.x + sinf(g_pulseTime * 0.1f + i) * 80;
        float y = winPos.y + (0.3f + i * 0.2f) * winSize.y + cosf(g_pulseTime * 0.08f + i * 1.5f) * 60;
        float r = 200 + 100 * sinf(g_pulseTime * 0.05f + i);
        dl->AddCircleFilled(ImVec2(x, y), r,
            IM_COL32(255, 100, 200, (int)(8 + 6 * sinf(g_pulseTime * 0.07f + i))));
    }

    // Floating particles
    for (int i = 0; i < 60; i++) {
        float x = winPos.x + fmodf(i * 197.0f + 60 * i, winSize.x);
        float y = winPos.y + fmodf(i * 117.0f + 40 * i * 0.6f, winSize.y);
        float size = 0.8f + 2.5f * sinf(g_pulseTime * 0.3f + i * 0.6f);
        float alpha = 0.1f + 0.2f * sinf(g_pulseTime * 0.5f + i * 0.9f);
        dl->AddCircleFilled(ImVec2(x, y), size, IM_COL32(255, 180, 230, (int)(alpha * 50)));
    }

    // =========================================================================
    // 2. TITLE BAR
    // =========================================================================
    float tbH = 55.f;
    ImVec2 tbPos = winPos;
    dl->AddRectFilled(tbPos, ImVec2(tbPos.x + winSize.x, tbPos.y + tbH),
        IM_COL32(0, 0, 0, 100));
    dl->AddLine(ImVec2(tbPos.x, tbPos.y + tbH), ImVec2(tbPos.x + winSize.x, tbPos.y + tbH),
        Color::Pink, 1.5f);

    ImGui::PushFont(g_fontTitle);
    dl->AddText(ImVec2(tbPos.x + 24, tbPos.y + 12), Color::Pink, "PINKMOON");
    ImGui::PopFont();
    ImGui::PushFont(g_fontRegular);
    dl->AddText(ImVec2(tbPos.x + 24 + 135, tbPos.y + 16), Color::TextSoft, "LAUNCHER ULTIMATE");
    ImGui::PopFont();

    // Minimize / Close
    ImGui::SetCursorScreenPos(ImVec2(winPos.x + winSize.x - 100, winPos.y + 12));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.08f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.15f));
    ImGui::PushFont(g_fontBold);
    if (ImGui::Button("—", ImVec2(36, 32))) ShowWindow(g_hwnd, SW_MINIMIZE);
    ImGui::SameLine(0, 2);
    if (ImGui::Button("✕", ImVec2(36, 32))) {
        if (g_isGameRunning.load()) ShowWindow(g_hwnd, SW_HIDE);
        else PostQuitMessage(0);
    }
    ImGui::PopFont();
    ImGui::PopStyleColor(3);

    // =========================================================================
    // 3. LAYOUT
    // =========================================================================
    float sidebarW = 220.f;
    float bottomH = 75.f;
    float contentH = winSize.y - tbH - bottomH - 16;

    ImVec2 sidebarPos = ImVec2(winPos.x + 12, tbPos.y + tbH + 8);
    ImVec2 sidebarSize(sidebarW, contentH);

    ImVec2 contentPos = ImVec2(sidebarPos.x + sidebarW + 12, tbPos.y + tbH + 8);
    ImVec2 contentSize(winSize.x - sidebarW - 36, contentH);

    ImVec2 bottomPos = ImVec2(winPos.x + 12, winPos.y + winSize.y - bottomH - 8);
    ImVec2 bottomSize(winSize.x - 24, bottomH);

    // =========================================================================
    // 4. SIDEBAR
    // =========================================================================
    DrawGlassCard(dl, sidebarPos, sidebarSize, 20.f, 0.65f);

    float y = sidebarPos.y + 28;
    ImGui::PushFont(g_fontBold);
    dl->AddText(ImVec2(sidebarPos.x + 24, y), Color::White, "MENU");
    ImGui::PopFont();
    y += 44;

    const char* items[] = { "HOME", "PLAY", "SETTINGS", "EXIT" };
    Page pages[] = { PAGE_HOME, PAGE_HOME, PAGE_SETTINGS, PAGE_HOME };
    for (int i = 0; i < 4; i++) {
        ImVec2 btnPos(sidebarPos.x + 12, y);
        ImVec2 btnSize(sidebarSize.x - 24, 46);
        bool hover = ImGui::IsMouseHoveringRect(btnPos, ImVec2(btnPos.x + btnSize.x, btnPos.y + btnSize.y));
        bool pressed = ImGui::IsMouseClicked(0) && hover;

        if (hover) {
            dl->AddRectFilled(btnPos, ImVec2(btnPos.x + btnSize.x, btnPos.y + btnSize.y),
                IM_COL32(255, 100, 200, 30), 12.f);
        }

        ImGui::PushFont(g_fontRegular);
        ImVec2 textPos(btnPos.x + 20, btnPos.y + 12);
        dl->AddText(textPos, hover ? Color::White : Color::TextDim, items[i]);
        ImGui::PopFont();

        if (pressed) {
            if (i == 3) { // EXIT
                if (g_isGameRunning.load()) ShowWindow(g_hwnd, SW_HIDE);
                else PostQuitMessage(0);
            }
            else {
                g_currentPage = pages[i];
            }
        }
        y += btnSize.y + 6;
    }

    // =========================================================================
    // 5. CONTENT AREA
    // =========================================================================
    DrawGlassCard(dl, contentPos, contentSize, 20.f, 0.55f);

    if (g_currentPage == PAGE_HOME) {
        DrawHomePage(dl, contentPos, contentSize);
    }
    else {
        DrawSettingsPage(dl, contentPos, contentSize);
    }

    // =========================================================================
    // 6. BOTTOM BAR
    // =========================================================================
    DrawGlassCard(dl, bottomPos, bottomSize, 16.f, 0.5f);

    float prog = g_progress.load();
    size_t dl_ = g_downloadedBytes.load();
    size_t tot_ = g_totalBytes.load();
    float spd_ = g_downloadSpeed.load();

    ImVec2 barPos(bottomPos.x + 20, bottomPos.y + 12);
    float barW = bottomSize.x - 40;
    float barH = 12;
    dl->AddRectFilled(barPos, ImVec2(barPos.x + barW, barPos.y + barH),
        IM_COL32(40, 40, 50, 180), 8.f);
    if (prog > 0.0f) {
        ImVec2 fillEnd(barPos.x + barW * prog, barPos.y + barH);
        dl->AddRectFilledMultiColor(barPos, fillEnd,
            IM_COL32(255, 102, 204, 255),
            IM_COL32(200, 80, 230, 255),
            IM_COL32(200, 80, 230, 255),
            IM_COL32(255, 102, 204, 255));
        dl->AddRectFilled(
            ImVec2(barPos.x + barW * prog * 0.2f, barPos.y + 2),
            ImVec2(barPos.x + barW * prog * 0.8f, barPos.y + barH - 2),
            IM_COL32(255, 255, 255, 40), 6.f);
    }

    // Status text
    {
        std::lock_guard<std::mutex> lk(g_statusMutex);
        ImGui::PushFont(g_fontSmall);
        dl->AddText(ImVec2(barPos.x, barPos.y + barH + 6), Color::TextSoft, g_statusText.c_str());
        ImGui::PopFont();
    }

    // Download info
    if (tot_ > 1 && prog > 0.f && prog < 1.f) {
        char buf[128];
        snprintf(buf, sizeof(buf), "%s / %s  •  %s",
            FormatSize(dl_).c_str(), FormatSize(tot_).c_str(), FormatSpeed(spd_).c_str());
        ImGui::PushFont(g_fontSmall);
        ImVec2 txtSize = ImGui::CalcTextSize(buf);
        dl->AddText(ImVec2(barPos.x + barW - txtSize.x, barPos.y + barH + 6),
            Color::TextSoft, buf);
        ImGui::PopFont();
    }

    // Version
    ImGui::PushFont(g_fontSmall);
    dl->AddText(ImVec2(bottomPos.x + bottomSize.x - 140, bottomPos.y + bottomSize.y - 20),
        Color::TextSoft, "vULTIMATE  •  PINKMOON");
    ImGui::PopFont();

    ImGui::End();
}

// =============================================================================
// WINMAIN
// =============================================================================
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
    constexpr int WIN_W = 1280, WIN_H = 780;
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc); wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc; wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"PinkmoonLauncherUltimate";
    RegisterClassExW(&wc);

    g_hwnd = CreateWindowW(wc.lpszClassName, L"Pinkmoon Town",
        WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, WIN_W, WIN_H,
        nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDeviceD3D(g_hwnd)) { CleanupDeviceD3D(); UnregisterClassW(wc.lpszClassName, wc.hInstance); return 1; }
    ShowWindow(g_hwnd, SW_SHOWDEFAULT); UpdateWindow(g_hwnd);

    g_publicIP = GetPublicIP();
    g_hardwareID = GetHardwareID();
    LoadConfig();
    AddLog("[INFO] IP=" + g_publicIP + "  HWID=" + g_hardwareID);
    AddLog("[INFO] Player=" + std::string(g_playerName) + " Mic=" + (g_modMic ? "1" : "0") + " ENB=" + (g_modENB ? "1" : "0"));

    std::thread queryThread(ServerQueryThread);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); io.IniFilename = nullptr;

    // =========================================================================
    // LOAD PREMIUM FONTS (Segoe UI)
    // =========================================================================
    ImFontConfig config;
    config.OversampleH = 2;
    config.OversampleV = 1;
    config.PixelSnapH = true;

    // Try Segoe UI first, fallback to Tahoma
    const char* fontPaths[] = {
        "C:\\Windows\\Fonts\\segoeui.ttf",
        "C:\\Windows\\Fonts\\tahoma.ttf"
    };
    const char* fontBoldPaths[] = {
        "C:\\Windows\\Fonts\\segoeuib.ttf",
        "C:\\Windows\\Fonts\\tahomabd.ttf"
    };

    g_fontRegular = io.Fonts->AddFontFromFileTTF(fontPaths[0], 16.0f, &config, io.Fonts->GetGlyphRangesThai());
    if (!g_fontRegular) g_fontRegular = io.Fonts->AddFontFromFileTTF(fontPaths[1], 16.0f, &config, io.Fonts->GetGlyphRangesThai());
    if (!g_fontRegular) g_fontRegular = io.Fonts->AddFontDefault();

    g_fontBold = io.Fonts->AddFontFromFileTTF(fontBoldPaths[0], 18.0f, &config, io.Fonts->GetGlyphRangesThai());
    if (!g_fontBold) g_fontBold = io.Fonts->AddFontFromFileTTF(fontBoldPaths[1], 18.0f, &config, io.Fonts->GetGlyphRangesThai());
    if (!g_fontBold) g_fontBold = g_fontRegular;

    g_fontTitle = io.Fonts->AddFontFromFileTTF(fontBoldPaths[0], 24.0f, &config, io.Fonts->GetGlyphRangesThai());
    if (!g_fontTitle) g_fontTitle = io.Fonts->AddFontFromFileTTF(fontBoldPaths[1], 24.0f, &config, io.Fonts->GetGlyphRangesThai());
    if (!g_fontTitle) g_fontTitle = g_fontBold;

    g_fontSmall = io.Fonts->AddFontFromFileTTF(fontPaths[0], 14.0f, &config, io.Fonts->GetGlyphRangesThai());
    if (!g_fontSmall) g_fontSmall = io.Fonts->AddFontFromFileTTF(fontPaths[1], 14.0f, &config, io.Fonts->GetGlyphRangesThai());
    if (!g_fontSmall) g_fontSmall = g_fontRegular;

    // Build fonts
    io.Fonts->Build();

    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
    CreateDirectoryIfNotExists(g_gamePath);

    g_manifestReady.store(false);
    g_manifestFetchSuccess.store(false);
    { std::lock_guard<std::mutex> lock(g_statusMsgMutex); g_manifestStatusMsg = "Initializing..."; }
    g_manifestProgress.store(0.0f);
    g_manifestError = false;
    std::thread fetchThread(FetchManifestThread);

    bool splashDone = false;
    std::chrono::steady_clock::time_point readyTime;

    bool done = false;
    while (!done) {
        MSG msg = {};
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg); DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        g_pulseTime += io.DeltaTime;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (!g_manifestReady.load()) {
            RenderSplash();
        }
        else {
            if (!splashDone) {
                if (readyTime.time_since_epoch().count() == 0) readyTime = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - readyTime).count();
                if (elapsed < 1500) RenderSplash();
                else splashDone = true;
            }
            if (splashDone) RenderMainUI();
        }

        const float cc[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, cc);
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);

        if (g_manifestReady.load() && g_manifestError && !g_manifestFetchSuccess.load()) {
            MessageBoxA(g_hwnd, "Cannot load manifest.\nPlease check your internet connection.",
                "Pinkmoon Launcher", MB_OK | MB_ICONERROR);
            done = true;
        }
    }

    g_serverQueryRunning.store(false);
    if (queryThread.joinable()) queryThread.join();
    if (fetchThread.joinable()) fetchThread.join();
    g_isMonitorRunning.store(false); g_isGameRunning.store(false);
    if (g_workerThread.joinable()) g_workerThread.join();
    if (g_monitorThread.joinable()) g_monitorThread.join();

    if (g_hJob) { CloseHandle(g_hJob); g_hJob = nullptr; }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(g_hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}