/**
 * =============================================================================
 *  TcpClientMain.cpp  ——  S-DES TCP 客户端（扩展要求 3.3.1：网络传输密文）
 * =============================================================================
 *  功能：
 *    将用户输入的明文字符串（支持中文）用 S-DES 加密后，
 *    通过 TCP 发送"密钥 + 十六进制密文"到服务端 127.0.0.1:8888，
 *    并打印服务端返回的解密结果，验证网络传输链路。
 *
 *  编译：g++ -std=c++17 -O2 -o SdesClient.exe TcpClientMain.cpp SDES.cpp -lws2_32
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

// 接收服务端的一行回复
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

    string KeyBits;
    uint32_t KeyValue = 0;
    cout << "请输入 10bit 密钥：";
    cin >> KeyBits;
    if (!ParseBitString(KeyBits, KeyValue) || KeyBits.size() != 10)
    {
        cout << "错误：密钥必须为 10 位二进制串！" << endl;
        return 1;
    }

    cin.ignore();
    cout << "请输入要加密发送的明文（支持中文）：";
    string PlainText;
    getline(cin, PlainText);

    // 本地加密，得到十六进制密文
    string HexCipher = EncryptString(PlainText, (uint16_t)KeyValue);
    cout << "\n本地加密得到密文（十六进制）：" << HexCipher << endl;

    // 初始化 Winsock 并连接服务端
    WSADATA WsaData;
    if (WSAStartup(MAKEWORD(2, 2), &WsaData) != 0)
    {
        cout << "Winsock 初始化失败！" << endl;
        return 1;
    }

    SOCKET ClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in ServerAddress{};
    ServerAddress.sin_family = AF_INET;
    ServerAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
    ServerAddress.sin_port = htons(8888);

    if (connect(ClientSocket, (sockaddr*)&ServerAddress, sizeof(ServerAddress)) == SOCKET_ERROR)
    {
        cout << "连接失败：请确认服务端已启动（127.0.0.1:8888）。" << endl;
        return 1;
    }

    // 发送报文：第 1 行密钥，第 2 行十六进制密文
    string Packet = KeyBits + "\n" + HexCipher + "\n";
    send(ClientSocket, Packet.c_str(), (int)Packet.size(), 0);
    cout << "已通过 TCP 发送密钥与密文。" << endl;

    // 接收服务端解密结果
    string Reply;
    if (RecvLine(ClientSocket, Reply))
    {
        cout << "服务端解密回复：" << Reply << endl;
    }

    closesocket(ClientSocket);
    WSACleanup();
    return 0;
}
