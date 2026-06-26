#include "agentxx/expand/text_selection_monitor.h"
#include "agentxx/util/log.h"
#include <mutex>
#include <thread>

#if XX_IS_WIN_D
#include <UIAutomation.h>
#include <windows.h>

#endif

namespace agentxx {
namespace expand {

#if XX_IS_WIN_D

class TextSelectionMonitor::Impl {
public:
  Impl() : running_(false), debounceMs_(300), lastEventTime_({}) {}

  ~Impl() { stop(); }

  void addListener(TextSelectionListener listener) {
    std::lock_guard<std::mutex> lock(listenersMutex_);
    listeners_.push_back(std::move(listener));
  }

  void removeAllListeners() {
    std::lock_guard<std::mutex> lock(listenersMutex_);
    listeners_.clear();
  }

  void setDebounceMs(int ms) { debounceMs_ = ms; }

  bool start() {
    if (running_.load()) {
      return true;
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED |
                                             COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
      XX_LOGE("TextSelectionMonitor: CoInitializeEx failed, hr=0x{:08X}",
              static_cast<unsigned>(hr));
      return false;
    }

    hr = CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                          IID_IUIAutomation,
                          reinterpret_cast<void **>(&automation_));
    if (FAILED(hr) || !automation_) {
      XX_LOGE("TextSelectionMonitor: CoCreateInstance CUIAutomation failed, "
              "hr=0x{:08X}",
              static_cast<unsigned>(hr));
      CoUninitialize();
      return false;
    }

    instancePtr() = this;
    running_.store(true);
    workerThread_ = std::thread(&Impl::workerLoop, this);
    XX_LOGI("TextSelectionMonitor: started");
    return true;
  }

  void stop() {
    if (!running_.load()) {
      return;
    }

    running_.store(false);

    if (hook_) {
      UnhookWinEvent(hook_);
      hook_ = nullptr;
    }

    if (workerThread_.joinable()) {
      PostThreadMessageW(GetThreadId(workerThread_.native_handle()), WM_QUIT, 0,
                         0);
      workerThread_.join();
    }

    if (automation_) {
      automation_->Release();
      automation_ = nullptr;
    }

    instancePtr() = nullptr;
    CoUninitialize();
    XX_LOGI("TextSelectionMonitor: stopped");
  }

  bool isRunning() const { return running_.load(); }

private:
  void workerLoop() {
    hook_ = SetWinEventHook(EVENT_OBJECT_TEXTSELECTIONCHANGED,
                            EVENT_OBJECT_TEXTSELECTIONCHANGED, nullptr,
                            WinEventProc, 0, 0,
                            WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    if (!hook_) {
      XX_LOGE("TextSelectionMonitor: SetWinEventHook failed");
      running_.store(false);
      return;
    }

    MSG msg;
    while (running_.load()) {
      while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
          return;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
      }

      if (running_.load() &&
          MsgWaitForMultipleObjects(0, nullptr, FALSE, 100, QS_ALLINPUT) ==
              WAIT_TIMEOUT) {
        continue;
      }
    }
  }

  static void CALLBACK WinEventProc(HWINEVENTHOOK /*hWinEventHook*/,
                                    DWORD event, HWND hwnd, LONG idObject,
                                    LONG /*idChild*/, DWORD /*dwEventThread*/,
                                    DWORD /*dwmsEventTime*/) {
    if (event != EVENT_OBJECT_TEXTSELECTIONCHANGED) {
      return;
    }

    if (idObject != OBJID_CLIENT && idObject != OBJID_WINDOW) {
      return;
    }

    Impl *self = instancePtr();
    if (!self || !self->running_.load()) {
      return;
    }

    auto now = std::chrono::steady_clock::now();
    {
      std::lock_guard<std::mutex> lock(self->debounceMutex_);
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
          now - self->lastEventTime_);
      if (elapsed.count() < self->debounceMs_) {
        return;
      }
      self->lastEventTime_ = now;
    }

    auto [selectedText, source] = self->getSelectedText(hwnd);
    if (selectedText.empty()) {
      return;
    }

    TextSelectionEvent evt;
    evt.text = std::move(selectedText);
    evt.timestamp = now;
    evt.source = source;

    self->notifyListeners(evt);
  }

  std::pair<std::string, TextSource> getSelectedText(HWND hwnd) {
    if (!automation_) {
      return {};
    }

    IUIAutomationElement *element = nullptr;
    HRESULT hr = automation_->ElementFromHandle(hwnd, &element);
    if (FAILED(hr) || !element) {
      return {};
    }

    IUIAutomationTextPattern *textPattern = nullptr;
    TextSource source = TextSource::TextPattern;
    hr = element->GetCurrentPatternAs(UIA_TextPatternId,
                                      IID_IUIAutomationTextPattern,
                                      reinterpret_cast<void **>(&textPattern));
    if (FAILED(hr) || !textPattern) {
      IUIAutomationElement *focused = nullptr;
      hr = automation_->GetFocusedElement(&focused);
      if (SUCCEEDED(hr) && focused) {
        hr = focused->GetCurrentPatternAs(
            UIA_TextPatternId, IID_IUIAutomationTextPattern,
            reinterpret_cast<void **>(&textPattern));
        focused->Release();
      }
    }

    if (!textPattern) {
      IUIAutomationTextChildPattern *textChildPattern = nullptr;
      hr = element->GetCurrentPatternAs(
          UIA_TextChildPatternId, IID_IUIAutomationTextChildPattern,
          reinterpret_cast<void **>(&textChildPattern));
      if (SUCCEEDED(hr) && textChildPattern) {
        IUIAutomationElement *container = nullptr;
        hr = textChildPattern->get_TextContainer(&container);
        if (SUCCEEDED(hr) && container) {
          hr = container->GetCurrentPatternAs(
              UIA_TextPatternId, IID_IUIAutomationTextPattern,
              reinterpret_cast<void **>(&textPattern));
          container->Release();
        }
        textChildPattern->Release();
        if (textPattern) {
          source = TextSource::TextChildPattern;
        }
      }
    }

    element->Release();

    if (textPattern) {
      std::string result;

      IUIAutomationTextRangeArray *selectionRanges = nullptr;
      hr = textPattern->GetSelection(&selectionRanges);
      if (SUCCEEDED(hr) && selectionRanges) {
        int count = 0;
        selectionRanges->get_Length(&count);

        for (int i = 0; i < count; ++i) {
          IUIAutomationTextRange *range = nullptr;
          hr = selectionRanges->GetElement(i, &range);
          if (SUCCEEDED(hr) && range) {
            BSTR text = nullptr;
            hr = range->GetText(-1, &text);
            if (SUCCEEDED(hr) && text) {
              int len = SysStringLen(text);
              if (len > 0) {
                int utf8Len = WideCharToMultiByte(CP_UTF8, 0, text, len,
                                                  nullptr, 0, nullptr, nullptr);
                if (utf8Len > 0) {
                  result.resize(utf8Len);
                  WideCharToMultiByte(CP_UTF8, 0, text, len, &result[0],
                                      utf8Len, nullptr, nullptr);
                }
              }
              SysFreeString(text);
            }
            range->Release();
          }
        }
        selectionRanges->Release();
      }

      textPattern->Release();
      return {result, source};
    }

    {
      IUIAutomationElement *elemForValue = nullptr;
      hr = automation_->ElementFromHandle(hwnd, &elemForValue);
      if (SUCCEEDED(hr) && elemForValue) {
        IUIAutomationValuePattern *valuePattern = nullptr;
        hr = elemForValue->GetCurrentPatternAs(
            UIA_ValuePatternId, IID_IUIAutomationValuePattern,
            reinterpret_cast<void **>(&valuePattern));
        if (SUCCEEDED(hr) && valuePattern) {
          BSTR value = nullptr;
          hr = valuePattern->get_CurrentValue(&value);
          if (SUCCEEDED(hr) && value) {
            std::string result;
            int len = SysStringLen(value);
            if (len > 0) {
              int utf8Len = WideCharToMultiByte(CP_UTF8, 0, value, len, nullptr,
                                                0, nullptr, nullptr);
              if (utf8Len > 0) {
                result.resize(utf8Len);
                WideCharToMultiByte(CP_UTF8, 0, value, len, &result[0], utf8Len,
                                    nullptr, nullptr);
              }
            }
            SysFreeString(value);
            valuePattern->Release();
            elemForValue->Release();
            return {result, TextSource::ValuePattern};
          }
          if (value) {
            SysFreeString(value);
          }
          valuePattern->Release();
        }
        elemForValue->Release();
      }
    }

    return {};
  }

  void notifyListeners(const TextSelectionEvent &event) {
    std::lock_guard<std::mutex> lock(listenersMutex_);
    for (auto &listener : listeners_) {
      if (listener) {
        listener(event);
      }
    }
  }

  static Impl *&instancePtr() {
    static Impl *ptr = nullptr;
    return ptr;
  }

  std::atomic<bool> running_;
  std::thread workerThread_;
  HWINEVENTHOOK hook_ = nullptr;
  IUIAutomation *automation_ = nullptr;
  int debounceMs_;
  std::chrono::steady_clock::time_point lastEventTime_;
  std::mutex debounceMutex_;
  std::mutex listenersMutex_;
  std::vector<TextSelectionListener> listeners_;
};

#else

class TextSelectionMonitor::Impl {
public:
  Impl() = default;
  ~Impl() = default;

  void addListener(TextSelectionListener /*listener*/) {}
  void removeAllListeners() {}
  void setDebounceMs(int /*ms*/) {}

  bool start() {
    XX_LOGW("TextSelectionMonitor: not supported on this platform");
    return false;
  }

  void stop() {}
  bool isRunning() const { return false; }
};

#endif

TextSelectionMonitor::TextSelectionMonitor()
    : impl_(std::make_unique<Impl>()) {}

TextSelectionMonitor::~TextSelectionMonitor() = default;

void TextSelectionMonitor::addListener(TextSelectionListener listener) {
  impl_->addListener(std::move(listener));
}

void TextSelectionMonitor::removeAllListeners() { impl_->removeAllListeners(); }

void TextSelectionMonitor::setDebounceMs(int ms) { impl_->setDebounceMs(ms); }

bool TextSelectionMonitor::start() { return impl_->start(); }

void TextSelectionMonitor::stop() { impl_->stop(); }

bool TextSelectionMonitor::isRunning() const { return impl_->isRunning(); }

} // namespace expand
} // namespace agentxx