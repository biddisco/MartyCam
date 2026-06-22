#include <QLoggingCategory>
#include <QObject>
#include <QThread>
#include <QtGui/QFontDatabase>
#include <QtWidgets/QApplication>
//
#include <fontconfig/fontconfig.h>
//
#include <hpx/functional/bind.hpp>
#include <hpx/future.hpp>
#include <hpx/hpx.hpp>
#include <hpx/hpx_init.hpp>
#include <hpx/program_options.hpp>

#include <cstddef>
#include <vector>

using hpx::chrono::high_resolution_timer;
//
#include "debug/logging.hpp"
#include "martycam.h"
#include "martycam/config-defines.hpp"
#include "martycam/config-python.hpp"

#include <opencv2/opencv.hpp>

#include <boost/lockfree/queue.hpp>
#include "ConcurrentCircularBuffer.h"
#include "capturethread.h"

#include <chrono>
#include <thread>

// save these to pass to Qt init.
static int argc;
static char** argv;

// ----------------------------------------------------------------------------
static auto app_log = martycam::log::create("App-Main");

// ----------------------------------------------------------------------------
int qt_main(hpx::program_options::variables_map& vm)
{
  setenv("QT_NO_GLIB", "1", 1);

  setenv("QT_NO_AVX2", "1", 1);

  // Force FFmpeg to completely skip hardware acceleration checks.
  // This stops libavutil from invoking libva and iHD_drv_video.so entirely.
  setenv("QT_AV_NO_HWACCEL", "1", 1);

  // Alternative option to bypass the Intel VA-API driver explicitly:
  setenv("LIBVA_DRIVER_NAME", "none", 1);

  // Silence the QPA main thread check warning noise
  QLoggingCategory::setFilterRules(QStringLiteral("qt.qpa.core.warning=false"));

  // make sure Qt < everything > is created on *<this thread>*
  QApplication app(argc, argv);

  // Force Fontconfig to initialize safely on the main GUI thread
  QFontDatabase::font("Helvetica", "Normal", 10);

  // setup resources that Qt uses for pics etc
  // Q_INIT_RESOURCE(images);
  QCoreApplication::setOrganizationName("MartyCam");
  QCoreApplication::setOrganizationDomain("MartyCam");
  QCoreApplication::setApplicationName("MartyCam");
  //
  // App icon
  //
  QIcon appIcon(":/images/Marty.png");
  app.setWindowIcon(appIcon);
  //
  hpx::execution::parallel_executor defaultExecutor(&hpx::resource::get_thread_pool("default"));
  std::cout << "[hpx_main] Created default pool_executor" << std::endl;
  MartyCam mcam(defaultExecutor, defaultExecutor);
  mcam.show();

  // Create a dummy timer bound strictly to this worker thread's loop
  QTimer wakeupTimer;
  QObject::connect(&wakeupTimer, &QTimer::timeout, []() {
    // Handled entirely on the worker thread.
    // Doesn't need to do anything; its mere existence forces __ppoll
    // to wake up every 10ms rather than sleeping forever.
  });
  // 10ms interval (~100Hz refresh)
  wakeupTimer.start(10);
  // this app.exec blocks, but is woken repeatedly by the timer
  // allowing us to process events and update the GUI
  app.exec();
  return EXIT_SUCCESS;
}

// void init_resource_partitioner_handler(
//     hpx::resource::partitioner& rp, hpx::program_options::variables_map const&)
// {
//   // Create the resource partitioner
//   std::cout << "[main] obtained reference to the resource_partitioner" << std::endl;

//   rp.create_thread_pool("default", hpx::resource::scheduling_policy::local_priority_fifo);
//   std::cout << "[main] " << "thread_pool default created" << std::endl;

//   // Create a thread pool using the default scheduler provided by HPX
//   rp.create_thread_pool("blocking", hpx::resource::scheduling_policy::local_priority_fifo);

//   std::cout << "[main] " << "thread_pool blocking created" << std::endl;

//   int blocking_tp_num_threads = 3;    // vm["blocking_tp_num_threads"].as<int>();

//   // add PUs to opencv pool
//   int count = 0;
//   for (hpx::resource::numa_domain const& d : rp.numa_domains())
//   {
//     for (hpx::resource::core const& c : d.cores())
//     {
//       for (hpx::resource::pu const& p : c.pus())
//       {
//         if (count < blocking_tp_num_threads)
//         {
//           std::cout << "[main] Added pu " << count++ << " to " << "blocking" << " thread pool"
//                     << std::endl;
//           rp.add_resource(p, "blocking");
//         }

//       }

//     }

//   }

//   std::cout << "[main] resources added to thread_pools" << std::endl;
// }

// int hpx_main(int argc, char** argv)
// {
//   // Get a reference to one of the main threads
//   hpx::execution::parallel_executor scheduler(&hpx::resource::get_thread_pool("default"));

//   // run an async function on the main thread to start the Qt application
//   // Store a pointer to the runtime here.
//   auto rt = hpx::get_runtime_ptr();
//   std::thread qt_thread([&]() {
//     hpx::error_code ec(hpx::throwmode::lightweight);
//     hpx::register_thread(rt, "Qt", ec);
//     //
//     qt_main(argc, argv);
//     //
//     hpx::unregister_thread(rt);
//   });
//   qt_thread.join();

//   return hpx::finalize();
// }

// int main(int argc, char** argv)
// {
//   namespace po = hpx::program_options;
//   po::options_description desc_cmdline("Options");
//   desc_cmdline.add_options()("blocking_tp_num_threads,m", po::value<int>()->default_value(1),
//       "Number of threads to assign to blocking pool");

//   // HPX uses a boost program options variable map, but we need it before
//   // hpx-main, so we will create another one here and throw it away after use
//   po::variables_map vm;
//   try
//   {
//     po::store(
//         po::command_line_parser(argc, argv).allow_unregistered().options(desc_cmdline).run(), vm);
//   }

//   catch (po::error& e)
//   {
//     std::cerr << "ERROR: " << e.what() << "\n\n";
//     std::cerr << desc_cmdline << std::endl;
//     return -1;
//   }

//   // Setup the init parameters
//   hpx::init_params init_args;
//   init_args.desc_cmdline = desc_cmdline;

//   // Set the callback to init the thread_pools
//   init_args.rp_callback = &init_resource_partitioner_handler;
//   return hpx::init(argc, argv, init_args);
// }

//----------------------------------------------------------------------------
std::string qt_pool_name = "Qt:pool";

//----------------------------------------------------------------------------
int hpx_main(hpx::program_options::variables_map& vm)
{
  namespace ex = hpx::execution::experimental;
  namespace tt = hpx::this_thread::experimental;

  // Get a scheduler on the thread pool we have reserved for Qt
  auto qt_sch = ex::thread_pool_scheduler{&hpx::resource::get_thread_pool(qt_pool_name)};

  // create a sender to transfer work to the qt pool scheduler
  auto snd = ex::just() | ex::continues_on(qt_sch) | ex::then([&vm]() {
    // run the main qt application entry on our thread
    qt_main(vm);
  });

  // launch and block on completion of the Qt application thread
  tt::sync_wait(std::move(snd));

  // allow hpx to shutdown
  hpx::finalize();
  return 0;
}

//----------------------------------------------------------------------------
void init_resource_partitioner_handler(
    hpx::resource::partitioner& rp, hpx::program_options::variables_map const& vm)
{
  // Don't create the pool if the user disabled it
  if (vm["no-qt-pool"].as<bool>())
  {
    qt_pool_name = "default";
    return;
  }

  using hpx::threads::policies::scheduler_mode;
#ifdef MARTYCAM_DISABLE_IDLE_BACKOFF
  auto mode = scheduler_mode::default_;
  mode = static_cast<scheduler_mode>(static_cast<std::uint32_t>(scheduler_mode::default_) &
      ~static_cast<std::uint32_t>(scheduler_mode::enable_idle_backoff));
#else
  auto mode = scheduler_mode::default_ | scheduler_mode::enable_idle_backoff;
#endif

  // Create a thread pool with a single core for Qt
  rp.create_thread_pool(qt_pool_name, hpx::resource::scheduling_policy::unspecified, mode);
  // set the schedule mode for the default pool
  //  rp.create_thread_pool("default", hpx::resource::scheduling_policy::shared_priority, mode);
  rp.create_thread_pool("default", hpx::resource::scheduling_policy::unspecified, mode);
  rp.add_resource(rp.numa_domains()[0].cores()[0].pus()[0], qt_pool_name);
}

//----------------------------------------------------------------------------
// the normal int main function that is called at startup and runs on an OS
// thread the user must call hpx::init to start the hpx runtime which
// will execute hpx_main on an hpx thread

int main(int argc, char* argv[])
{
  // Force standard, native OS thread initialization of fontconfig
  FcInit();

  // fix: Qt depends on a UTF-8 locale, and has switched to "C.UTF-8" instead
  // TODO: Find a real fix
  setenv("LC_ALL", "C.UTF-8", 1);

  // fix: Qt: Session management error: Could not open network socket
  // TODO: Find a real fix
  unsetenv("SESSION_MANAGER");

  martycam::log::init_from_env();

  //
  ::argc = argc;
  ::argv = argv;

  namespace po = hpx::program_options;

  // Configure application-specific options.
  po::options_description cmdline("usage: grox [options]");

  // clang-format off
  cmdline.add_options()("no-qt-pool",
    hpx::program_options::bool_switch(),
    "Disable the Qt pool.");

  cmdline.add_options()("decode",
    hpx::program_options::bool_switch(),
    "shortcut");

  cmdline.add_options()("python-debug",
     hpx::program_options::bool_switch(),
     "Enable debug mode for Python scripts");

  // po::variables_map vm;
  // po::store(po::command_line_parser(argc, argv)
  //                                .allow_unregistered()
  //                                .options(cmdline)
  //                                .run(), vm);
  // clang-format on

  // Initialize and run hpx.
  hpx::init_params init_args;
  init_args.desc_cmdline = cmdline;
  // Set the callback to init thread_pools
  init_args.rp_callback = &init_resource_partitioner_handler;
  // tell the scheduler to sleep quickly when there are no tasks to work on
  init_args.cfg = {
      "hpx.max_idle_loop_count=500",      // go into idle after 10 loops with no work
      "hpx.os_threads=6",                 // use 6 cores
      "hpx.stacks.small_size=0x80000",    // Increase stack size if needed
      "hpx.stacks.use_guard_pages=1"      // Enables hardware stack boundaries
  };
  auto result = hpx::init(hpx_main, argc, argv, init_args);
  return result;
}
