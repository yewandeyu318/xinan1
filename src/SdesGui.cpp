/**
 * =============================================================================
 *  GuiMain.cpp  ——  S-DES 图形界面程序（Win32 API，无第三方依赖）
 * =============================================================================
 *  功能：
 *    1. 第1关：8bit 明文 / 10bit 密钥 的加密与解密（含解密回环显示）
 *    2. 字符串加解密：支持 ASCII 与中文（UTF-8），密文以十六进制展示
 *    3. 第3关：暴力破解——输入明密文对，列出全部满足的密钥
 *    4. 第4关：封闭性分析——展示该明密文对被多少密钥满足
 *
 *  编译：g++ -std=c++17 -O2 -o SdesGui.exe SdesGui.cpp SDES.cpp -lgdi32 -luser32 -mwindows
 * =============================================================================
 */
#include "SDES.h"

#include <windows.h>
#include <string>
#include <vector>

using namespace SDES;

/* ---------------- 控件 ID ---------------- */
#define IDC_PLAIN_BITS      1001    // 8bit 明文输入框
#define IDC_KEY_BITS        1002    // 10bit 密钥输入框
#define IDC_BTN_ENCRYPT     1003    // 加密按钮
#define IDC_BTN_DECRYPT     1004    // 解密按钮
#define IDC_RESULT_BITS     1005    // 8bit 结果输出框
#define IDC_PLAIN_STRING    1006    // 明文字符串输入框
#define IDC_BTN_ENC_STR     1007    // 字符串加密按钮
#define IDC_HEX_CIPHER      1008    // 十六进制密文输入框
#define IDC_BTN_DEC_STR     1009    // 字符串解密按钮
#define IDC_RESULT_STRING   1010    // 字符串结果输出框
#define IDC_PLAIN_BITS_BF   1011    // 暴力破解：明文输入框
#define IDC_CIPHER_BITS_BF  1012    // 暴力破解：密文输入框
#define IDC_BTN_BRUTE       1013    // 暴力破解按钮
#define IDC_KEY_LIST        1014    // 满足密钥列表输出框

static HWND g_hPlainBits, g_hKeyBits, g_hResultBits;
static HWND g_hPlainString, g_hHexCipher, g_hResultString;
static HWND g_hPlainBitsBf, g_hCipherBitsBf, g_hKeyList;

/* ---------------- 工具函数 ---------------- */

// 读取单行编辑框内容（宽字符 -> UTF-8 窄字符串，便于复用核心库）
static std::string GetEditText(HWND hEdit)
{
    wchar_t Buffer[1024];
    int Length = GetWindowTextW(hEdit, Buffer, 1024);
    int Utf8Len = WideCharToMultiByte(CP_UTF8, 0, Buffer, Length, NULL, 0, NULL, NULL);
    std::string Result(Utf8Len, 0);
    WideCharToMultiByte(CP_UTF8, 0, Buffer, Length, &Result[0], Utf8Len, NULL, NULL);
    return Result;
}

// 向编辑框写入文本（UTF-8 -> 宽字符）
static void SetEditText(HWND hEdit, const std::string& TextUtf8)
{
    int WideLen = MultiByteToWideChar(CP_UTF8, 0, TextUtf8.c_str(), -1, NULL, 0);
    std::wstring WideText(WideLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, TextUtf8.c_str(), -1, &WideText[0], WideLen);
    SetWindowTextW(hEdit, WideText.c_str());
}

// 创建静态标签控件
static HWND CreateLabel(HWND hParent, const wchar_t* Text, int X, int Y, int Width)
{
    return CreateWindowW(L"STATIC", Text, WS_CHILD | WS_VISIBLE | SS_LEFT,
                         X, Y, Width, 20, hParent, NULL, NULL, NULL);
}

// 创建编辑框控件
static HWND CreateEdit(HWND hParent, int Id, int X, int Y, int Width, bool MultiLine = false)
{
    DWORD Style = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL;
    if (MultiLine) Style |= ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL;
    return CreateWindowW(L"EDIT", L"", Style, X, Y, Width, MultiLine ? 110 : 24,
                         hParent, (HMENU)(INT_PTR)Id, NULL, NULL);
}

// 创建按钮控件
static HWND CreateButton(HWND hParent, const wchar_t* Text, int Id, int X, int Y, int Width)
{
    return CreateWindowW(L"BUTTON", Text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                         X, Y, Width, 28, hParent, (HMENU)(INT_PTR)Id, NULL, NULL);
}

// 创建分组框控件
static HWND CreateGroupBox(HWND hParent, const wchar_t* Text, int X, int Y, int Width, int Height)
{
    return CreateWindowW(L"BUTTON", Text, WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                         X, Y, Width, Height, hParent, NULL, NULL, NULL);
}

/**
 * 第1关：8bit 加密按钮响应
 */
static void OnEncryptBits()
{
    std::string PlainBits = GetEditText(g_hPlainBits);
    std::string KeyBits   = GetEditText(g_hKeyBits);
    uint32_t PlainValue = 0, KeyValue = 0;

    if (!ParseBitString(PlainBits, PlainValue) || PlainBits.size() != 8)
    {
        MessageBoxW(NULL, L"明文必须为 8 位二进制串（仅含 0 和 1）！", L"输入错误", MB_ICONERROR);
        return;
    }
    if (!ParseBitString(KeyBits, KeyValue) || KeyBits.size() != 10)
    {
        MessageBoxW(NULL, L"密钥必须为 10 位二进制串（仅含 0 和 1）！", L"输入错误", MB_ICONERROR);
        return;
    }

    uint8_t CipherValue = EncryptBit8((uint8_t)PlainValue, (uint16_t)KeyValue);
    SetEditText(g_hResultBits, ToBitString8(CipherValue));
}

/**
 * 第1关：8bit 解密按钮响应（解密回环演示）
 */
static void OnDecryptBits()
{
    std::string CipherBits = GetEditText(g_hResultBits);
    std::string KeyBits    = GetEditText(g_hKeyBits);
    uint32_t CipherValue = 0, KeyValue = 0;

    if (!ParseBitString(CipherBits, CipherValue) || CipherBits.size() != 8)
    {
        MessageBoxW(NULL, L"请先得到 8bit 密文，或直接输入 8 位二进制密文！", L"输入错误", MB_ICONERROR);
        return;
    }
    if (!ParseBitString(KeyBits, KeyValue) || KeyBits.size() != 10)
    {
        MessageBoxW(NULL, L"密钥必须为 10 位二进制串！", L"输入错误", MB_ICONERROR);
        return;
    }

    uint8_t PlainValue = DecryptBit8((uint8_t)CipherValue, (uint16_t)KeyValue);
    SetEditText(g_hPlainBits, ToBitString8(PlainValue));
}

/**
 * 字符串加密按钮响应
 */
static void OnEncryptString()
{
    std::string PlainText = GetEditText(g_hPlainString);
    std::string KeyBits   = GetEditText(g_hKeyBits);
    uint32_t KeyValue = 0;

    if (!ParseBitString(KeyBits, KeyValue) || KeyBits.size() != 10)
    {
        MessageBoxW(NULL, L"密钥必须为 10 位二进制串！", L"输入错误", MB_ICONERROR);
        return;
    }
    SetEditText(g_hHexCipher, EncryptString(PlainText, (uint16_t)KeyValue));
}

/**
 * 字符串解密按钮响应
 */
static void OnDecryptString()
{
    std::string HexCipher = GetEditText(g_hHexCipher);
    std::string KeyBits   = GetEditText(g_hKeyBits);
    uint32_t KeyValue = 0;

    if (!ParseBitString(KeyBits, KeyValue) || KeyBits.size() != 10)
    {
        MessageBoxW(NULL, L"密钥必须为 10 位二进制串！", L"输入错误", MB_ICONERROR);
        return;
    }
    std::string PlainText = DecryptHexString(HexCipher, (uint16_t)KeyValue);
    SetEditText(g_hResultString, PlainText.empty() ? std::string("(解密失败：密文格式非法)") : PlainText);
}

/**
 * 暴力破解 / 封闭性分析按钮响应（第3关 + 第4关）
 */
static void OnBruteForce()
{
    std::string PlainBits  = GetEditText(g_hPlainBitsBf);
    std::string CipherBits = GetEditText(g_hCipherBitsBf);
    uint32_t PlainValue = 0, CipherValue = 0;

    if (!ParseBitString(PlainBits, PlainValue) || PlainBits.size() != 8 ||
        !ParseBitString(CipherBits, CipherValue) || CipherBits.size() != 8)
    {
        MessageBoxW(NULL, L"明文与密文都必须为 8 位二进制串！", L"输入错误", MB_ICONERROR);
        return;
    }

    int KeyCount = 0;
    std::vector<uint16_t> FoundKeys = FindAllKeys((uint8_t)PlainValue, (uint8_t)CipherValue, KeyCount);

    std::string Report = "满足该明密文对的密钥共 " + std::to_string(KeyCount) + " 个（第5关：若大于 1，"
                         "说明明密文对不能唯一确定密钥）：\r\n";
    int PrintCount = 0;
    for (uint16_t FoundKey : FoundKeys)
    {
        Report += ToBitString10(FoundKey) + "  ";
        if (++PrintCount % 6 == 0) Report += "\r\n";
    }
    SetEditText(g_hKeyList, Report);
}

/**
 * 主窗口消息处理
 */
LRESULT CALLBACK WindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    switch (Msg)
    {
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDC_BTN_ENCRYPT: OnEncryptBits();   break;
                case IDC_BTN_DECRYPT: OnDecryptBits();   break;
                case IDC_BTN_ENC_STR: OnEncryptString(); break;
                case IDC_BTN_DEC_STR: OnDecryptString(); break;
                case IDC_BTN_BRUTE:   OnBruteForce();    break;
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcW(hWnd, Msg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int CmdShow)
{
    const wchar_t WindowClassName[] = L"SdesGuiWindow";
    WNDCLASSW WindowClass = {};
    WindowClass.lpfnWndProc   = WindowProc;
    WindowClass.hInstance     = hInstance;
    WindowClass.hCursor       = LoadCursor(NULL, IDC_ARROW);
    WindowClass.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    WindowClass.lpszClassName = WindowClassName;
    RegisterClassW(&WindowClass);

    HWND hWnd = CreateWindowW(WindowClassName, L"S-DES 加解密系统",
                              WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX),
                              CW_USEDEFAULT, CW_USEDEFAULT, 640, 640,
                              NULL, NULL, hInstance, NULL);

    /* ---------- 第1关分组：8bit 加解密 ---------- */
    CreateGroupBox(hWnd, L"第1关：8bit 数据加解密", 12, 10, 600, 130);
    CreateLabel(hWnd, L"8bit 明文：",        24, 40, 80);
    g_hPlainBits = CreateEdit(hWnd, IDC_PLAIN_BITS, 110, 38, 160);
    CreateLabel(hWnd, L"10bit 密钥：",       300, 40, 80);
    g_hKeyBits   = CreateEdit(hWnd, IDC_KEY_BITS, 390, 38, 160);
    CreateLabel(hWnd, L"8bit 密文：",        24, 75, 80);
    g_hResultBits = CreateEdit(hWnd, IDC_RESULT_BITS, 110, 73, 160);
    CreateButton(hWnd, L"加密", IDC_BTN_ENCRYPT, 300, 70, 100);
    CreateButton(hWnd, L"解密回环", IDC_BTN_DECRYPT, 420, 70, 100);

    /* ---------- 字符串加解密分组 ---------- */
    CreateGroupBox(hWnd, L"字符串加解密（ASCII / 中文，密文十六进制展示）", 12, 150, 600, 230);
    CreateLabel(hWnd, L"明文字符串：", 24, 180, 90);
    g_hPlainString = CreateEdit(hWnd, IDC_PLAIN_STRING, 120, 178, 470);
    CreateLabel(hWnd, L"十六进制密文：", 24, 215, 90);
    g_hHexCipher = CreateEdit(hWnd, IDC_HEX_CIPHER, 120, 213, 470);
    CreateLabel(hWnd, L"解密结果：", 24, 250, 90);
    g_hResultString = CreateEdit(hWnd, IDC_RESULT_STRING, 120, 248, 470);
    CreateButton(hWnd, L"加密字符串", IDC_BTN_ENC_STR, 120, 285, 150);
    CreateButton(hWnd, L"解密密文", IDC_BTN_DEC_STR, 320, 285, 150);

    /* ---------- 第3关/第4关分组：暴力破解与封闭性分析 ---------- */
    CreateGroupBox(hWnd, L"第4关 暴力破解 / 第5关 封闭性分析", 12, 390, 600, 210);
    CreateLabel(hWnd, L"明文(8bit)：", 24, 420, 80);
    g_hPlainBitsBf = CreateEdit(hWnd, IDC_PLAIN_BITS_BF, 110, 418, 160);
    CreateLabel(hWnd, L"密文(8bit)：", 300, 420, 80);
    g_hCipherBitsBf = CreateEdit(hWnd, IDC_CIPHER_BITS_BF, 390, 418, 160);
    CreateButton(hWnd, L"遍历 1024 密钥查找", IDC_BTN_BRUTE, 110, 450, 200);
    g_hKeyList = CreateEdit(hWnd, IDC_KEY_LIST, 24, 488, 570, true);
    SetEditText(g_hKeyList, "(点击上方按钮，结果将显示在这里)");

    ShowWindow(hWnd, CmdShow);
    UpdateWindow(hWnd);

    MSG Message;
    while (GetMessageW(&Message, NULL, 0, 0) > 0)
    {
        TranslateMessage(&Message);
        DispatchMessageW(&Message);
    }
    return (int)Message.wParam;
}
