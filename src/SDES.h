/**
 * =============================================================================
 *  SDES.h  ——  S-DES（Simplified DES，简化 DES）加密算法核心库
 * =============================================================================
 *  说明：
 *    1. 本库实现 S-DES 算法的全部流程，供控制台程序、GUI 程序、TCP 程序共用，
 *       保证"交叉测试"环节所有人使用相同的算法流程与转换单元（P-Box、S-Box）。
 *    2. 所有置换表（P-Box）与 S-Box 集中定义在本文件顶部，若与课程 PPT 略有
 *       差异，只需修改此处的常量即可，无需改动算法逻辑。
 *    3. 命名规范：类名、函数名、全局变量统一采用帕斯卡命名法（大驼峰），
 *       局部变量采用驼峰命名法，见名知意。
 *    4. 比特约定：bit0 表示最低位（二进制串最右侧），例如 8bit 明文 "10010111"
 *       中最左位的编号是 bit7。
 * =============================================================================
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace SDES
{
    /* ============================ 算法常量表（如与 PPT 有出入只改这里） ============================ */

    // P10 置换盒：10bit 密钥 -> 10bit（表中数字为输入比特编号，1 表示最左位）
    static const int P10Table[10] = { 3, 5, 2, 7, 4, 10, 1, 9, 8, 6 };

    // P8 置换（压缩置换）盒：10bit -> 8bit 子密钥
    static const int P8Table[8] = { 6, 3, 7, 4, 8, 5, 10, 9 };

    // IP 初始置换盒：8bit 明文 -> 8bit
    static const int IPTable[8] = { 2, 6, 3, 1, 4, 8, 5, 7 };

    // IP^-1 逆初始置换盒：8bit -> 8bit
    static const int IPInverseTable[8] = { 4, 1, 3, 5, 7, 2, 8, 6 };

    // EP 扩展置换盒（E/P）：4bit -> 8bit
    static const int EPTable[8] = { 4, 1, 2, 3, 2, 3, 4, 1 };

    // P4 置换盒：4bit -> 4bit
    static const int P4Table[4] = { 2, 4, 3, 1 };

    // S0 盒：4bit 输入（行 = b0b3，列 = b1b2）-> 2bit 输出
    static const int S0Box[4][4] = {
        { 1, 0, 3, 2 },
        { 3, 2, 1, 0 },
        { 0, 3, 2, 1 },
        { 3, 1, 3, 2 }
    };

    // S1 盒：4bit 输入（行 = b0b3，列 = b1b2）-> 2bit 输出
    static const int S1Box[4][4] = {
        { 0, 1, 2, 3 },
        { 2, 0, 1, 3 },
        { 3, 0, 1, 0 },
        { 2, 1, 0, 3 }
    };

    /* ============================ 子密钥扩展 ============================ */

    /**
     * 由 10bit 主密钥生成两个 8bit 子密钥 K1、K2。
     * 流程：P10 -> 循环左移1位 -> P8 得 K1；再循环左移2位 -> P8 得 K2。
     * @param KeyBit10  10bit 主密钥（0~1023）
     * @param Key1Out   输出子密钥 K1（0~255）
     * @param Key2Out   输出子密钥 K2（0~255）
     */
    void GenerateSubKeys(uint16_t KeyBit10, uint8_t& Key1Out, uint8_t& Key2Out);

    /* ============================ 核心加解密（第1关） ============================ */

    /**
     * S-DES 加密：8bit 明文 + 10bit 密钥 -> 8bit 密文。
     * 流程：IP -> fk(K1) -> SW 交换 -> fk(K2) -> IP^-1。
     */
    uint8_t EncryptBit8(uint8_t PlainBit8, uint16_t KeyBit10);

    /**
     * S-DES 解密：8bit 密文 + 10bit 密钥 -> 8bit 明文（与加密结构相同，仅子密钥顺序颠倒）。
     */
    uint8_t DecryptBit8(uint8_t CipherBit8, uint16_t KeyBit10);

    /* ============================ 字符串加解密（扩展要求 3.3.1） ============================ */

    /**
     * 字符串加密：将输入字符串按字节（1 Byte = 8bit）分组，逐字节 S-DES 加密。
     * 支持 ASCII 与 UTF-8 编码的任意字符（含中文），密钥为 10bit（0~1023）。
     * @return 十六进制密文字符串（每字节 2 个 hex 字符），保证中文等不可见字节可安全显示与传输
     */
    std::string EncryptString(const std::string& PlainText, uint16_t KeyBit10);

    /**
     * 字符串解密：输入十六进制密文字符串，解密还原为原始字符串。
     * @return 解密失败（密文格式非法）时返回空串
     */
    std::string DecryptHexString(const std::string& HexCipherText, uint16_t KeyBit10);

    /**
     * 原始字节序列逐字节加密（供 TCP 等需要原始字节的场合使用）。
     */
    std::vector<uint8_t> EncryptBytes(const std::vector<uint8_t>& PlainBytes, uint16_t KeyBit10);

    /**
     * 原始字节序列逐字节解密。
     */
    std::vector<uint8_t> DecryptBytes(const std::vector<uint8_t>& CipherBytes, uint16_t KeyBit10);

    /* ============================ 暴力破解（第3关） ============================ */

    /**
     * 暴力破解：遍历全部 1024 个可能的 10bit 密钥，
     * 找出所有满足 EncryptBit8(PlainBit8, Key) == CipherBit8 的密钥。
     * @return 满足条件的所有密钥（按从小到大排列）
     */
    std::vector<uint16_t> BruteForceKeys(uint8_t PlainBit8, uint8_t CipherBit8);

    /* ============================ 封闭性分析（第4关） ============================ */

    /**
     * 封闭性分析：对给定明密文对 (PlainBit8, CipherBit8)，
     * 返回所有能将该明文加密为该密文的密钥，并统计密钥数量，
     * 用于分析"S-DES 明密文对是否唯一确定密钥"的问题。
     * @param KeyCountOut 输出满足条件的密钥个数
     */
    std::vector<uint16_t> FindAllKeys(uint8_t PlainBit8, uint8_t CipherBit8, int& KeyCountOut);

    /* ============================ 工具函数 ============================ */

    /**
     * 将 8bit 数值格式化为二进制字符串（如 0b10010111 -> "10010111"）。
     */
    std::string ToBitString8(uint8_t Value);

    /**
     * 将 10bit 数值格式化为二进制字符串（如 1010000010）。
     */
    std::string ToBitString10(uint16_t Value);

    /**
     * 解析二进制字符串（仅允许字符 '0' 和 '1'），转换为数值。
     * @param ValueOut 输出解析结果
     * @return 字符串合法返回 true，否则返回 false
     */
    bool ParseBitString(const std::string& BitString, uint32_t& ValueOut);
}
