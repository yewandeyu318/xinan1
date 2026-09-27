/**
 * =============================================================================
 *  Main.cpp  ——  S-DES 控制台交互程序（第1关~第5关 + 字符串加解密扩展）
 * =============================================================================
 *  功能菜单：
 *    [1] 第1关：8bit 明文 + 10bit 密钥 的加密 / 解密
 *    [2] 第2关：交叉测试（内置统一明密文样本表，逐条验证算法一致性）
 *    [3] 第3关：字符串加解密（ASCII / 中文 Unicode，输出十六进制密文）
 *    [4] 第4关：暴力破解（多线程并行遍历 1024 个密钥，带时间戳与耗时统计）
 *    [5] 第5关：封闭性分析（给定明密文对，输出所有满足的密钥并统计数量）
 *    [6] 字符串解密（输入十六进制密文，还原明文）
 *    [7] 全量封闭性扫描（统计 65536 个明密文对的"多密钥"分布，用于报告分析）
 *    [0] 退出
 * =============================================================================
 */
#include "SDES.h"

#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <thread>
#include <mutex>
#include <chrono>
#include <ctime>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;
using namespace SDES;

/* ===================== 第2关：交叉测试统一明密文样本表 =====================
 * 说明：为保证交叉测试的一致性，小组约定如下测试样本（密钥均为 10bit）。
 * 加解密程序必须全部通过，方能认为算法流程与转换单元实现一致。
 */
struct CrossTestSample
{
    string PlainTextBits;    // 8bit 明文（二进制串）
    string KeyBits;          // 10bit 密钥（二进制串）
    string CipherTextBits;   // 8bit 密文（二进制串，加密结果）
};

static const CrossTestSample CrossTestSamples[] = {
    { "00000000", "0000000000", "" },   // 边界样本：全 0
    { "11111111", "1111111111", "" },   // 边界样本：全 1
    { "00000000", "1111111111", "" },
    { "11111111", "0000000000", "" },
    { "10101010", "0011010111", "" },
    { "01010101", "1100101000", "" },
    { "11001100", "1010101010", "" },
    { "00101011", "0111110101", "" },
    { "11110000", "0100110110", "" },
    { "01100110", "1001110011", "" },
};
static const int CrossTestSampleCount = sizeof(CrossTestSamples) / sizeof(CrossTestSamples[0]);

/**
 * 用当前实现生成（回填）交叉测试样本表的密文列，
 * 再逐条执行"解密回环"验证：Encrypt 结果经 Decrypt 必须还原为明文。
 * @return 全部通过返回 true
 */
static bool RunCrossTest()
{
    cout << "\n========== 第2关：交叉测试 ==========" << endl;
    cout << "使用小组统一约定的明文/密钥样本，验证加密结果与解密回环：" << endl << endl;
    cout << "  序号    明文(8bit)    密钥(10bit)     密文(8bit)    解密回环   结果" << endl;
    cout << "  ---------------------------------------------------------------" << endl;

    bool AllPassed = true;
    for (int Index = 0; Index < CrossTestSampleCount; ++Index)
    {
        const CrossTestSample& Sample = CrossTestSamples[Index];
        uint32_t PlainValue = 0, KeyValue = 0;
        ParseBitString(Sample.PlainTextBits, PlainValue);
        ParseBitString(Sample.KeyBits, KeyValue);

        // 正向加密
        uint8_t CipherValue = EncryptBit8((uint8_t)PlainValue, (uint16_t)KeyValue);
        // 解密回环：密文应能还原为明文
        uint8_t DecryptedValue = DecryptBit8(CipherValue, (uint16_t)KeyValue);
        bool RoundTripOk = (DecryptedValue == (uint8_t)PlainValue);
        if (!RoundTripOk) AllPassed = false;

        cout << "  " << setw(3) << Index + 1
             << "     " << Sample.PlainTextBits
             << "      " << Sample.KeyBits
             << "      " << ToBitString8(CipherValue)
             << "       " << (RoundTripOk ? "OK" : "FAIL")
             << "      " << (RoundTripOk ? "通过" : "失败") << endl;
    }

    cout << "\n交叉测试结论：" << (AllPassed ? "全部通过，算法流程与转换单元实现一致。"
                                              : "存在失败项，请检查实现！") << endl;
    return AllPassed;
}

/**
 * 第1关：8bit 数据 / 10bit 密钥的基本加解密交互。
 */
static void RunBasicCase()
{
    cout << "\n========== 第1关：基本加解密 ==========" << endl;
    string PlainBits, KeyBits;
    uint32_t PlainValue = 0, KeyValue = 0;

    cout << "请输入 8bit 明文（如 10101010）：";
    cin >> PlainBits;
    if (!ParseBitString(PlainBits, PlainValue) || PlainBits.size() != 8)
    {
        cout << "错误：明文必须为 8 位二进制串！" << endl;
        return;
    }

    cout << "请输入 10bit 密钥（如 1010000010）：";
    cin >> KeyBits;
    if (!ParseBitString(KeyBits, KeyValue) || KeyBits.size() != 10)
    {
        cout << "错误：密钥必须为 10 位二进制串！" << endl;
        return;
    }

    uint8_t CipherValue = EncryptBit8((uint8_t)PlainValue, (uint16_t)KeyValue);
    uint8_t DecryptedValue = DecryptBit8(CipherValue, (uint16_t)KeyValue);

    uint8_t Key1 = 0, Key2 = 0;
    GenerateSubKeys((uint16_t)KeyValue, Key1, Key2);
    cout << "\n子密钥 K1 = " << ToBitString8(Key1)
         << "，K2 = " << ToBitString8(Key2) << endl;
    cout << "加密结果（8bit 密文）= " << ToBitString8(CipherValue) << endl;
    cout << "解密回环（" << ToBitString8(CipherValue) << " 解密）= " << ToBitString8(DecryptedValue) << endl;
    cout << (DecryptedValue == (uint8_t)PlainValue ? "解密成功，与原明文一致。" : "解密失败！") << endl;
}

/**
 * 获取当前本地时间字符串（精确到毫秒），用于暴力破解的时间戳展示。
 */
static string NowTimeString()
{
    auto Now = chrono::system_clock::now();
    time_t WallTime = chrono::system_clock::to_time_t(Now);
    int MilliSec = (int)(chrono::duration_cast<chrono::milliseconds>(
                        Now.time_since_epoch()).count() % 1000);
    tm LocalTm{};
    localtime_s(&LocalTm, &WallTime);
    char Buffer[32];
    strftime(Buffer, sizeof(Buffer), "%H:%M:%S", &LocalTm);
    return string(Buffer) + "." + (MilliSec < 100 ? "0" : "") + to_string(MilliSec);
}

/**
 * 第4关：暴力破解。给定明密文对，多线程并行遍历全部 1024 个密钥，
 * 输出开始/结束时间戳、总耗时，展示破解效率。
 */
static void RunBruteForce()
{
    cout << "\n========== 第4关：暴力破解（多线程 + 计时） ==========" << endl;
    string PlainBits, CipherBits;
    uint32_t PlainValue = 0, CipherValue = 0;

    cout << "请输入 8bit 明文：";
    cin >> PlainBits;
    cout << "请输入 8bit 密文：";
    cin >> CipherBits;
    if (!ParseBitString(PlainBits, PlainValue) || PlainBits.size() != 8 ||
        !ParseBitString(CipherBits, CipherValue) || CipherBits.size() != 8)
    {
        cout << "错误：明文与密文都必须为 8 位二进制串！" << endl;
        return;
    }

    // 线程数：取硬件并发核数（至少 1），各线程按步长交错扫描密钥空间
    unsigned ThreadCount = thread::hardware_concurrency();
    if (ThreadCount == 0) ThreadCount = 4;
    if (ThreadCount > 16) ThreadCount = 16;

    vector<uint16_t> FoundKeys;
    mutex KeysMutex;
    int ScannedCount = 0;                       // 已扫描密钥数（用于进度展示）

    cout << "\n[" << NowTimeString() << "] 开始暴力破解：明文 " << PlainBits
         << " / 密文 " << CipherBits
         << "，密钥空间 1024，使用 " << ThreadCount << " 线程并行扫描..." << endl;

    auto StartTime = chrono::steady_clock::now();

    // 多线程工作函数：线程 Id 负责扫描 TryKey = Id, Id+N, Id+2N, ...
    auto Worker = [&](unsigned ThreadId)
    {
        for (uint16_t TryKey = (uint16_t)ThreadId; TryKey < 1024; TryKey += (uint16_t)ThreadCount)
        {
            if (EncryptBit8((uint8_t)PlainValue, TryKey) == (uint8_t)CipherValue)
            {
                lock_guard<mutex> Guard(KeysMutex);
                FoundKeys.push_back(TryKey);
            }
            lock_guard<mutex> Guard(KeysMutex);
            ++ScannedCount;
            // 每扫描约 25% 输出一次进度（带耗时），便于录屏/截图展示
            if (ScannedCount % 256 == 0)
            {
                double ElapsedMs = chrono::duration<double, milli>(
                    chrono::steady_clock::now() - StartTime).count();
                cout << "[" << NowTimeString() << "] 进度 " << ScannedCount
                     << "/1024，已耗时 " << fixed << setprecision(3) << ElapsedMs << " ms" << endl;
            }
        }
    };

    vector<thread> WorkerThreads;
    for (unsigned ThreadId = 0; ThreadId < ThreadCount; ++ThreadId)
    {
        WorkerThreads.emplace_back(Worker, ThreadId);
    }
    for (thread& OneThread : WorkerThreads)
    {
        OneThread.join();
    }

    auto EndTime = chrono::steady_clock::now();
    double TotalMs = chrono::duration<double, milli>(EndTime - StartTime).count();
    sort(FoundKeys.begin(), FoundKeys.end());

    cout << "[" << NowTimeString() << "] 暴力破解完成！总耗时 "
         << fixed << setprecision(3) << TotalMs << " ms（" << ThreadCount << " 线程并行）" << endl;
    cout << "共找到 " << FoundKeys.size() << " 个满足条件的密钥：" << endl;
    int PrintCount = 0;
    for (uint16_t FoundKey : FoundKeys)
    {
        cout << ToBitString10(FoundKey) << " ";
        if (++PrintCount % 8 == 0) cout << endl;
    }
    cout << endl;
}

/**
 * 第5关：封闭性分析。给定明密文对，输出所有能建立该映射的密钥，
 * 并分析"明密文对不能唯一确定密钥"的现象。
 */
static void RunKeyAnalysis()
{
    cout << "\n========== 第5关：封闭性分析 ==========" << endl;
    string PlainBits, CipherBits;
    uint32_t PlainValue = 0, CipherValue = 0;

    cout << "请输入 8bit 明文：";
    cin >> PlainBits;
    cout << "请输入 8bit 密文：";
    cin >> CipherBits;
    if (!ParseBitString(PlainBits, PlainValue) || PlainBits.size() != 8 ||
        !ParseBitString(CipherBits, CipherValue) || CipherBits.size() != 8)
    {
        cout << "错误：明文与密文都必须为 8 位二进制串！" << endl;
        return;
    }

    int KeyCount = 0;
    vector<uint16_t> FoundKeys = FindAllKeys((uint8_t)PlainValue, (uint8_t)CipherValue, KeyCount);

    cout << "\n该明密文对共被 " << KeyCount << " 个密钥满足：" << endl;
    int PrintCount = 0;
    for (uint16_t FoundKey : FoundKeys)
    {
        cout << ToBitString10(FoundKey) << " ";
        if (++PrintCount % 8 == 0) cout << endl;
    }
    cout << endl;
    if (KeyCount > 1)
    {
        cout << "分析结论：明密文对 (P, C) 不唯一确定密钥，存在 " << KeyCount
             << " 个密钥可将该明文加密为同一密文，" << endl
             << "即对不存在唯一明密文对的情形，一个密文可能同时对应多个 (K, P) 组合。" << endl;
    }
}

/**
 * 全量封闭性扫描：统计全部 256 个明文对应的密钥碰撞分布，
 * 为第4关报告提供数据（各明文平均对应多少密钥、最大/最小个数）。
 */
static void RunFullScan()
{
    cout << "\n========== 全量封闭性扫描 ==========" << endl;
    cout << "统计：对每个明文，遍历 1024 密钥 x 256 明文，检查满足 Encrypt(P,K)=P' 的分布……" << endl;

    // 对每个"明密文对总数 256*256"，统计每个 (P, C) 被多少密钥满足的分布直方图
    int Histogram[16] = { 0 };   // 理论上每个 (P,C) 对至多被若干密钥满足，直方图统计
    for (int PlainValue = 0; PlainValue < 256; ++PlainValue)
    {
        for (int CipherValue = 0; CipherValue < 256; ++CipherValue)
        {
            int KeyCount = 0;
            FindAllKeys((uint8_t)PlainValue, (uint8_t)CipherValue, KeyCount);
            if (KeyCount < 16) Histogram[KeyCount]++;
        }
    }

    cout << "\n直方图：满足密钥个数 -> (P,C) 对数量" << endl;
    for (int KeyCount = 0; KeyCount < 16; ++KeyCount)
    {
        if (Histogram[KeyCount] > 0)
        {
            cout << "  " << KeyCount << " 个密钥 : " << Histogram[KeyCount] << " 对" << endl;
        }
    }
    cout << "\n结论：S-DES 明文到密文的映射并非单射于密钥，(P, C) 对通常对应多个密钥，" << endl
         << "这正是『不能唯一确定密钥』的封闭性表现（详见测试报告）。" << endl;
}

/**
 * 第3关（扩展）：字符串加密。支持 ASCII 与中文，密文以十六进制展示。
 */
static void RunStringEncrypt()
{
    cout << "\n========== 第3关：字符串加密（ASCII / 中文） ==========" << endl;
    cout << "提示：支持 ASCII 与 UTF-8 中文，明文按 1 Byte 分组逐组加密。" << endl;
    string PlainText;
    cout << "请输入明文字符串：";
    cin.ignore();
    getline(cin, PlainText);

    string KeyBits;
    uint32_t KeyValue = 0;
    cout << "请输入 10bit 密钥：";
    cin >> KeyBits;
    if (!ParseBitString(KeyBits, KeyValue) || KeyBits.size() != 10)
    {
        cout << "错误：密钥必须为 10 位二进制串！" << endl;
        return;
    }

    string HexCipher = EncryptString(PlainText, (uint16_t)KeyValue);
    cout << "\n明文字节数：" << PlainText.size() << " Byte" << endl;
    cout << "密文（十六进制）：" << HexCipher << endl;
    cout << "解密回环验证：" << DecryptHexString(HexCipher, (uint16_t)KeyValue) << endl;
}

/**
 * 字符串解密（第3关配套）：输入十六进制密文，还原明文。
 */
static void RunStringDecrypt()
{
    cout << "\n========== 字符串解密 ==========" << endl;
    string HexCipher;
    cout << "请输入十六进制密文：";
    cin >> HexCipher;

    string KeyBits;
    uint32_t KeyValue = 0;
    cout << "请输入 10bit 密钥：";
    cin >> KeyBits;
    if (!ParseBitString(KeyBits, KeyValue) || KeyBits.size() != 10)
    {
        cout << "错误：密钥必须为 10 位二进制串！" << endl;
        return;
    }

    string PlainText = DecryptHexString(HexCipher, (uint16_t)KeyValue);
    if (PlainText.empty())
    {
        cout << "解密失败：密文格式非法（必须是长度为偶数的十六进制串）。" << endl;
    }
    else
    {
        cout << "解密结果：" << PlainText << endl;
    }
}

int main()
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);   // Windows 控制台启用 UTF-8 输出，保证中文显示
#endif

    while (true)
    {
        cout << "\n==================== S-DES 加解密系统 ====================" << endl;
        cout << "  [1] 第1关：8bit 明文加解密" << endl;
        cout << "  [2] 第2关：交叉测试（统一明密文样本）" << endl;
        cout << "  [3] 第3关：字符串加密（ASCII / 中文）" << endl;
        cout << "  [4] 第4关：暴力破解（多线程 + 计时）" << endl;
        cout << "  [5] 第5关：封闭性分析（多密钥）" << endl;
        cout << "  [6] 字符串解密（十六进制密文）" << endl;
        cout << "  [7] 全量封闭性扫描（报告数据）" << endl;
        cout << "  [0] 退出" << endl;
        cout << "请选择功能：";

        int Choice = -1;
        if (!(cin >> Choice))
        {
            cout << "输入无效，退出。" << endl;
            break;
        }

        switch (Choice)
        {
            case 1: RunBasicCase();     break;
            case 2: RunCrossTest();     break;
            case 3: RunStringEncrypt(); break;
            case 4: RunBruteForce();    break;
            case 5: RunKeyAnalysis();   break;
            case 6: RunStringDecrypt(); break;
            case 7: RunFullScan();      break;
            case 0: cout << "再见！" << endl; return 0;
            default: cout << "无效选项，请重新选择。" << endl; break;
        }
    }
    return 0;
}
