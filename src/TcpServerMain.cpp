/**
 * =============================================================================
 *  TcpServerMain.cpp  ——  S-DES TCP 服务端（扩展要求 3.3.1：网络传输密文）
 * =============================================================================
 *  功能：
 *    监听 127.0.0.1:8888，接收客户端发来的"十六进制密文 + 密钥"报文，
 *    解密后将明文回显给客户端，演示 S-DES 密文经 TCP Socket 的传输流程。
 *
 *  报文格式（文本行，'\n' 结尾）：
 *    第 1 行：10bit 密钥（二进制串，如 0111110101）
 *    第 2 行：十六进制密文
 *
 *  编译：g++ -std=c++17 -O2 -o SdesServer.exe TcpServerMain.cpp SDES.cpp -lws2_32
 * =============================================================================
 */
#include "SDES.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>

#pragma comment(lib, "ws2_32.lib")

using namespace std;
using namespace SDES;

// 从套接字读取一行（以 '\n' 结尾）
static bool RecvLine(SOCKET ClientSocket, string& LineOut)
{
    LineOut.clear();
    char CharBuf;
    while (true)
    {
        int RecvLen = recv(ClientSocket, &CharBuf, 1, 0);
        if (RecvLen <= 0) return false;
        if (CharBuf == '\n') break;
        if (CharBuf != '\r') LineOut += CharBuf;
    }
    return true;
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);

    // 初始化 Winsock
    WSADATA WsaData;
    if (WSAStartup(MAKEWORD(2, 2), &WsaData) != 0)
    {
        cout << "Winsock 初始化失败！" << endl;
        return 1;
    }

    // 创建监听套接字并绑定 127.0.0.1:8888
    SOCKET ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in ServerAddress{};
    ServerAddress.sin_family = AF_INET;
    ServerAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
    ServerAddress.sin_port = htons(8888);

    if (bind(ListenSocket, (sockaddr*)&ServerAddress, sizeof(ServerAddress)) == SOCKET_ERROR ||
        listen(ListenSocket, 1) == SOCKET_ERROR)
    {
        cout << "端口 8888 绑定失败（可能已被占用）。" << endl;
        return 1;
    }

    cout << "S-DES 服务端已启动，监听 127.0.0.1:8888，等待客户端连接……" << endl;

    while (true)
    {
        SOCKET ClientSocket = accept(ListenSocket, NULL, NULL);
        if (ClientSocket == INVALID_SOCKET) continue;
        cout << "\n客户端已连接。" << endl;

        string KeyBits, HexCipher;
        while (RecvLine(ClientSocket, KeyBits) && RecvLine(ClientSocket, HexCipher))
        {
            uint32_t KeyValue = 0;
            if (!ParseBitString(KeyBits, KeyValue) || KeyBits.size() != 10)
            {
                string ErrorReply = "ERROR: invalid key\n";
                send(ClientSocket, ErrorReply.c_str(), (int)ErrorReply.size(), 0);
                continue;
            }

            string PlainText = DecryptHexString(HexCipher, (uint16_t)KeyValue);
            cout << "收到密钥 " << KeyBits << "，密文 " << HexCipher << endl;
            cout << "解密结果：" << (PlainText.empty() ? "(解密失败)" : PlainText) << endl;

            // 回复解密结果（UTF-8 字节流）
            string Reply = "OK: " + PlainText + "\n";
            send(ClientSocket, Reply.c_str(), (int)Reply.size(), 0);
        }
        closesocket(ClientSocket);
        cout << "客户端已断开，继续等待下一个连接……" << endl;
    }

    WSACleanup();
    return 0;
}
