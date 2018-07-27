#ifdef WIN32
 #include <windows.h>
#endif

#include <hpx/config.hpp>
#include <hpx/hpx_init.hpp>
#include <hpx/include/async.hpp>
#include <hpx/include/iostreams.hpp>
#include <hpx/parallel/execution.hpp>
#include <hpx/runtime/resource/partitioner.hpp>
//
#include "system_characteristics.hpp"
//
#include "martycam.h"

#pragma comment(linker, "/NODEFAULTLIB:atlthunk.lib")

void print_system_params() {
    // print partition characteristics
    hpx::cout << "\n\n[hpx_main] print resource_partitioner characteristics : "
              << "\n";
    hpx::resource::get_partitioner().print_init_pool_data(std::cout);

    // print partition characteristics
    hpx::cout << "\n\n[hpx_main] print thread-manager pools : "
              << "\n";
    hpx::threads::get_thread_manager().print_pools(std::cout);

    // print system characteristics
    hpx::cout << "\n\n[hpx_main] print System characteristics : "
              << "\n";
    print_system_characteristics();
}


void qt_main(int argc, char ** argv) {
#ifdef AAWIN32
    AllocConsole();
    freopen("conin$", "r", stdin);                                                `
    freopen("conout$", "w", stdout);
    freopen("conout$", "w", stderr);

    //
    // Prevent machine going to sleep whilst MartyCam is running
    //
    if (SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED) == NULL)
    {
       // try XP variant as well just to make sure
       SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
    }
#endif

  //  QApplication app(nCmdShow, NULL);
  QApplication app(argc, argv);
  QCoreApplication::setOrganizationName("MartyCam");
  QCoreApplication::setOrganizationDomain("MartyCam");
  QCoreApplication::setApplicationName("MartyCam");
  //
  // App icon
  //
  QIcon appIcon(":/images/Marty.png");
  app.setWindowIcon(appIcon);
  //
  //
  //
  hpx::threads::executors::pool_executor defaultExecutor("default");
  hpx::threads::executors::pool_executor blockingExecutor("blocking");
  hpx::cout << "[hpx_main] Created default and " << "blocking"
            << " pool_executors \n";
  MartyCam *tracker = new MartyCam(defaultExecutor, blockingExecutor);
  tracker->show();
  app.exec();
  //
  // set power state back to normal
  //
#ifdef WIN32
    SetThreadExecutionState(ES_CONTINUOUS);
#endif
}

int hpx_main(int argc, char ** argv)
{
    hpx::cout << "[hpx_main] starting hpx_main in "
              << hpx::threads::get_pool(hpx::threads::get_self_id())->get_pool_name()
              << "\n";

    print_system_params();

    hpx::threads::executors::pool_executor blockingExecutor("blocking");
    hpx::future<void> qt_application
            = hpx::async(blockingExecutor, qt_main, argc, argv);

  {
//    // Get a reference to one of the main thread
//    hpx::threads::executors::main_pool_executor scheduler;
//    // run an async function on the main thread to start the Qt application

//
//    // do something else while qt is executing in the background ...
//
//    qt_application.wait();
  }


    return hpx::finalize();
}


//int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
int main(int argc, char **argv) {

    namespace po = boost::program_options;
    po::options_description desc_cmdline("Options");
    desc_cmdline.add_options()
            ("blocking_tp_num_threads,m",
             po::value<int>()->default_value(1),
             "Number of threads to assign to blocking pool");

    // HPX uses a boost program options variable map, but we need it before
    // hpx-main, so we will create another one here and throw it away after use
    po::variables_map vm;
    try {
        po::store(po::command_line_parser(argc, argv).allow_unregistered()
                          .options(desc_cmdline).run(), vm);
    }
    catch(po::error& e) {
        std::cerr << "ERROR: " << e.what() << "\n\n";
        std::cerr << desc_cmdline << "\n";
        return -1;
    }

    int blocking_tp_num_threads = vm["blocking_tp_num_threads"].as<int>();

    // Create the resource partitioner
    hpx::resource::partitioner rp(desc_cmdline, argc, argv);
    std::cout << "[main] obtained reference to the resource_partitioner\n";

    rp.create_thread_pool("default",
                          hpx::resource::scheduling_policy::local_priority_fifo);
    std::cout << "[main] " << "thread_pool default created\n";

    // Create a thread pool using the default scheduler provided by HPX
    rp.create_thread_pool("blocking",
                          hpx::resource::scheduling_policy::local_priority_fifo);

    std::cout << "[main] " << "thread_pool blocking created\n";

    // add PUs to opencv pool
    int count = 0;
    for (const hpx::resource::numa_domain& d : rp.numa_domains())
    {
        for (const hpx::resource::core& c : d.cores())
        {
            for (const hpx::resource::pu& p : c.pus())
            {
                if (count < blocking_tp_num_threads)
                {
                    std::cout << "[main] Added pu " << count++ << " to " <<
                              "blocking" << "thread pool\n";
                    rp.add_resource(p, "blocking");
                }
            }
        }
    }

    std::cout << "[main] resources added to thread_pools \n";

    return hpx::init(argc, argv);
}
