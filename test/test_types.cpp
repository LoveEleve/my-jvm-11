/*
 * my_jvm - Simple type test
 * 测试基本类型定义是否能正常工作
 */

#include <iostream>
#include <cassert>
#include "utilities/macros.hpp"
#include "utilities/globalDefinitions.hpp"
#include "utilities/align.hpp"
#include "runtime/atomic.hpp"
#include "oops/oop.hpp"
#include "oops/klass.hpp"
#include "oops/method.hpp"
#include "oops/constMethod.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/markOop.hpp"

int main() {
    std::cout << "=== my_jvm Type Test ===" << std::endl;
    
    // 测试基本类型
    std::cout << "Testing basic types..." << std::endl;
    
    jint ji = 42;
    jlong jl = 12345678901234LL;
    juint jui = 100;
    julong jul = 98765432109876ULL;
    
    std::cout << "  jint: " << ji << std::endl;
    std::cout << "  jlong: " << jl << std::endl;
    std::cout << "  juint: " << jui << std::endl;
    std::cout << "  julong: " << jul << std::endl;
    
    // 测试指针类型
    std::cout << "Testing pointer types..." << std::endl;
    
    oop obj = nullptr;
    Klass* klass = nullptr;
    Method* method = nullptr;
    markOop mark = 0;
    
    std::cout << "  oop: " << obj << std::endl;
    std::cout << "  Klass*: " << klass << std::endl;
    std::cout << "  Method*: " << method << std::endl;
    std::cout << "  markOop: " << mark << std::endl;
    
    // 测试 oopDesc
    std::cout << "Testing oopDesc..." << std::endl;
    
    oopDesc java_obj;
    java_obj.set_mark(markWord_unlocked());
    java_obj.set_klass(klass);
    
    std::cout << "  oopDesc created: OK" << std::endl;
    std::cout << "  is_unlocked: " << (java_obj.is_unlocked() ? "true" : "false") << std::endl;
    
    // 测试 markOop
    std::cout << "Testing markOop..." << std::endl;
    
    markOop unlocked = markWord_unlocked();
    std::cout << "  unlocked: " << unlocked << std::endl;
    std::cout << "  is_unlocked: " << (markWord_is_unlocked(unlocked) ? "true" : "false") << std::endl;
    
    markOop gc_marked = markWord_gc_marked();
    std::cout << "  gc_marked: " << gc_marked << std::endl;
    std::cout << "  is_gc_marked: " << (markWord_is_gc_marked(gc_marked) ? "true" : "false") << std::endl;
    
    // 测试轻量级锁
    void* lock_rec = (void*)0x1000;
    markOop lightweight = markWord_lightweight_locked(lock_rec);
    std::cout << "  lightweight_locked: " << lightweight << std::endl;
    std::cout << "  is_lightweight_locked: " << (markWord_is_lightweight_locked(lightweight) ? "true" : "false") << std::endl;
    
    // 测试重量级锁
    void* monitor = (void*)0x2000;
    markOop heavyweight = markWord_heavyweight_locked(monitor);
    std::cout << "  heavyweight_locked: " << heavyweight << std::endl;
    std::cout << "  is_heavyweight_locked: " << (markWord_is_heavyweight_locked(heavyweight) ? "true" : "false") << std::endl;
    
    // 测试 Klass
    std::cout << "Testing Klass..." << std::endl;
    
    Klass klass_obj;
    klass_obj.set_access_flags(0x0001);  // public
    klass_obj.set_super(nullptr);
    
    std::cout << "  Klass created: OK" << std::endl;
    std::cout << "  is_klass: " << (klass_obj.is_klass() ? "true" : "false") << std::endl;
    std::cout << "  is_instance_klass: " << (klass_obj.is_instance_klass() ? "true" : "false") << std::endl;
    
    // 测试 ConstMethod 和 Method
    std::cout << "Testing ConstMethod and Method..." << std::endl;

    ConstMethod const_method_obj;
    const_method_obj.set_name_index(1);
    const_method_obj.set_signature_index(2);
    const_method_obj.set_code_size(100);

    Method method_obj;
    method_obj.set_access_flags(0x0001);  // public
    method_obj.set_constMethod(&const_method_obj);

    std::cout << "  ConstMethod created: OK" << std::endl;
    std::cout << "  Method created: OK" << std::endl;
    std::cout << "  is_method: " << (method_obj.is_method() ? "true" : "false") << std::endl;
    std::cout << "  code_size: " << method_obj.constMethod()->code_size() << std::endl;
    
    // 测试原子操作
    std::cout << "Testing atomic operations..." << std::endl;
    
    intptr_t atomic_val = 0;
    atomic_store(&atomic_val, (intptr_t)42);
    intptr_t loaded = atomic_load(&atomic_val);
    assert(loaded == 42);
    std::cout << "  atomic_store/load: OK" << std::endl;
    
    // Add 测试
    atomic_store(&atomic_val, (intptr_t)50);
    intptr_t add_result = atomic_add(&atomic_val, (intptr_t)50);
    assert(atomic_load(&atomic_val) == 100);
    std::cout << "  atomic_add: OK" << std::endl;
    
    // 测试对齐宏
    std::cout << "Testing alignment macros..." << std::endl;
    
    size_t addr1 = 100;
    size_t aligned8 = align_up(addr1, 8);
    size_t aligned16 = align_up(addr1, 16);
    
    assert(aligned8 == 104);
    assert(aligned16 == 112);
    
    std::cout << "  align_up(100, 8) = " << aligned8 << std::endl;
    std::cout << "  align_up(100, 16) = " << aligned16 << std::endl;
    
    std::cout << std::endl;
    std::cout << "=== All Tests Passed! ===" << std::endl;
    
    return 0;
}
