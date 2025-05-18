#ifdef _WIN32
// Windows固有のヘッダー
#include <Windows.h>          // Windows API
#include <mmdeviceapi.h>      // IMMDeviceEnumerator, IMMDevice など
#include <endpointvolume.h>   // IAudioMeterInformation
#include <Audioclient.h>      // WASAPI
#endif

#ifdef __linux__
// Linux固有のヘッダー
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#endif

#include <iostream>           // std::cerr などの標準入出力

#include <M5Unified.h>
#include <M5GFX.h>
#include <Avatar.h>
#include "custom-face/AsciiFace.h"
#include "custom-face/ChiikawaFace.h"
#include "custom-face/DanboFace.h"
#include "custom-face/GirlFace.h"
#include "custom-face/KenFace.h"
#include "custom-face/MaroFace.h"

using namespace m5avatar;
Avatar avatar;

// ----------------------------------------------------------
// デスクトップ上の音量取得関連

#ifdef _WIN32
// Windows用の音量取得実装
__CRT_UUID_DECL(IAudioMeterInformation, 0xC02216F6, 0x8C67, 0x4B5B, 0x9D, 0x00, 0xD0, 0x08, 0xE7, 0x3E, 0x00, 0x64);

// Define the IAudioMeterInformation interface
MIDL_INTERFACE("C02216F6-8C67-4B5B-9D00-D008E73E0064")
IAudioMeterInformation : public IUnknown
{
public:
    virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE GetPeakValue(
        /* [out] */ float *pfPeak) = 0;

    virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE GetMeteringChannelCount(
        /* [out] */ UINT *pnChannelCount) = 0;

    virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE GetChannelsPeakValues(
        /* [in] */ UINT32 u32ChannelCount,
        /* [size_is][out] */ float *afPeakValues) = 0;

    virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE QueryHardwareSupport(
        /* [out] */ DWORD *pdwHardwareSupportMask) = 0;
};

float getVolumeLevel() {
    CoInitialize(NULL);

    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDevice* pDevice = NULL;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pEnumerator));
    if (FAILED(hr)) {
        std::cerr << "IMMDeviceEnumeratorの作成に失敗しました。" << std::endl;
        return -1;
    }

    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
    if (FAILED(hr)) {
        std::cerr << "デフォルトオーディオエンドポイントの取得に失敗しました。" << std::endl;
        pEnumerator->Release();
        return -1;
    }

    IAudioMeterInformation* pMeterInfo = NULL;
    hr = pDevice->Activate(__uuidof(IAudioMeterInformation), CLSCTX_INPROC_SERVER, nullptr, (void**)&pMeterInfo);
    if (FAILED(hr)) {
        std::cerr << "IAudioMeterInformationの取得に失敗しました。" << std::endl;
        pDevice->Release();
        pEnumerator->Release();
        return -1;
    }

    float peakLevel = 0.0f;
    hr = pMeterInfo->GetPeakValue(&peakLevel);
    if (FAILED(hr)) {
        std::cerr << "音量レベルの取得に失敗しました。" << std::endl;
    }

    pMeterInfo->Release();
    pDevice->Release();
    pEnumerator->Release();
    CoUninitialize();

    return peakLevel;
}
#endif

#ifdef __linux__
// Linux用の音量取得実装（PulseAudio使用）
static pa_mainloop* s_mainloop = NULL;
static pa_mainloop_api* s_mainloop_api = NULL;
static pa_context* s_context = NULL;
static float s_volume_level = 0.0f;
static bool s_volume_ready = false;

// PulseAudioのコールバック関数
static void context_state_callback(pa_context* context, void* userdata) {
    pa_context_state_t state = pa_context_get_state(context);
    if (state == PA_CONTEXT_READY) {
        pa_operation* op = pa_context_get_sink_input_info_list(context, 
            [](pa_context* c, const pa_sink_input_info* i, int eol, void* userdata) {
                if (eol > 0 || !i) return;
                
                // 音量レベルを取得（0.0〜1.0の範囲に正規化）
                float volume = pa_cvolume_avg(&i->volume) / (float)PA_VOLUME_NORM;
                s_volume_level = volume;
                s_volume_ready = true;
            }, NULL);
        if (op) pa_operation_unref(op);
    }
}

// PulseAudioの初期化
static bool init_pulseaudio() {
    if (s_mainloop) return true; // 既に初期化済み
    
    s_mainloop = pa_mainloop_new();
    if (!s_mainloop) {
        std::cerr << "PulseAudio mainloopの作成に失敗しました。" << std::endl;
        return false;
    }
    
    s_mainloop_api = pa_mainloop_get_api(s_mainloop);
    if (!s_mainloop_api) {
        std::cerr << "PulseAudio mainloop APIの取得に失敗しました。" << std::endl;
        pa_mainloop_free(s_mainloop);
        s_mainloop = NULL;
        return false;
    }
    
    s_context = pa_context_new(s_mainloop_api, "M5Stack Avatar");
    if (!s_context) {
        std::cerr << "PulseAudio contextの作成に失敗しました。" << std::endl;
        pa_mainloop_free(s_mainloop);
        s_mainloop = NULL;
        return false;
    }
    
    pa_context_set_state_callback(s_context, context_state_callback, NULL);
    if (pa_context_connect(s_context, NULL, PA_CONTEXT_NOFLAGS, NULL) < 0) {
        std::cerr << "PulseAudioサーバーへの接続に失敗しました。" << std::endl;
        pa_context_unref(s_context);
        pa_mainloop_free(s_mainloop);
        s_context = NULL;
        s_mainloop = NULL;
        return false;
    }
    
    return true;
}

// PulseAudioの終了処理
static void cleanup_pulseaudio() {
    if (s_context) {
        pa_context_disconnect(s_context);
        pa_context_unref(s_context);
        s_context = NULL;
    }
    
    if (s_mainloop) {
        pa_mainloop_free(s_mainloop);
        s_mainloop = NULL;
    }
    
    s_mainloop_api = NULL;
}

float getVolumeLevel() {
    if (!init_pulseaudio()) {
        return -1.0f;
    }
    
    // メインループを短時間実行して音量情報を更新
    int ret;
    if (pa_mainloop_iterate(s_mainloop, 0, &ret) < 0) {
        std::cerr << "PulseAudio mainloopの実行に失敗しました。" << std::endl;
        return -1.0f;
    }
    
    if (!s_volume_ready) {
        // まだ音量情報が取得できていない場合
        return 0.0f;
    }
    
    return s_volume_level;
}
#endif

// ----------------------------------------------------------
void setup() {
	auto cfg = M5.config();		/// 設定用の構造体を取得。
	M5.begin(cfg);				/// M5Unifiedを使用する準備をする。

    // カラーセット変更用
    // デフォルト
    // COLOR_PRIMARY -> TFT_WHITE
    // COLOR_BACKGROUND -> TFT_BLACK
    ColorPalette *cp;
    cp = new ColorPalette();
    cp->set(COLOR_PRIMARY, TFT_WHITE);
    cp->set(COLOR_BACKGROUND, TFT_BLACK);
    // cp->set(COLOR_PRIMARY, TFT_BLACK);
    // cp->set(COLOR_BACKGROUND, TFT_WHITE);
    avatar.setColorPalette(*cp);

    // カスタムアバター（サンプル、未定義の場合デフォルトのｽﾀｯｸﾁｬﾝの顔）
    // avatar.setFace(new AsciiFace());
    // avatar.setFace(new ChiikawaFace());
    // avatar.setFace(new DanboFace());
    // avatar.setFace(new GirlFace());
    // avatar.setFace(new KenFace());
    // avatar.setFace(new MaroFace());

    avatar.init(8);
}

// ----------------------------------------------------------
using lgfx::v1::delay;
void loop() {
    M5.update();
    float volumeLevel = getVolumeLevel();
    if (volumeLevel >= 0) {
        avatar.setMouthOpenRatio(volumeLevel);
    }
    delay(100);
}

// プログラム終了時の処理
void cleanup() {
#ifdef __linux__
    cleanup_pulseaudio();
#endif
}
