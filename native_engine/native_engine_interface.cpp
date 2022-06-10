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

#include "native_engine_interface.h"

#if !defined(WINDOWS_PLATFORM) && !defined(MAC_PLATFORM) && !defined(IOS_PLATFORM)
#include <sys/epoll.h>
#endif
#include <uv.h>

#include "utils/log.h"

namespace {
const char* g_errorMessages[] = {
    nullptr,
    "Invalid parameter",
    "Need object",
    "Need string",
    "Need string or symbol",
    "Need function",
    "Need number",
    "Need boolean",
    "Need array",
    "Generic failure",
    "An exception is blocking",
    "Asynchronous work cancelled",
    "Escape called twice",
    "Handle scope mismatch",
    "Callback scope mismatch",
    "Asynchronous work queue is full",
    "Asynchronous work handle is closing",
    "Need bigint",
    "Need date",
    "Need arraybuffer",
    "Need detachable arraybuffer",
};
} // namespace

NativeEngineInterface::NativeEngineInterface(NativeEngine* engine, void* jsEngineInterface)
    : jsEngineInterface_(jsEngineInterface), rootNativeEngine__(engine)
{
    HILOG_INFO("NativeEngineInterface::NativeEngineInterface");
}

void NativeEngineInterface::Init()
{
    HILOG_INFO("NativeEngineInterface::Init");
    moduleManager_ = NativeModuleManager::GetInstance();
    referenceManager_ = new NativeReferenceManager();
    scopeManager_ = new NativeScopeManager();
    callbackScopeManager_ = new NativeCallbackScopeManager();
    loop_ = uv_loop_new();
    if (loop_ == nullptr) {
        return;
    }
    tid_ = pthread_self();
    uv_async_init(loop_, &uvAsync_, nullptr);
    uv_sem_init(&uvSem_, 0);
    lastException_ = nullptr;
}

NativeEngineInterface::~NativeEngineInterface()
{
    HILOG_INFO("NativeEngineInterface::~NativeEngineInterface");
    if (cleanEnv_ != nullptr) {
        cleanEnv_();
    }
}

void NativeEngineInterface::Deinit()
{
    HILOG_INFO("NativeEngineInterface::Deinit");
    if (referenceManager_ != nullptr) {
        delete referenceManager_;
        referenceManager_ = nullptr;
    }
    if (scopeManager_ != nullptr) {
        delete scopeManager_;
        scopeManager_ = nullptr;
    }

    SetStopping(true);
    uv_sem_destroy(&uvSem_);
    uv_close((uv_handle_t*)&uvAsync_, nullptr);
    uv_run(loop_, UV_RUN_ONCE);
    uv_loop_delete(loop_);
}

NativeScopeManager* NativeEngineInterface::GetScopeManager()
{
    return scopeManager_;
}

NativeReferenceManager* NativeEngineInterface::GetReferenceManager()
{
    return referenceManager_;
}

NativeModuleManager* NativeEngineInterface::GetModuleManager()
{
    return moduleManager_;
}

NativeCallbackScopeManager* NativeEngineInterface::GetCallbackScopeManager()
{
    return callbackScopeManager_;
}

uv_loop_t* NativeEngineInterface::GetUVLoop() const
{
    return loop_;
}

pthread_t NativeEngineInterface::GetTid() const
{
    return tid_;
}

void NativeEngineInterface::Loop(LoopMode mode, bool needSync)
{
    bool more = true;
    switch (mode) {
        case LoopMode::LOOP_DEFAULT:
            more = uv_run(loop_, UV_RUN_DEFAULT);
            break;
        case LoopMode::LOOP_ONCE:
            more = uv_run(loop_, UV_RUN_ONCE);
            break;
        case LoopMode::LOOP_NOWAIT:
            more = uv_run(loop_, UV_RUN_NOWAIT);
            break;
        default:
            return;
    }
    if (more == false) {
        more = uv_loop_alive(loop_);
    }

    if (needSync) {
        uv_sem_post(&uvSem_);
    }
}

NativeAsyncWork* NativeEngineInterface::CreateAsyncWork(NativeEngine* engine, NativeValue* asyncResource,
    NativeValue* asyncResourceName, NativeAsyncExecuteCallback execute, NativeAsyncCompleteCallback complete,
    void* data)
{
    (void)asyncResource;
    (void)asyncResourceName;
    return new NativeAsyncWork(engine, execute, complete, data);
}

NativeAsyncWork* NativeEngineInterface::CreateAsyncWork(
    NativeEngine* engine, NativeAsyncExecuteCallback execute, NativeAsyncCompleteCallback complete, void* data)
{
    return new NativeAsyncWork(engine, execute, complete, data);
}

NativeSafeAsyncWork* NativeEngineInterface::CreateSafeAsyncWork(NativeEngine* engine, NativeValue* func,
    NativeValue* asyncResource, NativeValue* asyncResourceName, size_t maxQueueSize, size_t threadCount,
    void* finalizeData, NativeFinalize finalizeCallback, void* context, NativeThreadSafeFunctionCallJs callJsCallback)
{
    return new NativeSafeAsyncWork(engine, func, asyncResource, asyncResourceName, maxQueueSize, threadCount,
        finalizeData, finalizeCallback, context, callJsCallback);
}

void NativeEngineInterface::InitAsyncWork(
    NativeEngine* engine, NativeAsyncExecuteCallback execute, NativeAsyncCompleteCallback complete, void* data)
{
    asyncWorker_ = std::make_unique<NativeAsyncWork>(engine, execute, complete, data);
    asyncWorker_->Init();
}

bool NativeEngineInterface::SendAsyncWork(void* data)
{
    if (!asyncWorker_) {
        HILOG_ERROR("asyncWorker_ is nullptr");
        return false;
    }
    asyncWorker_->Send(data);
    return true;
}

void NativeEngineInterface::CloseAsyncWork()
{
    if (!asyncWorker_) {
        HILOG_ERROR("asyncWorker_ is nullptr");
        return;
    }
    asyncWorker_->Close();
}

NativeErrorExtendedInfo* NativeEngineInterface::GetLastError()
{
    return &lastError_;
}

void NativeEngineInterface::SetLastError(int errorCode, uint32_t engineErrorCode, void* engineReserved)
{
    lastError_.errorCode = errorCode;
    lastError_.engineErrorCode = engineErrorCode;
    lastError_.message = g_errorMessages[lastError_.errorCode];
    lastError_.reserved = engineReserved;
}

void NativeEngineInterface::ClearLastError()
{
    lastError_.errorCode = 0;
    lastError_.engineErrorCode = 0;
    lastError_.message = nullptr;
    lastError_.reserved = nullptr;
}

bool NativeEngineInterface::IsExceptionPending() const
{
    return !(lastException_ == nullptr);
}

NativeValue* NativeEngineInterface::GetAndClearLastException()
{
    NativeValue* temp = lastException_;
    lastException_ = nullptr;
    return temp;
}

void NativeEngineInterface::EncodeToUtf8(
    NativeValue* nativeValue, char* buffer, int32_t* written, size_t bufferSize, int32_t* nchars)
{
    if (nativeValue == nullptr || nchars == nullptr || written == nullptr) {
        HILOG_ERROR("NativeEngine EncodeToUtf8 args is nullptr");
        return;
    }

    auto nativeString = reinterpret_cast<NativeString*>(nativeValue->GetInterface(NativeString::INTERFACE_ID));
    if (nativeString == nullptr) {
        HILOG_ERROR("nativeValue GetInterface is nullptr");
        return;
    }
    *written = nativeString->EncodeWriteUtf8(buffer, bufferSize, nchars);
}

void NativeEngineInterface::EncodeToChinese(NativeValue* nativeValue, std::string& buffer, const std::string& encoding)
{
    if (nativeValue == nullptr) {
        HILOG_ERROR("NativeEngine is nullptr");
        return;
    }

    auto nativeString = reinterpret_cast<NativeString*>(nativeValue->GetInterface(NativeString::INTERFACE_ID));
    if (nativeString == nullptr) {
        HILOG_ERROR("nativeValue GetInterface is nullptr");
        return;
    }
    nativeString->EncodeWriteChinese(buffer, encoding.c_str());
}

#if !defined(WINDOWS_PLATFORM) && !defined(MAC_PLATFORM)
void NativeEngineInterface::CheckUVLoop()
{
#ifndef IOS_PLATFORM
    checkUVLoop_ = true;
    uv_thread_create(&uvThread_, NativeEngineInterface::UVThreadRunner, this);
#endif
}

void NativeEngineInterface::CancelCheckUVLoop()
{
#ifndef IOS_PLATFORM
    checkUVLoop_ = false;
    RunCleanup();
    uv_async_send(&uvAsync_);
    uv_sem_post(&uvSem_);
    uv_thread_join(&uvThread_);
#endif
}

void NativeEngineInterface::PostLoopTask()
{
#ifndef IOS_PLATFORM
    postTask_(true);
    uv_sem_wait(&uvSem_);
#endif
}

void NativeEngineInterface::UVThreadRunner(void* nativeEngineImpl)
{
#ifndef IOS_PLATFORM
    auto engineImpl = static_cast<NativeEngineInterface*>(nativeEngineImpl);
    engineImpl->PostLoopTask();
    while (engineImpl->checkUVLoop_) {
        int32_t fd = uv_backend_fd(engineImpl->loop_);
        int32_t timeout = uv_backend_timeout(engineImpl->loop_);
        struct epoll_event ev;
        int32_t result = epoll_wait(fd, &ev, 1, timeout);
        if (!engineImpl->checkUVLoop_) {
            HILOG_INFO("break thread after epoll wait");
            break;
        }
        if (result >= 0) {
            engineImpl->PostLoopTask();
        } else {
            HILOG_ERROR("epoll wait fail: result: %{public}d, errno: %{public}d", result, errno);
        }
        if (!engineImpl->checkUVLoop_) {
            HILOG_INFO("break thread after post loop task");
            break;
        }
    }
#endif
}
#endif

void NativeEngineInterface::SetPostTask(PostTask postTask)
{
    HILOG_INFO("SetPostTask in");
    postTask_ = postTask;
}

void NativeEngineInterface::TriggerPostTask()
{
    if (postTask_ == nullptr) {
        HILOG_ERROR("postTask_ is nullptr");
        return;
    }
    postTask_(false);
}

void* NativeEngineInterface::GetJsEngine()
{
    return jsEngineInterface_;
}

void NativeEngineInterface::AddCleanupHook(CleanupCallback fun, void* arg)
{
    HILOG_INFO("%{public}s, start.", __func__);
    auto insertion_info = cleanup_hooks_.emplace(CleanupHookCallback { fun, arg, cleanup_hook_counter_++ });
    if (insertion_info.second != true) {
        HILOG_ERROR("AddCleanupHook Failed.");
    }
    HILOG_INFO("%{public}s, end.", __func__);
}

void NativeEngineInterface::RemoveCleanupHook(CleanupCallback fun, void* arg)
{
    HILOG_INFO("%{public}s, start.", __func__);
    CleanupHookCallback hook { fun, arg, 0 };
    cleanup_hooks_.erase(hook);
    HILOG_INFO("%{public}s, end.", __func__);
}

void NativeEngineInterface::RunCleanup()
{
    HILOG_INFO("%{public}s, start.", __func__);
    CleanupHandles();
    // sync clean up
    while (!cleanup_hooks_.empty()) {
        HILOG_INFO("NativeEngineInterface::RunCleanup cleanup_hooks is not empty");
        // Copy into a vector, since we can't sort an unordered_set in-place.
        std::vector<CleanupHookCallback> callbacks(cleanup_hooks_.begin(), cleanup_hooks_.end());
        // We can't erase the copied elements from `cleanup_hooks_` yet, because we
        // need to be able to check whether they were un-scheduled by another hook.

        std::sort(callbacks.begin(), callbacks.end(), [](const CleanupHookCallback& a, const CleanupHookCallback& b) {
            // Sort in descending order so that the most recently inserted callbacks are run first.
            return a.insertion_order_counter_ > b.insertion_order_counter_;
        });
        HILOG_INFO(
            "NativeEngineInterface::RunCleanup cleanup_hooks callbacks size:%{public}d", (int32_t)callbacks.size());
        for (const CleanupHookCallback& cb : callbacks) {
            if (cleanup_hooks_.count(cb) == 0) {
                // This hook was removed from the `cleanup_hooks_` set during another
                // hook that was run earlier. Nothing to do here.
                continue;
            }
            cb.fn_(cb.arg_);
            cleanup_hooks_.erase(cb);
        }
        CleanupHandles();
    }
    HILOG_INFO("%{public}s, end.", __func__);
}

void NativeEngineInterface::CleanupHandles()
{
    HILOG_INFO("%{public}s, start.", __func__);
    while (request_waiting_ > 0) {
        HILOG_INFO("%{public}s, request waiting:%{public}d.", __func__, request_waiting_);
        uv_run(loop_, UV_RUN_ONCE);
    }
    HILOG_INFO("%{public}s, end.", __func__);
}

void NativeEngineInterface::IncreaseWaitingRequestCounter()
{
    request_waiting_++;
    HILOG_INFO("%{public}s, request waiting:%{public}d.", __func__, request_waiting_);
}

void NativeEngineInterface::DecreaseWaitingRequestCounter()
{
    request_waiting_--;
    HILOG_INFO("%{public}s, request waiting:%{public}d.", __func__, request_waiting_);
}

NativeEngine* NativeEngineInterface::GetRootNativeEngine(void)
{
    return rootNativeEngine__;
}
