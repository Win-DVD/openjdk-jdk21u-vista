#ifndef SHARE_UTILITIES_THREADLOCALVALUE_HPP
#define SHARE_UTILITIES_THREADLOCALVALUE_HPP

#include "utilities/debug.hpp"
#include <type_traits>

#if defined(_WIN32)
#include <windows.h>
#endif

template <typename T>
class ThreadLocalValue {
 public:
  ThreadLocalValue()
      : _initial_value()
#if defined(_WIN32)
      , _tls_index(TLS_OUT_OF_INDEXES)
      , _tls_state(0)
#endif
  {}

  explicit ThreadLocalValue(const T& initial_value)
      : _initial_value(initial_value)
#if defined(_WIN32)
      , _tls_index(TLS_OUT_OF_INDEXES)
      , _tls_state(0)
#endif
  {}

  T& value() {
#if defined(_WIN32)
    ensure_tls();
    T* data = static_cast<T*>(TlsGetValue(_tls_index));
    if (data == nullptr) {
      data = new T(_initial_value);
      BOOL ok = TlsSetValue(_tls_index, data);
      assert(ok, "TlsSetValue failed with error code: %lu", GetLastError());
    }
    return *data;
#else
    thread_local T data = _initial_value;
    return data;
#endif
  }

  const T& value() const {
    return const_cast<ThreadLocalValue*>(this)->value();
  }

  ThreadLocalValue& operator=(const T& value) {
    this->value() = value;
    return *this;
  }

  operator T&() {
    return value();
  }

  operator const T&() const {
    return value();
  }

  template <typename U = T>
  U operator->() const {
    static_assert(std::is_pointer<U>::value, "operator-> requires pointer type");
    return value();
  }

 private:
#if defined(_WIN32)
  void ensure_tls() {
    if (_tls_state == 2) {
      return;
    }
    LONG previous = InterlockedCompareExchange(&_tls_state, 1, 0);
    if (previous == 0) {
      DWORD index = TlsAlloc();
      guarantee(index != TLS_OUT_OF_INDEXES, "TlsAlloc failed: out of indices");
      _tls_index = index;
      InterlockedExchange(&_tls_state, 2);
      return;
    }
    while (_tls_state != 2) {
      Sleep(0);
    }
  }

  DWORD _tls_index;
  volatile LONG _tls_state;
#endif
  T _initial_value;
};

#endif // SHARE_UTILITIES_THREADLOCALVALUE_HPP