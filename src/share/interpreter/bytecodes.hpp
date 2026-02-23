#ifndef SHARE_INTERPRETER_BYTECODES_HPP
#define SHARE_INTERPRETER_BYTECODES_HPP

// ============================================================================
// Mini JVM - 字节码定义
// 对应 HotSpot: src/hotspot/share/interpreter/bytecodes.hpp
//
// JVM 规范定义了 203 个标准字节码 (0x00 ~ 0xCA)。
// 我们先实现最核心的子集，足以运行 HelloWorld 级别的程序。
//
// 字节码分类：
//   1. 常量加载：iconst_N, ldc, bipush, sipush, aconst_null
//   2. 局部变量：iload, istore, aload, astore, 及其 _N 变体
//   3. 栈操作：  dup, pop, swap
//   4. 算术：    iadd, isub, imul, idiv, irem, ineg
//   5. 比较跳转：if_icmpxx, ifeq, ifne, goto
//   6. 方法调用：invokevirtual, invokespecial, invokestatic
//   7. 字段访问：getfield, putfield, getstatic, putstatic
//   8. 对象：    new, instanceof, checkcast
//   9. 返回：    ireturn, areturn, return
//  10. 其他：    nop, athrow
// ============================================================================

#include "utilities/globalDefinitions.hpp"

class Bytecodes {
public:
    enum Code {
        _illegal      = -1,

        // 常量
        _nop          = 0x00,
        _aconst_null  = 0x01,
        _iconst_m1    = 0x02,
        _iconst_0     = 0x03,
        _iconst_1     = 0x04,
        _iconst_2     = 0x05,
        _iconst_3     = 0x06,
        _iconst_4     = 0x07,
        _iconst_5     = 0x08,
        _lconst_0     = 0x09,
        _lconst_1     = 0x0A,
        _fconst_0     = 0x0B,
        _fconst_1     = 0x0C,
        _fconst_2     = 0x0D,
        _dconst_0     = 0x0E,
        _dconst_1     = 0x0F,
        _bipush       = 0x10,
        _sipush       = 0x11,
        _ldc          = 0x12,
        _ldc_w        = 0x13,
        _ldc2_w       = 0x14,

        // 加载
        _iload        = 0x15,
        _lload        = 0x16,
        _fload        = 0x17,
        _dload        = 0x18,
        _aload        = 0x19,
        _iload_0      = 0x1A,
        _iload_1      = 0x1B,
        _iload_2      = 0x1C,
        _iload_3      = 0x1D,
        _lload_0      = 0x1E,
        _lload_1      = 0x1F,
        _lload_2      = 0x20,
        _lload_3      = 0x21,
        _fload_0      = 0x22,
        _fload_1      = 0x23,
        _fload_2      = 0x24,
        _fload_3      = 0x25,
        _dload_0      = 0x26,
        _dload_1      = 0x27,
        _dload_2      = 0x28,
        _dload_3      = 0x29,
        _aload_0      = 0x2A,
        _aload_1      = 0x2B,
        _aload_2      = 0x2C,
        _aload_3      = 0x2D,

        // 数组加载
        _iaload       = 0x2E,
        _laload       = 0x2F,
        _faload       = 0x30,
        _daload       = 0x31,
        _aaload       = 0x32,
        _baload       = 0x33,
        _caload       = 0x34,
        _saload       = 0x35,

        // 存储
        _istore       = 0x36,
        _lstore       = 0x37,
        _fstore       = 0x38,
        _dstore       = 0x39,
        _astore       = 0x3A,
        _istore_0     = 0x3B,
        _istore_1     = 0x3C,
        _istore_2     = 0x3D,
        _istore_3     = 0x3E,
        _lstore_0     = 0x3F,
        _lstore_1     = 0x40,
        _lstore_2     = 0x41,
        _lstore_3     = 0x42,
        _fstore_0     = 0x43,
        _fstore_1     = 0x44,
        _fstore_2     = 0x45,
        _fstore_3     = 0x46,
        _dstore_0     = 0x47,
        _dstore_1     = 0x48,
        _dstore_2     = 0x49,
        _dstore_3     = 0x4A,
        _astore_0     = 0x4B,
        _astore_1     = 0x4C,
        _astore_2     = 0x4D,
        _astore_3     = 0x4E,

        // 数组存储
        _iastore      = 0x4F,
        _lastore      = 0x50,
        _fastore      = 0x51,
        _dastore      = 0x52,
        _aastore      = 0x53,
        _bastore      = 0x54,
        _castore      = 0x55,
        _sastore      = 0x56,

        // 栈操作
        _pop          = 0x57,
        _pop2         = 0x58,
        _dup          = 0x59,
        _dup_x1       = 0x5A,
        _dup_x2       = 0x5B,
        _dup2         = 0x5C,
        _dup2_x1      = 0x5D,
        _dup2_x2      = 0x5E,
        _swap         = 0x5F,

        // 整数算术
        _iadd         = 0x60,
        _ladd         = 0x61,
        _fadd         = 0x62,
        _dadd         = 0x63,
        _isub         = 0x64,
        _lsub         = 0x65,
        _fsub         = 0x66,
        _dsub         = 0x67,
        _imul         = 0x68,
        _lmul         = 0x69,
        _fmul         = 0x6A,
        _dmul         = 0x6B,
        _idiv         = 0x6C,
        _ldiv         = 0x6D,
        _fdiv         = 0x6E,
        _ddiv         = 0x6F,
        _irem         = 0x70,
        _lrem         = 0x71,
        _frem         = 0x72,
        _drem         = 0x73,
        _ineg         = 0x74,
        _lneg         = 0x75,
        _fneg         = 0x76,
        _dneg         = 0x77,

        // 位移
        _ishl         = 0x78,
        _lshl         = 0x79,
        _ishr         = 0x7A,
        _lshr         = 0x7B,
        _iushr        = 0x7C,
        _lushr        = 0x7D,

        // 位运算
        _iand         = 0x7E,
        _land         = 0x7F,
        _ior          = 0x80,
        _lor          = 0x81,
        _ixor         = 0x82,
        _lxor         = 0x83,

        // 自增
        _iinc         = 0x84,

        // 类型转换
        _i2l          = 0x85,
        _i2f          = 0x86,
        _i2d          = 0x87,
        _l2i          = 0x88,
        _l2f          = 0x89,
        _l2d          = 0x8A,
        _f2i          = 0x8B,
        _f2l          = 0x8C,
        _f2d          = 0x8D,
        _d2i          = 0x8E,
        _d2l          = 0x8F,
        _d2f          = 0x90,
        _i2b          = 0x91,
        _i2c          = 0x92,
        _i2s          = 0x93,

        // 比较
        _lcmp         = 0x94,
        _fcmpl        = 0x95,
        _fcmpg        = 0x96,
        _dcmpl        = 0x97,
        _dcmpg        = 0x98,

        // 条件跳转
        _ifeq         = 0x99,
        _ifne         = 0x9A,
        _iflt         = 0x9B,
        _ifge         = 0x9C,
        _ifgt         = 0x9D,
        _ifle         = 0x9E,
        _if_icmpeq    = 0x9F,
        _if_icmpne    = 0xA0,
        _if_icmplt    = 0xA1,
        _if_icmpge    = 0xA2,
        _if_icmpgt    = 0xA3,
        _if_icmple    = 0xA4,
        _if_acmpeq    = 0xA5,
        _if_acmpne    = 0xA6,

        // 无条件跳转
        _goto         = 0xA7,
        _jsr          = 0xA8,
        _ret          = 0xA9,

        // switch
        _tableswitch  = 0xAA,
        _lookupswitch = 0xAB,

        // 返回
        _ireturn      = 0xAC,
        _lreturn      = 0xAD,
        _freturn      = 0xAE,
        _dreturn      = 0xAF,
        _areturn      = 0xB0,
        _return       = 0xB1,

        // 字段访问
        _getstatic    = 0xB2,
        _putstatic    = 0xB3,
        _getfield     = 0xB4,
        _putfield     = 0xB5,

        // 方法调用
        _invokevirtual   = 0xB6,
        _invokespecial   = 0xB7,
        _invokestatic    = 0xB8,
        _invokeinterface = 0xB9,
        _invokedynamic   = 0xBA,

        // 对象
        _new          = 0xBB,
        _newarray     = 0xBC,
        _anewarray    = 0xBD,
        _arraylength  = 0xBE,
        _athrow       = 0xBF,
        _checkcast    = 0xC0,
        _instanceof   = 0xC1,

        // 同步
        _monitorenter = 0xC2,
        _monitorexit  = 0xC3,

        // 扩展
        _wide         = 0xC4,
        _multianewarray = 0xC5,
        _ifnull       = 0xC6,
        _ifnonnull    = 0xC7,
        _goto_w       = 0xC8,
        _jsr_w        = 0xC9,

        // 断点（调试）
        _breakpoint   = 0xCA,

        number_of_java_codes
    };

    // 字节码名称（调试用）
    static const char* name(Code code) {
        switch (code) {
            case _nop:          return "nop";
            case _aconst_null:  return "aconst_null";
            case _iconst_m1:    return "iconst_m1";
            case _iconst_0:     return "iconst_0";
            case _iconst_1:     return "iconst_1";
            case _iconst_2:     return "iconst_2";
            case _iconst_3:     return "iconst_3";
            case _iconst_4:     return "iconst_4";
            case _iconst_5:     return "iconst_5";
            case _bipush:       return "bipush";
            case _sipush:       return "sipush";
            case _ldc:          return "ldc";
            case _iload:        return "iload";
            case _iload_0:      return "iload_0";
            case _iload_1:      return "iload_1";
            case _iload_2:      return "iload_2";
            case _iload_3:      return "iload_3";
            case _aload:        return "aload";
            case _aload_0:      return "aload_0";
            case _aload_1:      return "aload_1";
            case _aload_2:      return "aload_2";
            case _aload_3:      return "aload_3";
            case _istore:       return "istore";
            case _istore_0:     return "istore_0";
            case _istore_1:     return "istore_1";
            case _istore_2:     return "istore_2";
            case _istore_3:     return "istore_3";
            case _astore:       return "astore";
            case _astore_0:     return "astore_0";
            case _astore_1:     return "astore_1";
            case _astore_2:     return "astore_2";
            case _astore_3:     return "astore_3";
            case _pop:          return "pop";
            case _dup:          return "dup";
            case _swap:         return "swap";
            case _iadd:         return "iadd";
            case _isub:         return "isub";
            case _imul:         return "imul";
            case _idiv:         return "idiv";
            case _irem:         return "irem";
            case _ineg:         return "ineg";
            case _iand:         return "iand";
            case _ior:          return "ior";
            case _ixor:         return "ixor";
            case _ishl:         return "ishl";
            case _ishr:         return "ishr";
            case _iushr:        return "iushr";
            case _i2l:          return "i2l";
            case _i2f:          return "i2f";
            case _i2d:          return "i2d";
            case _i2b:          return "i2b";
            case _i2c:          return "i2c";
            case _i2s:          return "i2s";
            case _ifeq:         return "ifeq";
            case _ifne:         return "ifne";
            case _iflt:         return "iflt";
            case _ifge:         return "ifge";
            case _ifgt:         return "ifgt";
            case _ifle:         return "ifle";
            case _if_icmpeq:    return "if_icmpeq";
            case _if_icmpne:    return "if_icmpne";
            case _if_icmplt:    return "if_icmplt";
            case _if_icmpge:    return "if_icmpge";
            case _if_icmpgt:    return "if_icmpgt";
            case _if_icmple:    return "if_icmple";
            case _goto:         return "goto";
            case _ireturn:      return "ireturn";
            case _lreturn:      return "lreturn";
            case _freturn:      return "freturn";
            case _dreturn:      return "dreturn";
            case _areturn:      return "areturn";
            case _return:       return "return";
            case _getstatic:    return "getstatic";
            case _putstatic:    return "putstatic";
            case _getfield:     return "getfield";
            case _putfield:     return "putfield";
            case _invokevirtual:   return "invokevirtual";
            case _invokespecial:   return "invokespecial";
            case _invokestatic:    return "invokestatic";
            case _new:          return "new";
            case _newarray:     return "newarray";
            case _arraylength:  return "arraylength";
            case _iaload:       return "iaload";
            case _iastore:      return "iastore";
            case _baload:       return "baload";
            case _bastore:      return "bastore";
            case _caload:       return "caload";
            case _castore:      return "castore";
            case _saload:       return "saload";
            case _sastore:      return "sastore";
            case _athrow:       return "athrow";
            case _checkcast:    return "checkcast";
            case _instanceof:   return "instanceof";
            default:            return "unknown";
        }
    }

    // 字节码长度（固定长度部分，不含变长指令如 tableswitch）
    static int length_for(Code code) {
        switch (code) {
            // 1 字节（无操作数）
            case _nop: case _aconst_null:
            case _iconst_m1: case _iconst_0: case _iconst_1:
            case _iconst_2: case _iconst_3: case _iconst_4: case _iconst_5:
            case _lconst_0: case _lconst_1:
            case _fconst_0: case _fconst_1: case _fconst_2:
            case _dconst_0: case _dconst_1:
            case _iload_0: case _iload_1: case _iload_2: case _iload_3:
            case _lload_0: case _lload_1: case _lload_2: case _lload_3:
            case _fload_0: case _fload_1: case _fload_2: case _fload_3:
            case _dload_0: case _dload_1: case _dload_2: case _dload_3:
            case _aload_0: case _aload_1: case _aload_2: case _aload_3:
            case _iaload: case _laload: case _faload: case _daload:
            case _aaload: case _baload: case _caload: case _saload:
            case _istore_0: case _istore_1: case _istore_2: case _istore_3:
            case _lstore_0: case _lstore_1: case _lstore_2: case _lstore_3:
            case _fstore_0: case _fstore_1: case _fstore_2: case _fstore_3:
            case _dstore_0: case _dstore_1: case _dstore_2: case _dstore_3:
            case _astore_0: case _astore_1: case _astore_2: case _astore_3:
            case _iastore: case _lastore: case _fastore: case _dastore:
            case _aastore: case _bastore: case _castore: case _sastore:
            case _pop: case _pop2:
            case _dup: case _dup_x1: case _dup_x2:
            case _dup2: case _dup2_x1: case _dup2_x2:
            case _swap:
            case _iadd: case _ladd: case _fadd: case _dadd:
            case _isub: case _lsub: case _fsub: case _dsub:
            case _imul: case _lmul: case _fmul: case _dmul:
            case _idiv: case _ldiv: case _fdiv: case _ddiv:
            case _irem: case _lrem: case _frem: case _drem:
            case _ineg: case _lneg: case _fneg: case _dneg:
            case _ishl: case _lshl: case _ishr: case _lshr:
            case _iushr: case _lushr:
            case _iand: case _land: case _ior: case _lor:
            case _ixor: case _lxor:
            case _i2l: case _i2f: case _i2d:
            case _l2i: case _l2f: case _l2d:
            case _f2i: case _f2l: case _f2d:
            case _d2i: case _d2l: case _d2f:
            case _i2b: case _i2c: case _i2s:
            case _lcmp: case _fcmpl: case _fcmpg: case _dcmpl: case _dcmpg:
            case _ireturn: case _lreturn: case _freturn: case _dreturn:
            case _areturn: case _return:
            case _arraylength:
            case _athrow:
            case _monitorenter: case _monitorexit:
                return 1;

            // 2 字节（1字节操作数）
            case _bipush:
            case _iload: case _lload: case _fload: case _dload: case _aload:
            case _istore: case _lstore: case _fstore: case _dstore: case _astore:
            case _ldc:
            case _newarray:
                return 2;

            // 3 字节（2字节操作数）
            case _sipush:
            case _ldc_w: case _ldc2_w:
            case _ifeq: case _ifne: case _iflt: case _ifge: case _ifgt: case _ifle:
            case _if_icmpeq: case _if_icmpne: case _if_icmplt:
            case _if_icmpge: case _if_icmpgt: case _if_icmple:
            case _if_acmpeq: case _if_acmpne:
            case _goto:
            case _jsr:
            case _getstatic: case _putstatic:
            case _getfield: case _putfield:
            case _invokevirtual: case _invokespecial: case _invokestatic:
            case _new:
            case _anewarray:
            case _checkcast: case _instanceof:
            case _ifnull: case _ifnonnull:
            case _iinc:  // iinc index const
                return 3;

            // 5 字节
            case _invokeinterface:
            case _invokedynamic:
            case _goto_w: case _jsr_w:
            case _multianewarray:
                return 5;

            // 变长
            case _tableswitch:
            case _lookupswitch:
            case _wide:
                return 0; // 变长，需要特殊处理

            default:
                return -1; // 未知
        }
    }
};

#endif // SHARE_INTERPRETER_BYTECODES_HPP
