#define CPPHTTPLIB_THREAD_POOL_COUNT 2
#include "../lib/cpp-httplib/httplib.h"
#include "http_server.h"
#include <iostream>
#include <Avatar.h>

extern m5avatar::Avatar avatar; // main.cppで定義されているアバターへの参照

namespace avatar_http
{

    AvatarHttpServer::AvatarHttpServer() : running_(false), port_(8080) {}

    AvatarHttpServer::~AvatarHttpServer()
    {
        stop();
    }

    bool AvatarHttpServer::start(int port)
    {
        if (running_)
        {
            return false; // 既に実行中
        }

        port_ = port;
        running_ = true;
        server_thread_ = std::thread(&AvatarHttpServer::serverThread, this);
        return true;
    }

    void AvatarHttpServer::stop()
    {
        if (running_)
        {
            running_ = false;
            if (server_thread_.joinable())
            {
                server_thread_.join();
            }
        }
    }

    bool AvatarHttpServer::isRunning() const
    {
        return running_;
    }

    void AvatarHttpServer::serverThread()
    {
        httplib::Server svr;

        // アバターの状態を取得するエンドポイント
        svr.Get("/status", [](const httplib::Request &, httplib::Response &res)
                {
    float gazeV, gazeH;
    avatar.getGaze(&gazeV, &gazeH);
    
    // JSONレスポンスを構築
    std::string json = "{\n";
    json += "  \"mouth_open_ratio\": " + std::to_string(avatar.getMouthOpenRatio()) + ",\n";
    json += "  \"eye_open_ratio\": " + std::to_string(avatar.getEyeOpenRatio()) + ",\n";
    json += "  \"gaze_v\": " + std::to_string(gazeV) + ",\n";
    json += "  \"gaze_h\": " + std::to_string(gazeH) + ",\n";
    json += "  \"expression\": " + std::to_string(static_cast<int>(avatar.getExpression())) + "\n";
    json += "}";
    
    res.set_content(json, "application/json"); });

        // アバターを制御するエンドポイント
        svr.Get("/control", [](const httplib::Request &req, httplib::Response &res)
                {
    // 口の開き具合を設定
    if (req.has_param("mouth")) {
      float mouth = std::stof(req.get_param_value("mouth"));
      avatar.setMouthOpenRatio(mouth);
    }
    
    // 目の開き具合を設定
    if (req.has_param("eye")) {
      float eye = std::stof(req.get_param_value("eye"));
      avatar.setEyeOpenRatio(eye);
    }
    
    // 表情を設定
    if (req.has_param("expression")) {
      int exp = std::stoi(req.get_param_value("expression"));
      avatar.setExpression(static_cast<m5avatar::Expression>(exp));
    }
    
    res.set_content("OK", "text/plain"); });

        // サーバーの設定
        svr.set_read_timeout(5); // 5秒のタイムアウト
        svr.set_write_timeout(5);
        svr.set_keep_alive_max_count(8); // 最大キープアライブ接続数

        // サーバーを起動（非同期）
        if (!svr.bind_to_port("0.0.0.0", port_))
        {
            std::cerr << "Failed to bind to port " << port_ << std::endl;
            running_ = false;
            return;
        }

        std::cout << "HTTP server started on port " << port_ << std::endl;

        // サーバーを実行（running_がfalseになるまで）
        while (running_)
        {
            svr.listen_after_bind(); // リクエストを処理
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // スリープを追加
        }

        std::cout << "HTTP server stopped" << std::endl;
    }

} // namespace avatar_http
