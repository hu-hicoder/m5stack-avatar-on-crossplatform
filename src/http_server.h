#ifndef HTTP_SERVER_H_
#define HTTP_SERVER_H_

#include <string>
#include <thread>
#include <atomic>

namespace avatar_http
{

    class AvatarHttpServer
    {
    public:
        AvatarHttpServer();
        ~AvatarHttpServer();

        // サーバーの開始・停止
        bool start(int port = 8080);
        void stop();

        // サーバーの状態
        bool isRunning() const;

    private:
        std::thread server_thread_;
        std::atomic<bool> running_;
        int port_;

        // サーバースレッドのメイン関数
        void serverThread();
    };

} // namespace avatar_http

#endif // HTTP_SERVER_H_
