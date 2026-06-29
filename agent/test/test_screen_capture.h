#pragma once

#include "agentxx/expand/screen_capture.h"
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#if XX_IS_WIN_D
#include <windows.h>
#endif
#include <assert.h>

inline void test_screen_capture() {
#if XX_IS_WIN_D
  std::cout << "======= ScreenCapture Test =======" << std::endl;

  agentxx::expand::ScreenCapture capture;

  int screen_count = capture.getScreenCount();
  std::cout << "[INFO] 检测到 " << screen_count << " 个屏幕" << std::endl;
  if (screen_count <= 0) {
    std::cerr << "[FAIL] getScreenCount() 返回 <= 0" << std::endl;
    assert(false);
    return;
  }
  std::cout << "[PASS] getScreenCount() = " << screen_count << std::endl;

  {
    auto frames = capture.captureAllScreens();
    std::cout << "[INFO] captureAllScreens() 返回 " << frames.size() << " 帧"
              << std::endl;
    if (frames.empty()) {
      std::cerr << "[FAIL] captureAllScreens() 返回空数组" << std::endl;
      assert(false);
      return;
    }

    for (size_t i = 0; i < frames.size(); ++i) {
      const auto &f = frames[i];
      std::cout << "  Frame[" << i << "]: " << f.width << "x" << f.height
                << " offset=(" << f.offsetX << "," << f.offsetY << ")"
                << " screenIndex=" << f.screenIndex
                << " name=" << f.screenName
                << " primary=" << (f.isPrimary ? "true" : "false")
                << " pixels=" << f.pixelData.size() << " bytes" << std::endl;

      if (f.width <= 0 || f.height <= 0) {
        std::cerr << "[FAIL] Frame[" << i << "] 宽高无效" << std::endl;
        assert(false);
        return;
      }
      if (f.pixelData.size() != static_cast<size_t>(f.width * f.height * 4)) {
        std::cerr << "[FAIL] Frame[" << i << "] 像素数据大小不匹配: 期望 "
                  << (f.width * f.height * 4) << " 实际 " << f.pixelData.size()
                  << std::endl;
        assert(false);
        return;
      }
    }
    std::cout << "[PASS] captureAllScreens() 所有帧信息正确" << std::endl;
  }

  {
    auto frame = capture.captureScreen(0);
    std::cout << "[INFO] captureScreen(0): " << frame.width << "x"
              << frame.height << " offset=(" << frame.offsetX << ","
              << frame.offsetY << ") name=" << frame.screenName << std::endl;
    if (frame.width <= 0 || frame.height <= 0) {
      std::cerr << "[FAIL] captureScreen(0) 无效" << std::endl;
      assert(false);
      return;
    }
    if (frame.pixelData.empty()) {
      std::cerr << "[FAIL] captureScreen(0) 像素数据为空" << std::endl;
      assert(false);
      return;
    }
    std::cout << "[PASS] captureScreen(0) 正确" << std::endl;
  }

  {
    auto frame = capture.captureMouseScreen();
    std::cout << "[INFO] captureMouseScreen(): " << frame.width << "x"
              << frame.height << " offset=(" << frame.offsetX << ","
              << frame.offsetY << ") name=" << frame.screenName << std::endl;
    if (frame.width <= 0 || frame.height <= 0) {
      std::cerr << "[FAIL] captureMouseScreen() 无效" << std::endl;
      assert(false);
      return;
    }
    if (frame.pixelData.empty()) {
      std::cerr << "[FAIL] captureMouseScreen() 像素数据为空" << std::endl;
      assert(false);
      return;
    }
    std::cout << "[PASS] captureMouseScreen() 正确" << std::endl;
  }

  {
    auto frame = capture.captureScreen(9999);
    if (frame.width == 0 && frame.height == 0 && frame.pixelData.empty()) {
      std::cout << "[PASS] captureScreen(9999) 无效索引正确返回空帧"
                << std::endl;
    } else {
      std::cerr << "[FAIL] captureScreen(9999) 应返回空帧" << std::endl;
      assert(false);
      return;
    }
  }

  {
    std::cout << "[INFO] 测试流式订阅 (5 fps, 持续 2 秒)..." << std::endl;
    int frame_count = 0;
    bool streaming_started = capture.startStreaming(
        5, [&frame_count](const std::vector<agentxx::expand::ScreenFrame> &) {
          frame_count++;
        });

    if (!streaming_started) {
      std::cerr << "[FAIL] startStreaming() 返回 false" << std::endl;
      assert(false);
      return;
    }
    std::cout << "[PASS] startStreaming() 成功启动" << std::endl;

    if (!capture.isStreaming()) {
      std::cerr << "[FAIL] isStreaming() 应返回 true" << std::endl;
      assert(false);
      return;
    }
    std::cout << "[PASS] isStreaming() = true" << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(2200));

    capture.stopStreaming();

    if (capture.isStreaming()) {
      std::cerr << "[FAIL] stopStreaming() 后 isStreaming() 应返回 false"
                << std::endl;
      assert(false);
      return;
    }
    std::cout << "[PASS] stopStreaming() 成功停止" << std::endl;

    std::cout << "[INFO] 2 秒内收到 " << frame_count << " 帧 (期望约 10 帧)"
              << std::endl;
    if (frame_count < 5) {
      std::cerr << "[FAIL] 流式订阅帧数过少: " << frame_count << std::endl;
      assert(false);
      return;
    }
    std::cout << "[PASS] 流式订阅帧率正常" << std::endl;
  }

  std::cout << "======= ScreenCapture Test All Passed =======" << std::endl;
#else
  std::cout << "======= ScreenCapture Test =======" << std::endl;
  std::cout << "[SKIP] ScreenCapture 仅支持 Windows 平台" << std::endl;
#endif
}