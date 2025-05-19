#define CPPHTTPLIB_THREAD_POOL_COUNT 2
#include "../lib/cpp-httplib/httplib.h"
#include "http_server.h"
#include <iostream>
#include <Avatar.h>
#include <sstream>
#include <map>

extern m5avatar::Avatar avatar; // main.cppで定義されているアバターへの参照

// 表情名と列挙型の変換用ヘルパー関数
std::string expressionToString(m5avatar::Expression exp)
{
    switch (exp)
    {
    case m5avatar::Expression::Default:
        return "default";
    case m5avatar::Expression::Cry:
        return "cry";
    case m5avatar::Expression::Sad:
        return "sad";
    case m5avatar::Expression::Happy:
        return "happy";
    case m5avatar::Expression::Angry:
        return "angry";
    case m5avatar::Expression::Gangimari:
        return "gangimari";
    default:
        return "unknown";
    }
}

m5avatar::Expression stringToExpression(const std::string &str)
{
    if (str == "default")
        return m5avatar::Expression::Default;
    if (str == "cry")
        return m5avatar::Expression::Cry;
    if (str == "sad")
        return m5avatar::Expression::Sad;
    if (str == "happy")
        return m5avatar::Expression::Happy;
    if (str == "angry")
        return m5avatar::Expression::Angry;
    if (str == "gangimari")
        return m5avatar::Expression::Gangimari;
    return m5avatar::Expression::Default; // デフォルト
}

// 簡易的なJSONパーサー
std::map<std::string, std::string> parseJson(const std::string &json)
{
    std::map<std::string, std::string> result;
    size_t pos = 0;
    while (pos < json.size())
    {
        // キーを探す
        size_t keyStart = json.find("\"", pos);
        if (keyStart == std::string::npos)
            break;
        size_t keyEnd = json.find("\"", keyStart + 1);
        if (keyEnd == std::string::npos)
            break;

        std::string key = json.substr(keyStart + 1, keyEnd - keyStart - 1);

        // コロンを探す
        size_t colon = json.find(":", keyEnd);
        if (colon == std::string::npos)
            break;

        // 値を探す
        size_t valueStart = json.find_first_not_of(" \t\n\r", colon + 1);
        if (valueStart == std::string::npos)
            break;

        std::string value;
        if (json[valueStart] == '\"')
        {
            // 文字列値
            size_t valueEnd = json.find("\"", valueStart + 1);
            if (valueEnd == std::string::npos)
                break;
            value = json.substr(valueStart + 1, valueEnd - valueStart - 1);
            pos = valueEnd + 1;
        }
        else
        {
            // 数値または他の値
            size_t valueEnd = json.find_first_of(",}", valueStart);
            if (valueEnd == std::string::npos)
                valueEnd = json.size();
            value = json.substr(valueStart, valueEnd - valueStart);
            // 末尾の空白を削除
            while (!value.empty() && isspace(value.back()))
                value.pop_back();
            pos = valueEnd;
        }

        result[key] = value;

        // 次のキーへ
        pos = json.find(",", pos);
        if (pos == std::string::npos)
            break;
        pos++;
    }

    return result;
}

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

        // 現在の表情を取得するGETエンドポイント
        svr.Get("/expression", [](const httplib::Request &, httplib::Response &res)
                {
            m5avatar::Expression exp = avatar.getExpression();
            std::string expStr = expressionToString(exp);
            
            // JSONレスポンスを構築
            std::string json = "{\n";
            json += "  \"currentExpression\": \"" + expStr + "\"\n";
            json += "}";
            
            res.set_content(json, "application/json"); });

        // 表情を設定するPOSTエンドポイント
        svr.Post("/expression", [](const httplib::Request &req, httplib::Response &res)
                 {
            // リクエストボディからJSONを解析
            try {
                auto jsonMap = parseJson(req.body);
                
                if (jsonMap.find("expression") != jsonMap.end()) {
                    std::string expStr = jsonMap["expression"];
                    m5avatar::Expression exp = stringToExpression(expStr);
                    
                    // アバターの表情を設定
                    avatar.setExpression(exp);
                    
                    // 成功レスポンス
                    res.set_content("{\"code\":0,\"message\":\"OK\"}", "application/json");
                } else {
                    // エラーレスポンス
                    res.set_content("{\"code\":1,\"message\":\"Missing 'expression' field\"}", "application/json");
                    res.status = 400;
                }
            } catch (const std::exception &e) {
                // JSONパース失敗などのエラーレスポンス
                res.set_content("{\"code\":2,\"message\":\"Invalid JSON format\"}", "application/json");
                res.status = 400;
            } });

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
            svr.listen_after_bind();                                     // リクエストを処理
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // スリープを追加
        }

        std::cout << "HTTP server stopped" << std::endl;
    }

} // namespace avatar_http
