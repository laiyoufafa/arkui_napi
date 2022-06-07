/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FOUNDATION_ACE_NAPI_NATIVE_ENGINE_IMPL_ARK_ARK_NATIVE_ENGINE_IMPL_H
#define FOUNDATION_ACE_NAPI_NATIVE_ENGINE_IMPL_ARK_ARK_NATIVE_ENGINE_IMPL_H

#include <unordered_map>

#include "ark_headers.h"
#include "ecmascript/napi/include/jsnapi.h"
#include "ecmascript/napi/include/dfx_jsnapi.h"
#include "native_engine/native_engine_interface.h"
#include "ark_native_engine.h"

using panda::ecmascript::EcmaVM;
using panda::Local;
using panda::LocalScope;
using panda::JSValueRef;
using panda::JSNApi;
using panda::DFXJSNApi;
class SerializationData {
public:
    SerializationData() : data_(nullptr), size_(0) {}
    ~SerializationData() = default;

    uint8_t* GetData() const
    {
        return data_.get();
    }
    size_t GetSize() const
    {
        return size_;
    }

private:
    struct DataDeleter {
        void operator()(uint8_t* p) const
        {
            free(p);
        }
    };

    std::unique_ptr<uint8_t, DataDeleter> data_;
    size_t size_;
};

class ArkNativeEngineImpl : public NativeEngineInterface {
public:
    // ArkNativeEngineImpl constructor
    ArkNativeEngineImpl(EcmaVM* vm, NativeEngine* engine, void* jsEngine);
    // ArkNativeEngineImpl destructor
    ~ArkNativeEngineImpl() override;

    EcmaVM* GetEcmaVm() const
    {
        return vm_;
    }

    void Loop(LoopMode mode, bool needSync = false) override;

    // Get global native object value
    NativeValue* GetGlobal(NativeEngine* engine) override;
    // Create native null value
    NativeValue* CreateNull(NativeEngine* engine) override;
    // Create native undefined value
    NativeValue* CreateUndefined(NativeEngine* engine) override;
    // Create native boolean value
    NativeValue* CreateBoolean(NativeEngine* engine, bool value) override;
    // Create number value by int32_t
    NativeValue* CreateNumber(NativeEngine* engine, int32_t value) override;
    // Create number value by uint32_t
    NativeValue* CreateNumber(NativeEngine* engine, uint32_t value) override;
    // Create native number value by int64_t
    NativeValue* CreateNumber(NativeEngine* engine, int64_t value) override;
    // Create native number value by double
    NativeValue* CreateNumber(NativeEngine* engine, double value) override;
    // Create native bigint value by int64_t
    NativeValue* CreateBigInt(NativeEngine* engine, int64_t value) override;
    // Create native bigint value by uint64_t
    NativeValue* CreateBigInt(NativeEngine* engine, uint64_t value) override;
    // Create native string value by const char pointer
    NativeValue* CreateString(NativeEngine* engine, const char* value, size_t length) override;
    // Create native string value by const char16_t pointer
    NativeValue* CreateString16(NativeEngine* engine, const char16_t* value, size_t length) override;
    // Create native symbol value
    NativeValue* CreateSymbol(NativeEngine* engine, NativeValue* value) override;
    // Create native value of external pointer
    NativeValue* CreateExternal(NativeEngine* engine, void* value, NativeFinalize callback, void* hint) override;
    // Create native object value
    NativeValue* CreateObject(NativeEngine* engine) override;
    // Create native function value
    NativeValue* CreateFunction(
        NativeEngine* engine, const char* name, size_t length, NativeCallback cb, void* value) override;
    // Create native array value
    NativeValue* CreateArray(NativeEngine* engine, size_t length) override;
    // Create native array buffer value
    NativeValue* CreateArrayBuffer(NativeEngine* engine, void** value, size_t length) override;
    // Create native array buffer value of external
    NativeValue* CreateArrayBufferExternal(
        NativeEngine* engine, void* value, size_t length, NativeFinalize cb, void* hint) override;
    NativeValue* CreateBuffer(NativeEngine* engine, void** value, size_t length) override;
    NativeValue* CreateBufferCopy(NativeEngine* engine, void** value, size_t length, const void* data) override;
    NativeValue* CreateBufferExternal(
        NativeEngine* engine, void* value, size_t length, NativeFinalize cb, void* hint) override;
    // Create native typed array value
    NativeValue* CreateTypedArray(
        NativeEngine* engine, NativeTypedArrayType type, NativeValue* value, size_t length, size_t offset) override;
    // Create native data view value
    NativeValue* CreateDataView(NativeEngine* engine, NativeValue* value, size_t length, size_t offset) override;
    // Create native promise value
    NativeValue* CreatePromise(NativeEngine* engine, NativeDeferred** deferred) override;
    void SetPromiseRejectCallback(
        NativeEngine* engine, NativeReference* rejectCallbackRef, NativeReference* checkCallbackRef) override;
    static void PromiseRejectCallback(void* values);
    // Create native error value
    NativeValue* CreateError(NativeEngine* engine, NativeValue* code, NativeValue* message) override;
    // Call function
    NativeValue* CallFunction(NativeEngine* engine, NativeValue* thisVar, NativeValue* function,
        NativeValue* const *argv, size_t argc) override;
    // Run script
    NativeValue* RunScript(NativeEngine* engine, NativeValue* script) override;
    // Run buffer script
    NativeValue* RunBufferScript(NativeEngine* engine, std::vector<uint8_t>& buffer) override;
    // Run actor
    NativeValue* RunActor(NativeEngine* engine, std::vector<uint8_t>& buffer, const char* descriptor) override;
    // Set lib path
    void SetPackagePath(const std::string& packagePath);
    // Define native class
    NativeValue* DefineClass(NativeEngine* engine, const char* name, NativeCallback callback, void* data,
        const NativePropertyDescriptor* properties, size_t length) override;
    // Create instance by defined class
    NativeValue* CreateInstance(
        NativeEngine* engine, NativeValue* constructor, NativeValue* const *argv, size_t argc) override;

    // Create native reference
    NativeReference* CreateReference(NativeEngine* engine, NativeValue* value, uint32_t initialRefcount,
        NativeFinalize callback = nullptr, void* data = nullptr, void* hint = nullptr) override;
    // Throw exception
    bool Throw(NativeValue* error) override;
    // Throw exception
    bool Throw(NativeEngine* engine, NativeErrorType type, const char* code, const char* message) override;

    void* CreateRuntime(NativeEngine* engine) override;
    NativeValue* Serialize(NativeEngine* context, NativeValue* value, NativeValue* transfer) override;
    NativeValue* Deserialize(NativeEngine* engine, NativeEngine* context, NativeValue* recorder) override;
    void DeleteSerializationData(NativeValue* value) const override;
    ExceptionInfo* GetExceptionForWorker() const override;
    NativeValue* LoadModule(NativeEngine* engine, NativeValue* str, const std::string& fileName) override;
    NativeValue* LoadArkModule(NativeEngine* engine, const char* str, int32_t len, const std::string& fileName);

    static NativeValue* ArkValueToNativeValue(ArkNativeEngine* engine, Local<JSValueRef> value);

    NativeValue* ValueToNativeValue(NativeEngine* engine, JSValueWrapper& value) override;

    bool ExecuteJsBin(const std::string& fileName);
    panda::Global<panda::ObjectRef> LoadModuleByName(ArkNativeEngine* engine, const std::string& moduleName,
        bool isAppModule, const std::string& param, const std::string& instanceName, void* instance);

    virtual bool TriggerFatalException(NativeValue* error) override;
    NativeValue* CreateDate(NativeEngine* engine, double value) override;
    NativeValue* CreateBigWords(NativeEngine* engine, int sign_bit, size_t word_count, const uint64_t* words) override;
    bool AdjustExternalMemory(int64_t ChangeInBytes, int64_t* AdjustedValue) override;

    // Detect performance to obtain cpuprofiler file
    void StartCpuProfiler(const std::string& fileName = "") override;
    void StopCpuProfiler() override;

    void ResumeVM() override;
    bool SuspendVM() override;
    bool IsSuspended() override;
    bool CheckSafepoint() override;

    void DumpHeapSnapshot(const std::string& path, bool isVmMode = true,
        DumpFormat dumpFormat = DumpFormat::JSON) override;
    void DumpHeapSnapshotExt(bool isVmMode = true, DumpFormat dumpFormat = DumpFormat::JSON,
        bool isPrivate = false) override;
    bool BuildNativeAndJsStackTrace(std::string& stackTraceStr) override;
    bool BuildJsStackTrace(std::string& stackTraceStr) override;
    bool StartHeapTracking(double timeInterval, bool isVmMode = true) override;
    bool StopHeapTracking(const std::string& filePath) override;
    
    void PrintStatisticResult() override;
    void StartRuntimeStat() override;
    void StopRuntimeStat() override;
    size_t GetArrayBufferSize() override;
    size_t GetHeapTotalSize() override;
    size_t GetHeapUsedSize() override;
    void NotifyApplicationState(bool inBackground) override;
    virtual void NotifyMemoryPressure(bool inHighMemoryPressure = false) override;

    void RegisterUncaughtExceptionHandler(UncaughtExceptionCallback callback) override;
    void HandleUncaughtException(NativeEngine* engine) override;

    NativeReference* GetPromiseRejectCallBackRef()
    {
        return promiseRejectCallbackRef_;
    }

    NativeReference* GetCheckCallbackRef()
    {
        return checkCallbackRef_;
    }
    panda::Global<panda::ObjectRef> GetModuleFromName(NativeEngine* engine,
        const std::string& moduleName, bool isAppModule, const std::string& id, const std::string& param,
        const std::string& instanceName, void** instance);

private:
    static NativeEngine* CreateRuntimeFunc(NativeEngine* engine, void* jsEngine);

    EcmaVM* vm_ = nullptr;
    std::string exceptionStr_;
    panda::LocalScope topScope_;
    NativeReference* promiseRejectCallbackRef_ { nullptr };
    NativeReference* checkCallbackRef_ { nullptr };
    std::unordered_map<NativeModule*, panda::Global<panda::JSValueRef>> loadedModules_;
    UncaughtExceptionCallback uncaughtExceptionCallback_ { nullptr };
};

#endif /* FOUNDATION_ACE_NAPI_NATIVE_ENGINE_IMPL_ARK_ARK_NATIVE_ENGINE_IMPL_H */