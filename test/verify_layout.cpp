/*
 * verify_layout.cpp
 *
 * 验证 my_jvm 数据结构的内存布局（sizeof + 字段偏移）
 * 与 OpenJDK 11 真实值对比
 *
 * 策略：用 getter 返回字段地址，或用 char* 强转计算偏移
 * 因为字段是 private，不能直接用 offsetof
 */

#include <iostream>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "utilities/globalDefinitions.hpp"
#include "oops/markOop.hpp"
#include "oops/metadata.hpp"
#include "oops/oop.hpp"
#include "oops/klass.hpp"
#include "oops/method.hpp"
#include "oops/constMethod.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/array.hpp"

// ========== 辅助宏 ==========

#define PRINT_SIZEOF(T) \
    std::cout << "  sizeof(" #T ") = " << sizeof(T) << std::endl

// 通过 getter 返回字段引用来计算偏移
// 用法：FIELD_OFFSET(obj, obj.mark())  → 计算 _mark 字段相对于 obj 的偏移
#define FIELD_OFFSET_BY_ADDR(obj, field_addr) \
    ((size_t)((char*)(field_addr) - (char*)(&(obj))))

// ========== 验证函数 ==========

void verify_markOopDesc() {
    std::cout << "\n[markOopDesc / markOop]" << std::endl;
    std::cout << "  sizeof(markOop)  = " << sizeof(markOop)
              << "  (pointer, = sizeof(void*) = " << sizeof(void*) << ")" << std::endl;

    // 验证锁状态编码常量
    std::cout << "  locked_value        = " << (int)markOopDesc::locked_value << std::endl;
    std::cout << "  unlocked_value      = " << (int)markOopDesc::unlocked_value << std::endl;
    std::cout << "  monitor_value       = " << (int)markOopDesc::monitor_value << std::endl;
    std::cout << "  marked_value        = " << (int)markOopDesc::marked_value << std::endl;
    std::cout << "  biased_lock_pattern = " << (int)markOopDesc::biased_lock_pattern << std::endl;

    std::cout << "  --- Expected (OpenJDK 11) ---" << std::endl;
    std::cout << "  sizeof(markOop) = 8  (pointer)" << std::endl;
    std::cout << "  locked_value=0, unlocked_value=1, monitor_value=2, marked_value=3, biased=5" << std::endl;
}

void verify_oopDesc() {
    std::cout << "\n[oopDesc]" << std::endl;
    PRINT_SIZEOF(oopDesc);

    // 通过 mark_addr_raw() 获取 _mark 的地址
    oopDesc obj;
    size_t mark_off   = (size_t)((char*)obj.mark_addr_raw() - (char*)&obj);
    // _metadata 紧跟在 _mark 后面，通过 klass() 的地址来推算
    // klass() 返回 _metadata._klass 的值，但我们需要 _metadata 的地址
    // 用 set_klass 后读取 klass() 的地址
    Klass* dummy_klass = (Klass*)0xDEADBEEF;
    obj.set_klass(dummy_klass);
    // 扫描 obj 内存找到 dummy_klass 的位置
    size_t metadata_off = 0;
    for (size_t i = 0; i < sizeof(oopDesc); i++) {
        uintptr_t val = 0;
        if (i + sizeof(void*) <= sizeof(oopDesc)) {
            memcpy(&val, (char*)&obj + i, sizeof(void*));
            if (val == (uintptr_t)dummy_klass) {
                metadata_off = i;
                break;
            }
        }
    }

    std::cout << "  offsetof(_mark)     = " << mark_off << std::endl;
    std::cout << "  offsetof(_metadata) = " << metadata_off << std::endl;

    std::cout << "  --- Expected (OpenJDK 11, 64-bit, no compressed oops) ---" << std::endl;
    std::cout << "  sizeof(oopDesc)     = 16  (mark=8 + klass*=8)" << std::endl;
    std::cout << "  offsetof(_mark)     = 0" << std::endl;
    std::cout << "  offsetof(_metadata) = 8" << std::endl;
}

void verify_Metadata() {
    std::cout << "\n[Metadata]" << std::endl;
    PRINT_SIZEOF(Metadata);
    std::cout << "  --- Expected (OpenJDK 11 slowdebug) ---" << std::endl;
    std::cout << "  sizeof(Metadata) = 16  (vtable + _valid:int + padding)" << std::endl;
}

void verify_Klass() {
    std::cout << "\n[Klass]" << std::endl;
    PRINT_SIZEOF(Klass);

    // 通过 set/get 找字段偏移
    Klass k;

    // _layout_helper: 通过 set_layout_helper(0xABCD) 后扫描内存
    k.set_layout_helper(0x12345678);
    size_t lh_off = 0;
    for (size_t i = 0; i + 4 <= sizeof(Klass); i++) {
        jint val = 0;
        memcpy(&val, (char*)&k + i, 4);
        if (val == 0x12345678) { lh_off = i; break; }
    }

    // _super: 通过 set_super 找
    Klass* dummy = (Klass*)0xCAFEBABE00000000ULL;
    k.set_super(dummy);
    size_t super_off = 0;
    for (size_t i = 0; i + 8 <= sizeof(Klass); i++) {
        uintptr_t val = 0;
        memcpy(&val, (char*)&k + i, 8);
        if (val == (uintptr_t)dummy) { super_off = i; break; }
    }

    std::cout << "  offsetof(_layout_helper) = " << lh_off << std::endl;
    std::cout << "  offsetof(_super)         = " << super_off << std::endl;

    std::cout << "  --- Expected (OpenJDK 11 slowdebug, 64-bit) ---" << std::endl;
    std::cout << "  sizeof(Klass)            = 208" << std::endl;
    std::cout << "  offsetof(_layout_helper) = 12  (after vtable + _valid + padding)" << std::endl;
    std::cout << "  offsetof(_super)         = 120" << std::endl;
}

void verify_Method() {
    std::cout << "\n[Method]" << std::endl;
    PRINT_SIZEOF(Method);

    Method m;
    ConstMethod* dummy_cm = (ConstMethod*)0xDEAD000000000000ULL;
    m.set_constMethod(dummy_cm);

    size_t cm_off = 0;
    for (size_t i = 0; i + 8 <= sizeof(Method); i++) {
        uintptr_t val = 0;
        memcpy(&val, (char*)&m + i, 8);
        if (val == (uintptr_t)dummy_cm) { cm_off = i; break; }
    }

    m.set_access_flags(0xBEEF);
    size_t af_off = 0;
    for (size_t i = 0; i + 2 <= sizeof(Method); i++) {
        u2 val = 0;
        memcpy(&val, (char*)&m + i, 2);
        if (val == 0xBEEF) { af_off = i; break; }
    }

    m.set_vtable_index(0x7ABCDEF0);
    size_t vi_off = 0;
    for (size_t i = 0; i + 4 <= sizeof(Method); i++) {
        int val = 0;
        memcpy(&val, (char*)&m + i, 4);
        if (val == 0x7ABCDEF0) { vi_off = i; break; }
    }

    std::cout << "  offsetof(_constMethod)  = " << cm_off << std::endl;
    std::cout << "  offsetof(_access_flags) = " << af_off << std::endl;
    std::cout << "  offsetof(_vtable_index) = " << vi_off << std::endl;

    std::cout << "  --- Expected (OpenJDK 11 slowdebug, 64-bit) ---" << std::endl;
    std::cout << "  sizeof(Method)          = 104" << std::endl;
    std::cout << "  offsetof(_constMethod)  = 16  (after Metadata 16 bytes)" << std::endl;
    std::cout << "  offsetof(_access_flags) = 40  (after _constMethod, _method_data, _method_counters)" << std::endl;
    std::cout << "  offsetof(_vtable_index) = 44" << std::endl;
}

void verify_ConstMethod() {
    std::cout << "\n[ConstMethod]" << std::endl;
    PRINT_SIZEOF(ConstMethod);

    ConstMethod cm;
    cm.set_name_index(0xABCD);
    size_t ni_off = 0;
    for (size_t i = 0; i + 2 <= sizeof(ConstMethod); i++) {
        u2 val = 0;
        memcpy(&val, (char*)&cm + i, 2);
        if (val == 0xABCD) { ni_off = i; break; }
    }

    cm.set_max_stack(0x1234);
    size_t ms_off = 0;
    for (size_t i = 0; i + 2 <= sizeof(ConstMethod); i++) {
        u2 val = 0;
        memcpy(&val, (char*)&cm + i, 2);
        if (val == 0x1234) { ms_off = i; break; }
    }

    std::cout << "  offsetof(_name_index) = " << ni_off << std::endl;
    std::cout << "  offsetof(_max_stack)  = " << ms_off << std::endl;

    std::cout << "  --- Expected (OpenJDK 11 slowdebug, 64-bit) ---" << std::endl;
    std::cout << "  sizeof(ConstMethod)   = 56" << std::endl;
    std::cout << "  offsetof(_name_index) = 42" << std::endl;
    std::cout << "  CRITICAL: In OpenJDK, bytecode is stored INLINE after ConstMethod" << std::endl;
    std::cout << "            (no _code pointer!), accessed via: (u1*)(this + 1)" << std::endl;
    std::cout << "  NOTE: ConstMethod has NO vtable (no virtual functions)" << std::endl;
}

void verify_InstanceKlass() {
    std::cout << "\n[InstanceKlass]" << std::endl;
    PRINT_SIZEOF(InstanceKlass);

    InstanceKlass ik;

    // _vtable_len
    ik.set_vtable_length(0x7EADBEEF);
    size_t vl_off = 0;
    for (size_t i = 0; i + 4 <= sizeof(InstanceKlass); i++) {
        int val = 0;
        memcpy(&val, (char*)&ik + i, 4);
        if (val == 0x7EADBEEF) { vl_off = i; break; }
    }

    // _methods
    Array<Method*>* dummy_arr = (Array<Method*>*)0xFEEDFACE00000000ULL;
    ik.set_methods(dummy_arr);
    size_t methods_off = 0;
    for (size_t i = 0; i + 8 <= sizeof(InstanceKlass); i++) {
        uintptr_t val = 0;
        memcpy(&val, (char*)&ik + i, 8);
        if (val == (uintptr_t)dummy_arr) { methods_off = i; break; }
    }

    // _init_state
    ik.set_init_state(0x42);
    size_t is_off = 0;
    for (size_t i = 0; i < sizeof(InstanceKlass); i++) {
        u1 val = *((u1*)((char*)&ik + i));
        if (val == 0x42) { is_off = i; break; }
    }

    std::cout << "  offsetof(_vtable_len) = " << vl_off << std::endl;
    std::cout << "  offsetof(_methods)    = " << methods_off << std::endl;
    std::cout << "  offsetof(_init_state) = " << is_off << std::endl;

    std::cout << "  --- Expected (OpenJDK 11 slowdebug, 64-bit) ---" << std::endl;
    std::cout << "  sizeof(InstanceKlass)  = 472" << std::endl;
    std::cout << "  offsetof(_vtable_len)  = 196  (Klass internal field)" << std::endl;
    std::cout << "  offsetof(_methods)     = 416" << std::endl;
    std::cout << "  offsetof(_init_state)  = 394" << std::endl;
}

void verify_OopMapBlock() {
    std::cout << "\n[OopMapBlock]" << std::endl;
    PRINT_SIZEOF(OopMapBlock);
    // OopMapBlock 是 struct，字段是 public
    std::cout << "  offsetof(_offset) = " << offsetof(OopMapBlock, _offset) << std::endl;
    std::cout << "  offsetof(_count)  = " << offsetof(OopMapBlock, _count) << std::endl;

    std::cout << "  --- Expected (OpenJDK 11) ---" << std::endl;
    std::cout << "  sizeof(OopMapBlock) = 8  (_offset:int=4 + _count:uint=4)" << std::endl;
    std::cout << "  offsetof(_offset)   = 0" << std::endl;
    std::cout << "  offsetof(_count)    = 4" << std::endl;
}

// ========== main ==========

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  my_jvm Layout Verification" << std::endl;
    std::cout << "  Platform: " << (sizeof(void*) == 8 ? "64-bit" : "32-bit") << std::endl;
    std::cout << "  sizeof(void*) = " << sizeof(void*) << std::endl;
    std::cout << "========================================" << std::endl;

    verify_markOopDesc();
    verify_oopDesc();
    verify_Metadata();
    verify_Klass();
    verify_Method();
    verify_ConstMethod();
    verify_InstanceKlass();
    verify_OopMapBlock();

    std::cout << "\n========================================" << std::endl;
    std::cout << "  Done. Compare with OpenJDK GDB output." << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
