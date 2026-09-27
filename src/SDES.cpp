/**
 * =============================================================================
 *  SDES.cpp  ——  S-DES（简化 DES）算法核心实现
 * =============================================================================
 *  算法流程总览：
 *
 *    加密：  明文(8bit) --IP-->  L,R --fk(K1)-->  --SW交换-->  --fk(K2)-->  --IP^-1-->  密文(8bit)
 *    解密：  与加密结构完全相同，只是子密钥使用顺序颠倒（先 K2 后 K1）。
 *    密钥：  主密钥(10bit) --P10--> 循环左移1位 --P8--> K1
 *                              └--> 再循环左移2位 --P8--> K2
 *
 *  fk 函数内部：
 *    左半 L  ⊕  P4( S0( EP(R) ⊕ K , ... ) , S1( ... ) )   （右半 R 先经 EP 扩展）
 * =============================================================================
 */
#include "SDES.h"

#include <sstream>
#include <iomanip>

namespace SDES
{
    /* ---------------------------- 内部工具：通用置换 ---------------------------- */

    /**
     * 通用置换函数：对输入值按置换表重排比特。
     * @param Value       输入值
     * @param Table       置换表（表中数字为输入比特编号，编号 1 表示输入位宽的最高位）
     * @param TableLen    置换表长度（即输出位宽）
     * @param InputWidth  输入值的有效位宽（如 P8 表输入为 10bit，则为 10）
     * @return 置换后的值
     */
    static uint32_t Permute(uint32_t Value, const int* Table, int TableLen, int InputWidth)
    {
        uint32_t Result = 0;
        for (int Index = 0; Index < TableLen; ++Index)
        {
            // 表中第 Index 个元素指定：取输入比特编号 Table[Index]（从 1 开始，1 为输入最高位）
            int BitIndex = Table[Index];
            uint32_t Bit = (Value >> (InputWidth - BitIndex)) & 1u;
            Result |= (Bit << (TableLen - 1 - Index));
        }
        return Result;
    }

    /**
     * 10bit 循环左移：对 5bit 的左、右半部分别循环左移 Shift 位后拼接。
     */
    static uint16_t LeftShift10(uint16_t Value, int Shift)
    {
        uint16_t Left  = (Value >> 5) & 0x1F;             // 高 5 位
        uint16_t Right = Value & 0x1F;                    // 低 5 位
        Left  = ((Left  << Shift) | (Left  >> (5 - Shift))) & 0x1F;
        Right = ((Right << Shift) | (Right >> (5 - Shift))) & 0x1F;
        return (uint16_t)((Left << 5) | Right);
    }

    /* ---------------------------- fk 轮函数 ---------------------------- */

    /**
     * 轮函数 fk：对 8bit 数据的左 4bit 做如下运算
     *   L = L ⊕ P4( S0(左2bit) | S1(右2bit) )，其中 S 盒输入为 EP(R) ⊕ 子密钥
     * @param DataBit8  8bit 数据
     * @param SubKeyBit8 8bit 子密钥（K1 或 K2）
     * @return 运算后的 8bit 数据
     */
    static uint8_t FunctionFk(uint8_t DataBit8, uint8_t SubKeyBit8)
    {
        uint8_t Left  = (DataBit8 >> 4) & 0x0F;           // 左 4bit
        uint8_t Right = DataBit8 & 0x0F;                  // 右 4bit

        // 第 1 步：右 4bit 经 EP 盒扩展为 8bit，再与子密钥异或
        uint8_t Expanded = (uint8_t)Permute(Right, EPTable, 8, 4);
        uint8_t XorResult = Expanded ^ SubKeyBit8;

        // 第 2 步：高 4bit 进入 S0，低 4bit 进入 S1
        uint8_t LeftPart  = (XorResult >> 4) & 0x0F;
        uint8_t RightPart = XorResult & 0x0F;

        // S 盒行号 = 输入的 b0b3（第 1、4 位），列号 = b1b2（第 2、3 位）
        int Row0    = ((LeftPart >> 3) & 1) * 2 + (LeftPart & 1);
        int Col0    = ((LeftPart >> 2) & 1) * 2 + ((LeftPart >> 1) & 1);
        int Row1    = ((RightPart >> 3) & 1) * 2 + (RightPart & 1);
        int Col1    = ((RightPart >> 2) & 1) * 2 + ((RightPart >> 1) & 1);

        uint8_t S0Out = (uint8_t)S0Box[Row0][Col0];       // 2bit 输出
        uint8_t S1Out = (uint8_t)S1Box[Row1][Col1];       // 2bit 输出

        // 第 3 步：S0 输出拼接 S1 输出得到 4bit，再经 P4 置换
        uint8_t SOut = (uint8_t)((S0Out << 2) | S1Out);
        uint8_t P4Out = (uint8_t)Permute(SOut, P4Table, 4, 4);

        // 第 4 步：与左 4bit 异或
        return (uint8_t)((Left ^ P4Out) << 4 | Right);
    }

    /**
     * SW 交换函数：交换 8bit 数据的左、右 4bit。
     */
    static uint8_t SwitchSW(uint8_t DataBit8)
    {
        return (uint8_t)(((DataBit8 & 0x0F) << 4) | ((DataBit8 >> 4) & 0x0F));
    }

    /* ---------------------------- 子密钥扩展 ---------------------------- */

    void GenerateSubKeys(uint16_t KeyBit10, uint8_t& Key1Out, uint8_t& Key2Out)
    {
        // 第 1 步：P10 置换
        uint16_t AfterP10 = (uint16_t)Permute(KeyBit10, P10Table, 10, 10);

        // 第 2 步：循环左移 1 位后经 P8 压缩，得到子密钥 K1
        uint16_t Shift1 = LeftShift10(AfterP10, 1);
        Key1Out = (uint8_t)Permute(Shift1, P8Table, 8, 10);

        // 第 3 步：在左移 1 位的基础上再循环左移 2 位（共 3 位），经 P8 得到子密钥 K2
        uint16_t Shift3 = LeftShift10(Shift1, 2);
        Key2Out = (uint8_t)Permute(Shift3, P8Table, 8, 10);
    }

    /* ---------------------------- 核心加解密 ---------------------------- */

    uint8_t EncryptBit8(uint8_t PlainBit8, uint16_t KeyBit10)
    {
        uint8_t Key1 = 0, Key2 = 0;
        GenerateSubKeys(KeyBit10, Key1, Key2);

        uint8_t AfterIP = (uint8_t)Permute(PlainBit8, IPTable, 8, 8);
        uint8_t AfterFk1 = FunctionFk(AfterIP, Key1);
        uint8_t AfterSW  = SwitchSW(AfterFk1);
        uint8_t AfterFk2 = FunctionFk(AfterSW, Key2);
        return (uint8_t)Permute(AfterFk2, IPInverseTable, 8, 8);
    }

    uint8_t DecryptBit8(uint8_t CipherBit8, uint16_t KeyBit10)
    {
        // 解密与加密结构相同，仅子密钥顺序颠倒：先 K2 后 K1
        uint8_t Key1 = 0, Key2 = 0;
        GenerateSubKeys(KeyBit10, Key1, Key2);

        uint8_t AfterIP  = (uint8_t)Permute(CipherBit8, IPTable, 8, 8);
        uint8_t AfterFk1 = FunctionFk(AfterIP, Key2);
        uint8_t AfterSW  = SwitchSW(AfterFk1);
        uint8_t AfterFk2 = FunctionFk(AfterSW, Key1);
        return (uint8_t)Permute(AfterFk2, IPInverseTable, 8, 8);
    }

    /* ---------------------------- 字符串加解密 ---------------------------- */

    std::vector<uint8_t> EncryptBytes(const std::vector<uint8_t>& PlainBytes, uint16_t KeyBit10)
    {
        std::vector<uint8_t> CipherBytes;
        CipherBytes.reserve(PlainBytes.size());
        for (uint8_t Byte : PlainBytes)
        {
            CipherBytes.push_back(EncryptBit8(Byte, KeyBit10));   // 每 1 Byte 为一组独立加密
        }
        return CipherBytes;
    }

    std::vector<uint8_t> DecryptBytes(const std::vector<uint8_t>& CipherBytes, uint16_t KeyBit10)
    {
        std::vector<uint8_t> PlainBytes;
        PlainBytes.reserve(CipherBytes.size());
        for (uint8_t Byte : CipherBytes)
        {
            PlainBytes.push_back(DecryptBit8(Byte, KeyBit10));
        }
        return PlainBytes;
    }

    std::string EncryptString(const std::string& PlainText, uint16_t KeyBit10)
    {
        // 直接按字节处理：ASCII 为单字节，中文（UTF-8 为 3 字节、GBK 为 2 字节）
        // 也被拆分为字节逐组加密，因此天然支持 Unicode 字符串
        std::vector<uint8_t> PlainBytes(PlainText.begin(), PlainText.end());
        std::vector<uint8_t> CipherBytes = EncryptBytes(PlainBytes, KeyBit10);

        // 以十六进制展示，避免加密后字节不可打印、无法在界面/网络中传输
        std::ostringstream Output;
        Output << std::hex << std::uppercase << std::setfill('0');
        for (uint8_t Byte : CipherBytes)
        {
            Output << std::setw(2) << (int)Byte;
        }
        return Output.str();
    }

    static int HexCharToInt(char HexChar)
    {
        if (HexChar >= '0' && HexChar <= '9') return HexChar - '0';
        if (HexChar >= 'A' && HexChar <= 'F') return HexChar - 'A' + 10;
        if (HexChar >= 'a' && HexChar <= 'f') return HexChar - 'a' + 10;
        return -1;
    }

    std::string DecryptHexString(const std::string& HexCipherText, uint16_t KeyBit10)
    {
        // 密文必须是长度为偶数的十六进制串
        if (HexCipherText.size() % 2 != 0) return "";

        std::vector<uint8_t> CipherBytes;
        for (size_t Index = 0; Index + 1 < HexCipherText.size(); Index += 2)
        {
            int High = HexCharToInt(HexCipherText[Index]);
            int Low  = HexCharToInt(HexCipherText[Index + 1]);
            if (High < 0 || Low < 0) return "";               // 非法字符，解密失败
            CipherBytes.push_back((uint8_t)((High << 4) | Low));
        }

        std::vector<uint8_t> PlainBytes = DecryptBytes(CipherBytes, KeyBit10);
        return std::string(PlainBytes.begin(), PlainBytes.end());
    }

    /* ---------------------------- 暴力破解 / 封闭性分析 ---------------------------- */

    std::vector<uint16_t> BruteForceKeys(uint8_t PlainBit8, uint8_t CipherBit8)
    {
        std::vector<uint16_t> FoundKeys;
        for (uint16_t TryKey = 0; TryKey < 1024; ++TryKey)    // 10bit 密钥空间共 1024 个
        {
            if (EncryptBit8(PlainBit8, TryKey) == CipherBit8)
            {
                FoundKeys.push_back(TryKey);
            }
        }
        return FoundKeys;
    }

    std::vector<uint16_t> FindAllKeys(uint8_t PlainBit8, uint8_t CipherBit8, int& KeyCountOut)
    {
        // 与暴力破解同源：给定明密文对，遍历全部密钥，找出所有能建立该映射的密钥
        std::vector<uint16_t> FoundKeys = BruteForceKeys(PlainBit8, CipherBit8);
        KeyCountOut = (int)FoundKeys.size();
        return FoundKeys;
    }

    /* ---------------------------- 工具函数 ---------------------------- */

    std::string ToBitString8(uint8_t Value)
    {
        std::string Result(8, '0');
        for (int Index = 0; Index < 8; ++Index)
        {
            Result[7 - Index] = ((Value >> Index) & 1) ? '1' : '0';
        }
        return Result;
    }

    std::string ToBitString10(uint16_t Value)
    {
        std::string Result(10, '0');
        for (int Index = 0; Index < 10; ++Index)
        {
            Result[9 - Index] = ((Value >> Index) & 1) ? '1' : '0';
        }
        return Result;
    }

    bool ParseBitString(const std::string& BitString, uint32_t& ValueOut)
    {
        if (BitString.empty()) return false;
        uint32_t Result = 0;
        for (char BitChar : BitString)
        {
            if (BitChar != '0' && BitChar != '1') return false;
            Result = (Result << 1) | (uint32_t)(BitChar - '0');
        }
        ValueOut = Result;
        return true;
    }
}
