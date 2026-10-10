#include <jni.h>
#include <android/log.h>
#include <dobby.h>
#include "runtime/module_runtime.h"
#include "hooks/hook_chain.h"
#include "runtime/game_services.h"
#include <mutex>
#include <thread>
#include <chrono>

namespace {
std::mutex lifecycle;
EML::ModuleRuntime* host=nullptr;
EML::HookService* hooks=nullptr;
jboolean Start(JNIEnv* env,jclass,jstring index,jstring mode){
    if(!index||!mode)return JNI_FALSE;std::lock_guard lock(lifecycle);if(host)return JNI_FALSE;
    const char* indexText=env->GetStringUTFChars(index,nullptr);const char* modeText=env->GetStringUTFChars(mode,nullptr);
    if(!indexText||!modeText){if(indexText)env->ReleaseStringUTFChars(index,indexText);if(modeText)env->ReleaseStringUTFChars(mode,modeText);return JNI_FALSE;}
    std::string path=indexText,role=modeText;env->ReleaseStringUTFChars(index,indexText);env->ReleaseStringUTFChars(mode,modeText);
    if(role!="offline"&&role!="game")return JNI_FALSE;
    if(!hooks){hooks=new EML::HookService;if(!hooks->Initialize())return JNI_FALSE;}
    auto* runtime=new EML::ModuleRuntime;
    auto log=[](const std::string& id,const std::string& text){__android_log_print(ANDROID_LOG_INFO,"EModLoader","%s: %s",id.c_str(),text.c_str());};
    if(!runtime->Start(std::filesystem::path(path),"android-arm64",log,nullptr,hooks->Api(),role)){delete runtime;return JNI_FALSE;}
    if(role=="game")std::thread([runtime,path,log]{
        auto* services=new EML::GameServices(*hooks,std::filesystem::path(path),log);
        std::this_thread::sleep_for(std::chrono::seconds(3));
        auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(90);std::string error;
        while(!services->Initialize(error)){if(std::chrono::steady_clock::now()>=deadline){log("runtime",error);return;}std::this_thread::sleep_for(std::chrono::milliseconds(100));}
        services->Detach();runtime->SetRuntime(services->Api());log("runtime","Optional game helpers ready");
    }).detach();
    host=runtime;return JNI_TRUE;
}
jboolean Update(JNIEnv* env,jclass,jstring value){if(!value||env->GetStringUTFLength(value)>1024*1024)return JNI_FALSE;std::lock_guard lock(lifecycle);if(!host)return JNI_FALSE;const char* text=env->GetStringUTFChars(value,nullptr);if(!text)return JNI_FALSE;bool result=host->UpdateIndex(text);env->ReleaseStringUTFChars(value,text);return result?JNI_TRUE:JNI_FALSE;}
void Stop(JNIEnv*,jclass){std::lock_guard lock(lifecycle);if(host)host->Stop();}
}
extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* vm,void*){
    JNIEnv* env=nullptr;if(vm->GetEnv(reinterpret_cast<void**>(&env),JNI_VERSION_1_6)!=JNI_OK)return JNI_ERR;
    jclass threadClass=env->FindClass("java/lang/Thread");auto current=env->GetStaticMethodID(threadClass,"currentThread","()Ljava/lang/Thread;");jobject thread=env->CallStaticObjectMethod(threadClass,current);
    auto getLoader=env->GetMethodID(threadClass,"getContextClassLoader","()Ljava/lang/ClassLoader;");jobject loader=env->CallObjectMethod(thread,getLoader);
    jclass loaderClass=env->FindClass("java/lang/ClassLoader");auto loadClass=env->GetMethodID(loaderClass,"loadClass","(Ljava/lang/String;)Ljava/lang/Class;");jstring name=env->NewStringUTF("dev.endfield.modloader.NativeBridge");
    auto bridge=static_cast<jclass>(env->CallObjectMethod(loader,loadClass,name));
    if(env->ExceptionCheck()||!bridge)return JNI_ERR;
    const JNINativeMethod methods[]{
        {const_cast<char*>("start"),const_cast<char*>("(Ljava/lang/String;Ljava/lang/String;)Z"),reinterpret_cast<void*>(Start)},
        {const_cast<char*>("update"),const_cast<char*>("(Ljava/lang/String;)Z"),reinterpret_cast<void*>(Update)},
        {const_cast<char*>("stop"),const_cast<char*>("()V"),reinterpret_cast<void*>(Stop)}};
    return env->RegisterNatives(bridge,methods,3)==0?JNI_VERSION_1_6:JNI_ERR;
}
