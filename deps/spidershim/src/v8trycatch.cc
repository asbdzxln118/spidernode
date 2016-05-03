// Copyright Mozilla Foundation. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#include <assert.h>

#include "v8.h"
#include "v8local.h"
#include "jsapi.h"
#include "jsfriendapi.h"

namespace v8 {

struct TryCatch::Impl {
  Impl(Isolate* iso)
    : isolate_(iso),
      verbose_(false) {
    Reset();
  }
  ~Impl() {
    JSContext* cx = JSContextFromIsolate(isolate_);
    if (!rethrow_ && JS_IsExceptionPending(cx)) {
      JS_ClearPendingException(cx);
    }
    // TODO: Propagate exceptions to the caller
    //       https://github.com/mozilla/spidernode/issues/51
  }
  Isolate* Isolate() const { return isolate_; }
  bool HasException() const {
    if (!hasExceptionSet_ &&
        !GetAndClearExceptionIfNeeded()) {
      // TODO: Report the error in a some way.
      return false;
    }
    assert(hasExceptionSet_);
    return hasException_;
  }
  bool HasExceptionSet() const {
    return hasExceptionSet_;
  }
  static bool HasExceptionPending(JSContext* cx) {
    return JS_IsExceptionPending(cx);
  }
  void SetException(JS::HandleValue exception) const {
    assert(!hasExceptionSet_ || !HasException());
    hasExceptionSet_ = true;
    exception_ = exception.get();
  }
  void ReThrow() {
    if (!GetAndClearExceptionIfNeeded()) {
      return;
    }
    JSContext* cx = JSContextFromIsolate(isolate_);
    assert(hasExceptionSet_ && HasException() && !HasExceptionPending(cx));
    JS::RootedValue exc(cx, *exception_.unsafeGet());
    JS_SetPendingException(cx, exc);
    rethrow_ = true;
  }
  JS::Value* Exception() {
    assert(hasExceptionSet_ && HasException());
    return exception_.unsafeGet();
  }
  void SetVerbose(bool verbose) { verbose_ = verbose; }

  void Reset() {
    hasException_ = false;
    hasExceptionSet_ = false;
    rethrow_ = false;
    exception_ = JS::UndefinedValue();
  }
  bool GetAndClearExceptionIfNeeded() const {
    if (HasExceptionSet()) {
      return true;
    }
    hasException_ = false;
    JSContext* cx = JSContextFromIsolate(isolate_);
    if (!HasExceptionPending(cx)) {
      return false;
    }
    JS::RootedValue exc(cx);
    if (JS_GetPendingException(cx, &exc)) {
      if (verbose_) {
        JS_ReportPendingException(cx);
      }
      JS_ClearPendingException(cx);
      SetException(exc);
      hasException_ = true;
      return true;
    }
    return false;
  }

private:
  class Isolate* isolate_;
  mutable JS::Heap<JS::Value> exception_;
  mutable bool hasException_;
  mutable bool hasExceptionSet_;
  mutable bool rethrow_;
  bool verbose_;
};

TryCatch::TryCatch(Isolate* iso)
  : pimpl_(new Impl(iso)) {
}

TryCatch::~TryCatch() {
  delete pimpl_;
}

bool TryCatch::HasCaught() const {
  return pimpl_->HasException();
}

bool TryCatch::HasTerminated() const {
  // TODO: Expose SpiderMonkey uncatchable exceptions
  //       https://github.com/mozilla/spidernode/issues/52
  return false;
}

Local<Value> TryCatch::ReThrow() {
  pimpl_->ReThrow();
  return internal::Local<Value>::New(pimpl_->Isolate(), *pimpl_->Exception());
}

Local<Value> TryCatch::Exception() const {
  if (!pimpl_->HasException()) {
    return Local<Value>();
  }
  return internal::Local<Value>::New(pimpl_->Isolate(), *pimpl_->Exception());
}

void TryCatch::Reset() {
  pimpl_->Reset();
}

void TryCatch::SetVerbose(bool verbose) {
  pimpl_->SetVerbose(verbose);
}

}
