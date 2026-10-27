#include "jni_bridge.h"
#include <windows.h>
#include <iostream>

JavaVM* g_JavaVM = nullptr;
JNIEnv* g_JNIEnv = nullptr;

bool JNIBridge::AttachToJVM() {
    if (g_JavaVM) {
        return GetEnv() != nullptr;
    }

    HMODULE hJvm = GetModuleHandleA("jvm.dll");
    if (!hJvm) {
        return false;
    }

    typedef jint(JNICALL* JNI_GetCreatedJavaVMs_t)(JavaVM**, jsize, jsize*);
    auto GetCreatedJavaVMs = (JNI_GetCreatedJavaVMs_t)GetProcAddress(hJvm, "JNI_GetCreatedJavaVMs");

    if (!GetCreatedJavaVMs) {
        return false;
    }

    jsize numVMs = 0;
    if (GetCreatedJavaVMs(&g_JavaVM, 1, &numVMs) != JNI_OK || numVMs == 0) {
        return false;
    }

    return GetEnv() != nullptr;
}

void JNIBridge::DetachFromJVM() {
    if (g_JavaVM) {
        g_JavaVM->DetachCurrentThread();
    }
}

JNIEnv* JNIBridge::GetEnv() {
    if (!g_JavaVM && !AttachToJVM()) {
        return nullptr;
    }

    JNIEnv* env = nullptr;
    jint res = g_JavaVM->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8);
    if (res == JNI_EDETACHED) {
        JavaVMAttachArgs args{};
        args.version = JNI_VERSION_1_8;
        args.name = const_cast<char*>("LegitClient");
        if (g_JavaVM->AttachCurrentThreadAsDaemon(reinterpret_cast<void**>(&env), &args) != JNI_OK) {
            return nullptr;
        }
    } else if (res != JNI_OK) {
        return nullptr;
    }

    g_JNIEnv = env;
    return env;
}
