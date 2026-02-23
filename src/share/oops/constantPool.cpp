// ============================================================================
// Mini JVM - 常量池实现
// 对应 HotSpot: src/hotspot/share/oops/constantPool.cpp
// ============================================================================

#include "oops/constantPool.hpp"

void ConstantPool::print_on(FILE* out) const {
    fprintf(out, "Constant Pool [%d entries]:\n", _length);

    for (int i = 1; i < _length; i++) {
        constantTag tag = tag_at(i);
        fprintf(out, "  #%-4d = %-20s ", i, tag.to_string());

        switch (tag.value()) {
            case JVM_CONSTANT_Utf8:
                fprintf(out, "%s", utf8_at(i));
                break;

            case JVM_CONSTANT_Integer:
                fprintf(out, "%d", int_at(i));
                break;

            case JVM_CONSTANT_Float:
                fprintf(out, "%f", float_at(i));
                break;

            case JVM_CONSTANT_Long:
                fprintf(out, "%ldL", long_at(i));
                i++;  // 跳过下一个 slot
                break;

            case JVM_CONSTANT_Double:
                fprintf(out, "%f", double_at(i));
                i++;  // 跳过下一个 slot
                break;

            case JVM_CONSTANT_Class:
            case JVM_CONSTANT_ClassIndex:
            case JVM_CONSTANT_UnresolvedClass:
                fprintf(out, "#%d", klass_name_index_at(i));
                // 尝试打印类名
                {
                    int name_idx = klass_name_index_at(i);
                    if (name_idx > 0 && name_idx < _length &&
                        _tags[name_idx] == JVM_CONSTANT_Utf8) {
                        fprintf(out, "  // %s", utf8_at(name_idx));
                    }
                }
                break;

            case JVM_CONSTANT_String:
            case JVM_CONSTANT_StringIndex:
                fprintf(out, "#%d", string_utf8_index_at(i));
                {
                    int utf8_idx = string_utf8_index_at(i);
                    if (utf8_idx > 0 && utf8_idx < _length &&
                        _tags[utf8_idx] == JVM_CONSTANT_Utf8) {
                        fprintf(out, "  // %s", utf8_at(utf8_idx));
                    }
                }
                break;

            case JVM_CONSTANT_Fieldref:
            case JVM_CONSTANT_Methodref:
            case JVM_CONSTANT_InterfaceMethodref:
                fprintf(out, "#%d.#%d",
                    unchecked_klass_ref_index_at(i),
                    unchecked_name_and_type_ref_index_at(i));
                break;

            case JVM_CONSTANT_NameAndType:
                fprintf(out, "#%d:#%d",
                    name_ref_index_at(i),
                    signature_ref_index_at(i));
                {
                    int n = name_ref_index_at(i);
                    int s = signature_ref_index_at(i);
                    if (n > 0 && n < _length && _tags[n] == JVM_CONSTANT_Utf8 &&
                        s > 0 && s < _length && _tags[s] == JVM_CONSTANT_Utf8) {
                        fprintf(out, "  // %s:%s", utf8_at(n), utf8_at(s));
                    }
                }
                break;

            case JVM_CONSTANT_MethodHandle:
                fprintf(out, "kind=%d, #%d",
                    (int)(_data[i] & 0xFFFF),
                    (int)((_data[i] >> 16) & 0xFFFF));
                break;

            case JVM_CONSTANT_MethodType:
                fprintf(out, "#%d", (int)_data[i]);
                break;

            case JVM_CONSTANT_InvokeDynamic:
            case JVM_CONSTANT_Dynamic:
                fprintf(out, "bsm=#%d, #%d",
                    (int)(_data[i] & 0xFFFF),
                    (int)((_data[i] >> 16) & 0xFFFF));
                break;

            case JVM_CONSTANT_Invalid:
                fprintf(out, "(invalid/padding)");
                break;

            default:
                fprintf(out, "(unknown tag %d)", tag.value());
                break;
        }

        fprintf(out, "\n");
    }
}
