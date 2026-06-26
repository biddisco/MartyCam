#include <hpx/execution/execution.hpp>
#include <hpx/hpx.hpp>
#include <hpx/hpx_init.hpp>
#include <hpx/program_options.hpp>

#include <csignal>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

#include "debug/logging.hpp"
#include "martycam/core/ConcurrentCircularBuffer.h"
#include "martycam/core/capturethread.h"
#include "martycam/core/stream_buffer_recorder.hpp"

#ifdef __linux__
static constexpr char const* DEFAULT_CAMERA = "/dev/video0";
#else
static constexpr char const* DEFAULT_CAMERA = "0";
#endif

static std::atomic<bool> g_running{true};

static void signal_handler(int)
{
  g_running = false;
}

//----------------------------------------------------------------------------
static int run_capture_fps_display(hpx::program_options::variables_map& vm,
    hpx::execution::parallel_executor const& exec)
{
  std::string const camera = vm["camera"].as<std::string>();
  int const fps = vm["fps"].as<int>();
  int const width = vm["width"].as<int>();
  int const height = vm["height"].as<int>();

  ImageBuffer buffer(new ConcurrentCircularBuffer<cv::Mat>(5));
  CaptureThread capture(buffer, cv::Size(width, height), 0, camera, exec, fps);

  if (!capture.startCapture())
  {
    std::cerr << "[CLI] Failed to start capture thread\n";
    return 1;
  }

  std::cout << "[CLI] Capture started on " << camera << "\n";
  std::cout << "[CLI] Ctrl+C to stop\n\n";

  while (g_running)
  {
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << "Grab FPS: " << capture.getGrabFps() << " | Actual FPS: " << capture.getActualFps()
              << " | Capture FPS: " << capture.getCaptureFps()
              << " | Frames: " << capture.GetFrameCounter() << "\n";
  }

  std::cout << "\n[CLI] Stopping capture...\n";
  capture.stopCapture();
  return 0;
}

//----------------------------------------------------------------------------
static int run_stream_buffer_recorder(hpx::program_options::variables_map& vm,
    hpx::execution::parallel_executor const& exec)
{
  std::string const camera = vm["camera"].as<std::string>();
  std::string const output = vm["output"].as<std::string>();
  int const fps = vm["fps"].as<int>();
  int const width = vm["width"].as<int>();
  int const height = vm["height"].as<int>();

  stream_buffer_recorder recorder(camera, 30.0, exec);
  if (!recorder.open(width, height))
  {
    std::cerr << "[CLI] Failed to open camera: " << camera << "\n";
    return 1;
  }

  if (!output.empty())
  {
    if (!recorder.startRecording(output, static_cast<double>(fps)))
    {
      std::cerr << "[CLI] Failed to start recording to: " << output << "\n";
      return 1;
    }
    std::cout << "[CLI] Recording started: " << output << "\n";
  }

  std::cout << "[CLI] Streaming from " << camera << "\n";
  std::cout << "[CLI] Ctrl+C to stop\n\n";

  cv::Mat frame;
  int frame_count = 0;
  auto last_report = std::chrono::steady_clock::now();

  while (g_running)
  {
    if (recorder.readFrame(frame)) { frame_count++; }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_report).count();
    if (elapsed >= 1)
    {
      std::cout << "FPS: " << frame_count << "\n";
      frame_count = 0;
      last_report = now;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  std::cout << "\n[CLI] Stopping recorder...\n";
  recorder.stopRecording();
  recorder.close();
  return 0;
}

//----------------------------------------------------------------------------
static int hpx_main(hpx::program_options::variables_map& vm)
{
  if (vm.count("help"))
  {
    namespace po = hpx::program_options;
    po::options_description help_opts("MartyCam CLI");
    help_opts.add_options()("help,h", "Show this help message");
    help_opts.add_options()("record,r", po::bool_switch()->default_value(false),
        "Record video using stream buffer recorder");
    help_opts.add_options()("camera,c", po::value<std::string>()->default_value(DEFAULT_CAMERA),
        "Camera device path or URL");
    help_opts.add_options()("output,o", po::value<std::string>()->default_value("output.mp4"),
        "Output filename for recording (used with --record)");
    help_opts.add_options()("fps", po::value<int>()->default_value(30), "Target FPS");
    help_opts.add_options()("width", po::value<int>()->default_value(640), "Capture width");
    help_opts.add_options()("height", po::value<int>()->default_value(480), "Capture height");
    std::cout << help_opts << std::endl;
    hpx::finalize();
    return 0;
  }

  bool const record = vm["record"].as<bool>();

  hpx::execution::parallel_executor defaultExecutor(
      &hpx::resource::get_thread_pool("default"));

  int result = record ? run_stream_buffer_recorder(vm, defaultExecutor)
                      : run_capture_fps_display(vm, defaultExecutor);

  hpx::finalize();
  return result;
}

//----------------------------------------------------------------------------
int main(int argc, char* argv[])
{
  martycam::log::init_from_env();

  namespace po = hpx::program_options;

  po::options_description cmdline("MartyCam CLI");
  cmdline.add_options()("help,h", "Show this help message");
  cmdline.add_options()("record,r", po::bool_switch()->default_value(false),
      "Record video using stream buffer recorder");
  cmdline.add_options()("camera,c", po::value<std::string>()->default_value(DEFAULT_CAMERA),
      "Camera device path or URL");
  cmdline.add_options()("output,o", po::value<std::string>()->default_value("output.mp4"),
      "Output filename for recording (used with --record)");
  cmdline.add_options()("fps", po::value<int>()->default_value(30), "Target FPS");
  cmdline.add_options()("width", po::value<int>()->default_value(640), "Capture width");
  cmdline.add_options()("height", po::value<int>()->default_value(480), "Capture height");

  std::signal(SIGINT, signal_handler);
#ifndef _WIN32
  std::signal(SIGTERM, signal_handler);
#endif

  hpx::init_params init_args;
  init_args.desc_cmdline = cmdline;
  init_args.cfg = {"hpx.max_idle_loop_count=500", "hpx.os_threads=2",
      "hpx.stacks.small_size=0x80000", "hpx.stacks.use_guard_pages=1"};

  return hpx::init(hpx_main, argc, argv, init_args);
}
