#ifndef FLUTTER_INAPPWEBVIEW_PLUGIN_CALLBACK_UTIL_H_
#define FLUTTER_INAPPWEBVIEW_PLUGIN_CALLBACK_UTIL_H_

#include <exception>
#include <string>
#include <utility>
#include <Windows.h>
#include <wrl.h>

#include "log.h"
#include "strconv.h"

namespace flutter_inappwebview_plugin
{
  // WebView2のイベント/完了ハンドラ（COMのInvoke）から、C++例外を外へ出さない
  // ためのラッパー。例外がCOM境界を越えると動作は未定義で、実際に不正な
  // WebMessage（キー欠け）を1通受け取っただけで、そのWebViewが双方向とも応答
  // しなくなることを確認している（wonder-screen-factor Issue #89）。
  //
  // 使い方は Microsoft::WRL::Callback と同じで、`Callback<T>(lambda)` を
  // `SafeCallback<T>(lambda)` に置き換えるだけでよい。例外を捕まえた場合は
  // ログを残し、そのハンドラの処理だけを打ち切って S_OK を返す（イベント
  // ハンドラの戻り値はWebView2側で使われないため）。
  //
  // 注意: /EHsc のため、アクセス違反などのSEH例外は対象外。
  namespace callback_detail
  {
    inline void logCallbackException(const std::string& what)
    {
      auto message = "Exception escaped from a WebView2 callback and was suppressed: " + what;
      // debugLog はリリースビルドでは何も出力しないため、実機でも DebugView 等で
      // 確認できるよう、ここでは常に OutputDebugString へ出す。
      OutputDebugString(utf8_to_wide("\n[flutter_inappwebview] " + message + "\n").c_str());
      debugLog(message, true);
    }

    template <typename TInterface, typename TMethod>
    struct SafeCallbackFactory;

    template <typename TInterface, typename... TArgs>
    struct SafeCallbackFactory<TInterface, HRESULT(STDMETHODCALLTYPE TInterface::*)(TArgs...)>
    {
      template <typename TLambda>
      static Microsoft::WRL::ComPtr<TInterface> make(TLambda&& lambda)
      {
        return Microsoft::WRL::Callback<TInterface>(
          [lambda = std::forward<TLambda>(lambda)](TArgs... args) -> HRESULT
          {
            try {
              return lambda(args...);
            }
            catch (const std::exception& e) {
              logCallbackException(e.what());
            }
            catch (...) {
              logCallbackException("unknown exception");
            }
            return S_OK;
          });
      }
    };
  }

  template <typename TInterface, typename TLambda>
  static inline Microsoft::WRL::ComPtr<TInterface> SafeCallback(TLambda&& lambda)
  {
    return callback_detail::SafeCallbackFactory<TInterface, decltype(&TInterface::Invoke)>::make(std::forward<TLambda>(lambda));
  }
}

#endif //FLUTTER_INAPPWEBVIEW_PLUGIN_CALLBACK_UTIL_H_
