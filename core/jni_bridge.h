#pragma once
#include <jni.h>

namespace JNIBridge {
    bool AttachToJVM();
    void DetachFromJVM();
    JNIEnv* GetEnv();
}

extern JavaVM* g_JavaVM;
extern JNIEnv* g_JNIEnv;
