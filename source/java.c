#include <falso_jni/FalsoJNI.h>
#include <falso_jni/FalsoJNI_Impl.h>
#include <falso_jni/FalsoJNI_Logger.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <psp2/kernel/processmgr.h>
#include <so_util/so_util.h>
#include "config.h"
#include "reimpl/audio_track.h"

/*
 * Gameloft JNI Method Implementations
 */

static jobject java_string(const char *value) {
    return (jobject)jni->NewStringUTF(&jni, value);
}

static jobject getPreferenceString(jmethodID id, va_list args) {
    fjni_log_info("getPreferenceString()");
    (void)va_arg(args, jstring); // Preference key.
    jstring fallback = va_arg(args, jstring);
    return fallback ? (jobject)fallback : java_string("");
}

static jobject getPackage(jmethodID id, va_list args) {
    fjni_log_info("getPackage()");
    return java_string(PACKAGE_NAME);
}

static jobject getSaveFolder(jmethodID id, va_list args) {
    fjni_log_info("getSaveFolder()");
    return java_string(DATA_PATH);
}

static jobject getSDFolder(jmethodID id, va_list args) {
    fjni_log_info("getSDFolder()");
    return java_string(DATA_PATH);
}

static jobject getContext(jmethodID id, va_list args) {
    return (jobject)0x42424242;
}

static jobject getUserAgent(jmethodID id, va_list args) {
    return java_string("Mozilla/5.0 (Linux; U; Android 4.4.2; en-us; PlayStation Vita)");
}

static void setupPaths(jmethodID id, va_list args) {
    fjni_log_info("setupPaths()");
    extern so_module so_mod;
    void (*set_paths)(const char *, const char *, const char *) = (void *)so_symbol(
        &so_mod, "_ZN3glf15AndroidSetPathsEPKcS1_S1_");
    if (set_paths) set_paths(DATA_PATH, DATA_PATH, DATA_PATH);
}

static void createView(jmethodID id, va_list args) {
    fjni_log_info("createView()");
}

static void setViewSettings(jmethodID id, va_list args) {
    fjni_log_info("setViewSettings()");
}

static jboolean setCurrentContext(jmethodID id, va_list args) {
    return JNI_TRUE;
}

static void enableAccelerometer(jmethodID id, va_list args) {
    fjni_log_info("enableAccelerometer()");
}

static void openBrowser(jmethodID id, va_list args) {
    fjni_log_info("openBrowser()");
}

static void openGLLive(jmethodID id, va_list args) {
    fjni_log_info("openGLLive()");
}

static jobject getDeviceLanguage(jmethodID id, va_list args) {
    return java_string("en");
}

static jobject getDeviceName(jmethodID id, va_list args) {
    return java_string("PlayStation Vita");
}

static jobject getDeviceFW(jmethodID id, va_list args) {
    return java_string("3.65");
}

static jobject getIMEI(jmethodID id, va_list args) {
    return java_string("353456789012345");
}

static jobject getHDIDFVd(jmethodID id, va_list args) {
    return java_string("vita_hdidfv_12345678");
}

static jobject getCountryCode(jmethodID id, va_list args) {
    return java_string("US");
}

static jint getDisplayWidth(jmethodID id, va_list args) {
    return SCREEN_W;
}

static jint getDisplayHeight(jmethodID id, va_list args) {
    return SCREEN_H;
}

static jfloat getDisplayDensity(jmethodID id, va_list args) {
    return 1.5f;
}

static void dummyVoid(jmethodID id, va_list args) {
}

static jint dummyInt(jmethodID id, va_list args) {
    return 0;
}

static jboolean dummyBoolean(jmethodID id, va_list args) {
    return JNI_TRUE;
}

static jobject dummyObject(jmethodID id, va_list args) {
    return java_string("");
}


static jobject phoneManufacturer(jmethodID id, va_list args) { return java_string("Sony"); }
static jobject phoneCPU(jmethodID id, va_list args) { return java_string("ARM Cortex-A9"); }
static jobject phoneGPU(jmethodID id, va_list args) { return java_string("PowerVR SGX543MP4+"); }
static jobject currentLocale(jmethodID id, va_list args) { return java_string("en_US"); }
static jobject mainObbName(jmethodID id, va_list args) { return java_string("main.1501.com.gameloft.android.ANMP.GloftZOHM.obb"); }
static jobject patchObbName(jmethodID id, va_list args) { return java_string("patch.15325.com.gameloft.android.ANMP.GloftZOHM.obb"); }
// Android's sysfs scaling_max_freq is expressed in kHz.
static jfloat phoneCPUFreq(jmethodID id, va_list args) { return 444000.0f; }
// Vita applications can schedule on three cores; the fourth is system-reserved.
static jint cpuCores(jmethodID id, va_list args) { return 3; }
static jboolean falseBoolean(jmethodID id, va_list args) { return JNI_FALSE; }
static jlong currentTime(jmethodID id, va_list args) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (jlong)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
static jlong uptimeMillis(jmethodID id, va_list args) {
    return (jlong)(sceKernelGetProcessTimeWide() / 1000);
}
static jint trueInt(jmethodID id, va_list args) { return 1; }
static void printDebug(jmethodID id, va_list args) {
    jstring message = va_arg(args, jstring);
    if (!message) return;
    char *text = jni->GetStringUTFChars(&jni, message, NULL);
    if (text) {
        fjni_logv_info("[GAME] %s", text);
        jni->ReleaseStringUTFChars(&jni, message, text);
    }
}
static jobject getResource(jmethodID id, va_list args) {
    jstring resource = va_arg(args, jstring);
    if (!resource) return NULL;
    char *name = jni->GetStringUTFChars(&jni, resource, NULL);
    if (!name) return NULL;
    char path[512];
    snprintf(path, sizeof(path), DATA_PATH "assets/%s", name);
    fjni_logv_info("Loading APK resource %s", path);
    FILE *file = fopen(path, "rb");
    jni->ReleaseStringUTFChars(&jni, resource, name);
    if (!file) return NULL;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    if (size < 0 || size > 16 * 1024 * 1024) { fclose(file); return NULL; }
    char *data = malloc(size ? size : 1);
    if (!data) { fclose(file); return NULL; }
    if (fread(data, 1, size, file) != (size_t)size) { free(data); fclose(file); return NULL; }
    fclose(file);
    jbyteArray array = jni->NewByteArray(&jni, size);
    if (array) jni->SetByteArrayRegion(&jni, array, 0, size, (jbyte *)data);
    free(data);
    return (jobject)array;
}

static jint getAssetLength(jmethodID id, va_list args) {
    jstring resource = va_arg(args, jstring);
    if (!resource) return 0;
    char *name = jni->GetStringUTFChars(&jni, resource, NULL);
    if (!name) return 0;
    char path[512];
    snprintf(path, sizeof(path), DATA_PATH "assets/%s", name);
    jni->ReleaseStringUTFChars(&jni, resource, name);
    struct stat info;
    if (stat(path, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size > INT32_MAX) return 0;
    return (jint)info.st_size;
}

static jobject getAssetList(jmethodID id, va_list args) {
    jstring resource = va_arg(args, jstring);
    char *name = resource ? jni->GetStringUTFChars(&jni, resource, NULL) : NULL;
    char path[512];
    snprintf(path, sizeof(path), DATA_PATH "assets/%s", name ? name : "");
    if (name) jni->ReleaseStringUTFChars(&jni, resource, name);
    DIR *dir = opendir(path);
    unsigned count = 0;
    struct dirent *entry;
    if (dir) while ((entry = readdir(dir))) if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) ++count;
    jobjectArray result = jni->NewObjectArray(&jni, count, (jclass)0x42424242, NULL);
    if (dir) {
        rewinddir(dir);
        unsigned index = 0;
        while ((entry = readdir(dir))) {
            if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
            if (index < count) jni->SetObjectArrayElement(&jni, result, index++, java_string(entry->d_name));
        }
        closedir(dir);
    }
    return (jobject)result;
}

NameToMethodID nameToMethodId[] = {
    { 129, "android/media/AudioTrack/<init>", METHOD_TYPE_OBJECT },
    { 130, "getMinBufferSize", METHOD_TYPE_INT },
    { 131, "getNativeOutputSampleRate", METHOD_TYPE_INT },
    { 132, "write", METHOD_TYPE_INT },
    { 133, "play", METHOD_TYPE_VOID },
    { 134, "pause", METHOD_TYPE_VOID },
    { 135, "stop", METHOD_TYPE_VOID },
    { 136, "release", METHOD_TYPE_VOID },
    { 137, "getPlayState", METHOD_TYPE_INT },
    { 138, "setThreadPriority", METHOD_TYPE_VOID },
    { 124, "getSystemUpTimeMillis", METHOD_TYPE_LONG },
    { 125, "hasConnectivity", METHOD_TYPE_BOOLEAN },
    { 126, "hasClosedActivity", METHOD_TYPE_INT },
    { 127, "GetPhoneLanguage", METHOD_TYPE_INT },
    { 128, "GetTextureFormat", METHOD_TYPE_INT },
    { 121, "getLength", METHOD_TYPE_INT },
    { 122, "getBytes", METHOD_TYPE_OBJECT },
    { 123, "getAssetList", METHOD_TYPE_OBJECT },
    { 118, "GetCurrentTime", METHOD_TYPE_LONG },
    { 119, "PrintDebug", METHOD_TYPE_VOID },
    { 120, "getResource", METHOD_TYPE_OBJECT },
    { 10, "getPreferenceString", METHOD_TYPE_OBJECT },
    { 11, "getPackage", METHOD_TYPE_OBJECT },
    { 12, "getSaveFolder", METHOD_TYPE_OBJECT },
    { 13, "getSDFolder", METHOD_TYPE_OBJECT },
    { 14, "getContext", METHOD_TYPE_OBJECT },
    { 15, "getUserAgent", METHOD_TYPE_OBJECT },
    { 16, "setupPaths", METHOD_TYPE_VOID },
    { 17, "createView", METHOD_TYPE_VOID },
    { 18, "setViewSettings", METHOD_TYPE_VOID },
    { 19, "setCurrentContext", METHOD_TYPE_BOOLEAN },
    { 20, "enableAccelerometer", METHOD_TYPE_VOID },
    { 21, "openBrowser", METHOD_TYPE_VOID },
    { 22, "openGLLive", METHOD_TYPE_VOID },
    { 23, "getDeviceLanguage", METHOD_TYPE_OBJECT },
    { 24, "getDeviceName", METHOD_TYPE_OBJECT },
    { 25, "getDeviceFW", METHOD_TYPE_OBJECT },
    { 26, "getIMEI", METHOD_TYPE_OBJECT },
    { 27, "getHDIDFVd", METHOD_TYPE_OBJECT },
    { 28, "getCountryCode", METHOD_TYPE_OBJECT },
    { 29, "getWidth", METHOD_TYPE_INT },
    { 30, "getHeight", METHOD_TYPE_INT },
    { 31, "getDensity", METHOD_TYPE_FLOAT },
    { 32, "getPackageName", METHOD_TYPE_OBJECT },
    { 33, "getAbsolutePath", METHOD_TYPE_OBJECT },
    { 34, "hasPermission", METHOD_TYPE_BOOLEAN },
    { 35, "isNetworkAvailable", METHOD_TYPE_BOOLEAN },
    { 36, "checkBillingSupported", METHOD_TYPE_INT },
    { 37, "requestPurchase", METHOD_TYPE_INT },
    { 100, "GetPhoneLanguageS", METHOD_TYPE_OBJECT },
    { 101, "GetPhoneModel", METHOD_TYPE_OBJECT },
    { 102, "GetPhoneFirmware", METHOD_TYPE_OBJECT },
    { 103, "GetPhoneDevice", METHOD_TYPE_OBJECT },
    { 104, "GetIMEI", METHOD_TYPE_OBJECT },
    { 105, "GetPhoneManufacturer", METHOD_TYPE_OBJECT },
    { 106, "GetPhoneCPUName", METHOD_TYPE_OBJECT },
    { 107, "GetPhoneGPUName", METHOD_TYPE_OBJECT },
    { 108, "GetObbPath", METHOD_TYPE_OBJECT },
    { 109, "GetMainObbName", METHOD_TYPE_OBJECT },
    { 110, "GetPatchObbName", METHOD_TYPE_OBJECT },
    { 111, "GetExtraObbName", METHOD_TYPE_OBJECT },
    { 112, "GetPhoneCPUFreq", METHOD_TYPE_FLOAT },
    { 113, "getNumberOfCPUCores", METHOD_TYPE_INT },
    { 114, "isWifiEnabled", METHOD_TYPE_INT },
    { 115, "GetWSClose", METHOD_TYPE_BOOLEAN },
    { 116, "GetCurrentLocale", METHOD_TYPE_OBJECT },
    { 117, "checkFreeCashReady", METHOD_TYPE_BOOLEAN },
};

MethodsBoolean methodsBoolean[] = {
    { 19, setCurrentContext },
    { 34, dummyBoolean },
    { 35, falseBoolean },
    { 115, dummyBoolean },
    { 117, falseBoolean },
    { 125, falseBoolean },
};

MethodsByte methodsByte[] = {};
MethodsChar methodsChar[] = {};
MethodsDouble methodsDouble[] = {};

MethodsFloat methodsFloat[] = {
    { 31, getDisplayDensity },
    { 112, phoneCPUFreq },
};

MethodsInt methodsInt[] = {
    { 130, audio_track_min_buffer },
    { 131, audio_track_sample_rate },
    { 132, audio_track_write },
    { 137, audio_track_play_state },
    { 121, getAssetLength },
    { 126, trueInt },
    { 127, dummyInt },
    { 128, dummyInt },
    { 29, getDisplayWidth },
    { 30, getDisplayHeight },
    { 36, dummyInt },
    { 37, dummyInt },
    { 113, cpuCores },
    { 114, dummyInt },
};

MethodsLong methodsLong[] = {{118, currentTime}, {124, uptimeMillis}};

MethodsObject methodsObject[] = {
    { 129, audio_track_create },
    { 122, getResource },
    { 123, getAssetList },
    { 120, getResource },
    { 10, getPreferenceString },
    { 11, getPackage },
    { 12, getSaveFolder },
    { 13, getSDFolder },
    { 14, getContext },
    { 15, getUserAgent },
    { 23, getDeviceLanguage },
    { 24, getDeviceName },
    { 25, getDeviceFW },
    { 26, getIMEI },
    { 27, getHDIDFVd },
    { 28, getCountryCode },
    { 32, getPackage },
    { 33, getSaveFolder },
    { 100, getDeviceLanguage },
    { 101, getDeviceName },
    { 102, getDeviceFW },
    { 103, getDeviceName },
    { 104, getIMEI },
    { 105, phoneManufacturer },
    { 106, phoneCPU },
    { 107, phoneGPU },
    { 108, getSaveFolder },
    { 109, mainObbName },
    { 110, patchObbName },
    { 111, dummyObject },
    { 116, currentLocale },
};

MethodsShort methodsShort[] = {};

MethodsVoid methodsVoid[] = {
    { 133, audio_track_play },
    { 134, audio_track_pause },
    { 135, audio_track_stop },
    { 136, audio_track_release },
    { 138, dummyVoid },
    { 119, printDebug },
    { 16, setupPaths },
    { 17, createView },
    { 18, setViewSettings },
    { 20, enableAccelerometer },
    { 21, openBrowser },
    { 22, openGLLive },
};

const int SDK_INT = 19; // Android 4.4 KitKat

NameToFieldID nameToFieldId[] = {
    { 2, "WINDOW_SERVICE", FIELD_TYPE_OBJECT },
    { 1, "SDK_INT", FIELD_TYPE_INT },
    { 3, "mPreferencesName", FIELD_TYPE_OBJECT },
};

FieldsBoolean fieldsBoolean[] = {};
FieldsByte fieldsByte[] = {};
FieldsChar fieldsChar[] = {};
FieldsDouble fieldsDouble[] = {};
FieldsFloat fieldsFloat[] = {};
FieldsInt fieldsInt[] = {
    { 1, SDK_INT },
};
FieldsObject fieldsObject[] = {
    { 2, NULL },
    { 3, NULL },
};
FieldsLong fieldsLong[] = {};
FieldsShort fieldsShort[] = {};

void java_fields_init(void) {
    fieldsObject[0].value = java_string("window");
    fieldsObject[1].value = java_string(PACKAGE_NAME);
}

__FALSOJNI_IMPL_CONTAINER_SIZES
