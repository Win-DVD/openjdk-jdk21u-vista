#ifndef SHARE_UTILITIES_THREADLOCALVALUE_HPP
#define SHARE_UTILITIES_THREADLOCALVALUE_HPP

#include "utilities/debug.hpp"
#include <type_traits>

#if defined(_WIN32)
#include <windows.h>
#endif

#if defined(_WIN32)
struct ThreadLocalValueCleanupNode {
  void* value;
  DWORD value_tls_index;
  void (*destroy)(void*);
  ThreadLocalValueCleanupNode* next;
};

struct ThreadLocalValueIndexNode {
  DWORD value_tls_index;
  ThreadLocalValueIndexNode* next;
};

class ThreadLocalValueSupport : public AllStatic {
 private:
  static DWORD& cleanup_tls_index() {
    static DWORD index = TLS_OUT_OF_INDEXES;
    return index;
  }

  static volatile LONG& cleanup_tls_state() {
    static volatile LONG state = 0;
    return state;
  }

  static ThreadLocalValueIndexNode*& tls_index_head() {
    static ThreadLocalValueIndexNode* head = nullptr;
    return head;
  }

  static volatile LONG& tls_index_lock() {
    static volatile LONG state = 0;
    return state;
  }

  static void lock_tls_index_list() {
    while (InterlockedCompareExchange(&tls_index_lock(), 1, 0) != 0) {
      Sleep(0);
    }
  }

  static void unlock_tls_index_list() {
    InterlockedExchange(&tls_index_lock(), 0);
  }

  static void ensure_cleanup_tls() {
    if (cleanup_tls_state() == 2) {
      return;
    }
    LONG previous = InterlockedCompareExchange(&cleanup_tls_state(), 1, 0);
    if (previous == 0) {
      DWORD index = TlsAlloc();
      guarantee(index != TLS_OUT_OF_INDEXES, "TlsAlloc failed: out of indices");
      cleanup_tls_index() = index;
      InterlockedExchange(&cleanup_tls_state(), 2);
      return;
    }
    while (cleanup_tls_state() != 2) {
      Sleep(0);
    }
  }

 public:
  static void add_cleanup_node(ThreadLocalValueCleanupNode* node) {
    ensure_cleanup_tls();
    ThreadLocalValueCleanupNode* head = static_cast<ThreadLocalValueCleanupNode*>(TlsGetValue(cleanup_tls_index()));
    node->next = head;
    BOOL ok = TlsSetValue(cleanup_tls_index(), node);
    assert(ok, "TlsSetValue failed with error code: %lu", GetLastError());
  }

  static void remove_cleanup_node(DWORD value_tls_index) {
    if (cleanup_tls_state() != 2) {
      return;
    }
    ThreadLocalValueCleanupNode* head = static_cast<ThreadLocalValueCleanupNode*>(TlsGetValue(cleanup_tls_index()));
    ThreadLocalValueCleanupNode* prev = nullptr;
    ThreadLocalValueCleanupNode* current = head;
    while (current != nullptr) {
      if (current->value_tls_index == value_tls_index) {
        if (prev == nullptr) {
          head = current->next;
        } else {
          prev->next = current->next;
        }
        delete current;
        BOOL ok = TlsSetValue(cleanup_tls_index(), head);
        assert(ok, "TlsSetValue failed with error code: %lu", GetLastError());
        return;
      }
      prev = current;
      current = current->next;
    }
  }

  static void on_thread_detach() {
    if (cleanup_tls_state() != 2) {
      return;
    }
    ThreadLocalValueCleanupNode* node = static_cast<ThreadLocalValueCleanupNode*>(TlsGetValue(cleanup_tls_index()));
    BOOL ok = TlsSetValue(cleanup_tls_index(), nullptr);
    assert(ok, "TlsSetValue failed with error code: %lu", GetLastError());
    while (node != nullptr) {
      ThreadLocalValueCleanupNode* next = node->next;
      ok = TlsSetValue(node->value_tls_index, nullptr);
      assert(ok, "TlsSetValue failed with error code: %lu", GetLastError());
      node->destroy(node->value);
      delete node;
      node = next;
    }
  }

  static void on_process_detach() {
    lock_tls_index_list();
    ThreadLocalValueIndexNode* node = tls_index_head();
    tls_index_head() = nullptr;
    unlock_tls_index_list();
    while (node != nullptr) {
      ThreadLocalValueIndexNode* next = node->next;
      TlsFree(node->value_tls_index);
      delete node;
      node = next;
    }
    if (cleanup_tls_state() != 2) {
      return;
    }
    TlsFree(cleanup_tls_index());
    cleanup_tls_index() = TLS_OUT_OF_INDEXES;
    InterlockedExchange(&cleanup_tls_state(), 0);
  }

  static void register_tls_index(DWORD value_tls_index) {
    ThreadLocalValueIndexNode* node = new ThreadLocalValueIndexNode();
    node->value_tls_index = value_tls_index;
    lock_tls_index_list();
    node->next = tls_index_head();
    tls_index_head() = node;
    unlock_tls_index_list();
  }

};

inline void thread_local_value_on_thread_detach() {
  ThreadLocalValueSupport::on_thread_detach();
}

inline void thread_local_value_on_process_detach() {
  ThreadLocalValueSupport::on_process_detach();
}
#else
inline void thread_local_value_on_thread_detach() {}
inline void thread_local_value_on_process_detach() {}
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

  ThreadLocalValue(const ThreadLocalValue&) = delete;
  ThreadLocalValue& operator=(const ThreadLocalValue&) = delete;

  T& value() {
#if defined(_WIN32)
    ensure_tls();
    T* data = static_cast<T*>(TlsGetValue(_tls_index));
    if (data == nullptr) {
      data = new T(_initial_value);
      BOOL ok = TlsSetValue(_tls_index, data);
      if (!ok) {
        delete data;
        assert(ok, "TlsSetValue failed with error code: %lu", GetLastError());
      }
      ThreadLocalValueCleanupNode* node = new ThreadLocalValueCleanupNode();
      node->value = data;
      node->value_tls_index = _tls_index;
      node->destroy = &destroy_value;
      node->next = nullptr;
      ThreadLocalValueSupport::add_cleanup_node(node);
    }
    return *data;
#else
    return thread_storage().get(this, _initial_value);
#endif
  }

  const T& value() const {
#if defined(_WIN32)
    return const_cast<ThreadLocalValue*>(this)->value();
#else
    return value_for_const();
#endif
  }

  void release_current_thread() {
#if defined(_WIN32)
    if (_tls_state != 2) {
      return;
    }
    T* data = static_cast<T*>(TlsGetValue(_tls_index));
    if (data == nullptr) {
      return;
    }
    ThreadLocalValueSupport::remove_cleanup_node(_tls_index);
    BOOL ok = TlsSetValue(_tls_index, nullptr);
    assert(ok, "TlsSetValue failed with error code: %lu", GetLastError());
    delete data;
#endif
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
  static void destroy_value(void* value) {
    delete static_cast<T*>(value);
  }

  void ensure_tls() {
    if (_tls_state == 2) {
      return;
    }
    LONG previous = InterlockedCompareExchange(&_tls_state, 1, 0);
    if (previous == 0) {
      DWORD index = TlsAlloc();
      guarantee(index != TLS_OUT_OF_INDEXES, "TlsAlloc failed: out of indices");
      _tls_index = index;
      ThreadLocalValueSupport::register_tls_index(index);
      InterlockedExchange(&_tls_state, 2);
      return;
    }
    while (_tls_state != 2) {
      Sleep(0);
    }
  }

  DWORD _tls_index;
  volatile LONG _tls_state;
#else
  struct Node {
    ThreadLocalValue* owner;
    T value;
    Node* next;

    Node(ThreadLocalValue* owner_, const T& initial_value, Node* next_)
      : owner(owner_), value(initial_value), next(next_) {}
  };

  struct ThreadStorage {
    Node* head;

    ThreadStorage() : head(nullptr) {}

    ~ThreadStorage() {
      Node* node = head;
      while (node != nullptr) {
        Node* next = node->next;
        delete node;
        node = next;
      }
      head = nullptr;
    }

    T& get(ThreadLocalValue* owner, const T& initial_value) {
      Node* node = head;
      while (node != nullptr) {
        if (node->owner == owner) {
          return node->value;
        }
        node = node->next;
      }
      Node* created = new Node(owner, initial_value, head);
      head = created;
      return created->value;
    }
  };

  static ThreadStorage& thread_storage() {
    static thread_local ThreadStorage storage;
    return storage;
  }

  const T& value_for_const() const {
    return thread_storage().get(const_cast<ThreadLocalValue*>(this), _initial_value);
  }
#endif
  T _initial_value;
};

#endif // SHARE_UTILITIES_THREADLOCALVALUE_HPP